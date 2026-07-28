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
    /** Existing project integration contract: 200 x 56 at 20 Hz. */
    IYOBOT_MLX80 UMETA(DisplayName = "IYOBOT ML-X(80) - Integration 200"),
    /** Appended for serialized-enum compatibility. Public-spec native density. */
    IYOBOT_MLX80_NATIVE UMETA(DisplayName = "IYOBOT ML-X(80) - Native")
};

UENUM(BlueprintType)
enum class EVirtualLidarProfileClass : uint8
{
    Generic UMETA(DisplayName = "Generic"),
    PublicSpecNative UMETA(DisplayName = "Public Spec Native"),
    IntegrationDownsampled UMETA(DisplayName = "Integration / Downsampled"),
    UserCustom UMETA(DisplayName = "User Custom")
};

UENUM(BlueprintType)
enum class EVirtualSensorFidelityMode : uint8
{
    IdealTruth UMETA(DisplayName = "Ideal Truth"),
    PublicSpecBased UMETA(DisplayName = "Public Spec Based"),
    HardwareCalibrated UMETA(DisplayName = "Hardware Calibrated"),
    ReplayOrHardwareInput UMETA(DisplayName = "Replay / Hardware Input")
};

UENUM(BlueprintType)
enum class EVirtualLidarAcquisitionBackend : uint8
{
    Auto UMETA(DisplayName = "Auto"),
    AccurateCpuTrace UMETA(DisplayName = "Accurate CPU Trace"),
    GpuDepthProjection UMETA(DisplayName = "GPU Depth Projection"),
    HardwareRayTracing UMETA(DisplayName = "Hardware Ray Tracing"),
    ReplayOrExternal UMETA(DisplayName = "Replay / External")
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
    float HorizontalAngularResolutionDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float VerticalAngularResolutionDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    int32 MaxEchoesPerPixel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float MaxDistanceErrorMillimeters = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float WavelengthNanometers = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float ReferenceReflectivityPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    float ReferenceAmbientLux = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    FString OutputFields;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|DeviceProfile")
    bool bProtocolVerifiedAgainstHardware = false;

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
    EVirtualLidarProfileClass ProfileClass = EVirtualLidarProfileClass::Generic;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    EVirtualSensorFidelityMode FidelityMode = EVirtualSensorFidelityMode::IdealTruth;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    EVirtualLidarAcquisitionBackend RecommendedBackend = EVirtualLidarAcquisitionBackend::Auto;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    FString ProfileKey = TEXT("generic");

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|DeviceProfile")
    FString CalibrationId = TEXT("none");

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
