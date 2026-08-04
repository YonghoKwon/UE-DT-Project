#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"

#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Core/DTCoreSettings.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "IStompClient.h"
#include "IStompMessage.h"
#include "StompModule.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/CameraJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarHttpJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLinesReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarUdpJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualCameraStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualLidarStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/ma0t10_dt.h"
#include "WebSocket/TransactionCodeMessage.h"

AVirtualSensorExternalSourceHostActor::AVirtualSensorExternalSourceHostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	LidarCsvReplay = CreateDefaultSubobject<ULidarCsvReplaySourceComponent>(TEXT("LidarCsvReplay"));
	LidarJsonLinesReplay = CreateDefaultSubobject<ULidarJsonLinesReplaySourceComponent>(TEXT("LidarJsonLinesReplay"));
	LidarBufferedJson = CreateDefaultSubobject<ULidarJsonLiveSourceComponent>(TEXT("LidarBufferedJson"));
	LidarHttpJson = CreateDefaultSubobject<ULidarHttpJsonLiveSourceComponent>(TEXT("LidarHttpJson"));
	LidarUdpJson = CreateDefaultSubobject<ULidarUdpJsonLiveSourceComponent>(TEXT("LidarUdpJson"));
	CameraBufferedJson = CreateDefaultSubobject<UCameraJsonLiveSourceComponent>(TEXT("CameraBufferedJson"));
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson), static_cast<URealSensorSourceComponent*>(CameraBufferedJson) })
	{
		Source->SetupAttachment(Root);
		Source->bAutoStartSource = false;
		Source->bSendTransportByDefault = false;
	}
	LidarCsvReplay->SourceId = TEXT("Demo-LiDAR-CSV");
	LidarCsvReplay->CsvFilePath = TEXT("Samples/slab_replay_sample.csv");
	LidarJsonLinesReplay->SourceId = TEXT("Demo-LiDAR-JSONL");
	LidarBufferedJson->SourceId = TEXT("Live-LiDAR-Buffered");
	LidarJsonLinesReplay->JsonLinesFilePath = TEXT("Samples/slab_replay_sample.jsonl");
	LidarHttpJson->SourceId = TEXT("Live-LiDAR-HTTP");
	LidarHttpJson->ListenPort = 8082;
	LidarHttpJson->RoutePath = TEXT("/ma0t10/lidar/live");
	LidarUdpJson->SourceId = TEXT("Live-LiDAR-UDP");
	LidarUdpJson->BindAddress = TEXT("127.0.0.1");
	LidarUdpJson->BindPort = 0;
	CameraBufferedJson->SourceId = TEXT("Live-Camera-JSON");
}

void AVirtualSensorExternalSourceHostActor::BeginPlay()
{
	Super::BeginPlay();
	bEndingPlay = false;
	AVirtualLidarSensorActor* Lidar = nullptr;
	AVirtualCameraSensorActor* Camera = nullptr;
	for (TActorIterator<AVirtualLidarSensorActor> It(GetWorld()); It; ++It) { Lidar = *It; break; }
	for (TActorIterator<AVirtualCameraSensorActor> It(GetWorld()); It; ++It) { Camera = *It; break; }
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson) })
	{
		Source->TargetSensorActor = Lidar;
		Source->TargetLidar = Lidar ? Lidar->ScanComponent : nullptr;
	}
	CameraBufferedJson->TargetSensorActor = Camera;
	CameraBufferedJson->TargetCamera = Camera ? Camera->CaptureComponent : nullptr;

	InitializeTopicReceiverRuntime();
	if (bAutoStartTopicReceivers)
	{
		bTopicReceiversRequested = true;
		GetWorldTimerManager().SetTimer(
			TopicReceiverRetryTimer,
			this,
			&AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions,
			FMath::Max(0.01f, InitialSubscribeDelaySeconds),
			false);
	}
}

void AVirtualSensorExternalSourceHostActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopTopicReceivers();
	Super::EndPlay(EndPlayReason);
}

void AVirtualSensorExternalSourceHostActor::InitializeTopicReceiverRuntime()
{
	if (!LidarTopicHandler) LidarTopicHandler = NewObject<UVirtualLidarStreamReceiverTC>(this, TEXT("LidarTopicHandler"));
	if (!CameraTopicHandler) CameraTopicHandler = NewObject<UVirtualCameraStreamReceiverTC>(this, TEXT("CameraTopicHandler"));
	if (!PointCloudTopicHandler) PointCloudTopicHandler = NewObject<UVirtualPointCloudStreamReceiverTC>(this, TEXT("PointCloudTopicHandler"));
	if (ReceiverRuntimes.Num() == 3) return;
	ReceiverRuntimes.Reset();
	for (const EVirtualSensorTopicReceiveKind Kind : { EVirtualSensorTopicReceiveKind::Lidar, EVirtualSensorTopicReceiveKind::Camera, EVirtualSensorTopicReceiveKind::PointCloud })
	{
		FReceiverRuntime& Runtime = ReceiverRuntimes.AddDefaulted_GetRef();
		Runtime.Status.Kind = Kind;
		Runtime.Status.Topic = GetTopic(Kind);
		Runtime.Status.State = EVirtualSensorTopicReceiverState::Stopped;
	}
}

