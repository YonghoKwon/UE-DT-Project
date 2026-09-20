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

TArray<int32> USlabScenarioChartWidget::BuildMinMaxIndices(const TArray<FSlabChartSample>& Data, ESlabChartMetric M, int32 Series, bool Frames, double MinX, double MaxX, int32 Width)
{
	TArray<int32> Result;
	if (Data.IsEmpty() || MaxX <= MinX || Width <= 0) return Result;
	Width = FMath::Clamp(Width, 1, 4096);
	Result.Reserve(FMath::Min(Data.Num(), Width * 2 + 2));
	int32 Bucket = INDEX_NONE, Low = INDEX_NONE, High = INDEX_NONE;
	auto Flush = [&]()
	{
		if (Low == INDEX_NONE) return;
		Result.Add(FMath::Min(Low, High));
		if (Low != High) Result.Add(FMath::Max(Low, High));
	};
	for (int32 I = 0; I < Data.Num(); ++I)
	{
		const auto& P = Data[I];
		const double V = SlabChart::X(P, Frames);
		if (V < MinX || V > MaxX || !SlabChart::Valid(P, M)) continue;
		const int32 B = FMath::Clamp(FMath::FloorToInt((V - MinX) / (MaxX - MinX) * Width), 0, Width - 1);
		if (B != Bucket) { Flush(); Bucket = B; Low = High = I; }
		else
		{
			if (SlabChart::Y(P, M, Series) < SlabChart::Y(Data[Low], M, Series)) Low = I;
			if (SlabChart::Y(P, M, Series) > SlabChart::Y(Data[High], M, Series)) High = I;
		}
	}
	Flush();
	return Result;
}

class SSlabScenarioChart : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSlabScenarioChart) {} SLATE_END_ARGS()
	void Construct(const FArguments&) { SetCanTick(false); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(620, 300); }
	void SetData(TSharedPtr<const TArray<FSlabChartSample>> InData) { Data = MoveTemp(InData); ResetView(); }
	void SetMetric(ESlabChartMetric V) { Metric = V; Dirty(); }
	void SetFrames(bool V) { if (Frames != V) { Frames = V; ResetView(); } }
	void SetSeries(int32 I, bool V) { if (I >= 0 && I < 2) { Visible[I] = V; Dirty(); } }
	void SetCursor(double T, int64 F) { Cursor = Frames ? F : T; Invalidate(EInvalidateWidgetReason::Paint); }
	void SetFontScale(float V) { if (!FMath::IsNearlyEqual(V, FontScale)) { FontScale = V; Dirty(); } }
	void ResetView()
	{
		MinX = Data.IsValid() && !Data->IsEmpty() ? SlabChart::X((*Data)[0], Frames) : 0;
		MaxX = Data.IsValid() && !Data->IsEmpty() ? SlabChart::X(Data->Last(), Frames) : 30;
		MaxX = FMath::Max(MaxX, MinX + (Frames ? 1.0 : .05)); Dirty();
	}
	FString HoverText() const { return Hover; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
	{
		const FVector2D Size = G.GetLocalSize();
		if (bDirty || !CachedSize.Equals(Size, .1)) RebuildCache(Size);
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(.025f, .04f, .065f, 1));
		for (const auto& L : Grid) FSlateDrawElement::MakeLines(Out, Layer + 1, G.ToPaintGeometry(), L, ESlateDrawEffect::None, FLinearColor(.15f, .2f, .28f), true, 1);
		for (const auto& Label : Labels) FSlateDrawElement::MakeText(Out, Layer + 2, G.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(Label.Key)), Label.Value, FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(12 * FontScale)), ESlateDrawEffect::None, FLinearColor(.75f, .82f, .9f));
		for (int32 I = 0; I < 2; ++I) if (Lines[I].Num() > 1 && Visible[I]) FSlateDrawElement::MakeLines(Out, Layer + 3, G.ToPaintGeometry(), Lines[I], ESlateDrawEffect::None, I == 0 ? FLinearColor(.2f, .8f, 1) : FLinearColor(1, .55f, .2f), true, 2);
		if (Cursor >= MinX && Cursor <= MaxX && PlotSize.X > 0)
		{
			const float X = PlotOrigin.X + (Cursor - MinX) / (MaxX - MinX) * PlotSize.X;
			TArray<FVector2D> CursorLine{FVector2D(X, PlotOrigin.Y), FVector2D(X, PlotOrigin.Y + PlotSize.Y)};
			FSlateDrawElement::MakeLines(Out, Layer + 4, G.ToPaintGeometry(), CursorLine, ESlateDrawEffect::None, FLinearColor(.4f, 1, .45f), true, 1);
		}
		return Layer + 4;
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
	{
		if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
		LastMouse = G.AbsoluteToLocal(E.GetScreenSpacePosition());
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& E) override
	{ return E.GetEffectingButton() == EKeys::LeftMouseButton ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled(); }
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
	{
		const FVector2D P = G.AbsoluteToLocal(E.GetScreenSpacePosition());
		if (HasMouseCapture() && PlotSize.X > 1)
		{
			const double Shift = (LastMouse.X - P.X) / PlotSize.X * (MaxX - MinX); MinX += Shift; MaxX += Shift; ClampView(); Dirty();
		}
		LastMouse = P;
		if (Data.IsValid() && !Data->IsEmpty() && PlotSize.X > 1)
		{
			const double Target = MinX + FMath::Clamp((P.X - PlotOrigin.X) / PlotSize.X, 0.0, 1.0) * (MaxX - MinX);
			int32 A = 0, B = Data->Num() - 1;
			while (A < B) { int32 Mid = (A + B) / 2; if (SlabChart::X((*Data)[Mid], Frames) < Target) A = Mid + 1; else B = Mid; }
			if (A > 0 && Target - SlabChart::X((*Data)[A - 1], Frames) < SlabChart::X((*Data)[A], Frames) - Target) --A;
			const auto& V = (*Data)[A];
			Hover = FString::Printf(TEXT("%.2f초 · frame %lld · %s"), V.Time, V.Frame, SlabChart::Valid(V, Metric) ? *FString::Printf(TEXT("좌/값 %.3f  우 %.3f"), SlabChart::Y(V, Metric, 0), SlabChart::Y(V, Metric, 1)) : TEXT("가드레일 미설정: N/A"));
		}
		return FReply::Handled();
	}
	virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override
	{
		if (PlotSize.X <= 1) return FReply::Unhandled();
		const double Alpha = FMath::Clamp((G.AbsoluteToLocal(E.GetScreenSpacePosition()).X - PlotOrigin.X) / PlotSize.X, 0.0, 1.0);
		const double Anchor = MinX + Alpha * (MaxX - MinX);
		const double Span = FMath::Max(Frames ? 1.0 : .05, (MaxX - MinX) * FMath::Pow(.8, E.GetWheelDelta()));
		MinX = Anchor - Alpha * Span; MaxX = MinX + Span; ClampView(); Dirty(); return FReply::Handled();
	}
