#pragma once

#include "CoreMinimal.h"

/** Immutable values needed to encode a virtual-camera.v1 payload. */
struct MA0T10_DT_API FVirtualCameraPayloadSnapshot
{
    FString SensorId;
    FString Manufacturer;
    FString Model;
    FString SimulationQuality;
    int64 FrameId = 0;
    FDateTime TimestampUtc;
    int32 Width = 0;
    int32 Height = 0;
    float HorizontalFov = 0.0f;
    float VerticalFov = 0.0f;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Forward = FVector::ForwardVector;
    FVector Up = FVector::UpVector;
};

/** Stateless camera payload codec shared by synchronous and scheduled capture. */
class MA0T10_DT_API FVirtualCameraPayloadCodec
{
public:
    static FString EncodeJpegBase64(
        const FVirtualCameraPayloadSnapshot& Snapshot,
        const FString& Base64Image,
        int64 ByteSize);
};
