#include "SlabChartsPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SlabChartsPanel"

namespace
{
	FText MetricLabel(ESlabChartMetric M)
	{
		switch(M)
		{
		case ESlabChartMetric::Angles:return LOCTEXT("Angles","좌·우 기울기");
		case ESlabChartMetric::CenterOffset:return LOCTEXT("Center","중심 이탈");
		case ESlabChartMetric::RailMargins:return LOCTEXT("Margins","좌·우 Margin");
		case ESlabChartMetric::CornerOffset:return LOCTEXT("Corner","꼭지점 이탈");
		default:return LOCTEXT("Speed","최근 구간 속도");
		}
	}
	const TCHAR* MetricUnit(ESlabChartMetric M) { return M==ESlabChartMetric::Angles?TEXT("도"):M==ESlabChartMetric::Speed?TEXT("cm/s"):TEXT("cm"); }
	FString CurrentValue(const FSlabChartSample& S,ESlabChartMetric M)
	{
		FString Values;
		switch(M)
		{
		case ESlabChartMetric::Angles:Values=FString::Printf(TEXT("좌 %.3f / 우 %.3f 도"),S.LeftAngle,S.RightAngle);break;
		case ESlabChartMetric::RailMargins:Values=S.bMarginsValid?FString::Printf(TEXT("좌 %.3f / 우 %.3f cm"),S.MarginLeftCm,S.MarginRightCm):TEXT("가드레일 미설정: N/A");break;
		case ESlabChartMetric::CenterOffset:Values=FString::Printf(TEXT("%.3f cm"),S.CenterOffsetCm);break;
		case ESlabChartMetric::CornerOffset:Values=FString::Printf(TEXT("%.3f cm"),S.MaxCornerOffsetCm);break;
		default:Values=FString::Printf(TEXT("%.3f cm/s (직전→현재 표본)"),S.SpeedCmPerSec);break;
		}
		return FString::Printf(TEXT("현재 표본 · %.2f초 / frame %lld · %s"),S.Time,S.Frame,*Values);
	}
}

