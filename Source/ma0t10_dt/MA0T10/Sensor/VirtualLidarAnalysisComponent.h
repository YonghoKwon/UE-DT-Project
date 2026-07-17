#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualLidarAnalysisComponent.generated.h"

class UVirtualLidarScanComponent;

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualLidarAnalysisComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualLidarAnalysisComponent();
	void BindScanComponent(UVirtualLidarScanComponent* InScanComponent);
	void AnalyzeLatestFrame();
	void ApplyPrecomputedStatistics(int32 HitPointCount, const TMap<FString, int32>& SemanticCounts);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Analysis")
	int32 GetLatestHitPointCount() const { return LatestHitPointCount; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Analysis")
	TMap<FString, int32> GetLatestSemanticCounts() const { return LatestSemanticCounts; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UVirtualLidarScanComponent> ScanComponent;

	UPROPERTY(Transient)
	TMap<FString, int32> LatestSemanticCounts;

	int32 LatestHitPointCount = 0;
};
