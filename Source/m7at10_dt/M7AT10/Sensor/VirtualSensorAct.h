#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorAct.generated.h"

class UVirtualDistanceSensorComp;

UCLASS()
class M7AT10_DT_API AVirtualSensorAct : public AActor
{
	GENERATED_BODY()
	
public:	
	AVirtualSensorAct();

	// 센서 컴포넌트를 이 액터의 핵심(Root)으로 사용합니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<UVirtualDistanceSensorComp> DistanceSensorComp;
};