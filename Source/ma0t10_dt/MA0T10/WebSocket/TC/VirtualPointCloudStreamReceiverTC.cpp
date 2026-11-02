#include "VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/Sensor/DeviceSensorStreamCodec.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorIdentity.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPcdReceiveContract.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "ma0t10_dt/MA0T10/Core/DeviceSensorLog.h"
UVirtualPointCloudStreamReceiverTC::UVirtualPointCloudStreamReceiverTC()
{
	TransactionCode = TEXT("VIRTUAL_POINTCLOUD_STREAM");
}

TSharedPtr<FTransactionCodeDataBase> UVirtualPointCloudStreamReceiverTC::ParseToStruct(const FString& JsonString) const
{ return DeviceSensorStreamCodec::ParsePointCloudJson(JsonString); }

TSharedPtr<FTransactionCodeDataBase> UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(const TArray<uint8>& Body,const TMap<FName,FString>& Headers)
{ return DeviceSensorStreamCodec::ParseBinaryPcd(Body,Headers); }

void UVirtualPointCloudStreamReceiverTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	const TSharedPtr<FVirtualPointCloudStreamReceiverData> PointCloud = StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Data);
	if (!PointCloud.IsValid()) return;
	const double Now = FPlatformTime::Seconds();
	if (LastOutputLogSeconds < 0.0 || Now - LastOutputLogSeconds >= 1.0)
	{
		LastOutputLogSeconds = Now;
		UE_LOG(LogDTDevice, Log, TEXT("[SensorTopicReceiver][PointCloud] sensor=%s frame=%lld bytes=%d valid=%s %s"),
			*PointCloud->SensorId, PointCloud->FrameId, PointCloud->MessageBytes,
			PointCloud->bValid ? TEXT("true") : TEXT("false"), *PointCloud->Message);
	}
}
