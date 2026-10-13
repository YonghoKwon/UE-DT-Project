#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "SlabProgressPanelWidget.generated.h"
class ASlabActor;
struct FSlabSimulationStatus;
struct FVirtualSlabSessionStatus;

UCLASS(BlueprintType)
class MA0T10_DT_API USlabProgressPanelWidget : public UVirtualSensorPanelWidgetBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void BindSlabActor(ASlabActor* InSlab);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") ASlabActor* GetBoundSlabActor() const { return Slab.Get(); }
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") bool CanResetSlabToInitialPlacement(FString& OutReason) const;
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") bool ResetSlabToInitialPlacement(FString& OutError);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& G, float D) override;
private:
	UPROPERTY(Transient) TObjectPtr<ASlabActor> Slab;
	float RefreshAccumulator = 0;
	float Progress = 0;
	FString Summary, Detail, SensorStatus, SetupStatus;
	static FText ResolveOwnedMovementState(const FSlabSimulationStatus& Simulation,const FVirtualSlabSessionStatus& Session);
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabProgressOutcomeTest;
#endif
};
