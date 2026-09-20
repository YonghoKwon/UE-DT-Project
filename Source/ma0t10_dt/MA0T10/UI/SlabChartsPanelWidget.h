#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "SlabScenarioChartWidget.h"
#include "SlabChartsPanelWidget.generated.h"
class ASlabActor;
struct FSlabScenarioData;

UCLASS(BlueprintType)
class MA0T10_DT_API USlabChartsPanelWidget : public UVirtualSensorPanelWidgetBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void BindSlabActor(ASlabActor* InSlab);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void SetChartMetric(ESlabChartMetric Metric);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void RefreshChartData();
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") ASlabActor* GetBoundSlabActor() const { return Slab.Get(); }
protected:
	/** Optional designer binding; a nested colleague-owned UserWidget is never traversed. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="DigitalTwin|Slab|UI") TObjectPtr<USlabScenarioChartWidget> ScenarioChart;
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& G, float D) override;
private:
	UPROPERTY(Transient) TObjectPtr<ASlabActor> Slab;
	UPROPERTY(Transient) TObjectPtr<USlabScenarioChartWidget> Chart;
	const FSlabScenarioData* LastScenario = nullptr;
	uint32 LastConfigurationHash = 0;
	bool bWasBodyVisible = false;
	bool bFrameAxis = false;
	bool bShowSeries[2] = {true, true};
	ESlabChartMetric SelectedMetric = ESlabChartMetric::Angles;
	float RefreshAccumulator = 0;
	float TextAccumulator = 0;
	FString StatusText;
};
