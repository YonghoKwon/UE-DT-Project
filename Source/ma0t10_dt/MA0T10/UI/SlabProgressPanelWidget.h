#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "SlabProgressPanelWidget.generated.h"
class ASlabActor;
struct FSlabSimulationStatus;
struct FVirtualSlabSessionStatus;
class SExpandableArea;

UCLASS(BlueprintType)
class MA0T10_DT_API USlabProgressPanelWidget : public UVirtualSensorPanelWidgetBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void BindSlabActor(ASlabActor* InSlab);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") ASlabActor* GetBoundSlabActor() const { return Slab.Get(); }
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") bool CanResetSlabToInitialPlacement(FString& OutReason) const;
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") bool ResetSlabToInitialPlacement(FString& OutError);
	bool ToggleOwnedCharts();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& G, float D) override;
private:
	UPROPERTY(Transient) TObjectPtr<ASlabActor> Slab;
	float RefreshAccumulator = 0;
	float Progress = 0;
	FString Summary, Detail, SensorStatus, SetupStatus;
	bool bOwnedDetailsExpanded=false;
	TSharedPtr<SExpandableArea> OwnedDisplayArea;
	void RefreshOwnedStatus();
	bool bOwnedCanPause=false,bOwnedCanStop=false,bOwnedCanReset=false;
	FText OwnedPauseLabel,OwnedResetTooltip,OwnedSummaryLabel,OwnedStatusLabel,OwnedDetailLabel;
	static FText ResolveOwnedMovementState(const FSlabSimulationStatus& Simulation,const FVirtualSlabSessionStatus& Session);
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabProgressOutcomeTest;
	friend class FSlabScenarioScreenTest;
	friend class FUxrScenarioRender;
#endif
};
