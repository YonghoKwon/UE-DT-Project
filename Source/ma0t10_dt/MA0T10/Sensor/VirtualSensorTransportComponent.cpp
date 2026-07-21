#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "HAL/PlatformTime.h"
#include "IStompClient.h"
#include "IStompMessage.h"
#include "StompModule.h"

UVirtualSensorTransportComponent::UVirtualSensorTransportComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SendJson(const FString& SensorId, const FString& SensorType, const FString& JsonText)
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();

    if (TransportMode == EVirtualSensorTransportMode::None)
    {
        Result.bSubmitted = true;
        Result.bAccepted = true;
        Result.Message = TEXT("Transport disabled");
    }
    else if (TransportMode == EVirtualSensorTransportMode::SaveToFile)
    {
        Result = SaveJson(SensorId, SensorType, JsonText);
    }
    else if (TransportMode == EVirtualSensorTransportMode::HttpPost)
    {
        Result = SendHttp(SensorId, SensorType, JsonText);
    }
	else if (TransportMode == EVirtualSensorTransportMode::StompWebSocket)
	{
		Result = SendStomp(SensorId, SensorType, TEXT("frame"), 0, JsonText, false, false);
	}
    else
    {
        Result.bSubmitted = true;
        Result.bAccepted = true;
        Result.Message = FString::Printf(TEXT("[%s:%s] dataLength=%d"), *SensorType, *SensorId, JsonText.Len());
        UE_LOG(LogTemp, Log, TEXT("%s"), *Result.Message);
    }

    LastResult = Result;
    OnDataSent.Broadcast(Result);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SendJsonRequest(
	const FString& SensorId,
	const FString& SensorType,
	const FString& DataKind,
	int64 FrameId,
	const FString& JsonText,
	bool bManualRequest)
{
	if (TransportMode == EVirtualSensorTransportMode::StompWebSocket)
	{
		FVirtualSensorTransportResult Result = SendStomp(SensorId, SensorType, DataKind, FrameId, JsonText, bManualRequest, bManualRequest);
		if (bManualRequest)
		{
			LastResult = Result;
		}
		OnDataSent.Broadcast(Result);
		return Result;
	}
	return SendJson(SensorId, SensorType, JsonText);
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SendJsonStreamRequest(
	const FString& SensorId,
	const FString& SensorType,
	const FString& DataKind,
	int64 FrameId,
	const FString& JsonText,
	bool bRequestReceipt)
{
	if (TransportMode == EVirtualSensorTransportMode::StompWebSocket)
	{
		FVirtualSensorTransportResult Result = SendStomp(SensorId, SensorType, DataKind, FrameId, JsonText, false, bRequestReceipt);
		OnDataSent.Broadcast(Result);
		return Result;
	}
	FVirtualSensorTransportResult Result = SendJson(SensorId, SensorType, JsonText);
	Result.SensorId = SensorId;
	Result.SensorType = SensorType;
	Result.DataKind = DataKind;
	Result.bManualRequest = false;
	return Result;
}

void UVirtualSensorTransportComponent::ConfigureTransportProfile(const FVirtualSensorTransportProfile& InProfile)
{
	const bool bBrokerChanged = TransportProfile.BrokerUrl != InProfile.BrokerUrl || TransportProfile.UserName != InProfile.UserName || TransportProfile.AckTopic != InProfile.AckTopic;
	TransportProfile = InProfile;
	TransportProfile.MaxMessageBytes = FMath::Max(1024, TransportProfile.MaxMessageBytes);
	TransportProfile.TimeoutSeconds = FMath::Clamp(TransportProfile.TimeoutSeconds, 1, 120);
	HttpEndpoint = TransportProfile.HttpEndpoint;
	HttpTimeoutSeconds = TransportProfile.TimeoutSeconds;
	if (bBrokerChanged && StompClient.IsValid())
	{
		StompClient->Disconnect();
		StompClient.Reset();
	}
}

void UVirtualSensorTransportComponent::SetSessionCredentials(const FString& InPasscode, const FString& InBearerToken)
{
	SessionPasscode = InPasscode;
	SessionBearerToken = InBearerToken;
}

bool UVirtualSensorTransportComponent::IsStompConnected() const
{
	return StompClient.IsValid() && StompClient->IsConnected();
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::TestConnection()
{
	FVirtualSensorTransportResult Result;
	Result.Protocol = TransportMode == EVirtualSensorTransportMode::StompWebSocket ? TEXT("STOMP/WS") : TEXT("HTTP");
	Result.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	if (TransportMode == EVirtualSensorTransportMode::StompWebSocket)
	{
		EnsureStompClient();
		Result.bSubmitted = StompClient.IsValid();
		Result.bAccepted = IsStompConnected();
		Result.Destination = TransportProfile.BrokerUrl;
		Result.Message = Result.bAccepted ? TEXT("Artemis STOMP 연결됨") : TEXT("Artemis STOMP 연결을 시작했습니다");
	}
	else
	{
		Result = SendHttp(TEXT("CONNECTION-TEST"), TEXT("diagnostic"), TEXT("{\"type\":\"connection-test\"}"));
	}
	LastResult = Result;
	OnDataSent.Broadcast(Result);
	return Result;
}

FString UVirtualSensorTransportComponent::ResolveTopic(const FString& SensorType, const FString& DataKind) const
{
	if (DataKind.Contains(TEXT("export"), ESearchCase::IgnoreCase) || DataKind.Contains(TEXT("pointcloud"), ESearchCase::IgnoreCase)) return TransportProfile.ExportTopic;
	if (SensorType.Contains(TEXT("lidar"), ESearchCase::IgnoreCase)) return TransportProfile.LidarTopic;
	return TransportProfile.CameraTopic;
}

void UVirtualSensorTransportComponent::RequestStompReconnect()
{
	if (StompClient.IsValid())
	{
		StompClient->Disconnect();
		StompClient.Reset();
	}
	EnsureStompClient();
}

void UVirtualSensorTransportComponent::EnsureStompClient()
{
	if (StompClient.IsValid())
	{
		if (!StompClient->IsConnected()) StompClient->Connect();
		return;
	}
	if (TransportProfile.BrokerUrl.IsEmpty()) return;
	StompClient = FStompModule::Get().CreateClient(TransportProfile.BrokerUrl, SessionBearerToken);
	StompClient->OnConnected().AddUObject(this, &UVirtualSensorTransportComponent::HandleStompConnected);
	StompClient->OnConnectionError().AddUObject(this, &UVirtualSensorTransportComponent::HandleStompFailure);
	StompClient->OnError().AddUObject(this, &UVirtualSensorTransportComponent::HandleStompFailure);
	StompClient->OnClosed().AddUObject(this, &UVirtualSensorTransportComponent::HandleStompFailure);
	FStompHeader Headers;
	if (!TransportProfile.UserName.IsEmpty()) Headers.Add(TEXT("login"), TransportProfile.UserName);
	if (!SessionPasscode.IsEmpty()) Headers.Add(TEXT("passcode"), SessionPasscode);
	StompClient->Connect(Headers);
}

void UVirtualSensorTransportComponent::HandleStompConnected(const FString& ProtocolVersion, const FString& SessionId, const FString& ServerString)
{
	LastResult.Protocol = TEXT("STOMP/WS");
	LastResult.bSubmitted = true;
	LastResult.bAccepted = true;
	LastResult.Destination = TransportProfile.BrokerUrl;
	LastResult.Message = FString::Printf(TEXT("Artemis STOMP 연결됨 · protocol=%s · session=%s"), *ProtocolVersion, *SessionId);
	OnDataSent.Broadcast(LastResult);
	SubscribeToAckTopic();
}

void UVirtualSensorTransportComponent::HandleStompFailure(const FString& Error)
{
	AckSubscriptionId.Reset();
	LastResult.Protocol = TEXT("STOMP/WS");
	LastResult.bAccepted = false;
	LastResult.Message = FString::Printf(TEXT("Artemis STOMP 오류: %s"), *Error);
	OnDataSent.Broadcast(LastResult);
}

void UVirtualSensorTransportComponent::SubscribeToAckTopic()
{
	if (!StompClient.IsValid() || !StompClient->IsConnected() || TransportProfile.AckTopic.IsEmpty() || !AckSubscriptionId.IsEmpty()) return;
	TWeakObjectPtr<UVirtualSensorTransportComponent> WeakThis(this);
	AckSubscriptionId = StompClient->Subscribe(
		TransportProfile.AckTopic,
		FStompSubscriptionEvent::CreateLambda([WeakThis](const IStompMessage& Message)
		{
			if (WeakThis.IsValid()) WeakThis->HandleAckMessage(Message);
		}),
		FStompRequestCompleted::CreateLambda([WeakThis](bool bSuccess, const FString& Error)
		{
			if (!WeakThis.IsValid()) return;
			FVirtualSensorTransportResult Result;
			Result.Protocol = TEXT("STOMP/WS");
			Result.Destination = WeakThis->TransportProfile.AckTopic;
			Result.bSubmitted = true;
			Result.bAccepted = bSuccess;
			Result.DataKind = TEXT("ack-subscription");
			Result.Message = bSuccess ? TEXT("소비자 ACK Topic 구독 완료") : FString::Printf(TEXT("ACK Topic 구독 실패: %s"), *Error);
			WeakThis->OnDataSent.Broadcast(Result);
		}));
}

void UVirtualSensorTransportComponent::HandleAckMessage(const IStompMessage& Message)
{
	FString RequestId;
	const FStompHeader& Headers = Message.GetHeader();
	if (const FString* HeaderRequestId = Headers.Find(TEXT("x-request-id"))) RequestId = *HeaderRequestId;
	if (RequestId.IsEmpty())
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message.GetBodyAsString());
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()) Root->TryGetStringField(TEXT("requestId"), RequestId);
	}
	FVirtualSensorTransportResult Result;
	Result.Protocol = TEXT("STOMP/WS");
	Result.RequestId = RequestId;
	Result.Destination = TransportProfile.AckTopic;
	Result.DataKind = TEXT("consumer-ack");
	Result.bSubmitted = true;
	Result.bAccepted = !RequestId.IsEmpty();
	Result.bConsumerAckReceived = !RequestId.IsEmpty();
	Result.Message = RequestId.IsEmpty() ? TEXT("소비자 ACK를 받았지만 requestId가 없습니다.") : TEXT("소비자 처리 ACK 수신");
	OnDataSent.Broadcast(Result);
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SendStomp(
	const FString& SensorId,
	const FString& SensorType,
	const FString& DataKind,
	int64 FrameId,
	const FString& JsonText,
	bool bManualRequest,
	bool bRequestReceipt)
{
	FVirtualSensorTransportResult Result;
	Result.Protocol = TEXT("STOMP/WS");
	Result.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Result.SensorId = SensorId;
	Result.SensorType = SensorType;
	Result.DataKind = DataKind;
	Result.bManualRequest = bManualRequest;
	Result.bReceiptRequested = bRequestReceipt;
	Result.Destination = ResolveTopic(SensorType, DataKind);
	FTCHARToUTF8 Utf8(*JsonText);
	Result.DataLength = Utf8.Length();
	if (Result.DataLength > TransportProfile.MaxMessageBytes)
	{
		Result.Message = FString::Printf(TEXT("STOMP 메시지가 제한을 초과했습니다: %d / %d bytes"), Result.DataLength, TransportProfile.MaxMessageBytes);
		return Result;
	}
	EnsureStompClient();
	if (!IsStompConnected())
	{
		Result.Message = TEXT("Artemis STOMP 연결 중입니다. 연결 후 다시 전송하세요.");
		return Result;
	}
	FStompHeader Headers;
	Headers.Add(TEXT("destination-type"), TEXT("MULTICAST"));
	Headers.Add(TEXT("content-type"), TEXT("application/json;charset=utf-8"));
	Headers.Add(TEXT("x-sensor-id"), SensorId);
	Headers.Add(TEXT("x-sensor-type"), SensorType);
	Headers.Add(TEXT("x-data-kind"), DataKind);
	Headers.Add(TEXT("x-frame-id"), LexToString(FrameId));
	Headers.Add(TEXT("x-request-id"), Result.RequestId);
	const double StartedSeconds = FPlatformTime::Seconds();
	const FString RequestId = Result.RequestId;
	const FString Destination = Result.Destination;
	TWeakObjectPtr<UVirtualSensorTransportComponent> WeakThis(this);
	if (!bRequestReceipt)
	{
		StompClient->Send(Result.Destination, JsonText, Headers);
		Result.bSubmitted = true;
		Result.Message = TEXT("STOMP 전송 제출됨 · 이번 프레임은 receipt 생략");
		LastStompSubmitSeconds = StartedSeconds;
		return Result;
	}
	StompClient->Send(Result.Destination, JsonText, Headers, FStompRequestCompleted::CreateLambda(
		[WeakThis, SubmittedResult = Result, RequestId, Destination, StartedSeconds](bool bSuccess, const FString& Error)
		{
			if (!WeakThis.IsValid()) return;
			FVirtualSensorTransportResult Receipt = SubmittedResult;
			Receipt.Protocol = TEXT("STOMP/WS");
			Receipt.RequestId = RequestId;
			Receipt.Destination = Destination;
			Receipt.bSubmitted = true;
			Receipt.bAccepted = bSuccess;
			Receipt.bReceiptReceived = bSuccess;
			Receipt.LatencyMs = static_cast<float>((FPlatformTime::Seconds() - StartedSeconds) * 1000.0);
			Receipt.Message = bSuccess
				? (WeakThis->TransportProfile.AckTopic.IsEmpty() ? TEXT("Broker 수락, 소비자 처리 미확인") : TEXT("Broker receipt 수락 · 소비자 ACK 대기"))
				: FString::Printf(TEXT("STOMP 전송 실패: %s"), *Error);
			if (Receipt.bManualRequest)
			{
				WeakThis->LastResult = Receipt;
			}
			WeakThis->OnDataSent.Broadcast(Receipt);
		}));
	Result.bSubmitted = true;
	Result.Message = TEXT("STOMP 전송 제출됨 · Broker receipt 대기");
	LastStompSubmitSeconds = StartedSeconds;
	return Result;
}

void UVirtualSensorTransportComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StompClient.IsValid())
	{
		StompClient->Disconnect();
		StompClient.Reset();
	}
	AckSubscriptionId.Reset();
	SessionPasscode.Reset();
	SessionBearerToken.Reset();
	Super::EndPlay(EndPlayReason);
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SendBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes)
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = Bytes.Num();

    if (TransportMode == EVirtualSensorTransportMode::None)
    {
        Result.bSubmitted = true;
        Result.bAccepted = true;
        Result.Message = TEXT("Transport disabled");
    }
    else if (TransportMode == EVirtualSensorTransportMode::SaveToFile)
    {
        Result = SaveBinary(SensorId, SensorType, Extension, Bytes);
    }
    else if (TransportMode == EVirtualSensorTransportMode::HttpPost)
    {
        Result = SendHttpBinary(SensorId, SensorType, Extension, Bytes);
    }
    else if (TransportMode == EVirtualSensorTransportMode::StompWebSocket)
    {
        const FString Encoded = FBase64::Encode(Bytes);
        const FString Envelope = FString::Printf(TEXT("{\"extension\":\"%s\",\"encoding\":\"base64\",\"data\":\"%s\"}"), *Extension, *Encoded);
        Result = SendStomp(SensorId, SensorType, TEXT("manual-export"), 0, Envelope, true, true);
    }
    else
    {
        Result.bSubmitted = true;
        Result.bAccepted = true;
        Result.Message = FString::Printf(TEXT("[%s:%s] binaryLength=%d"), *SensorType, *SensorId, Bytes.Num());
        UE_LOG(LogTemp, Log, TEXT("%s"), *Result.Message);
    }

    LastResult = Result;
    OnDataSent.Broadcast(Result);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SendHttpBinary(
    const FString& SensorId,
    const FString& SensorType,
    const FString& Extension,
    const TArray<uint8>& Bytes)
{
    FVirtualSensorTransportResult Result;
    Result.Protocol = TEXT("HTTP");
    Result.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    Result.SensorId = SensorId;
    Result.SensorType = SensorType;
    Result.DataKind = TEXT("manual-export");
    Result.Destination = HttpEndpoint;
    Result.DataLength = Bytes.Num();
    if (HttpEndpoint.IsEmpty())
    {
        Result.Message = TEXT("HTTP endpoint가 비어 있습니다.");
        return Result;
    }
    if (Bytes.Num() > TransportProfile.MaxMessageBytes)
    {
        Result.Message = FString::Printf(TEXT("HTTP binary가 제한을 초과했습니다: %d / %d bytes"), Bytes.Num(), TransportProfile.MaxMessageBytes);
        return Result;
    }
    if (InFlightHttpRequestCount >= FMath::Max(1, MaxInFlightHttpRequests))
    {
        Result.bBackpressureRejected = true;
        ++BackpressureRejectedRequestCount;
        Result.Message = TEXT("HTTP binary 전송이 backpressure 제한으로 거부되었습니다.");
        return Result;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(HttpEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
    Request->SetHeader(TEXT("X-Sensor-Id"), SensorId);
    Request->SetHeader(TEXT("X-Sensor-Type"), SensorType);
    Request->SetHeader(TEXT("X-Data-Kind"), TEXT("manual-export"));
    Request->SetHeader(TEXT("X-File-Extension"), Extension);
    Request->SetHeader(TEXT("X-Request-Id"), Result.RequestId);
    if (!SessionBearerToken.IsEmpty()) Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionBearerToken);
    Request->SetTimeout(static_cast<float>(FMath::Max(1, HttpTimeoutSeconds)));
    Request->SetContent(Bytes);
    const double StartedSeconds = FPlatformTime::Seconds();
    const TWeakObjectPtr<UVirtualSensorTransportComponent> WeakThis(this);
    const FString RequestId = Result.RequestId;
    ++InFlightHttpRequestCount;
    Request->OnProcessRequestComplete().BindLambda([WeakThis, RequestId, StartedSeconds](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        if (!WeakThis.IsValid()) return;
        UVirtualSensorTransportComponent* Self = WeakThis.Get();
        Self->InFlightHttpRequestCount = FMath::Max(0, Self->InFlightHttpRequestCount - 1);
        FVirtualSensorTransportResult Completed = Self->LastResult;
        Completed.Protocol = TEXT("HTTP");
        Completed.RequestId = RequestId;
        Completed.HttpStatusCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        Completed.bSubmitted = true;
        Completed.bAccepted = bSucceeded && Completed.HttpStatusCode >= 200 && Completed.HttpStatusCode < 300;
        Completed.LatencyMs = static_cast<float>((FPlatformTime::Seconds() - StartedSeconds) * 1000.0);
        Completed.Message = Completed.bAccepted
            ? FString::Printf(TEXT("HTTP binary 수락: %d"), Completed.HttpStatusCode)
            : FString::Printf(TEXT("HTTP binary 실패: %d"), Completed.HttpStatusCode);
        Self->LastResult = Completed;
        Self->OnDataSent.Broadcast(Completed);
    });
    Result.bSubmitted = Request->ProcessRequest();
    Result.Message = Result.bSubmitted ? TEXT("HTTP binary 전송 제출됨") : TEXT("HTTP binary 전송 제출 실패");
    if (!Result.bSubmitted) InFlightHttpRequestCount = FMath::Max(0, InFlightHttpRequestCount - 1);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SendHttp(const FString& SensorId, const FString& SensorType, const FString& JsonText)
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();

    if (HttpEndpoint.IsEmpty())
    {
        Result.bSubmitted = false;
        Result.Message = FString::Printf(TEXT("[%s:%s] HttpEndpoint is empty"), *SensorType, *SensorId);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Result.Message);
        return Result;
    }

    const int32 SafeMaxInFlightRequests = FMath::Max(1, MaxInFlightHttpRequests);
    if (InFlightHttpRequestCount >= SafeMaxInFlightRequests)
    {
        Result.bSubmitted = false;
        Result.bAccepted = false;
        Result.bBackpressureRejected = true;
        ++BackpressureRejectedRequestCount;
        Result.Message = FString::Printf(TEXT("[%s:%s] HTTP backpressure rejected inFlight=%d max=%d rejected=%d"),
            *SensorType,
            *SensorId,
            InFlightHttpRequestCount,
            SafeMaxInFlightRequests,
            BackpressureRejectedRequestCount);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Result.Message);
        return Result;
    }

    const int32 SubmittedDataLength = JsonText.Len();
    ++InFlightHttpRequestCount;
    Result.bSubmitted = SubmitHttpAttempt(SensorId, SensorType, JsonText, 0, SubmittedDataLength);
    if (!Result.bSubmitted)
    {
        InFlightHttpRequestCount = FMath::Max(0, InFlightHttpRequestCount - 1);
    }
    Result.bAccepted = false;
    Result.Message = Result.bSubmitted
        ? FString::Printf(TEXT("[%s:%s] HTTP request submitted"), *SensorType, *SensorId)
        : FString::Printf(TEXT("[%s:%s] HTTP request submit failed"), *SensorType, *SensorId);
    return Result;
}

