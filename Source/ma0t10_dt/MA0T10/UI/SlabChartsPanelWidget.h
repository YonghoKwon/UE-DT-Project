#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "SlabScenarioChartWidget.h"
#include "SlabChartsPanelWidget.generated.h"
class ASlabActor;
struct FSlabScenarioData;
struct FSlabScenarioRow;

UCLASS(BlueprintType)
class MA0T10_DT_API USlabChartsPanelWidget : public UVirtualSensorPanelWidgetBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void BindSlabActor(ASlabActor* InSlab);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void SetChartMetric(ESlabChartMetric Metric);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void SetChartSlotMetric(int32 SlotIndex, ESlabChartMetric Metric);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") ESlabChartMetric GetChartSlotMetric(int32 SlotIndex) const;
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") USlabScenarioChartWidget* GetChartWidget(int32 SlotIndex) const;
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void SetChartFrameAxis(bool bUseFrames);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void ResetAllChartViews();
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|UI") void RefreshChartData();
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|UI") ASlabActor* GetBoundSlabActor() const { return Slab.Get(); }
	static double CalculateHistoricalSpeed(const TArray<FSlabScenarioRow>& Rows, int32 Index, double PositionToCm);
protected:
	/** Optional designer binding; a nested colleague-owned UserWidget is never traversed. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="DigitalTwin|Slab|UI") TObjectPtr<USlabScenarioChartWidget> ScenarioChart;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="DigitalTwin|Slab|UI") TObjectPtr<USlabScenarioChartWidget> ScenarioChart2;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="DigitalTwin|Slab|UI") TObjectPtr<USlabScenarioChartWidget> ScenarioChart3;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& G, float D) override;
private:
	UPROPERTY(Transient) TObjectPtr<ASlabActor> Slab;
	UPROPERTY(Transient) TArray<TObjectPtr<USlabScenarioChartWidget>> Charts;
	TSharedPtr<const TArray<FSlabChartSample>> SharedSamples;
	TArray<TSharedPtr<ESlabChartMetric>> MetricOptions;
	FComboBoxStyle MetricComboStyle;
	const FSlabScenarioData* LastScenario = nullptr;
	uint32 LastConfigurationHash = 0;
	bool bWasBodyVisible = false;
	bool bFrameAxis = false;
	bool bShowSeries[3][2] = {{true,true},{true,true},{true,true}};
	ESlabChartMetric SelectedMetrics[3] = {ESlabChartMetric::Angles, ESlabChartMetric::CenterOffset, ESlabChartMetric::RailMargins};
	FString LastRunUUID;
	float RefreshAccumulator = 0;
	float TextAccumulator = 0;
	FString StatusText;
	FString HoverTexts[3];
	FString CurrentTexts[3];
	UFUNCTION() void SynchronizeViewRange(double Minimum, double Maximum);
	UFUNCTION() void HandleSlabStateChanged();
	void InitializeCharts(bool bCreateNative);
	void UpdateProgressiveState();
	TSharedRef<SWidget> BuildChartCard(int32 SlotIndex);
	TSharedRef<SWidget> BuildChartControls(int32 SlotIndex,bool bConfigurationMenu);
	bool UsesSimplifiedNativeLayout() const;
	bool AreSeriesSelectorsVisible(int32 SlotIndex) const;
	void ApplyChartSeriesVisibility(int32 SlotIndex);
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabUiSimplificationTest;
#endif
};
