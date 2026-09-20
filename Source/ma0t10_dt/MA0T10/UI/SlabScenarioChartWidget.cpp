#include "SlabScenarioChartWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace SlabChart
{
	double X(const FSlabChartSample& P, bool Frames) { return Frames ? P.Frame : P.Time; }
	double Y(const FSlabChartSample& P, ESlabChartMetric M, int32 Series)
	{
		switch (M)
		{
		case ESlabChartMetric::Angles: return Series == 0 ? P.LeftAngle : P.RightAngle;
		case ESlabChartMetric::CenterOffset: return P.CenterOffsetCm;
		case ESlabChartMetric::RailMargins: return Series == 0 ? P.MarginLeftCm : P.MarginRightCm;
		case ESlabChartMetric::CornerOffset: return P.MaxCornerOffsetCm;
		default: return P.SpeedCmPerSec;
		}
	}
	bool Valid(const FSlabChartSample& P, ESlabChartMetric M) { return M != ESlabChartMetric::RailMargins || P.bMarginsValid; }
	int32 SeriesCount(ESlabChartMetric M) { return M == ESlabChartMetric::Angles || M == ESlabChartMetric::RailMargins ? 2 : 1; }
}

int32 USlabScenarioChartWidget::CountRevealedSamples(const TArray<FSlabChartSample>& Data, double Time)
{
	if (!FMath::IsFinite(Time)) return 0;
	int32 Low = 0, High = Data.Num();
	while (Low < High) { const int32 Mid = Low + (High - Low) / 2; if (Data[Mid].Time <= Time + 1.e-9) Low = Mid + 1; else High = Mid; }
	return Low;
}
int32 USlabScenarioChartWidget::FindNearestRevealedSample(const TArray<FSlabChartSample>& Data, bool Frames, double Target, double Time)
{
	const int32 Count = CountRevealedSamples(Data, Time);
	if (!Count || Target < SlabChart::X(Data[0], Frames) || Target > SlabChart::X(Data[Count - 1], Frames) + 1.e-9) return INDEX_NONE;
	int32 A = 0, B = Count - 1;
	while (A < B) { const int32 Mid = (A + B) / 2; if (SlabChart::X(Data[Mid], Frames) < Target) A = Mid + 1; else B = Mid; }
	if (A > 0 && Target - SlabChart::X(Data[A - 1], Frames) < SlabChart::X(Data[A], Frames) - Target) --A;
	return A;
}
FVector2D USlabScenarioChartWidget::CalculateTimelineDomain(const TArray<FSlabChartSample>& Data, bool Frames, double Duration)
{
	const bool HasData=!Data.IsEmpty();
	double A=HasData?SlabChart::X(Data[0],Frames):0, B=HasData?SlabChart::X(Data.Last(),Frames):30;
	if(FMath::IsFinite(Duration)&&Duration>0)
	{
		A=0;B=Duration;
		if(Frames&&HasData)
		{
			const auto& Last=Data.Last();double Rate=20;
			if(Data.Num()>1){const auto& Prev=Data[Data.Num()-2];if(Last.Time>Prev.Time)Rate=(Last.Frame-Prev.Frame)/(Last.Time-Prev.Time);}
			B=Last.Frame+FMath::Max(0.0,Duration-Last.Time)*Rate;
		}
	}
	return FVector2D(A,FMath::Max(A+(Frames?1.0:.05),B));
}
FVector2D USlabScenarioChartWidget::CalculateVisibleYRange(const TArray<FSlabChartSample>& Data, ESlabChartMetric Metric, bool Frames, double MinX, double MaxX, double Time, bool First, bool Second, bool& Found)
{
	double MinY = 0, MaxY = 0; Found = false;
	const int32 Count = CountRevealedSamples(Data, Time);
	for (int32 I = 0; I < Count; ++I)
	{
		const auto& P = Data[I]; const double X = SlabChart::X(P, Frames);
		if (!SlabChart::Valid(P, Metric) || X < MinX || X > MaxX) continue;
		for (int32 S = 0; S < SlabChart::SeriesCount(Metric); ++S) if (S == 0 ? First : Second)
		{
			const double Y = SlabChart::Y(P, Metric, S); if (!FMath::IsFinite(Y)) continue;
			MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y); Found = true;
		}
	}
	const double Pad = FMath::Max(.1, (MaxY - MinY) * .1);
	return FVector2D(MinY - Pad, MaxY + Pad);
}
TArray<int32> USlabScenarioChartWidget::BuildMinMaxIndices(const TArray<FSlabChartSample>& Data, ESlabChartMetric M, int32 Series, bool Frames, double MinX, double MaxX, int32 Width, double Time)
{
	TArray<int32> Result;
	if (Data.IsEmpty() || MaxX <= MinX || Width <= 0) return Result;
	Width = FMath::Clamp(Width, 1, 4096);
	const int32 Count = CountRevealedSamples(Data, Time);
	Result.Reserve(FMath::Min(Count, Width * 2 + 2));
	int32 Bucket = INDEX_NONE, Low = INDEX_NONE, High = INDEX_NONE;
	auto Flush = [&]() { if (Low == INDEX_NONE) return; Result.Add(FMath::Min(Low, High)); if (Low != High) Result.Add(FMath::Max(Low, High)); };
	for (int32 I = 0; I < Count; ++I)
	{
		const auto& P = Data[I]; const double V = SlabChart::X(P, Frames);
		if (V < MinX || V > MaxX || !SlabChart::Valid(P, M) || !FMath::IsFinite(SlabChart::Y(P, M, Series))) continue;
		const int32 B = FMath::Clamp(FMath::FloorToInt((V - MinX) / (MaxX - MinX) * Width), 0, Width - 1);
		if (B != Bucket) { Flush(); Bucket = B; Low = High = I; }
		else { if (SlabChart::Y(P, M, Series) < SlabChart::Y(Data[Low], M, Series)) Low = I; if (SlabChart::Y(P, M, Series) > SlabChart::Y(Data[High], M, Series)) High = I; }
	}
	Flush(); return Result;
}