bool UVirtualSensorTransportComponent::SubmitHttpAttempt(
    const FString& SensorId,
    const FString& SensorType,
    const FString& JsonText,
    int32 AttemptIndex,
    int32 SubmittedDataLength)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(HttpEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Sensor-Id"), SensorId);
    Request->SetHeader(TEXT("X-Sensor-Type"), SensorType);
	if (!SessionBearerToken.IsEmpty()) Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionBearerToken);
    Request->SetTimeout(static_cast<float>(FMath::Max(1, HttpTimeoutSeconds)));
    Request->SetContentAsString(JsonText);

    const TWeakObjectPtr<UVirtualSensorTransportComponent> WeakThis(this);
    Request->OnProcessRequestComplete().BindLambda([WeakThis, SensorId, SensorType, JsonText, AttemptIndex, SubmittedDataLength](FHttpRequestPtr /*RequestPtr*/, FHttpResponsePtr Response, bool bSucceeded)
    {
        if (!WeakThis.IsValid())
        {
            return;
        }

        UVirtualSensorTransportComponent* TransportComp = WeakThis.Get();
        const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        const bool bResponseReceived = Response.IsValid();
        const bool bConnectionFailure = !bSucceeded || !bResponseReceived;
        const bool bServerError = bResponseReceived && ResponseCode >= 500 && ResponseCode < 600;
        const bool bRetryEligible =
            (bConnectionFailure && TransportComp->bRetryOnConnectionFailure) ||
            (bServerError && TransportComp->bRetryOnServerError);
        const bool bCanRetry = AttemptIndex < FMath::Max(0, TransportComp->MaxHttpRetryAttempts);
        const bool bShouldRetry = bCanRetry && bRetryEligible;
        int32 FinalAttemptIndex = AttemptIndex;

        if (bShouldRetry)
        {
            ++TransportComp->TotalHttpRetryAttemptCount;
            if (TransportComp->SubmitHttpAttempt(SensorId, SensorType, JsonText, AttemptIndex + 1, SubmittedDataLength))
            {
                return;
            }
            FinalAttemptIndex = AttemptIndex + 1;
        }

        TransportComp->InFlightHttpRequestCount = FMath::Max(0, TransportComp->InFlightHttpRequestCount - 1);
        const bool bAcceptedStatus = bResponseReceived && ResponseCode >= 200 && ResponseCode < 300;

        FVirtualSensorTransportResult CallbackResult;
        CallbackResult.bSubmitted = true;
        CallbackResult.bAccepted = bAcceptedStatus;
        CallbackResult.bRetryExhausted = !bAcceptedStatus && bRetryEligible &&
            FinalAttemptIndex >= FMath::Max(0, TransportComp->MaxHttpRetryAttempts);
        CallbackResult.DataLength = SubmittedDataLength;
        CallbackResult.HttpStatusCode = ResponseCode;
        CallbackResult.RetryAttemptCount = FinalAttemptIndex;
        CallbackResult.ResponseBody = Response.IsValid() ? Response->GetContentAsString() : FString();
        CallbackResult.Message = FString::Printf(TEXT("[%s:%s] HTTP completed submitted=%s accepted=%s code=%d retries=%d"),
            *SensorType,
            *SensorId,
            CallbackResult.bSubmitted ? TEXT("true") : TEXT("false"),
            CallbackResult.bAccepted ? TEXT("true") : TEXT("false"),
            CallbackResult.HttpStatusCode,
            CallbackResult.RetryAttemptCount);

        if (!CallbackResult.bAccepted)
        {
            ++TransportComp->FailedHttpRequestCount;
        }
        if (CallbackResult.bRetryExhausted)
        {
            ++TransportComp->RetryExhaustedRequestCount;
        }

        if (TransportComp->bLogHttpResponse)
        {
            UE_LOG(LogTemp, Log, TEXT("%s"), *CallbackResult.Message);
        }

        TransportComp->LastResult = CallbackResult;
        TransportComp->OnDataSent.Broadcast(CallbackResult);
    });

    return Request->ProcessRequest();
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SaveJson(const FString& SensorId, const FString& SensorType, const FString& JsonText) const
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();
    const FString Path = BuildSavePath(SensorId, SensorType, TEXT("json"));
    Result.bSubmitted = FFileHelper::SaveStringToFile(JsonText, *Path);
    Result.bAccepted = Result.bSubmitted;
    Result.SavedFilePath = Path;
    Result.Message = Result.bSubmitted ? FString::Printf(TEXT("Saved: %s"), *Path) : FString::Printf(TEXT("Save failed: %s"), *Path);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorTransportComponent::SaveBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes) const
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = Bytes.Num();
    const FString Path = BuildSavePath(SensorId, SensorType, Extension.IsEmpty() ? TEXT("bin") : Extension);
    Result.bSubmitted = FFileHelper::SaveArrayToFile(Bytes, *Path);
    Result.bAccepted = Result.bSubmitted;
    Result.SavedFilePath = Path;
    Result.Message = Result.bSubmitted ? FString::Printf(TEXT("Saved: %s"), *Path) : FString::Printf(TEXT("Save failed: %s"), *Path);
    return Result;
}

FString UVirtualSensorTransportComponent::BuildSavePath(const FString& SensorId, const FString& SensorType, const FString& Extension) const
{
    const FString SafeSensorId = SensorId.IsEmpty() ? TEXT("UNKNOWN_SENSOR") : SensorId;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), SaveSubDirectory, SafeSensorId);
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    const FString FileName = FString::Printf(TEXT("%s_%s_%s.%s"), *SafeSensorId, *SensorType, *Timestamp, *Extension);
    return FPaths::Combine(Directory, FileName);
}
