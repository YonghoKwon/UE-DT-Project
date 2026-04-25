#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorAct.generated.h"

class UVirtualLidarComp;
class UVirtualSensorPublisherComp;

UCLASS()
class M7AT10_DT_API AVirtualSensorAct : public AActor
{
	GENERATED_BODY()

public:
	AVirtualSensorAct();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<UVirtualLidarComp> LidarComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<UVirtualSensorPublisherComp> SensorPublisherComp;
};