private:
	void Dirty() { bDirty = true; Invalidate(EInvalidateWidgetReason::Paint); }
	void ClampView()
	{
		if (!Data.IsValid() || Data->IsEmpty()) return;
		const double First = SlabChart::X((*Data)[0], Frames), Last = FMath::Max(First + (Frames ? 1.0 : .05), SlabChart::X(Data->Last(), Frames));
		if (MaxX - MinX >= Last - First) { MinX = First; MaxX = Last; }
		else { if (MinX < First) { MaxX += First - MinX; MinX = First; } if (MaxX > Last) { MinX -= MaxX - Last; MaxX = Last; } }
	}
	void RebuildCache(FVector2D Size) const
	{
		CachedSize = Size; bDirty = false; Grid.Reset(); Labels.Reset(); Lines[0].Reset(); Lines[1].Reset();
		PlotOrigin = FVector2D(64 * FontScale, 20); PlotSize = FVector2D(FMath::Max(1.0, Size.X - PlotOrigin.X - 16), FMath::Max(1.0, Size.Y - 58 * FontScale));
		double MinY = 0, MaxY = 0; bool Found = false;
		if (Data.IsValid()) for (const auto& P : *Data) if (SlabChart::Valid(P, Metric) && SlabChart::X(P, Frames) >= MinX && SlabChart::X(P, Frames) <= MaxX)
			for (int32 S = 0; S < SlabChart::SeriesCount(Metric); ++S) if (Visible[S]) { const double Y = SlabChart::Y(P, Metric, S); if (!Found) { MinY = MaxY = Y; Found = true; } else { MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y); } }
		MinY = FMath::Min(MinY, 0.0); MaxY = FMath::Max(MaxY, 0.0);
		const double Pad = FMath::Max(.1, (MaxY - MinY) * .1); MinY -= Pad; MaxY += Pad;
		for (int32 I = 0; I <= 4; ++I)
		{
			const double A = I / 4.0; const double X = PlotOrigin.X + PlotSize.X * A, Y = PlotOrigin.Y + PlotSize.Y * A;
			Grid.Add({FVector2D(X, PlotOrigin.Y), FVector2D(X, PlotOrigin.Y + PlotSize.Y)});
			Grid.Add({FVector2D(PlotOrigin.X, Y), FVector2D(PlotOrigin.X + PlotSize.X, Y)});
			const double AxisValue = MinX + (MaxX - MinX) * A;
			Labels.Add({FVector2D(X - 12, PlotOrigin.Y + PlotSize.Y + 5), Frames ? FString::Printf(TEXT("%.0f"), AxisValue) : FString::Printf(TEXT("%.1fs"), AxisValue)});
			Labels.Add({FVector2D(2, Y - 6), FString::Printf(TEXT("%.2f"), MaxY - (MaxY - MinY) * A)});
		}
		const double ZeroY = PlotOrigin.Y + MaxY / (MaxY - MinY) * PlotSize.Y;
		Grid.Add({FVector2D(PlotOrigin.X, ZeroY), FVector2D(PlotOrigin.X + PlotSize.X, ZeroY)});
		Labels.Add({FVector2D(PlotOrigin.X + 4, ZeroY - 16 * FontScale), TEXT("0 기준선")});
		if (!Found) Labels.Add({PlotOrigin + FVector2D(12, 14), Metric == ESlabChartMetric::RailMargins ? TEXT("가드레일 미설정: N/A") : TEXT("시나리오 데이터 대기")});
		if (Data.IsValid()) for (int32 S = 0; S < SlabChart::SeriesCount(Metric); ++S)
		{
			for (int32 I : USlabScenarioChartWidget::BuildMinMaxIndices(*Data, Metric, S, Frames, MinX, MaxX, FMath::RoundToInt(PlotSize.X)))
			{
				const auto& P = (*Data)[I]; Lines[S].Add(FVector2D(PlotOrigin.X + (SlabChart::X(P, Frames) - MinX) / (MaxX - MinX) * PlotSize.X, PlotOrigin.Y + (MaxY - SlabChart::Y(P, Metric, S)) / (MaxY - MinY) * PlotSize.Y));
			}
		}
	}
	TSharedPtr<const TArray<FSlabChartSample>> Data;
	ESlabChartMetric Metric = ESlabChartMetric::Angles;
	bool Frames = false, Visible[2] = {true, true};
	float FontScale = 1;
	double MinX = 0, MaxX = 30, Cursor = 0;
	FVector2D LastMouse;
	FString Hover;
	mutable bool bDirty = true;
	mutable FVector2D CachedSize, PlotOrigin, PlotSize;
	mutable TArray<FVector2D> Lines[2];
	mutable TArray<TArray<FVector2D>> Grid;
	mutable TArray<TPair<FVector2D, FString>> Labels;
};