class SSlabScenarioChart : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSlabScenarioChart) {} SLATE_END_ARGS()
	void Construct(const FArguments&) { SetCanTick(false); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(620, 300); }
	void SetData(TSharedPtr<const TArray<FSlabChartSample>> InData) { if (Data == InData) return; Data = MoveTemp(InData); Hover.Empty(); ResetView(); }
	void SetMetric(ESlabChartMetric V) { Metric = V; Hover.Empty(); Dirty(); }
	void SetFrames(bool V) { if (Frames != V) { Frames = V; Hover.Empty(); ResetView(); } }
	void SetSeries(int32 I, bool V) { if (I >= 0 && I < 2) { Visible[I] = V; Dirty(); } }
	void SetCursor(double T, int64 F) { Cursor = Frames ? F : T; Invalidate(EInvalidateWidgetReason::Paint); }
	void SetFontScale(float V) { if (!FMath::IsNearlyEqual(V, FontScale)) { FontScale = V; Dirty(); } }
	void SetProgressive(bool V) { if (Progressive != V) { Progressive = V; Hover.Empty(); Dirty(); } }
	void SetRevealed(double V) { if (Revealed != V) { if(V<Revealed)ClearHover(); Revealed = V; RefreshHover(); Dirty(); } }
	void SetDuration(double V) { if (Duration != V) { Duration = V; ResetView(); } }
	void SetPalette(FLinearColor B, FLinearColor T, FLinearColor G) { if (Background != B || Text != T || GridTint != G) { Background = B; Text = T; GridTint = G; Dirty(); } }
	void ClearHover() { Hover.Empty(); bHasHoverTarget=false; }
	void SetView(double A, double B) { if (!FMath::IsFinite(A) || !FMath::IsFinite(B) || B <= A) return; MinX = A; MaxX = B; ClampView(); ClearHover(); Dirty(); }
	TFunction<void(double, double)> ViewChanged;
	void ResetView() { const auto R = Domain(); MinX = R.X; MaxX = R.Y; ClearHover(); Dirty(); }
	FString HoverText() const { return Hover; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
	{
		const FVector2D Size = G.GetLocalSize(); if (bDirty || !CachedSize.Equals(Size, .1)) RebuildCache(Size);
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Background);
		for (const auto& L : Grid) FSlateDrawElement::MakeLines(Out, Layer + 1, G.ToPaintGeometry(), L, ESlateDrawEffect::None, GridTint, true, 1);
		for (const auto& Label : Labels) FSlateDrawElement::MakeText(Out, Layer + 2, G.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(Label.Key)), Label.Value, FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(12 * FontScale)), ESlateDrawEffect::None, Text);
		for (int32 I = 0; I < 2; ++I) if (Visible[I])
		{
			const FLinearColor Color = I == 0 ? FLinearColor(.2f, .8f, 1) : FLinearColor(1, .55f, .2f);
			if (Lines[I].Num() > 1) FSlateDrawElement::MakeLines(Out, Layer + 3, G.ToPaintGeometry(), Lines[I], ESlateDrawEffect::None, Color, true, 2);
			else if (Lines[I].Num() == 1) FSlateDrawElement::MakeBox(Out, Layer + 3, G.ToPaintGeometry(FVector2D(4, 4), FSlateLayoutTransform(Lines[I][0] - FVector2D(2, 2))), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
		}
		if (Cursor >= MinX && Cursor <= MaxX && PlotSize.X > 0)
		{
			const float X = PlotOrigin.X + (Cursor - MinX) / (MaxX - MinX) * PlotSize.X;
			TArray<FVector2D> CursorLine{FVector2D(X, PlotOrigin.Y), FVector2D(X, PlotOrigin.Y + PlotSize.Y)};
			FSlateDrawElement::MakeLines(Out, Layer + 4, G.ToPaintGeometry(), CursorLine, ESlateDrawEffect::None, FLinearColor(.4f, 1, .45f), true, 1);
		}
		return Layer + 4;
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
	{ if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled(); LastMouse = G.AbsoluteToLocal(E.GetScreenSpacePosition()); return FReply::Handled().CaptureMouse(SharedThis(this)); }
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& E) override
	{ return E.GetEffectingButton() == EKeys::LeftMouseButton ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled(); }
	virtual void OnMouseLeave(const FPointerEvent& E) override { ClearHover(); SLeafWidget::OnMouseLeave(E); }
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
	{
		const FVector2D P = G.AbsoluteToLocal(E.GetScreenSpacePosition());
		if (HasMouseCapture() && PlotSize.X > 1) { const double Shift = (LastMouse.X - P.X) / PlotSize.X * (MaxX - MinX); MinX += Shift; MaxX += Shift; ClampView(); Dirty(); if (ViewChanged) ViewChanged(MinX, MaxX); }
		LastMouse = P;
		if (Data.IsValid() && !Data->IsEmpty() && PlotSize.X > 1)
		{
			HoverTarget = MinX + FMath::Clamp((P.X - PlotOrigin.X) / PlotSize.X, 0.0, 1.0) * (MaxX - MinX);
			bHasHoverTarget=true; RefreshHover();
		}
		return FReply::Handled();
	}
	virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override
	{
		if (PlotSize.X <= 1) return FReply::Unhandled();
		const double Alpha = FMath::Clamp((G.AbsoluteToLocal(E.GetScreenSpacePosition()).X - PlotOrigin.X) / PlotSize.X, 0.0, 1.0);
		const double Anchor = MinX + Alpha * (MaxX - MinX), Span = FMath::Max(Frames ? 1.0 : .05, (MaxX - MinX) * FMath::Pow(.8, E.GetWheelDelta()));
		MinX = Anchor - Alpha * Span; MaxX = MinX + Span; ClampView(); Dirty(); if (ViewChanged) ViewChanged(MinX, MaxX); return FReply::Handled();
	}