void AVirtualSensorExternalSourceHostActor::StartTopicReceivers()
{
	InitializeTopicReceiverRuntime();
	bTopicReceiversRequested = true;
	AttemptTopicSubscriptions();
}

void AVirtualSensorExternalSourceHostActor::StopTopicReceivers()
{
	bTopicReceiversRequested = false;
	GetWorldTimerManager().ClearTimer(TopicReceiverRetryTimer);
	UDxWebSocketSubsystem* WebSocket = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>() : nullptr;
	if (RawPointCloudClient.IsValid())
	{
		if (!RawPointCloudSubscriptionId.IsEmpty()) RawPointCloudClient->Unsubscribe(RawPointCloudSubscriptionId);
		RawPointCloudClient->Disconnect();
		RawPointCloudClient.Reset();
	}
	RawPointCloudSubscriptionId.Reset();
	for (FReceiverRuntime& Runtime : ReceiverRuntimes)
	{
		if (WebSocket && !Runtime.SubscriptionId.IsEmpty())
		{
			WebSocket->Unsubscribe(Runtime.SubscriptionId, FSTOMPRequestCompleted());
		}
		Runtime.SubscriptionId.Reset();
		Runtime.PendingBody.Reset();
		Runtime.PendingBinaryBodies.Reset();
		Runtime.bSubscriptionPending = false;
		Runtime.SubscriptionStartedSeconds = 0.0;
		Runtime.RetryAttempt = 0;
		Runtime.Status.State = EVirtualSensorTopicReceiverState::Stopped;
		Runtime.Status.LastMessage = TEXT("수신 구독을 중지했습니다.");
	}
}

void AVirtualSensorExternalSourceHostActor::ReconnectTopicReceivers()
{
	StopTopicReceivers();
	bEndingPlay = false;
	bTopicReceiversRequested = true;
	GetWorldTimerManager().SetTimerForNextTick(this, &AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions);
}

void AVirtualSensorExternalSourceHostActor::ConfigureReceiverTopics(
	const FString& InLidarTopic,
	const FString& InCameraTopic,
	const FString& InPointCloudTopic)
{
	const FString NewLidar = InLidarTopic.TrimStartAndEnd();
	const FString NewCamera = InCameraTopic.TrimStartAndEnd();
	const FString NewPointCloud = InPointCloudTopic.TrimStartAndEnd();
	const bool bChanged = LidarReceiveTopic != NewLidar || CameraReceiveTopic != NewCamera || PointCloudReceiveTopic != NewPointCloud;
	LidarReceiveTopic = NewLidar;
	CameraReceiveTopic = NewCamera;
	PointCloudReceiveTopic = NewPointCloud;
	for (FReceiverRuntime& Runtime : ReceiverRuntimes) Runtime.Status.Topic = GetTopic(Runtime.Status.Kind);
	if (bChanged && bTopicReceiversRequested) ReconnectTopicReceivers();
}

TArray<FVirtualSensorTopicReceiverStatus> AVirtualSensorExternalSourceHostActor::GetTopicReceiverStatuses() const
{
	TArray<FVirtualSensorTopicReceiverStatus> Result;
	Result.Reserve(ReceiverRuntimes.Num());
	for (const FReceiverRuntime& Runtime : ReceiverRuntimes) Result.Add(Runtime.Status);
	return Result;
}

