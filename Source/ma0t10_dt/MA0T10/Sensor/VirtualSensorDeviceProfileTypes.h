#pragma once

#include "CoreMinimal.h"
#include "VirtualSensorDeviceProfileTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualCameraDeviceProfile : uint8
{
    Generic UMETA(DisplayName = "Generic Camera"),
    IntelRealSenseD455 UMETA(DisplayName = "Intel RealSense D455")
};

UENUM(BlueprintType)
enum class EVirtualLidarDeviceProfile : uint8
{
    Generic UMETA(DisplayName = "Generic LiDAR"),
    LivoxMid360S UMETA(DisplayName = "Livox Mid-360S"),
    IYOBOT_MLX80 UMETA(DisplayName = "IYOBOT ML-X(80)")
};

UENUM(BlueprintType)
enum class EVirtualSensorSimulationQuality : uint8
{
    Debug UMETA(DisplayName = "Debug"),
    RealTimePreview UMETA(DisplayName = "Real-Time Preview"),
    Balanced UMETA(DisplayName = "Balanced"),
    FullSpec UMETA(DisplayName = "Full Spec"),
    Custom UMETA(DisplayName = "Custom")
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorDeviceSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    FString Manufacturer = TEXT("Generic");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    FString Model = TEXT("Generic");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float HorizontalFovDegrees = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float VerticalFovDegrees = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float MinRangeCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float TypicalRangeCm = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float MaxRangeCm = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float FrameRateHz = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    int32 Width = 1280;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    int32 Height = 720;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    int32 PointRate = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    FString Notes;
};

/** Resolved runtime values for a LiDAR device profile and quality pair. */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarProfilePreset
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    FVirtualSensorDeviceSpec DeviceSpec;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    float ScanIntervalSeconds = 0.25f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    float MaxDistanceCm = 4000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    int32 HorizontalSamples = 120;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    int32 VerticalChannels = 24;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    float HorizontalFovDegrees = 360.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    float MinVerticalAngleDegrees = -7.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    float MaxVerticalAngleDegrees = 52.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    int32 PreviewPointStride = 2;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    int32 MaxPreviewPoints = 3000;
};
