#include "SensorDataPublisherComp.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "m7at10_dt/m7at10_dt.h"

USensorDataPublisherComp::USensorDataPublisherComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USensorDataPublisherComp::PublishPacket(FName SensorType, const FString& SensorName, const FString& JsonPayload)
{
	if (TransportMode == ESensorTransportMode::Disabled)
	{
		return;
	}

	if (TransportMode == ESensorTransportMode::LogOnly)
	{
		UE_LOG(LogM7AT10, Log, TEXT("[SensorPublisher] %s(%s) payload bytes=%d"), *SensorType.ToString(), *SensorName, JsonPayload.Len());
		OnPacketPublished.Broadcast(SensorType, SensorName, true, TEXT("Logged"));
		return;
	}

	if (EndpointUrl.IsEmpty())
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[SensorPublisher] EndpointUrl is empty. Sensor=%s"), *SensorName);
		OnPacketPublished.Broadcast(SensorType, SensorName, false, TEXT("EndpointUrl is empty"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(EndpointUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetTimeout(HttpTimeoutSeconds);
	for (const TPair<FString, FString>& Header : AdditionalHeaders)
	{
		Request->SetHeader(Header.Key, Header.Value);
	}
	Request->SetContentAsString(JsonPayload);

	Request->OnProcessRequestComplete().BindLambda(
		[this, SensorType, SensorName](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !HttpResponse.IsValid())
			{
				UE_LOG(LogM7AT10, Warning, TEXT("[SensorPublisher] HTTP send failed. Sensor=%s"), *SensorName);
				OnPacketPublished.Broadcast(SensorType, SensorName, false, TEXT("HTTP connection failed"));
				return;
			}

			const int32 StatusCode = HttpResponse->GetResponseCode();
			const bool bOk = StatusCode >= 200 && StatusCode < 300;
			const FString Message = FString::Printf(TEXT("HTTP %d"), StatusCode);

			if (bOk)
			{
				UE_LOG(LogM7AT10, Verbose, TEXT("[SensorPublisher] HTTP success. Sensor=%s Code=%d"), *SensorName, StatusCode);
			}
			else
			{
				UE_LOG(LogM7AT10, Warning, TEXT("[SensorPublisher] HTTP failed. Sensor=%s Code=%d Body=%s"), *SensorName, StatusCode, *HttpResponse->GetContentAsString());
			}

			OnPacketPublished.Broadcast(SensorType, SensorName, bOk, Message);
		});

	Request->ProcessRequest();
}
