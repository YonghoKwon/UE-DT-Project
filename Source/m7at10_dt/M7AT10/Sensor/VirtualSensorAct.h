#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorAct.generated.h"

class UVirtualDistanceSensorComp;
class USensorDataPublisherComp;

UCLASS()
class M7AT10_DT_API AVirtualSensorAct : public AActor
{
	GENERATED_BODY()
	
public:	
	AVirtualSensorAct();

	// 센서 컴포넌트를 이 액터의 핵심(Root)으로 사용합니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<UVirtualDistanceSensorComp> DistanceSensorComp;

	// 센서 측정 결과를 외부 시스템으로 전달하는 전송 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<USensorDataPublisherComp> SensorPublisherComp;
};
