#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorStressSceneActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Deterministic large-scene fixture. HISM instances represent 10,000 static
 * primitives and 1,000 moving collision/render proxies without creating
 * 11,000 Actors or per-object Tick functions.
 */
UCLASS()
class MA0T10_DT_API AVirtualSensorStressSceneActor : public AActor
{
	GENERATED_BODY()

public:
	AVirtualSensorStressSceneActor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Stress")
	void GenerateStressScene();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stress")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stress")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StaticPrimitives;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stress")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MovingProxies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stress", meta = (ClampMin = "1", ClampMax = "20000"))
	int32 StaticPrimitiveCount = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stress", meta = (ClampMin = "0", ClampMax = "5000"))
	int32 MovingProxyCount = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stress", meta = (ClampMin = "1.0"))
	float GridSpacingCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stress", meta = (ClampMin = "0.0"))
	float MovingAmplitudeCm = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stress", meta = (ClampMin = "0.1", ClampMax = "60.0"))
	float MovingUpdateHz = 10.0f;

private:
	TArray<FTransform> MovingBaseTransforms;
	double NextMovingUpdateSeconds = 0.0;
};