private:
	double Cutoff() const { return Progressive ? Revealed : TNumericLimits<double>::Max(); }
	FVector2D Domain() const
	{
		static const TArray<FSlabChartSample> Empty;
		return USlabScenarioChartWidget::CalculateTimelineDomain(Data?*Data:Empty,Frames,Duration);
	}
	void RefreshHover()
	{
		if(!bHasHoverTarget||!Data)return;
		const int32 I=USlabScenarioChartWidget::FindNearestRevealedSample(*Data,Frames,HoverTarget,Cutoff());
		if(I==INDEX_NONE){Hover=Progressive?TEXT("아직 재생되지 않은 구간"):TEXT("표본이 없는 구간");return;}
		const auto& V=(*Data)[I];
		Hover=FString::Printf(TEXT("%.2f초 · frame %lld · %s"),V.Time,V.Frame,SlabChart::Valid(V,Metric)?*FString::Printf(TEXT("좌/값 %.3f  우 %.3f"),SlabChart::Y(V,Metric,0),SlabChart::Y(V,Metric,1)):TEXT("가드레일 미설정: N/A"));
	}
	void Dirty() { bDirty = true; Invalidate(EInvalidateWidgetReason::Paint); }
	void ClampView()
	{
		const auto R = Domain();
		if (MaxX - MinX >= R.Y - R.X) { MinX = R.X; MaxX = R.Y; }
		else { if (MinX < R.X) { MaxX += R.X - MinX; MinX = R.X; } if (MaxX > R.Y) { MinX -= MaxX - R.Y; MaxX = R.Y; } }
	}
	void RebuildCache(FVector2D Size) const
	{
		CachedSize = Size; bDirty = false; Grid.Reset(); Labels.Reset(); Lines[0].Reset(); Lines[1].Reset();
		PlotOrigin = FVector2D(64 * FontScale, 20); PlotSize = FVector2D(FMath::Max(1.0, Size.X - PlotOrigin.X - 16), FMath::Max(1.0, Size.Y - 58 * FontScale));
		bool Found = false; const FVector2D Range = Data ? USlabScenarioChartWidget::CalculateVisibleYRange(*Data, Metric, Frames, MinX, MaxX, Cutoff(), Visible[0], Visible[1], Found) : FVector2D(-.1, .1);
		const double MinY = Range.X, MaxY = Range.Y;
		for (int32 I = 0; I <= 4; ++I)
		{
			const double A = I / 4.0, X = PlotOrigin.X + PlotSize.X * A;
			Grid.Add({FVector2D(X, PlotOrigin.Y), FVector2D(X, PlotOrigin.Y + PlotSize.Y)});
			const double AxisValue = MinX + (MaxX - MinX) * A;
			Labels.Add({FVector2D(X - 12, PlotOrigin.Y + PlotSize.Y + 5), Frames ? FString::Printf(TEXT("%.0f"), AxisValue) : FString::Printf(TEXT("%.1fs"), AxisValue)});
		}
		const int32 YDivisions=FMath::Clamp(FMath::FloorToInt(PlotSize.Y/(18.0*FontScale)),1,4);
		for(int32 I=0;I<=YDivisions;++I)
		{
			const double A=static_cast<double>(I)/YDivisions,Y=PlotOrigin.Y+PlotSize.Y*A;
			Grid.Add({FVector2D(PlotOrigin.X,Y),FVector2D(PlotOrigin.X+PlotSize.X,Y)});
			Labels.Add({FVector2D(2, Y - 6), FString::Printf(TEXT("%.2f"), MaxY - (MaxY - MinY) * A)});
		}
		const double ZeroY = PlotOrigin.Y + MaxY / (MaxY - MinY) * PlotSize.Y;
		Grid.Add({FVector2D(PlotOrigin.X, ZeroY), FVector2D(PlotOrigin.X + PlotSize.X, ZeroY)});
		if (!Found) Labels.Add({PlotOrigin + FVector2D(12, 14), !Visible[0] && !Visible[1] ? TEXT("표시할 계열을 선택하세요") : Metric == ESlabChartMetric::RailMargins ? TEXT("가드레일 미설정 또는 미재생: N/A") : TEXT("현재 구간에 재생된 표본이 없습니다")});
		if (Data) for (int32 S = 0; S < SlabChart::SeriesCount(Metric); ++S) if (Visible[S])
			for (int32 I : USlabScenarioChartWidget::BuildMinMaxIndices(*Data, Metric, S, Frames, MinX, MaxX, FMath::RoundToInt(PlotSize.X), Cutoff()))
			{
				const auto& P = (*Data)[I]; Lines[S].Add(FVector2D(PlotOrigin.X + (SlabChart::X(P, Frames) - MinX) / (MaxX - MinX) * PlotSize.X, PlotOrigin.Y + (MaxY - SlabChart::Y(P, Metric, S)) / (MaxY - MinY) * PlotSize.Y));
			}
	}
	TSharedPtr<const TArray<FSlabChartSample>> Data;
	ESlabChartMetric Metric = ESlabChartMetric::Angles;
	bool Frames = false, Progressive = false, Visible[2] = {true, true};
	float FontScale = 1;
	double MinX = 0, MaxX = 30, Cursor = 0, Revealed = -1, Duration = 0;
	FLinearColor Background = FLinearColor(.025f, .04f, .065f, 1), Text = FLinearColor(.75f, .82f, .9f), GridTint = FLinearColor(.15f, .2f, .28f);
	FVector2D LastMouse;
	FString Hover;
	double HoverTarget=0;
	bool bHasHoverTarget=false;
	mutable bool bDirty = true;
	mutable FVector2D CachedSize, PlotOrigin, PlotSize;
	mutable TArray<FVector2D> Lines[2];
	mutable TArray<TArray<FVector2D>> Grid;
	mutable TArray<TPair<FVector2D, FString>> Labels;
};

