#pragma once

#include "CoreMinimal.h"
#include "DeviceUI/DeviceSensorPanelTypes.h"
#include "Device/Camera/VirtualCameraCaptureComponent.h"
#include "Device/Sensor/VirtualLidarScanComponent.h"
#include "Device/Sensor/VirtualSensorEditableTypes.h"
#include "VirtualSensorControlTypes.generated.h"



UENUM(BlueprintType)
enum class EVirtualSensorGizmoMode : uint8
{
    Translate UMETA(DisplayName = "Move"),
    Rotate UMETA(DisplayName = "Rotate")
};

UENUM(BlueprintType)
enum class EVirtualSensorCoordinateSpace : uint8
{
    Local UMETA(DisplayName = "Local"),
    World UMETA(DisplayName = "World")
};

UENUM(BlueprintType)
enum class EVirtualSensorPlacementInputMode : uint8
{
    Continuous UMETA(DisplayName="연속 이동"),
    Precision UMETA(DisplayName="정밀 단위")
};



USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorMapApplySnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    FString SourceMapPackage = TEXT("/Game/MA0T10/Maps/SensorTestMap");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl")
    TArray<FVirtualSensorEditableState> SensorStates;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorExportCompleted, const FVirtualSensorExportResult&, Result);
