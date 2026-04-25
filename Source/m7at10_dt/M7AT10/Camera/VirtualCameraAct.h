#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualCameraAct.generated.h"

class UVirtualCameraComp;
class UVirtualSensorPublisherComp;

UCLASS()
class M7AT10_DT_API AVirtualCameraAct : public AActor
{
	GENERATED_BODY()

public:
	AVirtualCameraAct();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UVirtualCameraComp> VirtualCameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UVirtualSensorPublisherComp> SensorPublisherComp;
};