void USlabScenarioChartWidget::SetSamples(const TArray<FSlabChartSample>& V) { SetSharedSamples(MakeShared<TArray<FSlabChartSample>>(V)); }
void USlabScenarioChartWidget::SetSharedSamples(TSharedPtr<const TArray<FSlabChartSample>> V) { Samples = MoveTemp(V); if (Chart) Chart->SetData(Samples); }
void USlabScenarioChartWidget::SetProgressiveReveal(bool V) { bProgressiveReveal = V; if (Chart) Chart->SetProgressive(V); }
void USlabScenarioChartWidget::SetRevealedTime(double V) { RevealedTime = FMath::IsFinite(V) ? V : -1; if (Chart) Chart->SetRevealed(RevealedTime); }
void USlabScenarioChartWidget::SetTimelineDuration(double V) { TimelineDuration = FMath::IsFinite(V) ? FMath::Max(0.0, V) : 0; if (Chart) Chart->SetDuration(TimelineDuration); }
void USlabScenarioChartWidget::SetViewRange(double A, double B) { if (Chart) Chart->SetView(A, B); }
void USlabScenarioChartWidget::ClearHover() { if (Chart) Chart->ClearHover(); }
int32 USlabScenarioChartWidget::GetRevealedSampleCount() const { return Samples ? CountRevealedSamples(*Samples, bProgressiveReveal ? RevealedTime : TNumericLimits<double>::Max()) : 0; }
void USlabScenarioChartWidget::SetMetric(ESlabChartMetric V) { Metric = V; if (Chart) Chart->SetMetric(V); }
void USlabScenarioChartWidget::SetFrameAxis(bool V) { bFrameAxis = V; if (Chart) Chart->SetFrames(V); }
void USlabScenarioChartWidget::SetSeriesVisible(int32 I, bool V) { if (I >= 0 && I < 2) { bSeriesVisible[I] = V; if (Chart) Chart->SetSeries(I, V); } }
void USlabScenarioChartWidget::SetPlaybackCursor(double T, int64 F) { CursorTime = T; CursorFrame = F; if (Chart) Chart->SetCursor(T, F); }
void USlabScenarioChartWidget::ResetChartView() { if (Chart) Chart->ResetView(); }
void USlabScenarioChartWidget::SetChartFontScale(float S) { FontScale = FMath::IsFinite(S) ? FMath::Clamp(S, .85f, 1.5f) : 1; if (Chart) Chart->SetFontScale(FontScale); }
void USlabScenarioChartWidget::SetChartPalette(FLinearColor B, FLinearColor T, FLinearColor G) { BackgroundColor = B; TextColor = T; GridColor = G; if (Chart) Chart->SetPalette(B, T, G); }
FString USlabScenarioChartWidget::GetHoverSummary() const { return Chart ? Chart->HoverText() : FString(); }
TSharedRef<SWidget> USlabScenarioChartWidget::RebuildWidget()
{
	SAssignNew(Chart, SSlabScenarioChart);
	Chart->SetData(Samples); Chart->SetMetric(Metric); Chart->SetFrames(bFrameAxis); Chart->SetFontScale(FontScale);
	Chart->SetProgressive(bProgressiveReveal); Chart->SetRevealed(RevealedTime); Chart->SetDuration(TimelineDuration); Chart->SetPalette(BackgroundColor, TextColor, GridColor);
	for (int32 I = 0; I < 2; ++I) Chart->SetSeries(I, bSeriesVisible[I]); Chart->SetCursor(CursorTime, CursorFrame);
	Chart->ViewChanged = [Weak = TWeakObjectPtr<USlabScenarioChartWidget>(this)](double A, double B) { if (Weak.IsValid()) Weak->OnViewRangeChanged.Broadcast(A, B); };
	return Chart.ToSharedRef();
}
void USlabScenarioChartWidget::ReleaseSlateResources(bool Children) { Super::ReleaseSlateResources(Children); Chart.Reset(); }
