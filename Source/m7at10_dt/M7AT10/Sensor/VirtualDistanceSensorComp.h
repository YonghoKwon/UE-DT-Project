#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualDistanceSensorComp.generated.h"

class USensorDataPublisherComp;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualLidarScanReady, const FString&, JsonPayload);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualDistanceSensorComp : public USceneComponent
{
	GENERATED_BODY()

public:
	UVirtualDistanceSensorComp();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Sensor|Lidar")
	void PerformScan();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar")
	FString SensorName = TEXT("Lidar_01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ScanInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float MaxDistanceCm = 30000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar", meta = (ClampMin = "1", UIMin = "1"))
	int32 HorizontalSamples = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar", meta = (ClampMin = "1", UIMin = "1"))
	int32 VerticalSamples = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar", meta = (ClampMin = "1.0", ClampMax = "360.0", UIMin = "1.0", UIMax = "360.0"))
	float HorizontalFovDeg = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar", meta = (ClampMin = "1.0", ClampMax = "120.0", UIMin = "1.0", UIMax = "120.0"))
	float VerticalFovDeg = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar")
	bool bIgnoreOwner = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar|Debug")
	bool bDrawDebugPoints = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Lidar|Debug", meta = (EditCondition = "bDrawDebugPoints", ClampMin = "0.0", UIMin = "0.0"))
	float DebugDuration = 0.25f;

	UPROPERTY(BlueprintAssignable, Category = "Sensor|Lidar")
	FOnVirtualLidarScanReady OnScanReady;

private:
	USensorDataPublisherComp* ResolvePublisher() const;

	FTimerHandle ScanTimerHandle;
};