void USlabChartsPanelWidget::InitializeCharts(bool bCreateNative)
{
	Charts.SetNum(3);
	MetricComboStyle=FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	FButtonStyle CompactButton=GetToolButtonStyle();CompactButton.SetNormalPadding(FMargin(8,2)).SetPressedPadding(FMargin(8,3,8,1));
	FComboButtonStyle Button=MetricComboStyle.ComboButtonStyle;Button.SetButtonStyle(CompactButton);MetricComboStyle.SetComboButtonStyle(Button);
	const auto Scenario=Slab?Slab->GetScenario():nullptr;
	if(ScenarioChart)Charts[0]=ScenarioChart;
	if(ScenarioChart2)Charts[1]=ScenarioChart2;
	if(ScenarioChart3)Charts[2]=ScenarioChart3;
	if(MetricOptions.IsEmpty())for(int32 I=0;I<5;++I)MetricOptions.Add(MakeShared<ESlabChartMetric>(static_cast<ESlabChartMetric>(I)));
	for(int32 I=0;I<3;++I)
	{
		if(!Charts[I]&&bCreateNative)Charts[I]=NewObject<USlabScenarioChartWidget>(this);
		if(auto* Chart=Charts[I].Get())
		{
			Chart->SetMetric(SelectedMetrics[I]);Chart->SetFrameAxis(bFrameAxis);
			Chart->SetProgressiveReveal(IsWorkspaceOwned());Chart->SetSharedSamples(SharedSamples);
			Chart->SetTimelineDuration(Scenario?Scenario->DurationSec:0);
			Chart->OnViewRangeChanged.AddUniqueDynamic(this,&ThisClass::SynchronizeViewRange);
			for(int32 S=0;S<2;++S)Chart->SetSeriesVisible(S,bShowSeries[I][S]);
		}
	}
}
void USlabChartsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();InitializeCharts(!(WidgetTree&&WidgetTree->RootWidget));
	if(IsValid(Slab))Slab->OnSlabStateChanged.AddUniqueDynamic(this,&ThisClass::HandleSlabStateChanged);
	const auto Scenario=Slab?Slab->GetScenario():nullptr;
	if(!SharedSamples.IsValid()||LastScenario!=Scenario.Get()||(Slab&&LastConfigurationHash!=Slab->GetMetricsConfigurationHash()))RefreshChartData();
	UpdateProgressiveState();
}
void USlabChartsPanelWidget::NativeDestruct()
{
	if(IsValid(Slab))Slab->OnSlabStateChanged.RemoveDynamic(this,&ThisClass::HandleSlabStateChanged);
	for(const auto& C:Charts)if(C)C->OnViewRangeChanged.RemoveDynamic(this,&ThisClass::SynchronizeViewRange);
	Super::NativeDestruct();
}
void USlabChartsPanelWidget::BindSlabActor(ASlabActor* InSlab)
{
	if(IsValid(Slab))Slab->OnSlabStateChanged.RemoveDynamic(this,&ThisClass::HandleSlabStateChanged);
	Slab=IsValid(InSlab)&&InSlab->GetWorld()==GetWorld()?InSlab:nullptr;
	if(Slab)Slab->OnSlabStateChanged.AddUniqueDynamic(this,&ThisClass::HandleSlabStateChanged);
	LastScenario=nullptr;LastRunUUID.Empty();RefreshChartData();UpdateProgressiveState();
}
void USlabChartsPanelWidget::HandleSlabStateChanged()
{
	if(!IsValid(Slab))return;
	const auto State=Slab->GetSimulationStatus();
	const bool BodyVisible=GetVisibility()!=ESlateVisibility::Collapsed&&GetVisibility()!=ESlateVisibility::Hidden&&!IsPanelCollapsed();
	if(!BodyVisible)
	{
		// Immediate no-future-data barrier, without doing hidden graph/metric work.
		if(LastRunUUID!=State.RunUUID)for(const auto& C:Charts)if(C){C->SetRevealedTime(-1);C->ClearHover();}
		return;
	}
	if(LastScenario!=Slab->GetScenario().Get()||LastConfigurationHash!=Slab->GetMetricsConfigurationHash())RefreshChartData();
	UpdateProgressiveState();
}
void USlabChartsPanelWidget::SetChartMetric(ESlabChartMetric M){SetChartSlotMetric(0,M);}
void USlabChartsPanelWidget::SetChartSlotMetric(int32 I,ESlabChartMetric M)
{
	if(I<0||I>=3||static_cast<uint8>(M)>static_cast<uint8>(ESlabChartMetric::Speed))return;
	SelectedMetrics[I]=M;HoverTexts[I].Empty();if(auto* C=GetChartWidget(I))C->SetMetric(M);
}
ESlabChartMetric USlabChartsPanelWidget::GetChartSlotMetric(int32 I) const{return I>=0&&I<3?SelectedMetrics[I]:ESlabChartMetric::Angles;}
USlabScenarioChartWidget* USlabChartsPanelWidget::GetChartWidget(int32 I) const{return Charts.IsValidIndex(I)?Charts[I].Get():nullptr;}
void USlabChartsPanelWidget::SetChartFrameAxis(bool V){bFrameAxis=V;for(const auto& C:Charts)if(C)C->SetFrameAxis(V);}
void USlabChartsPanelWidget::ResetAllChartViews(){for(const auto& C:Charts)if(C)C->ResetChartView();}
void USlabChartsPanelWidget::SynchronizeViewRange(double A,double B){for(const auto& C:Charts)if(C)C->SetViewRange(A,B);}
double USlabChartsPanelWidget::CalculateHistoricalSpeed(const TArray<FSlabScenarioRow>& Rows,int32 I,double Scale)
{
	if(I<=0||!Rows.IsValidIndex(I))return 0;
	const auto& P=Rows[I-1];const auto& C=Rows[I];
	return (C.CenterX-P.CenterX)*Scale/FMath::Max(UE_DOUBLE_SMALL_NUMBER,C.ElapsedSec-P.ElapsedSec);
}
void USlabChartsPanelWidget::RefreshChartData()
{
	const auto Scenario=Slab?Slab->GetScenario():nullptr;LastScenario=Scenario.Get();LastConfigurationHash=Slab?Slab->GetMetricsConfigurationHash():0;
	auto Samples=MakeShared<TArray<FSlabChartSample>>();
	if(Slab&&Scenario)
	{
		Samples->Reserve(Scenario->Rows.Num());
		const double Scale=Slab->GetPositionUnit()==ESlabInputUnit::Millimeters?.1:Slab->GetPositionUnit()==ESlabInputUnit::Meters?100:1;
		for(int32 I=0;I<Scenario->Rows.Num();++I)
		{
			const auto& R=Scenario->Rows[I];const auto M=Slab->CalculateMetricsForRow(R);
			auto& S=Samples->AddDefaulted_GetRef();
			S.Time=R.ElapsedSec;S.Frame=R.FrameNo;S.LeftAngle=R.LeftAngle;S.RightAngle=R.RightAngle;
			S.CenterOffsetCm=M.CenterOffsetCm;S.MarginLeftCm=M.MarginLeftCm;S.MarginRightCm=M.MarginRightCm;S.bMarginsValid=M.bMarginsValid;S.MaxCornerOffsetCm=M.MaxCornerOffsetCm;
			S.SpeedCmPerSec=CalculateHistoricalSpeed(Scenario->Rows,I,Scale);
		}
	}
	SharedSamples=Samples;
	for(const auto& C:Charts)if(C){C->SetSharedSamples(SharedSamples);C->SetTimelineDuration(Scenario?Scenario->DurationSec:0);C->ClearHover();}
}
void USlabChartsPanelWidget::UpdateProgressiveState()
{
	const auto State=Slab?Slab->GetSimulationStatus():FSlabSimulationStatus();
	const bool NewRun=LastRunUUID!=State.RunUUID;
	if(NewRun)
	{
		LastRunUUID=State.RunUUID;
		for(const auto& C:Charts)if(C){C->SetRevealedTime(-1);C->ClearHover();C->ResetChartView();}
		for(auto& H:HoverTexts)H.Empty();
	}
	for(const auto& C:Charts)if(C)
	{
		C->SetProgressiveReveal(IsWorkspaceOwned());
		C->SetRevealedTime(State.RunUUID.IsEmpty()?-1:State.ElapsedSec);
		C->SetPlaybackCursor(State.ElapsedSec,State.FrameNo);
		C->SetChartFontScale(GetSensorToolFontScale());
		if(IsWorkspaceOwned())C->SetChartPalette(GetToolSectionColor(),GetToolMutedColor(),GetToolMutedColor()*.24f);
	}
}
void USlabChartsPanelWidget::NativeTick(const FGeometry& G,float D)
{
	Super::NativeTick(G,D);if(!IsValid(Slab))Slab=nullptr;
	if(GetVisibility()==ESlateVisibility::Collapsed||GetVisibility()==ESlateVisibility::Hidden||IsPanelCollapsed()){bWasBodyVisible=false;return;}
	RefreshAccumulator+=D;TextAccumulator+=D;if(RefreshAccumulator<.1f)return;RefreshAccumulator=0;
	const auto Scenario=Slab?Slab->GetScenario():nullptr;const uint32 Config=Slab?Slab->GetMetricsConfigurationHash():0;
	if(LastScenario!=Scenario.Get()||Config!=LastConfigurationHash){RefreshChartData();LastConfigurationHash=Config;}
	bWasBodyVisible=true;UpdateProgressiveState();
	if(TextAccumulator>=.2f)
	{
		TextAccumulator=0;const auto State=Slab?Slab->GetSimulationStatus():FSlabSimulationStatus();
		StatusText=FString::Printf(TEXT("진행된 표본 %d / %d · %.2f / %.2f초 · 세 차트의 보기 범위는 함께 움직입니다"),GetChartWidget(0)?GetChartWidget(0)->GetRevealedSampleCount():0,SharedSamples?SharedSamples->Num():0,State.ElapsedSec,State.DurationSec);
		for(int32 I=0;I<3;++I)
		{
			auto* C=GetChartWidget(I);HoverTexts[I]=C?C->GetHoverSummary():FString();
			if(HoverTexts[I].IsEmpty())
			{
				const int32 N=C?C->GetRevealedSampleCount():0;
				HoverTexts[I]=N>0&&SharedSamples?CurrentValue((*SharedSamples)[N-1],SelectedMetrics[I]):TEXT("시뮬레이션 시작 대기");
			}
		}
	}
}
TSharedRef<SWidget> USlabChartsPanelWidget::BuildChartCard(int32 I)
{
	auto Controls=SNew(SHorizontalBox);
	Controls->AddSlot().FillWidth(1)
	[SNew(SComboBox<TSharedPtr<ESlabChartMetric>>).OptionsSource(&MetricOptions).ComboBoxStyle(&MetricComboStyle).ForegroundColor(GetToolTextColor()).ContentPadding(FMargin(4,2))
	 .OnGenerateWidget_Lambda([this](TSharedPtr<ESlabChartMetric> M){return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(M?MetricLabel(*M):FText::GetEmpty());})
	 .OnSelectionChanged_Lambda([this,I](TSharedPtr<ESlabChartMetric> M,ESelectInfo::Type){if(M)SetChartSlotMetric(I,*M);})
	 [SNewSensorTool(STextBlock).ColorAndOpacity(GetToolAccentColor()).Text_Lambda([this,I](){return FText::FromString(FString::Printf(TEXT("%d. %s · %s"),I+1,*MetricLabel(SelectedMetrics[I]).ToString(),MetricUnit(SelectedMetrics[I])));} )]];
	for(int32 S=0;S<2;++S)Controls->AddSlot().AutoWidth().Padding(8,0)
	[SNew(SCheckBox).Visibility_Lambda([this,I,S](){return S==0||SelectedMetrics[I]==ESlabChartMetric::Angles||SelectedMetrics[I]==ESlabChartMetric::RailMargins?EVisibility::Visible:EVisibility::Collapsed;})
	 .IsChecked_Lambda([this,I,S](){return bShowSeries[I][S]?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
	 .OnCheckStateChanged_Lambda([this,I,S](ECheckBoxState V){bShowSeries[I][S]=V==ECheckBoxState::Checked;if(auto* C=GetChartWidget(I))C->SetSeriesVisible(S,bShowSeries[I][S]);})
	 [SNewSensorTool(STextBlock).ColorAndOpacity(S==0?FLinearColor(.2,.8,1):FLinearColor(1,.55,.2)).Text(S==0?LOCTEXT("Left","좌/값"):LOCTEXT("Right","우"))]];
	return SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(GetToolSectionColor()).Padding(10)
	[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight()[Controls]
	 +SVerticalBox::Slot().AutoHeight().Padding(0,5)
	 [SNew(SBox).HeightOverride_Lambda([this](){const double Scale=GetSensorToolFontScale();return FMath::Max(100.0*Scale,(GetEffectivePanelSize().Y-420.0*Scale)/3.0);})[Charts[I]->TakeWidget()]]
	 +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).Text_Lambda([this,I](){return FText::FromString(HoverTexts[I]);})]
	];
}
TSharedRef<SWidget> USlabChartsPanelWidget::RebuildWidget()
{
	if(WidgetTree&&WidgetTree->RootWidget)return Super::RebuildWidget();
	InitializeCharts(true);
	const auto Scenario=Slab?Slab->GetScenario():nullptr;
	if(!SharedSamples.IsValid()||LastScenario!=Scenario.Get()||(Slab&&LastConfigurationHash!=Slab->GetMetricsConfigurationHash()))RefreshChartData();
	UpdateProgressiveState();
	auto Top=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12,5));
	Top->AddSlot()[SNew(SCheckBox).IsChecked_Lambda([this](){return bFrameAxis?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){SetChartFrameAxis(S==ECheckBoxState::Checked);})[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("Frames","X축: 원본 frame (해제: 초)"))]];
	Top->AddSlot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("Reset","전체 보기 초기화")).OnClicked_Lambda([this](){ResetAllChartViews();return FReply::Handled();})];
	auto Cards=SNew(SVerticalBox);
	for(int32 I=0;I<3;++I)Cards->AddSlot().AutoHeight().Padding(0,4)[BuildChartCard(I)];
	return SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(12)
	[SNew(SVerticalBox)
	 +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("Title","Slab 실시간 차트"))]
	 +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
	  +SScrollBox::Slot()[SNew(SVerticalBox)
	   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Top]
	   +SVerticalBox::Slot().AutoHeight()[Cards]
	   +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(StatusText);}).ToolTipText(LOCTEXT("Help","진행된 시간까지만 표시합니다. 차트 드래그/휠은 세 보기 범위만 함께 이동하며 재생 위치는 바꾸지 않습니다. 초록선: 현재 재생. center_x는 전진 위치이며 중심 이탈이 아닙니다. 계열 색상은 좌/값=청록, 우=주황입니다."))]
	  ]]
	];
}
#undef LOCTEXT_NAMESPACE
