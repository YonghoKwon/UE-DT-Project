#pragma once
#include "CoreMinimal.h"
#include "ActorComponent/StatusVisualizerCompBase.h"
#include "SlabVisualizationComponent.generated.h"
class UStaticMeshComponent;
UCLASS(ClassGroup=(Slab),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API USlabVisualizationComponent : public UStatusVisualizerCompBase
{
	GENERATED_BODY()
public:
	USlabVisualizationComponent();
	void UpdateGeometry(const FVector& SizeCm);
	UFUNCTION(BlueprintCallable,Category="Slab|Appearance") void SetHelpersVisible(bool bVisible);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void EnsureHelpers();
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Helpers;
	bool bHelpersVisible=true;
};