FString AVirtualSensorExternalSourceHostActor::GetReceiverBrokerUrl() const
{
	if (FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE")).Equals(TEXT("1")))
	{
		const FString TestBrokerUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_URL"));
		if (!TestBrokerUrl.IsEmpty()) return TestBrokerUrl;
	}
	FString RuntimeUrl;
	if (GConfig && GConfig->GetString(TEXT("DTCoreRuntimeOverride"), TEXT("WebSocketUrl"), RuntimeUrl, GGameIni) && !RuntimeUrl.IsEmpty()) return RuntimeUrl;
	const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();
	return Settings ? Settings->WebSocketUrl : FString();
}

AVirtualSensorExternalSourceHostActor::FReceiverRuntime* AVirtualSensorExternalSourceHostActor::FindRuntime(EVirtualSensorTopicReceiveKind Kind)
{
	return ReceiverRuntimes.FindByPredicate([Kind](const FReceiverRuntime& Runtime) { return Runtime.Status.Kind == Kind; });
}

const AVirtualSensorExternalSourceHostActor::FReceiverRuntime* AVirtualSensorExternalSourceHostActor::FindRuntime(EVirtualSensorTopicReceiveKind Kind) const
{
	return ReceiverRuntimes.FindByPredicate([Kind](const FReceiverRuntime& Runtime) { return Runtime.Status.Kind == Kind; });
}

UTransactionCodeMessage* AVirtualSensorExternalSourceHostActor::FindHandler(EVirtualSensorTopicReceiveKind Kind) const
{
	switch (Kind)
	{
	case EVirtualSensorTopicReceiveKind::Lidar: return LidarTopicHandler;
	case EVirtualSensorTopicReceiveKind::Camera: return CameraTopicHandler;
	case EVirtualSensorTopicReceiveKind::PointCloud: return PointCloudTopicHandler;
	default: return nullptr;
	}
}

FString AVirtualSensorExternalSourceHostActor::GetTopic(EVirtualSensorTopicReceiveKind Kind) const
{
	switch (Kind)
	{
	case EVirtualSensorTopicReceiveKind::Lidar: return LidarReceiveTopic;
	case EVirtualSensorTopicReceiveKind::Camera: return CameraReceiveTopic;
	case EVirtualSensorTopicReceiveKind::PointCloud: return PointCloudReceiveTopic;
	default: return FString();
	}
}

void AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions()
{
	if (bEndingPlay || !bTopicReceiversRequested) return;
	InitializeTopicReceiverRuntime();
	const double Now = FPlatformTime::Seconds();
	UDxWebSocketSubsystem* WebSocket = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>() : nullptr;
	for (FReceiverRuntime& Runtime : ReceiverRuntimes)
	{
		if (Runtime.bSubscriptionPending && Runtime.SubscriptionStartedSeconds > 0.0 && Now - Runtime.SubscriptionStartedSeconds >= 5.0)
		{
			if (Runtime.Status.Kind == EVirtualSensorTopicReceiveKind::PointCloud)
			{
				if (RawPointCloudClient.IsValid()) RawPointCloudClient->Disconnect();
				RawPointCloudClient.Reset();
				RawPointCloudSubscriptionId.Reset();
			}
			else if (WebSocket && !Runtime.SubscriptionId.IsEmpty())
			{
				WebSocket->Unsubscribe(Runtime.SubscriptionId, FSTOMPRequestCompleted());
			}
			Runtime.SubscriptionId.Reset();
			Runtime.bSubscriptionPending = false;
			Runtime.SubscriptionStartedSeconds = 0.0;
			++Runtime.RetryAttempt;
			Runtime.Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
			Runtime.Status.LastMessage = TEXT("구독 응답 시간 초과, 다시 시도합니다.");
		}
	}
	for (const EVirtualSensorTopicReceiveKind Kind : { EVirtualSensorTopicReceiveKind::Lidar, EVirtualSensorTopicReceiveKind::Camera, EVirtualSensorTopicReceiveKind::PointCloud })
	{
		SubscribeRuntime(Kind);
	}
	ScheduleSubscriptionRetry();
}

void AVirtualSensorExternalSourceHostActor::SubscribeRuntime(EVirtualSensorTopicReceiveKind Kind)
{
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime || !Runtime->SubscriptionId.IsEmpty() || Runtime->bSubscriptionPending) return;
	if (Kind == EVirtualSensorTopicReceiveKind::PointCloud)
	{
		EnsureRawPointCloudClient();
		return;
	}
	Runtime->Status.Topic = GetTopic(Kind);
	if (Runtime->Status.Topic.IsEmpty())
	{
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Error;
		Runtime->Status.LastMessage = TEXT("수신 Topic이 비어 있습니다.");
		return;
	}
	UDxWebSocketSubsystem* WebSocket = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>() : nullptr;
	if (!WebSocket)
	{
		Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
		Runtime->Status.LastMessage = TEXT("DTCore WebSocket Subsystem 대기 중");
		++Runtime->RetryAttempt;
		return;
	}
	FSTOMPSubscriptionEvent Event;
	FSTOMPRequestCompleted Completion;
	switch (Kind)
	{
	case EVirtualSensorTopicReceiveKind::Lidar:
		Event.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleLidarTopicMessage);
		Completion.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleLidarSubscriptionCompleted);
		break;
	case EVirtualSensorTopicReceiveKind::Camera:
		Event.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleCameraTopicMessage);
		Completion.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleCameraSubscriptionCompleted);
		break;
	case EVirtualSensorTopicReceiveKind::PointCloud:
		Event.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandlePointCloudTopicMessage);
		Completion.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandlePointCloudSubscriptionCompleted);
		break;
	default: return;
	}
	Runtime->bSubscriptionPending = true;
	Runtime->SubscriptionStartedSeconds = FPlatformTime::Seconds();
	Runtime->Status.State = EVirtualSensorTopicReceiverState::Subscribing;
	Runtime->Status.LastMessage = TEXT("DTCore STOMP 구독 요청 중");
	Runtime->SubscriptionId = WebSocket->Subscribe(Runtime->Status.Topic, Event, Completion);
	if (Runtime->SubscriptionId.IsEmpty())
	{
		Runtime->bSubscriptionPending = false;
		Runtime->SubscriptionStartedSeconds = 0.0;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
		Runtime->Status.LastMessage = TEXT("구독 요청이 시작되지 않아 재시도합니다.");
		++Runtime->RetryAttempt;
	}
}

