#pragma once
#include "CoreMinimal.h"
#include "DeviceSensorPanelTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorPanelPlacement : uint8
{
    RightCenter,
    LeftCenter,
    BottomCenter
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
