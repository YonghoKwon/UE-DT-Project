#include "VirtualSensorPublisherComp.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "m7at10_dt/m7at10_dt.h"

UVirtualSensorPublisherComp::UVirtualSensorPublisherComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualSensorPublisherComp::PublishSensorPacket(const FVirtualSensorPacket& Packet)
{
	if (!bEnableHttpPublish)
	{
		return;
	}

	if (EndpointUrl.IsEmpty())
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[VirtualSensorPublisher] EndpointUrl is empty. Skip publish."));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(EndpointUrl);
	Request->SetVerb(HttpMethod.IsEmpty() ? TEXT("POST") : HttpMethod);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	for (const TPair<FString, FString>& Header : AdditionalHeaders)
	{
		Request->SetHeader(Header.Key, Header.Value);
	}

	const FString RequestBody = Packet.PayloadJson;
	Request->SetContentAsString(RequestBody);
	Request->OnProcessRequestComplete().BindUObject(this, &UVirtualSensorPublisherComp::OnPublishResponse);
	Request->ProcessRequest();

	if (bLogPayload)
	{
		UE_LOG(LogM7AT10, Log, TEXT("[VirtualSensorPublisher] Sent %s payload: %s"), *Packet.SensorType, *RequestBody);
	}
}

void UVirtualSensorPublisherComp::OnPublishResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[VirtualSensorPublisher] Publish failed. URL=%s"), Request.IsValid() ? *Request->GetURL() : TEXT("InvalidRequest"));
		return;
	}

	UE_LOG(LogM7AT10, Log, TEXT("[VirtualSensorPublisher] Publish done. Code=%d"), Response->GetResponseCode());
}