void AVirtualSensorExternalSourceHostActor::EnsureRawPointCloudClient()
{
	FReceiverRuntime* Runtime = FindRuntime(EVirtualSensorTopicReceiveKind::PointCloud);
	if (!Runtime || bEndingPlay || !bTopicReceiversRequested) return;
	Runtime->Status.Topic = PointCloudReceiveTopic;
	if (Runtime->Status.Topic.IsEmpty())
	{
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Error;
		Runtime->Status.LastMessage = TEXT("Point Cloud receive Topic is empty.");
		return;
	}
	if (RawPointCloudClient.IsValid())
	{
		if (RawPointCloudClient->IsConnected()) SubscribeRawPointCloud();
		else RawPointCloudClient->Connect();
		return;
	}

	const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();
	const FString BrokerUrl = GetReceiverBrokerUrl();
	if (BrokerUrl.IsEmpty())
	{
		Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
		Runtime->Status.LastMessage = TEXT("DTCore WebSocket URL is empty.");
		++Runtime->RetryAttempt;
		return;
	}
	FString Login = Settings ? Settings->WebSocketLogin : FString();
	FString Passcode = Settings ? Settings->WebSocketPasscode : FString();
	if (GConfig)
	{
		GConfig->GetString(TEXT("DTCoreRuntimeOverride"), TEXT("WebSocketLogin"), Login, GGameIni);
		GConfig->GetString(TEXT("DTCoreRuntimeOverride"), TEXT("WebSocketPasscode"), Passcode, GGameIni);
	}
	if (FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE")).Equals(TEXT("1")))
	{
		const FString TestLogin = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_USER"));
		const FString TestPasscode = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD"));
		if (!TestLogin.IsEmpty()) Login = TestLogin;
		if (!TestPasscode.IsEmpty()) Passcode = TestPasscode;
	}
	RawPointCloudClient = FStompModule::Get().CreateClient(BrokerUrl);
	RawPointCloudClient->OnConnected().AddUObject(this, &AVirtualSensorExternalSourceHostActor::HandleRawPointCloudConnected);
	RawPointCloudClient->OnConnectionError().AddUObject(this, &AVirtualSensorExternalSourceHostActor::HandleRawPointCloudFailure);
	RawPointCloudClient->OnError().AddUObject(this, &AVirtualSensorExternalSourceHostActor::HandleRawPointCloudFailure);
	RawPointCloudClient->OnClosed().AddUObject(this, &AVirtualSensorExternalSourceHostActor::HandleRawPointCloudFailure);
	FStompHeader ConnectHeaders;
	if (!Login.IsEmpty()) ConnectHeaders.Add(TEXT("login"), Login);
	if (!Passcode.IsEmpty()) ConnectHeaders.Add(TEXT("passcode"), Passcode);
	Runtime->bSubscriptionPending = true;
	Runtime->SubscriptionStartedSeconds = FPlatformTime::Seconds();
	Runtime->Status.State = EVirtualSensorTopicReceiverState::Subscribing;
	Runtime->Status.LastMessage = TEXT("Raw STOMP Binary PCD connection starting.");
	RawPointCloudClient->Connect(ConnectHeaders);
}

void AVirtualSensorExternalSourceHostActor::HandleRawPointCloudConnected(
	const FString& ProtocolVersion,
	const FString& SessionId,
	const FString& ServerString)
{
	if (FReceiverRuntime* Runtime = FindRuntime(EVirtualSensorTopicReceiveKind::PointCloud))
	{
		Runtime->bSubscriptionPending = false;
		Runtime->SubscriptionStartedSeconds = 0.0;
		Runtime->Status.LastMessage = FString::Printf(TEXT("Raw STOMP connected: protocol=%s session=%s"), *ProtocolVersion, *SessionId);
	}
	SubscribeRawPointCloud();
}

void AVirtualSensorExternalSourceHostActor::SubscribeRawPointCloud()
{
	FReceiverRuntime* Runtime = FindRuntime(EVirtualSensorTopicReceiveKind::PointCloud);
	if (!Runtime || !RawPointCloudClient.IsValid() || !RawPointCloudClient->IsConnected() ||
		!RawPointCloudSubscriptionId.IsEmpty() || !bTopicReceiversRequested)
	{
		return;
	}
	Runtime->bSubscriptionPending = true;
	Runtime->SubscriptionStartedSeconds = FPlatformTime::Seconds();
	Runtime->Status.State = EVirtualSensorTopicReceiverState::Subscribing;
	const TWeakObjectPtr<AVirtualSensorExternalSourceHostActor> WeakThis(this);
	RawPointCloudSubscriptionId = RawPointCloudClient->Subscribe(
		PointCloudReceiveTopic,
		FStompSubscriptionEvent::CreateLambda([WeakThis](const IStompMessage& Message)
		{
			if (WeakThis.IsValid()) WeakThis->HandleRawPointCloudMessage(Message);
		}),
		FStompRequestCompleted::CreateLambda([WeakThis](bool bSuccess, const FString& Error)
		{
			if (!WeakThis.IsValid()) return;
			if (FReceiverRuntime* Runtime = WeakThis->FindRuntime(EVirtualSensorTopicReceiveKind::PointCloud))
			{
				Runtime->SubscriptionId = bSuccess ? WeakThis->RawPointCloudSubscriptionId : FString();
			}
			WeakThis->CompleteSubscription(EVirtualSensorTopicReceiveKind::PointCloud, bSuccess, Error);
		}));
}

void AVirtualSensorExternalSourceHostActor::HandleRawPointCloudFailure(const FString& Error)
{
	RawPointCloudSubscriptionId.Reset();
	RawPointCloudClient.Reset();
	FReceiverRuntime* Runtime = FindRuntime(EVirtualSensorTopicReceiveKind::PointCloud);
	if (!Runtime || bEndingPlay || !bTopicReceiversRequested) return;
	Runtime->SubscriptionId.Reset();
	Runtime->bSubscriptionPending = false;
	Runtime->SubscriptionStartedSeconds = 0.0;
	Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
	Runtime->Status.LastMessage = FString::Printf(TEXT("Raw STOMP Binary PCD connection error: %s"), *Error.Left(256));
	++Runtime->RetryAttempt;
	ScheduleSubscriptionRetry();
}

void AVirtualSensorExternalSourceHostActor::HandleRawPointCloudMessage(const IStompMessage& Message)
{
	const SIZE_T RawLength = Message.GetRawBodyLength();
	if (RawLength > static_cast<SIZE_T>(MAX_int32))
	{
		HandleRawPointCloudFailure(TEXT("Binary PCD body exceeds the processable size."));
		return;
	}
	TArray<uint8> Body;
	Body.Append(Message.GetRawBody(), static_cast<int32>(RawLength));
	TMap<FName, FString> Headers = Message.GetHeader();
	QueueRawPointCloudPayload(MoveTemp(Body), MoveTemp(Headers));
}

void AVirtualSensorExternalSourceHostActor::QueueRawPointCloudPayload(
	TArray<uint8>&& Body,
	TMap<FName, FString>&& Headers)
{
	if (bEndingPlay || !bTopicReceiversRequested) return;
	FReceiverRuntime* Runtime = FindRuntime(EVirtualSensorTopicReceiveKind::PointCloud);
	if (!Runtime) return;
	++Runtime->Status.ReceivedCount;
	Runtime->Status.LastReceivedUtc = FDateTime::UtcNow();
	Runtime->Status.LastMessageBytes = Body.Num();
	if (Body.Num() > FMath::Clamp(MaxReceiverMessageBytes, 1024, 16777216))
	{
		++Runtime->Status.ValidationFailureCount;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Error;
		Runtime->Status.LastMessage = FString::Printf(TEXT("Binary PCD receive size limit exceeded: %d bytes"), Body.Num());
		return;
	}
	if (Runtime->PendingBinaryBodies.Num() >= 20)
	{
		++Runtime->Status.ValidationFailureCount;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Error;
		Runtime->Status.LastMessage = TEXT("Binary PCD validation queue overloaded; no frame was silently replaced.");
		return;
	}
	FRawPointCloudPayload& Payload = Runtime->PendingBinaryBodies.AddDefaulted_GetRef();
	Payload.Body = MoveTemp(Body);
	Payload.Headers = MoveTemp(Headers);
	TryStartQueuedParses();
}

void AVirtualSensorExternalSourceHostActor::ScheduleSubscriptionRetry()
{
	if (bEndingPlay || !bTopicReceiversRequested) return;
	bool bNeedsRetry = false;
	int32 MaxAttempt = 0;
	for (const FReceiverRuntime& Runtime : ReceiverRuntimes)
	{
		bNeedsRetry |= Runtime.bSubscriptionPending || Runtime.SubscriptionId.IsEmpty();
		MaxAttempt = FMath::Max(MaxAttempt, Runtime.RetryAttempt);
	}
	if (!bNeedsRetry) return;
	const float Delay = FMath::Min(10.0f, FMath::Pow(2.0f, static_cast<float>(FMath::Clamp(MaxAttempt, 0, 4))));
	GetWorldTimerManager().SetTimer(TopicReceiverRetryTimer, this, &AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions, Delay, false);
}

void AVirtualSensorExternalSourceHostActor::CompleteSubscription(EVirtualSensorTopicReceiveKind Kind, bool bSuccess, const FString& Error)
{
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime || bEndingPlay || !bTopicReceiversRequested) return;
	Runtime->bSubscriptionPending = false;
	Runtime->SubscriptionStartedSeconds = 0.0;
	if (bSuccess)
	{
		Runtime->RetryAttempt = 0;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Active;
		Runtime->Status.LastMessage = TEXT("DTCore STOMP Topic 구독 완료");
		UE_LOG(LogMA0T10, Display, TEXT("[SensorTopicReceiver] subscribed topic=%s subscription=%s"), *Runtime->Status.Topic, *Runtime->SubscriptionId);
	}
	else
	{
		Runtime->SubscriptionId.Reset();
		++Runtime->RetryAttempt;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
		Runtime->Status.LastMessage = FString::Printf(TEXT("구독 실패, 재시도 예정: %s"), *Error.Left(256));
		ScheduleSubscriptionRetry();
	}
}

