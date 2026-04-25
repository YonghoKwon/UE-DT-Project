#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorAct.generated.h"

class USceneComponent;
class UVirtualCameraComp;
class UVirtualLidarComp;
class USensorDataRelayComp;

UCLASS()
class M7AT10_DT_API AVirtualSensorAct : public AActor
{
	GENERATED_BODY()
	
public:	
	AVirtualSensorAct();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<USceneComponent> SceneRoot;

	// 가상 카메라 센서
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<UVirtualCameraComp> VirtualCameraComp;

	// 가상 LiDAR 센서
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<UVirtualLidarComp> VirtualLidarComp;

	// 센서 데이터 릴레이(외부 전송 연동 포인트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor")
	TObjectPtr<USensorDataRelayComp> SensorDataRelayComp;
};
