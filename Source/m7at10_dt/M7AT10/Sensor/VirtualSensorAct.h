#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorAct.generated.h"

class UArrowComponent;
class UVirtualLidarSensorComp;

UCLASS()
class M7AT10_DT_API AVirtualSensorAct : public AActor
{
    GENERATED_BODY()

public:
    AVirtualSensorAct();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    TObjectPtr<UVirtualLidarSensorComp> LidarSensorComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|EditorVisualization")
    TObjectPtr<UArrowComponent> EditorForwardArrowComp;
};