void AVirtualSensorExternalSourceHostActor::HandleLidarSubscriptionCompleted(bool bSuccess, FString Error) { CompleteSubscription(EVirtualSensorTopicReceiveKind::Lidar, bSuccess, Error); }
void AVirtualSensorExternalSourceHostActor::HandleCameraSubscriptionCompleted(bool bSuccess, FString Error) { CompleteSubscription(EVirtualSensorTopicReceiveKind::Camera, bSuccess, Error); }
void AVirtualSensorExternalSourceHostActor::HandlePointCloudSubscriptionCompleted(bool bSuccess, FString Error) { CompleteSubscription(EVirtualSensorTopicReceiveKind::PointCloud, bSuccess, Error); }

void AVirtualSensorExternalSourceHostActor::HandleLidarTopicMessage(const FWebSocketMessage& Message) { QueueTopicPayload(EVirtualSensorTopicReceiveKind::Lidar, Message.BodyString); }
void AVirtualSensorExternalSourceHostActor::HandleCameraTopicMessage(const FWebSocketMessage& Message) { QueueTopicPayload(EVirtualSensorTopicReceiveKind::Camera, Message.BodyString); }
void AVirtualSensorExternalSourceHostActor::HandlePointCloudTopicMessage(const FWebSocketMessage& Message) { QueueTopicPayload(EVirtualSensorTopicReceiveKind::PointCloud, Message.BodyString); }

