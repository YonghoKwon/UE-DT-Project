#include "SensorDataRelayComp.h"

#include "m7at10_dt/m7at10_dt.h"

USensorDataRelayComp::USensorDataRelayComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USensorDataRelayComp::PublishPayload(const FString& SensorId, const FString& SensorType, const FString& PayloadJson)
{
	if (SensorId.IsEmpty())
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[SensorRelay] Empty SensorId. Payload ignored."));
		return;
	}

	LastPayloadBySensorId.FindOrAdd(SensorId) = PayloadJson;
	OnPayloadReady.Broadcast(SensorId, SensorType, PayloadJson);

	if (bEnableLogEcho)
	{
		UE_LOG(LogM7AT10, Log, TEXT("[SensorRelay] %s (%s) payload size: %d"), *SensorId, *SensorType, PayloadJson.Len());
	}
}

bool USensorDataRelayComp::GetLastPayload(const FString& SensorId, FString& OutPayloadJson) const
{
	if (const FString* FoundPayload = LastPayloadBySensorId.Find(SensorId))
	{
		OutPayloadJson = *FoundPayload;
		return true;
	}

	return false;
}
