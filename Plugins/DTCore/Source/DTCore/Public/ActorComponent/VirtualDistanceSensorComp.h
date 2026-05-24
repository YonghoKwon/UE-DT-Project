#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualDistanceSensorComp.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DTCORE_API UVirtualDistanceSensorComp : public USceneComponent
{
	GENERATED_BODY()

public:
	UVirtualDistanceSensorComp();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 최소 감지 거리 (단위: cm) - 사각지대 모사
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Specs")
	float MinSensorRange = 20.0f;

	// 최대 감지 거리 (단위: cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Specs")
	float MaxSensorRange = 5000.0f;

	// 센서 시야각 (단위: 도) - 값이 클수록 넓게 퍼져서 감지함 (예: 15도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Specs")
	float SensorConeAngle = 10.0f;

	// 한 번 측정할 때 발사할 레이저 가닥 수 (예: 10개)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Specs")
	int32 RayCount = 10;

	// 측정 노이즈 (오차 범위, 단위: cm) - 실제 센서의 불안정성 모사
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Specs")
	float NoiseMargin = 5.0f;

	// 측정 주기 (단위: 초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Operation")
	float MeasureInterval = 0.5f;

	// 디버그 라인 표시 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Operation")
	bool bDrawDebugLine = true;

private:
	FTimerHandle MeasureTimerHandle;

	UFUNCTION()
	void MeasureAndLogData();
};