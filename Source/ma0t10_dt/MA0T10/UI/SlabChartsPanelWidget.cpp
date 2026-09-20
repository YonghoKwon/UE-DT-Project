#include "SlabChartsPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "VirtualSensorUiStyle.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SlabChartsPanel"

void USlabChartsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ScenarioChart) { Chart = ScenarioChart; Chart->SetMetric(SelectedMetric); Chart->SetFrameAxis(bFrameAxis); RefreshChartData(); }
}
void USlabChartsPanelWidget::BindSlabActor(ASlabActor* InSlab) { Slab = IsValid(InSlab) && InSlab->GetWorld() == GetWorld() ? InSlab : nullptr; LastScenario = nullptr; RefreshChartData(); }
void USlabChartsPanelWidget::SetChartMetric(ESlabChartMetric M) { SelectedMetric = M; if (Chart) Chart->SetMetric(M); }
void USlabChartsPanelWidget::RefreshChartData()
{
	if (!Chart) return;
	const auto Scenario = Slab ? Slab->GetScenario() : nullptr;
	LastScenario = Scenario.Get();
	TArray<FSlabChartSample> Samples;
	if (Slab && Scenario)
	{
		Samples.Reserve(Scenario->Rows.Num());
		const double Scale = Slab->GetPositionUnit() == ESlabInputUnit::Millimeters ? .1 : Slab->GetPositionUnit() == ESlabInputUnit::Meters ? 100 : 1;
		for (int32 I = 0; I < Scenario->Rows.Num(); ++I)
		{
			const auto& Row = Scenario->Rows[I];
			const auto M = Slab->CalculateMetricsForRow(Row);
			auto& S = Samples.AddDefaulted_GetRef();
			S.Time = Row.ElapsedSec; S.Frame = Row.FrameNo; S.LeftAngle = Row.LeftAngle; S.RightAngle = Row.RightAngle;
			S.CenterOffsetCm = M.CenterOffsetCm; S.MarginLeftCm = M.MarginLeftCm; S.MarginRightCm = M.MarginRightCm; S.bMarginsValid = M.bMarginsValid;
			S.MaxCornerOffsetCm = M.MaxCornerOffsetCm;
			if (I + 1 < Scenario->Rows.Num()) { const auto& Next = Scenario->Rows[I + 1]; S.SpeedCmPerSec = (Next.CenterX - Row.CenterX) * Scale / FMath::Max(UE_DOUBLE_SMALL_NUMBER, Next.ElapsedSec - Row.ElapsedSec); }
		}
	}
	Chart->SetSamples(Samples);
}
void USlabChartsPanelWidget::NativeTick(const FGeometry& G, float D)
{
	Super::NativeTick(G, D);
	if (!IsValid(Slab)) Slab = nullptr;
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden || IsPanelCollapsed()) { bWasBodyVisible = false; return; }
	RefreshAccumulator += D; TextAccumulator += D; if (RefreshAccumulator < .1f) return; RefreshAccumulator = 0;
	const auto Scenario = Slab ? Slab->GetScenario() : nullptr;
	const uint32 Config = Slab ? Slab->GetMetricsConfigurationHash() : 0;
	if (!bWasBodyVisible || LastScenario != Scenario.Get() || Config != LastConfigurationHash) { RefreshChartData(); LastConfigurationHash = Config; }
	bWasBodyVisible = true;
	if (Chart)
	{
		Chart->SetChartFontScale(GetSensorToolFontScale());
		if (Slab) { const auto S = Slab->GetSimulationStatus(); Chart->SetPlaybackCursor(S.ElapsedSec, S.FrameNo); }
	}
	if (TextAccumulator >= .2f)
	{
		TextAccumulator = 0;
		const TCHAR* Unit = SelectedMetric == ESlabChartMetric::Angles ? TEXT("도") : SelectedMetric == ESlabChartMetric::Speed ? TEXT("cm/s") : TEXT("cm");
		StatusText = FString::Printf(TEXT("청록: 좌/값 · 주황: 우 · 단위 %s · 녹색: 현재 재생\n%s"), Unit, Chart ? *Chart->GetHoverSummary() : TEXT(""));
	}
}
TSharedRef<SWidget> USlabChartsPanelWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget) return Super::RebuildWidget();
	if (!Chart) Chart = NewObject<USlabScenarioChartWidget>(this);
	Chart->SetMetric(SelectedMetric); Chart->SetFrameAxis(bFrameAxis);
	auto Options = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(5, 5));
	const FText Labels[] = {LOCTEXT("Angles", "좌·우 기울기"), LOCTEXT("Center", "중심 이탈"), LOCTEXT("Margins", "좌·우 Margin"), LOCTEXT("Corners", "꼭지점 이탈"), LOCTEXT("Speed", "진행 속도")};
	for (int32 I = 0; I < 5; ++I)
	{
		const auto M = static_cast<ESlabChartMetric>(I);
		Options->AddSlot()[SNewSensorTool(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).OnClicked_Lambda([this, M]() { SetChartMetric(M); return FReply::Handled(); })
		[SNewSensorTool(STextBlock).Text(Labels[I]).ColorAndOpacity_Lambda([this, M]() { return SelectedMetric == M ? FVirtualSensorUiStyle::Accent : FVirtualSensorUiStyle::PrimaryText; })]];
	}
	auto Controls = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12, 5));
	Controls->AddSlot()[SNew(SCheckBox).IsChecked_Lambda([this]() { return bFrameAxis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bFrameAxis = S == ECheckBoxState::Checked; Chart->SetFrameAxis(bFrameAxis); })[SNewSensorTool(STextBlock).Text(LOCTEXT("FrameAxis", "X축: 원본 frame (해제: 초)"))]];
	for (int32 I = 0; I < 2; ++I) Controls->AddSlot()[SNew(SCheckBox).IsChecked_Lambda([this, I]() { return bShowSeries[I] ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this, I](ECheckBoxState S) { bShowSeries[I] = S == ECheckBoxState::Checked; Chart->SetSeriesVisible(I, bShowSeries[I]); })[SNewSensorTool(STextBlock).Text(I == 0 ? LOCTEXT("SeriesLeft", "좌/값") : LOCTEXT("SeriesRight", "우"))]];
	Controls->AddSlot()[SNewSensorTool(SButton).Text(LOCTEXT("Reset", "보기 초기화")).OnClicked_Lambda([this]() { Chart->ResetChartView(); return FReply::Handled(); })];
	RefreshChartData();
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FVirtualSensorUiStyle::PanelBackground).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Padding(12)
	[SNew(SVerticalBox)
	 + SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("Title", "Slab 차트"))]
	 + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
	   + SScrollBox::Slot()[SNew(SVerticalBox)
	     + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[Options]
	     + SVerticalBox::Slot().AutoHeight()[Controls]
	     + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNew(SBox).MinDesiredHeight(260).HeightOverride_Lambda([this]() { return FMath::Max(260.0, GetEffectivePanelSize().Y - 260); })[Chart->TakeWidget()]]
	     + SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(StatusText); })]
	     + SVerticalBox::Slot().AutoHeight().Padding(0, 6)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).Text(LOCTEXT("Help", "마우스 이동: 값 조회 · 드래그: 차트 이동 · 휠: 확대/축소\n차트 탐색은 재생 위치를 바꾸지 않습니다. center_x는 진행 위치이며 중심 이탈과 다릅니다. 가드레일 미설정은 N/A입니다."))]
	   ]]
	];
}
#undef LOCTEXT_NAMESPACE
