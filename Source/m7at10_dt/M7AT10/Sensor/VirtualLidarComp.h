#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualLidarComp.generated.h"

class USensorDataRelayComp;

USTRUCT(BlueprintType)
struct FLidarScanPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="VirtualSensor|Lidar")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="VirtualSensor|Lidar")
	float Distance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="VirtualSensor|Lidar")
	bool bHit = false;
};

/**
 * 단순 2D 스캔(수평 스윕) 방식의 가상 3D LiDAR 컴포넌트.
 * 포인트 샘플을 JSON으로 직렬화해 SensorDataRelayComp에 전달한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualLidarComp : public USceneComponent
{
	GENERATED_BODY()

public:
	UVirtualLidarComp();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable, Category="VirtualSensor|Lidar")
	void CaptureAndSendScan();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Lidar")
	FString SensorId = TEXT("lidar.main");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Lidar")
	float ScanInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Lidar")
	int32 HorizontalSamples = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Lidar")
	float HorizontalFovDeg = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Lidar")
	float MaxDistanceCm = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Lidar")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Lidar")
	bool bDrawDebugTrace = false;

private:
	FString BuildPayloadJson(const TArray<FLidarScanPoint>& ScanPoints) const;
	void ResolveRelayComponent();

	UPROPERTY(Transient)
	TObjectPtr<USensorDataRelayComp> RelayComp;

	FTimerHandle ScanTimerHandle;
};
