#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlabScenarioTypes.h"
#include "SlabMetricsComp.generated.h"

UCLASS(ClassGroup=(Slab),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API USlabMetricsComp : public UActorComponent
{
	GENERATED_BODY()
public:
	USlabMetricsComp();
	static FSlabMetrics Calculate(const FSlabScenarioRow& Row,const FTransform& SlabWorldTransform,const FVector& SizeCm,
		const FTransform& TrackWorldTransform,double LeftRailYcm,double RightRailYcm,bool bRailsConfigured);
	void Update(const FSlabMetrics& Value) { Current=Value; }
	UFUNCTION(BlueprintPure,Category="Slab|Metrics") FSlabMetrics GetMetrics() const { return Current; }
private:
	UPROPERTY(Transient) FSlabMetrics Current;
};