void AVirtualSensorExternalSourceHostActor::QueueTopicPayload(EVirtualSensorTopicReceiveKind Kind, FString Body)
{
	if (bEndingPlay || !bTopicReceiversRequested || Body.IsEmpty()) return;
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime) return;
	++Runtime->Status.ReceivedCount;
	Runtime->Status.LastReceivedUtc = FDateTime::UtcNow();
	const int32 Bytes = FTCHARToUTF8(*Body).Length();
	Runtime->Status.LastMessageBytes = Bytes;
	if (Bytes > FMath::Clamp(MaxReceiverMessageBytes, 1024, 16777216))
	{
		++Runtime->Status.ValidationFailureCount;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Error;
		Runtime->Status.LastMessage = FString::Printf(TEXT("수신 크기 제한 초과: %d bytes"), Bytes);
		return;
	}
	if (Runtime->PendingBody.IsSet()) ++Runtime->Status.ReplacedPendingCount;
	Runtime->PendingBody = MoveTemp(Body);
	TryStartQueuedParses();
}

void AVirtualSensorExternalSourceHostActor::TryStartQueuedParses()
{
	if (bEndingPlay || !bTopicReceiversRequested || ReceiverRuntimes.IsEmpty()) return;
	const int32 Limit = FMath::Clamp(MaxConcurrentReceiverParses, 1, 3);
	for (int32 Attempt = 0; Attempt < ReceiverRuntimes.Num() && ActiveReceiverParseCount < Limit; ++Attempt)
	{
		const int32 Index = (ReceiverRoundRobinCursor + Attempt) % ReceiverRuntimes.Num();
		FReceiverRuntime& Runtime = ReceiverRuntimes[Index];
		const bool bHasBinaryPointCloud = Runtime.Status.Kind == EVirtualSensorTopicReceiveKind::PointCloud && !Runtime.PendingBinaryBodies.IsEmpty();
		if (Runtime.bParsing || (!Runtime.PendingBody.IsSet() && !bHasBinaryPointCloud)) continue;
		UTransactionCodeMessage* Handler = FindHandler(Runtime.Status.Kind);
		if (!Handler) continue;
		if (bHasBinaryPointCloud)
		{
			FRawPointCloudPayload Payload = MoveTemp(Runtime.PendingBinaryBodies[0]);
			Runtime.PendingBinaryBodies.RemoveAt(0, 1, false);
			Runtime.bParsing = true;
			++ActiveReceiverParseCount;
			ReceiverRoundRobinCursor = (Index + 1) % ReceiverRuntimes.Num();
			const TWeakObjectPtr<AVirtualSensorExternalSourceHostActor> WeakThis(this);
			const TWeakObjectPtr<UVirtualPointCloudStreamReceiverTC> WeakHandler(PointCloudTopicHandler);
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
				[WeakThis, WeakHandler, Payload = MoveTemp(Payload)]() mutable
				{
					const double Started = FPlatformTime::Seconds();
					TSharedPtr<FTransactionCodeDataBase> Parsed;
					if (WeakHandler.IsValid()) Parsed = WeakHandler->ParseBinaryPcdToStruct(Payload.Body, Payload.Headers);
					const float LatencyMs = static_cast<float>((FPlatformTime::Seconds() - Started) * 1000.0);
					AsyncTask(ENamedThreads::GameThread, [WeakThis, Parsed = MoveTemp(Parsed), LatencyMs]()
					{
						if (WeakThis.IsValid()) WeakThis->CompleteTopicParse(EVirtualSensorTopicReceiveKind::PointCloud, Parsed, LatencyMs);
					});
				});
			Attempt = -1;
			continue;
		}
		FString Body = MoveTemp(Runtime.PendingBody.GetValue());
		Runtime.PendingBody.Reset();
		Runtime.bParsing = true;
		++ActiveReceiverParseCount;
		ReceiverRoundRobinCursor = (Index + 1) % ReceiverRuntimes.Num();
		const EVirtualSensorTopicReceiveKind Kind = Runtime.Status.Kind;
		const TWeakObjectPtr<AVirtualSensorExternalSourceHostActor> WeakThis(this);
		const TWeakObjectPtr<UTransactionCodeMessage> WeakHandler(Handler);
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, WeakHandler, Kind, Body = MoveTemp(Body)]()
		{
			const double Started = FPlatformTime::Seconds();
			TSharedPtr<FTransactionCodeDataBase> Parsed;
			if (WeakHandler.IsValid()) Parsed = WeakHandler->ParseToStruct(Body);
			const float LatencyMs = static_cast<float>((FPlatformTime::Seconds() - Started) * 1000.0);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Kind, Parsed = MoveTemp(Parsed), LatencyMs]()
			{
				if (WeakThis.IsValid()) WeakThis->CompleteTopicParse(Kind, Parsed, LatencyMs);
			});
		});
		Attempt = -1;
	}
}

