#include "VirtualCameraStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/Sensor/DeviceSensorStreamCodec.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualCameraStreamReceiverTC.h"
#include "Json.h"
#include "Misc/Base64.h"
#include "ma0t10_dt/MA0T10/Core/DeviceSensorLog.h"
UVirtualCameraStreamReceiverTC::UVirtualCameraStreamReceiverTC()
{
	TransactionCode = TEXT("VIRTUAL_CAMERA_STREAM");
}

TSharedPtr<FTransactionCodeDataBase> UVirtualCameraStreamReceiverTC::ParseToStruct(const FString& JsonString) const
{ return DeviceSensorStreamCodec::ParseCameraJson(JsonString); }

void UVirtualCameraStreamReceiverTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	const TSharedPtr<FVirtualCameraStreamReceiverData> Camera = StaticCastSharedPtr<FVirtualCameraStreamReceiverData>(Data);
	if (!Camera.IsValid()) return;
	const double Now = FPlatformTime::Seconds();
	if (!Camera->bValid || LastOutputLogSeconds < 0.0 || Now - LastOutputLogSeconds >= 1.0)
	{
		LastOutputLogSeconds = Now;
		UE_LOG(LogDTDevice, Log, TEXT("[SensorTopicReceiver][Camera] sensor=%s frame=%lld bytes=%d valid=%s %s"),
			*Camera->SensorId, Camera->FrameId, Camera->MessageBytes, Camera->bValid ? TEXT("true") : TEXT("false"), *Camera->Message);
	}
}
