#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "VirtualSensorControlTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorPanelPlacement : uint8
{
    RightCenter,
    LeftCenter,
    BottomCenter
};

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
struct MA0T10_DT_API FVirtualSensorSettingHelpDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FName Key = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Label;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Unit;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText PerformanceImpact;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText RecommendedValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Caution;
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
