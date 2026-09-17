#pragma once

#include "CoreMinimal.h"
#include "VirtualSlabFrameContext.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDeviceProfileTypes.h"

struct MA0T10_DT_API FVirtualLidarDepthAcquisitionRequest
{
	bool bRequestSemantics = false;
	TArray<float> HorizontalAngles;
	TArray<float> VerticalAngles;
	FVirtualSlabFrameContext SlabContext;
	FTransform AcquisitionTransform = FTransform::Identity;
	int64 FrameId = 0;
	int32 HorizontalSamples = 1;
	int32 VerticalChannels = 1;
	float HorizontalFovDegrees = 90.0f;
	float MinVerticalAngleDegrees = -10.0f;
	float MaxVerticalAngleDegrees = 10.0f;
	float MaxDistanceCm = 10000.0f;
	int64 AcquisitionStartUnixNanoseconds = 0;
};
struct FVirtualLidarGpuSemanticIdentity
{
	FName Label;
	FName ActorName;
	FName ActorClass;
	TArray<FName> ActorTags;
};
struct MA0T10_DT_API FVirtualLidarDepthAcquisitionFrame
{
	FVirtualLidarDepthAcquisitionRequest Request;
	int32 CaptureWidth = 0;
	int32 CaptureHeight = 0;
	TArray<float> ForwardDepthCentimeters;
	TArray<FLinearColor> SemanticIdDepth;
	TMap<int32, FVirtualLidarGpuSemanticIdentity> SemanticIdentities;
	FString SemanticStatus;
	int64 AcquisitionEndUnixNanoseconds = 0;
};

enum class EVirtualSensorBackendPollResult : uint8
{
	Idle,
	Pending,
	Completed,
	Failed
};

/** Runtime-pluggable acquisition boundary shared by scalable sensor backends. */
class MA0T10_DT_API IVirtualSensorAcquisitionBackend
{
public:
	virtual ~IVirtualSensorAcquisitionBackend() = default;
	virtual EVirtualLidarAcquisitionBackend GetBackendType() const = 0;
	virtual bool IsAvailable() const = 0;
	virtual bool BeginAcquisition(const FVirtualLidarDepthAcquisitionRequest& Request) = 0;
	virtual EVirtualSensorBackendPollResult PollAcquisition(FVirtualLidarDepthAcquisitionFrame& OutFrame) = 0;
	virtual void CancelAcquisition() = 0;
	virtual FString GetBackendStatusMessage() const = 0;
};
