#include "VirtualLidarStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/Sensor/DeviceSensorStreamCodec.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualLidarStreamReceiverTC.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Core/DeviceSensorLog.h"
UVirtualLidarStreamReceiverTC::UVirtualLidarStreamReceiverTC()
{
	TransactionCode = TEXT("VIRTUAL_LIDAR_STREAM");
}

TSharedPtr<FTransactionCodeDataBase> UVirtualLidarStreamReceiverTC::ParseToStruct(const FString& JsonString) const
{ return DeviceSensorStreamCodec::ParseLidarJson(JsonString); }

void UVirtualLidarStreamReceiverTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	const TSharedPtr<FVirtualLidarStreamReceiverData> Lidar = StaticCastSharedPtr<FVirtualLidarStreamReceiverData>(Data);
	if (!Lidar.IsValid()) return;
	const double Now = FPlatformTime::Seconds();
	if (!Lidar->bValid || LastOutputLogSeconds < 0.0 || Now - LastOutputLogSeconds >= 1.0)
	{
		LastOutputLogSeconds = Now;
		UE_LOG(LogDTDevice, Log, TEXT("[SensorTopicReceiver][LiDAR] sensor=%s frame=%lld bytes=%d valid=%s %s"),
			*Lidar->SensorId, Lidar->FrameId, Lidar->MessageBytes, Lidar->bValid ? TEXT("true") : TEXT("false"), *Lidar->Message);
	}
}