void USlabScenarioChartWidget::SetSamples(const TArray<FSlabChartSample>& V) { Samples = MakeShared<TArray<FSlabChartSample>>(V); if (Chart) Chart->SetData(Samples); }
void USlabScenarioChartWidget::SetMetric(ESlabChartMetric V) { Metric = V; if (Chart) Chart->SetMetric(V); }
void USlabScenarioChartWidget::SetFrameAxis(bool V) { bFrameAxis = V; if (Chart) Chart->SetFrames(V); }
void USlabScenarioChartWidget::SetSeriesVisible(int32 I, bool V) { if (I >= 0 && I < 2) { bSeriesVisible[I] = V; if (Chart) Chart->SetSeries(I, V); } }
void USlabScenarioChartWidget::SetPlaybackCursor(double T, int64 F) { CursorTime = T; CursorFrame = F; if (Chart) Chart->SetCursor(T, F); }
void USlabScenarioChartWidget::ResetChartView() { if (Chart) Chart->ResetView(); }
void USlabScenarioChartWidget::SetChartFontScale(float S) { FontScale = FMath::IsFinite(S) ? FMath::Clamp(S, .85f, 1.5f) : 1; if (Chart) Chart->SetFontScale(FontScale); }
FString USlabScenarioChartWidget::GetHoverSummary() const { return Chart ? Chart->HoverText() : FString(); }
TSharedRef<SWidget> USlabScenarioChartWidget::RebuildWidget()
{
	SAssignNew(Chart, SSlabScenarioChart); Chart->SetData(Samples); Chart->SetMetric(Metric); Chart->SetFrames(bFrameAxis); Chart->SetFontScale(FontScale);
	for (int32 I = 0; I < 2; ++I) Chart->SetSeries(I, bSeriesVisible[I]); Chart->SetCursor(CursorTime, CursorFrame); return Chart.ToSharedRef();
}
void USlabScenarioChartWidget::ReleaseSlateResources(bool Children) { Super::ReleaseSlateResources(Children); Chart.Reset(); }