void AVirtualSensorExternalSourceHostActor::CompleteTopicParse(
	EVirtualSensorTopicReceiveKind Kind,
	const TSharedPtr<FTransactionCodeDataBase>& ParsedData,
	float ParseLatencyMs)
{
	ActiveReceiverParseCount = FMath::Max(0, ActiveReceiverParseCount - 1);
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime) return;
	Runtime->bParsing = false;
	if (bEndingPlay || !bTopicReceiversRequested)
	{
		Runtime->PendingBody.Reset();
		return;
	}
	const TSharedPtr<FVirtualSensorTopicReceivedDataBase> Data = StaticCastSharedPtr<FVirtualSensorTopicReceivedDataBase>(ParsedData);
	if (!Data.IsValid())
	{
		++Runtime->Status.ValidationFailureCount;
		Runtime->Status.LastMessage = TEXT("수신 Handler가 결과를 반환하지 않았습니다.");
	}
	else
	{
		bool bStaleOrDuplicatePointCloudFrame = false;
		if (Kind == EVirtualSensorTopicReceiveKind::PointCloud && Data->bValid && Runtime->Status.LastFrameId >= 0)
		{
			if (Data->FrameId <= Runtime->Status.LastFrameId)
			{
				++Runtime->Status.DuplicateFrameCount;
				bStaleOrDuplicatePointCloudFrame = true;
			}
			else if (Data->FrameId > Runtime->Status.LastFrameId + 1)
			{
				Runtime->Status.FrameGapCount += Data->FrameId - Runtime->Status.LastFrameId - 1;
			}
		}
		if (bStaleOrDuplicatePointCloudFrame)
		{
			// A receipt timeout retry is intentionally at-least-once. Validate the
			// frame body, but do not move LastFrameId backwards or count it as a
			// second consumer result; doing so manufactured a large gap on the next
			// good frame. SensorId/FrameId is the PCD consumer idempotency key.
			Runtime->Status.LastMessage = FString::Printf(
				TEXT("Duplicate/stale Binary PCD ignored by SensorId/FrameId: %s/%lld"),
				*Data->SensorId,
				Data->FrameId);
			Runtime->Status.State = EVirtualSensorTopicReceiverState::Active;
			AddReceiveLog(*Runtime, *Data, ParseLatencyMs);
			TryStartQueuedParses();
			return;
		}
		Runtime->Status.LastSensorId = Data->SensorId;
		Runtime->Status.LastFrameId = Data->FrameId;
		Runtime->Status.LastMessageBytes = Data->MessageBytes;
		Runtime->Status.LastParseLatencyMs = ParseLatencyMs;
		const double NowSeconds = FPlatformTime::Seconds();
		if (Data->bValid)
		{
			if (Runtime->FirstValidatedSeconds <= 0.0) Runtime->FirstValidatedSeconds = NowSeconds;
			Runtime->Status.ValidatedHz = static_cast<float>(Runtime->Status.ValidatedCount + 1) /
				FMath::Max(0.001, static_cast<float>(NowSeconds - Runtime->FirstValidatedSeconds));
			if (Kind == EVirtualSensorTopicReceiveKind::PointCloud && Data->SourceTimestampUtc.GetTicks() > 0)
			{
				const float EndToEndMs = static_cast<float>((FDateTime::UtcNow() - Data->SourceTimestampUtc).GetTotalMilliseconds());
				Runtime->Status.LastEndToEndLatencyMs = FMath::Max(0.0f, EndToEndMs);
				Runtime->EndToEndLatencySamples.Add(Runtime->Status.LastEndToEndLatencyMs);
				if (Runtime->EndToEndLatencySamples.Num() > 256)
				{
					Runtime->EndToEndLatencySamples.RemoveAt(0, Runtime->EndToEndLatencySamples.Num() - 256, false);
				}
				TArray<float> SortedLatency = Runtime->EndToEndLatencySamples;
				SortedLatency.Sort();
				const int32 P95Index = FMath::Clamp(FMath::CeilToInt(SortedLatency.Num() * 0.95f) - 1, 0, SortedLatency.Num() - 1);
				Runtime->Status.EndToEndP95LatencyMs = SortedLatency[P95Index];
			}
		}
		Runtime->Status.LastMessage = Data->Message;
		Runtime->Status.State = Data->bValid ? EVirtualSensorTopicReceiverState::Active : EVirtualSensorTopicReceiverState::Error;
		if (Data->bValid) ++Runtime->Status.ValidatedCount;
		else ++Runtime->Status.ValidationFailureCount;
		if (Data->bDeepValidated && Data->bValid) ++Runtime->Status.DeepValidationCount;
		if (UTransactionCodeMessage* Handler = FindHandler(Kind)) Handler->ProcessStructData(ParsedData);
		AddReceiveLog(*Runtime, *Data, ParseLatencyMs);
	}
	TryStartQueuedParses();
}

void AVirtualSensorExternalSourceHostActor::AddReceiveLog(
	const FReceiverRuntime& Runtime,
	const FVirtualSensorTopicReceivedDataBase& Data,
	float ParseLatencyMs)
{
	FVirtualSensorTopicReceiveLogEntry Entry;
	Entry.TimestampUtc = FDateTime::UtcNow();
	Entry.Kind = Runtime.Status.Kind;
	Entry.Topic = Runtime.Status.Topic;
	Entry.SensorId = Data.SensorId;
	Entry.FrameId = Data.FrameId;
	Entry.MessageBytes = Data.MessageBytes;
	Entry.ParseLatencyMs = ParseLatencyMs;
	Entry.bValid = Data.bValid;
	Entry.bDeepValidated = Data.bDeepValidated;
	Entry.Message = Data.Message.Left(512);
	RecentTopicReceiveLogs.Insert(MoveTemp(Entry), 0);
	if (RecentTopicReceiveLogs.Num() > 200) RecentTopicReceiveLogs.SetNum(200, false);
}
