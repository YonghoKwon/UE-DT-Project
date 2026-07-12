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
    LivoxMid360S UMETA(DisplayName = "Livox Mid-360S")
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