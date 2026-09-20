#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SlabScenarioChartWidget.generated.h"

class SSlabScenarioChart;

/** Display data only: none of the chart navigation operations changes the simulation. */
USTRUCT(BlueprintType)
struct FSlabChartSample
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) double Time = 0;
	UPROPERTY(BlueprintReadWrite) int64 Frame = 0;
	UPROPERTY(BlueprintReadWrite) double LeftAngle = 0;
	UPROPERTY(BlueprintReadWrite) double RightAngle = 0;
	UPROPERTY(BlueprintReadWrite) double CenterOffsetCm = 0;
	UPROPERTY(BlueprintReadWrite) double MarginLeftCm = 0;
	UPROPERTY(BlueprintReadWrite) double MarginRightCm = 0;
	UPROPERTY(BlueprintReadWrite) double MaxCornerOffsetCm = 0;
	UPROPERTY(BlueprintReadWrite) double SpeedCmPerSec = 0;
	UPROPERTY(BlueprintReadWrite) bool bMarginsValid = false;
};

UENUM(BlueprintType)
enum class ESlabChartMetric : uint8 { Angles, CenterOffset, RailMargins, CornerOffset, Speed };

/** Lightweight native Slate chart. Original samples are retained; only drawing is decimated. */
UCLASS(BlueprintType)
class MA0T10_DT_API USlabScenarioChartWidget : public UWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetSamples(const TArray<FSlabChartSample>& InSamples);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetMetric(ESlabChartMetric InMetric);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetFrameAxis(bool bInFrameAxis);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetSeriesVisible(int32 Series, bool bVisible);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetPlaybackCursor(double Time, int64 Frame);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void ResetChartView();
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetChartFontScale(float Scale);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|Chart") FString GetHoverSummary() const;
	static TArray<int32> BuildMinMaxIndices(const TArray<FSlabChartSample>& Samples, ESlabChartMetric Metric, int32 Series, bool bFrameAxis, double MinX, double MaxX, int32 PixelWidth);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
	TSharedPtr<SSlabScenarioChart> Chart;
	TSharedPtr<const TArray<FSlabChartSample>> Samples;
	ESlabChartMetric Metric = ESlabChartMetric::Angles;
	bool bFrameAxis = false;
	bool bSeriesVisible[2] = {true, true};
	float FontScale = 1;
	double CursorTime = 0;
	int64 CursorFrame = 0;
};
