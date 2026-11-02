#pragma once
#include "CoreMinimal.h"
#include "DeviceSensorCoordinatorTypes.generated.h"
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EVirtualSensorViewMode : uint8
{
    RealWorld UMETA(DisplayName = "Real World"),
    Camera UMETA(DisplayName = "Camera"),
    Lidar UMETA(DisplayName = "Lidar"),
    PointCloudOnly UMETA(DisplayName = "Point Cloud Only")
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    FString SensorId;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    FString SensorType;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    int32 Index = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    bool bValid = false;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorHealthSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    FString Summary;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 CameraCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 LidarCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 RealSensorSourceCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 RunningRealSensorSourceCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 ErrorRealSensorSourceCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 ExternalEvidenceRequiredRealSensorSourceCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 StaleSensorCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    bool bHealthy = true;
};

USTRUCT()
struct MA0T10_DT_API FVirtualSensorHiddenComponentState
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UPrimitiveComponent> Component;

    UPROPERTY()
    bool bWasHiddenInGame = false;

    UPROPERTY()
    bool bWasVisible = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorViewModeChanged, EVirtualSensorViewMode, NewMode);
