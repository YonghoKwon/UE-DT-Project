#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SlabScenarioChartWidget.generated.h"

class SSlabScenarioChart;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlabChartViewRangeChanged, double, Minimum, double, Maximum);

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
	/** Sharing is explicit; the supplied immutable array must remain sorted by Time/Frame. */
	void SetSharedSamples(TSharedPtr<const TArray<FSlabChartSample>> InSamples);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetProgressiveReveal(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetRevealedTime(double Time);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetTimelineDuration(double DurationSeconds);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetViewRange(double Minimum, double Maximum);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void ClearHover();
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|Chart") bool IsProgressiveRevealEnabled() const { return bProgressiveReveal; }
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|Chart") int32 GetRevealedSampleCount() const;
	UPROPERTY(BlueprintAssignable, Category="DigitalTwin|Slab|Chart") FSlabChartViewRangeChanged OnViewRangeChanged;
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetMetric(ESlabChartMetric InMetric);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetFrameAxis(bool bInFrameAxis);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetSeriesVisible(int32 Series, bool bVisible);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetPlaybackCursor(double Time, int64 Frame);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void ResetChartView();
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Chart") void SetChartFontScale(float Scale);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|Chart") FString GetHoverSummary() const;
	void SetChartPalette(FLinearColor Background, FLinearColor Text, FLinearColor Grid);
	static int32 CountRevealedSamples(const TArray<FSlabChartSample>& Samples, double RevealedTime);
	static int32 FindNearestRevealedSample(const TArray<FSlabChartSample>& Samples, bool bFrameAxis, double TargetX, double RevealedTime);
	static FVector2D CalculateTimelineDomain(const TArray<FSlabChartSample>& Samples, bool bFrameAxis, double DurationSeconds);
	static FVector2D CalculateVisibleYRange(const TArray<FSlabChartSample>& Samples, ESlabChartMetric Metric, bool bFrameAxis, double MinX, double MaxX, double RevealedTime, bool bShowFirst, bool bShowSecond, bool& bHasData);
	static TArray<int32> BuildMinMaxIndices(const TArray<FSlabChartSample>& Samples, ESlabChartMetric Metric, int32 Series, bool bFrameAxis, double MinX, double MaxX, int32 PixelWidth, double RevealedTime = TNumericLimits<double>::Max());
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
	TSharedPtr<SSlabScenarioChart> Chart;
	TSharedPtr<const TArray<FSlabChartSample>> Samples;
	ESlabChartMetric Metric = ESlabChartMetric::Angles;
	bool bFrameAxis = false;
	// Opt-in: an unrelated caller of SetSamples still sees the entire dataset.
	bool bProgressiveReveal = false;
	double RevealedTime = -1;
	double TimelineDuration = 0;
	bool bSeriesVisible[2] = {true, true};
	float FontScale = 1;
	double CursorTime = 0;
	int64 CursorFrame = 0;
	FLinearColor BackgroundColor = FLinearColor(.025f, .04f, .065f, 1);
	FLinearColor TextColor = FLinearColor(.75f, .82f, .9f);
	FLinearColor GridColor = FLinearColor(.15f, .2f, .28f);
};
