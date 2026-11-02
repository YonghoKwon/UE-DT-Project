#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorStreamReceiverTypes.h"

struct FVirtualCameraStreamReceiverData : FVirtualSensorTopicReceivedDataBase
{
	int32 Width = 0;
	int32 Height = 0;
	int32 DeclaredByteSize = 0;
	int32 DecodedByteSize = 0;
	FString Encoding;
};
struct FVirtualLidarStreamReceiverData : FVirtualSensorTopicReceivedDataBase
{
	int32 HorizontalSamples = 0;
	int32 VerticalChannels = 0;
	int32 RayCount = 0;
	int32 TotalPointCount = 0;
	int32 HitPointCount = 0;
	int32 PayloadPointCount = 0;
};
struct FVirtualPointCloudStreamReceiverData : FVirtualSensorTopicReceivedDataBase
{
	FString Format;
	FString Encoding;
	int32 PointCount = 0;
	int32 DeclaredByteCount = 0;
	int32 DecodedByteCount = 0;
	int32 SourcePointCount = 0;
	int32 FilterRevision = 0;
	FString ChecksumSha1;
	FString ProfileKey;
};
