#include "SlabProgressPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "VirtualSensorUiStyle.h"
#include "SensorToolUiText.h"
#include "SlabInformationCard.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SlabProgressPanel"

namespace
{
	FText SlabUnitText(ESlabInputUnit U) { return FText::FromString(U == ESlabInputUnit::Millimeters ? TEXT("mm") : U == ESlabInputUnit::Meters ? TEXT("m") : TEXT("cm")); }
	ESlabInputUnit NextUnit(ESlabInputUnit U) { return U == ESlabInputUnit::Centimeters ? ESlabInputUnit::Millimeters : U == ESlabInputUnit::Millimeters ? ESlabInputUnit::Meters : ESlabInputUnit::Centimeters; }
}
void USlabProgressPanelWidget::BindSlabActor(ASlabActor* InSlab) { Slab = IsValid(InSlab) && InSlab->GetWorld() == GetWorld() ? InSlab : nullptr; RefreshAccumulator = 1;RefreshSurfaceSupport();if(OwnedScenarioScroll)OwnedScenarioScroll->ScrollToStart(); }
void USlabProgressPanelWidget::RefreshSurfaceSupport()
{
	SurfacePresetReasons.SetNum(5);for(int32 I=0;I<5;++I){SurfacePresetReasons[I].Empty();if(Slab)Slab->CanApplySurfacePreset(static_cast<ESlabSurfacePreset>(I),SurfacePresetReasons[I]);else SurfacePresetReasons[I]=TEXT("Slab를 먼저 선택하세요");}
}
bool USlabProgressPanelWidget::CanResetSlabToInitialPlacement(FString& OutReason) const
{
	if(!IsValid(Slab)||Slab->GetWorld()!=GetWorld()){OutReason=TEXT("Slab Actor 연결 필요");return false;}
	return Slab->CanResetToInitialPlacement(OutReason);
}
bool USlabProgressPanelWidget::ResetSlabToInitialPlacement(FString& OutError)
{
	if(!CanResetSlabToInitialPlacement(OutError))return false;
	const bool Result=Slab->ResetToInitialPlacement(OutError);RefreshAccumulator=1;return Result;
}
bool USlabProgressPanelWidget::ToggleOwnedCharts()
{
	auto* W=GetToolWorkspace();auto* Panel=W?W->GetOwnedPanel(ESensorToolPanelRole::SlabCharts):nullptr;
	if(!Panel)return false;
	const bool Open=!W->IsPanelOpen(ESensorToolPanelRole::SlabCharts);
	W->SetPanelOpen(ESensorToolPanelRole::SlabCharts,Open);
	if(Open){if(Panel->IsPanelCollapsed())Panel->TogglePanelCollapsed();W->BringOwnedPanelToFront(ESensorToolPanelRole::SlabCharts);}
	return true;
}
void USlabProgressPanelWidget::RefreshOwnedStatus()
{
	const auto Rows=SlabInformationCard::BuildRows(Slab,bOwnedDetailsExpanded);
	OwnedInformationValues.SetNum(12);
	TArray<FString> Lines,SourceLines;
	for(int32 I=0;I<Rows.Num();++I)
	{
		const auto& Row=Rows[I];
		if(Row.bSourceDetail)SourceLines.Add(Row.Label.ToString()+TEXT(": ")+Row.Value);
		else
		{
			Lines.Add(Row.Value);
			if(OwnedInformationValues[I].ToString()!=Row.Value)OwnedInformationValues[I]=FText::FromString(Row.Value);
		}
	}
	CachedInformationText=FString::Join(Lines,TEXT("\n"));
	if(bOwnedDetailsExpanded)OwnedSourceInformationLabel=FText::FromString(FString::Join(SourceLines,TEXT("\n")));
	if(!Slab){Summary=TEXT("시나리오 연결이 필요합니다");SensorStatus.Empty();Progress=0;bOwnedCanPause=bOwnedCanStop=bOwnedCanReset=false;OwnedSummaryLabel=FText::FromString(Summary);OwnedStatusLabel=FText::GetEmpty();return;}
	const auto S=Slab->GetSimulationStatus();Progress=S.Progress;
	bOwnedCanPause=S.State==ESlabSimulationState::Playing||S.State==ESlabSimulationState::Paused;
	bOwnedCanStop=Slab->IsSimulationActive();
	OwnedPauseLabel=S.State==ESlabSimulationState::Paused?LOCTEXT("Resume","재개"):LOCTEXT("Pause","일시정지");
	FString ResetReason;bOwnedCanReset=CanResetSlabToInitialPlacement(ResetReason);
	OwnedResetTooltip=FText::FromString(bOwnedCanReset?TEXT("레벨 배치 위치·회전으로 즉시 복귀합니다. 현재 형상·보관 목록은 유지하고 진행 표시만 초기화합니다."):SensorToolUiText::UserReason(ResetReason));
	const auto* Service=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	const auto Session=Service?Service->GetSlabSensorSessionStatus():FVirtualSlabSessionStatus();
	const FString State=ResolveOwnedMovementState(S,Session).ToString();
	Summary=State;
	if(S.State==ESlabSimulationState::Idle&&!S.Message.IsEmpty())Summary=SensorToolUiText::UserReason(S.Message);
	SensorStatus=S.State==ESlabSimulationState::Preparing?TEXT("연결 확인 후 시작합니다 · 준비 중에는 이동하지 않습니다"):TEXT("보관 목록에서 선택해 재생하세요 · 이전 실행은 결과에서 확인하세요");
	if(!S.TransmissionWarning.IsEmpty())SensorStatus=SensorToolUiText::UserReason(S.TransmissionWarning);
	else if(!S.RunUUID.IsEmpty()&&Session.RunId==S.RunUUID)SensorStatus=TEXT("전송 상태와 종료 결과는 결과 화면에서 확인하세요");
	OwnedSummaryLabel=FText::FromString(Summary);OwnedStatusLabel=FText::FromString(SensorStatus);
	if(bOwnedDetailsExpanded)
	{
		Detail=FString::Printf(TEXT("시나리오 UUID: %s\n실행 UUID: %s\n원본 행 번호: %lld\n%s"),*S.ScenarioUUID,*S.RunUUID,S.FrameNo,*S.Message);
		OwnedDetailLabel=FText::FromString(Detail);
	}
}
FText USlabProgressPanelWidget::ResolveOwnedMovementState(const FSlabSimulationStatus& Simulation,const FVirtualSlabSessionStatus& Session)
{
	switch(Simulation.State)
	{
	case ESlabSimulationState::Playing:return LOCTEXT("OutcomePlaying","실행 중");
	case ESlabSimulationState::Paused:return LOCTEXT("OutcomePaused","일시정지");
	case ESlabSimulationState::Failed:return LOCTEXT("OutcomeFailed","실패");
	case ESlabSimulationState::Preparing:return LOCTEXT("OutcomePreparing","연결 준비 중");
	case ESlabSimulationState::Completed:
	{
		const bool MatchingAbort=!Simulation.RunUUID.IsEmpty()&&Session.RunId==Simulation.RunUUID&&Session.bAborted;
		// The movement enum intentionally retains Completed for a user stop. A newer
		// sensor session must not erase the earlier run's visible early-stop outcome.
		const bool EndedEarly=FMath::IsFinite(Simulation.DurationSec)&&FMath::IsFinite(Simulation.ElapsedSec)&&
			Simulation.DurationSec>0&&Simulation.ElapsedSec+1.e-4<Simulation.DurationSec;
		return MatchingAbort||EndedEarly?LOCTEXT("OutcomeStopped","중단됨"):LOCTEXT("OutcomeComplete","움직임 완료");
	}
	default:return LOCTEXT("OutcomeIdle","대기");
	}
}
void USlabProgressPanelWidget::NativeTick(const FGeometry& G, float D)
{
	Super::NativeTick(G, D);
	if (!IsValid(Slab)) Slab = nullptr;
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden || IsPanelCollapsed()) return;
	RefreshAccumulator += D; if (RefreshAccumulator < .2f) return; RefreshAccumulator = 0;
	if(IsWorkspaceOwned()&&(!WidgetTree||!WidgetTree->RootWidget)){RefreshOwnedStatus();return;}
	if (!Slab) { Summary = TEXT("Slab Actor 연결 필요"); Detail.Empty(); SensorStatus.Empty(); Progress = 0; return; }
	const auto S = Slab->GetSimulationStatus(); Progress = S.Progress;
	if(S.State==ESlabSimulationState::Preparing){Summary=TEXT("데이터 필수 · 실제 Broker 연결 준비 중\n현재 프레임 — · 진행률 0%");Detail=S.Message;SensorStatus=TEXT("준비 완료 전 Slab 이동·해당 실행 송신 없음");return;}
	if(S.State==ESlabSimulationState::Failed&&S.RunUUID.IsEmpty())
	{Summary=TEXT("실패 · ")+S.Message+TEXT("\n현재 프레임 — · 진행률 0%");Detail.Empty();SensorStatus=TEXT("실행별 결과는 연결·진단에서 확인하세요.");return;}
	const auto* SessionSubsystem=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>():nullptr;
	const auto Session=SessionSubsystem?SessionSubsystem->GetSlabSensorSessionStatus():FVirtualSlabSessionStatus();
	FString State = S.State == ESlabSimulationState::Playing ? TEXT("실행 중") : S.State == ESlabSimulationState::Paused ? TEXT("일시정지") : S.State == ESlabSimulationState::Completed ? TEXT("움직임 완료") : S.State == ESlabSimulationState::Failed ? TEXT("실패") : TEXT("대기");
	if(IsWorkspaceOwned()&&(!WidgetTree||!WidgetTree->RootWidget)) State=ResolveOwnedMovementState(S,Session).ToString();
	Summary = FString::Printf(TEXT("%s · %s\n원본 frame %lld · 행 %d/%d · %.2f / %.2f초 (%.1f%%)"), *State, *S.MtlNo, S.FrameNo, S.RowCount > 0 ? S.RowIndex + 1 : 0, S.RowCount, S.ElapsedSec, S.DurationSec, S.Progress * 100);
	const auto Metrics = Slab->GetCurrentMetrics();
	Summary += FMath::IsNearlyZero(Metrics.CenterOffsetCm, .001) ? TEXT("\n중심선 일치") : FString::Printf(TEXT("\n중심 이탈 %+.1f cm"), Metrics.CenterOffsetCm);
	Summary += Metrics.bMarginsValid ? FString::Printf(TEXT(" · 좌/우 간격 %.1f / %.1f cm"), Metrics.MarginLeftCm, Metrics.MarginRightCm) : TEXT(" · 가드레일 N/A");
	Detail = FString::Printf(TEXT("보관 ID: %s\n실행 UUID: %s\n%s"), *S.ScenarioUUID, *S.RunUUID, *S.Message);
	if (const auto Scenario = Slab->GetScenario(); Scenario && !Scenario->Rows.IsEmpty())
	{
		const auto& R = Scenario->Rows[0]; const double Scale = Slab->GetDimensionUnit() == ESlabInputUnit::Millimeters ? .001 : Slab->GetDimensionUnit() == ESlabInputUnit::Meters ? 1 : .01;
		Detail += FString::Printf(TEXT("\n실제 크기: 길이 %.3fm · 폭 %.3fm · 두께 %.3fm\n무게 원본: %.1f (정보용, 물리 질량 미적용)"), R.Length * Scale, R.Width * Scale, R.Thickness * Scale, R.Weight);
	}
	SensorStatus = TEXT("센서 송신 세션 없음 · 관찰 전용 가능");
	if(S.State==ESlabSimulationState::Idle&&S.RunUUID.IsEmpty())
	{
		Summary=(S.Message.IsEmpty()?FString(TEXT("대기")):S.Message)+TEXT("\n현재 프레임 — · 진행률 0%");
		Detail=TEXT("보관 시나리오는 재생 목록에서 확인할 수 있습니다.");
		SensorStatus=TEXT("활성 Slab 송신 세션 없음 · 이전 결과는 연결·진단에서 확인");
		if(auto* Results=GetWorld()->GetSubsystem<USlabRunResultsSubsystem>()){const auto Runs=Results->GetRecentRunResults();if(!Runs.IsEmpty())SensorStatus+=TEXT("\n최근 실행 · ")+USlabRunResultsSubsystem::Describe(Runs.Last());}
		return;
	}
	if(!S.RunUUID.IsEmpty()&&Session.RunId==S.RunUUID) SensorStatus=FString::Printf(TEXT("센서 송신: %s · 미완료 %lld"),*Session.Message,Session.UnfinishedFrames);
	if(IsWorkspaceOwned()&&!S.TransmissionWarning.IsEmpty())SensorStatus+=TEXT("\n주의: ")+S.TransmissionWarning;
	if(IsWorkspaceOwned())if(auto* Results=GetWorld()->GetSubsystem<USlabRunResultsSubsystem>()){FSlabRunDeliverySummary R;if(Results->GetRunResult(S.RunUUID,R))SensorStatus+=TEXT("\n")+USlabRunResultsSubsystem::Describe(R);}
}
TSharedRef<SWidget> USlabProgressPanelWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget) return Super::RebuildWidget();
	auto Controls = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6));
	Controls->AddSlot()[SNewSensorTool(SButton).Text_Lambda([this]() { if(IsWorkspaceOwned())return OwnedPauseLabel;return Slab && Slab->GetSimulationStatus().State == ESlabSimulationState::Paused ? LOCTEXT("Resume", "재개") : LOCTEXT("Pause", "일시정지"); }).IsEnabled_Lambda([this]() { if(IsWorkspaceOwned())return bOwnedCanPause;if (!Slab) return false; auto S = Slab->GetSimulationStatus().State; return S == ESlabSimulationState::Playing || S == ESlabSimulationState::Paused; }).OnClicked_Lambda([this]() { if (Slab) Slab->SetSimulationPaused(Slab->GetSimulationStatus().State != ESlabSimulationState::Paused); return FReply::Handled(); })];
	Controls->AddSlot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle(true)).Text(LOCTEXT("Stop", "중단")).IsEnabled_Lambda([this]() { return IsWorkspaceOwned()?bOwnedCanStop:Slab&&Slab->IsSimulationActive(); }).OnClicked_Lambda([this]() { if (Slab) Slab->StopSimulation(); return FReply::Handled(); })];
	Controls->AddSlot()[SNewSensorTool(SButton).Text(LOCTEXT("ResetPlacement","초기 위치로 복귀"))
	 .IsEnabled_Lambda([this](){if(IsWorkspaceOwned())return bOwnedCanReset;FString Reason;return CanResetSlabToInitialPlacement(Reason);})
	 .ToolTipText_Lambda([this](){if(IsWorkspaceOwned())return OwnedResetTooltip;FString Reason;return FText::FromString(CanResetSlabToInitialPlacement(Reason)?TEXT("레벨 배치 위치·회전으로 즉시 복귀합니다. 현재 형상·보관 목록은 유지하고 진행 표시만 초기화합니다."):Reason);})
	 .OnClicked_Lambda([this](){FString Error;ResetSlabToInitialPlacement(Error);return FReply::Handled();})];
	auto Config = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6));
	for (bool Dimensions : {true, false}) Config->AddSlot()[SNewSensorTool(SButton)
	 .Text_Lambda([this, Dimensions]() { return FText::FromString((Dimensions ? FString(TEXT("치수 단위: ")) : FString(TEXT("위치 단위: "))) + (Slab ? SlabUnitText(Dimensions ? Slab->GetDimensionUnit() : Slab->GetPositionUnit()).ToString() : TEXT("cm"))); })
	 .ToolTipText(LOCTEXT("UnitsTip", "cm → mm → m 순환. 실행 전 선택하며 원본 JSON은 변경하지 않습니다."))
	 .IsEnabled_Lambda([this]() { if (!Slab) return false; auto S = Slab->GetSimulationStatus().State; return S != ESlabSimulationState::Playing && S != ESlabSimulationState::Paused; })
	 .OnClicked_Lambda([this, Dimensions]() { if (Slab) { if (Dimensions) Slab->SetDimensionUnit(NextUnit(Slab->GetDimensionUnit())); else Slab->SetPositionUnit(NextUnit(Slab->GetPositionUnit())); } return FReply::Handled(); })];
	Config->AddSlot()[SNew(SCheckBox).IsEnabled_Lambda([this]() { return Slab != nullptr; }).IsChecked_Lambda([this]() { return Slab && Slab->GetHotAppearance() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState S) { if (Slab) Slab->SetHotAppearance(S == ECheckBoxState::Checked); })[SNewSensorTool(STextBlock).Text(LOCTEXT("Hot", "고온 Slab 외형 (해제: 냉각 철강)"))]];
	if(IsWorkspaceOwned())
	{
		// Replace only the owned native hot checkbox. Designer and legacy paths retain it.
		Config=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,6));
		for(bool Dimensions:{true,false})Config->AddSlot()[SNewSensorTool(SButton)
		 .Text_Lambda([this,Dimensions](){return FText::FromString((Dimensions?FString(TEXT("치수 단위: ")):FString(TEXT("위치 단위: ")))+(Slab?SlabUnitText(Dimensions?Slab->GetDimensionUnit():Slab->GetPositionUnit()).ToString():TEXT("—")));})
		 .IsEnabled_Lambda([this](){return Slab&&!Slab->IsSimulationActive();})
		 .OnClicked_Lambda([this,Dimensions](){if(Slab){if(Dimensions)Slab->SetDimensionUnit(NextUnit(Slab->GetDimensionUnit()));else Slab->SetPositionUnit(NextUnit(Slab->GetPositionUnit()));}return FReply::Handled();})];
		RefreshSurfaceSupport();
		for(int32 I=0;I<5;++I)Config->AddSlot()[SNewSensorTool(SButton).Text(SensorToolUiText::SurfacePreset(I))
		 .IsEnabled_Lambda([this,I](){return Slab&&SurfacePresetReasons.IsValidIndex(I)&&SurfacePresetReasons[I].IsEmpty();})
		 .ToolTipText_Lambda([this,I](){return SensorToolUiText::SurfaceSupportReason(SurfacePresetReasons.IsValidIndex(I)?SurfacePresetReasons[I]:FString());})
		 .ButtonColorAndOpacity_Lambda([this,I](){return Slab&&int32(Slab->GetSurfacePreset())==I?FLinearColor(.35f,.70f,.85f,1):FLinearColor::White;})
		 .OnClicked_Lambda([this,I](){if(Slab)Slab->ApplySurfacePreset(static_cast<ESlabSurfacePreset>(I),Slab->IsSurfaceLidarLinked(),SurfaceActionMessage);RefreshAccumulator=1;return FReply::Handled();})];
		Config->AddSlot()[SNew(SCheckBox).IsEnabled_Lambda([this](){return Slab!=nullptr;})
		 .IsChecked_Lambda([this](){return Slab&&Slab->IsSurfaceLidarLinked()?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
		 .OnCheckStateChanged_Lambda([this](ECheckBoxState State){if(Slab)Slab->ApplySurfacePreset(Slab->GetSurfacePreset(),State==ECheckBoxState::Checked,SurfaceActionMessage);RefreshAccumulator=1;})
		 [SNewSensorTool(STextBlock).Text(SensorToolUiText::LinkSlabReflectivity())]];
		Config->AddSlot()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(SurfaceActionMessage);})];
	}
	if (IsWorkspaceOwned())
	{
		const auto InformationRows=SlabInformationCard::BuildRows(nullptr,false);
		OwnedInformationValues.SetNum(InformationRows.Num());
		auto InformationGrid=SNew(SGridPanel).FillColumn(1,1.f);
		FText PreviousSection;int32 GridRow=0;
		for(int32 I=0;I<InformationRows.Num();++I)
		{
			const auto& Row=InformationRows[I];
			if(!Row.Section.EqualTo(PreviousSection))
			{
				InformationGrid->AddSlot(0,GridRow++).ColumnSpan(2).Padding(0,8,0,4)[SNewSensorTool(STextBlock).Text(Row.Section).ColorAndOpacity(GetToolAccentColor())];
				PreviousSection=Row.Section;
			}
			InformationGrid->AddSlot(0,GridRow).VAlign(VAlign_Top).Padding(0,2,12,2)[SNewSensorTool(STextBlock).Text(Row.Label).ColorAndOpacity(GetToolMutedColor())];
			InformationGrid->AddSlot(1,GridRow++).Padding(0,2)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this,I](){return OwnedInformationValues[I];})];
		}
		Controls->AddSlot()[SNewSensorTool(SButton).Visibility_Lambda([this](){return GetToolWorkspace()&&GetToolWorkspace()->IsSidebarMode()?EVisibility::Visible:EVisibility::Collapsed;}).Text(LOCTEXT("OpenArchive","보관 목록"))
		 .OnClicked_Lambda([this](){if(auto* W=GetToolWorkspace()){W->EnsureOwnedReplayPanel();W->SetPanelOpen(ESensorToolPanelRole::Replay,true);}return FReply::Handled();})];
		Controls->AddSlot()[SNewSensorTool(SButton).Text(SensorToolUiText::ScenarioCharts())
		 .IsEnabled_Lambda([this](){auto* W=GetToolWorkspace();return W&&W->GetOwnedPanel(ESensorToolPanelRole::SlabCharts);})
		 .ButtonColorAndOpacity_Lambda([this](){auto* W=GetToolWorkspace();return W&&W->IsPanelOpen(ESensorToolPanelRole::SlabCharts)?FLinearColor(.35f,.70f,.85f,1):FLinearColor::White;})
		 .OnClicked_Lambda([this](){ToggleOwnedCharts();return FReply::Handled();})];
		auto MasterDisplay=SNew(SCheckBox).IsEnabled_Lambda([this](){return IsValid(Slab);})
		 .IsChecked_Lambda([this](){return IsValid(Slab)&&Slab->GetDiagnosticHelpersVisible()?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
		 .OnCheckStateChanged_Lambda([this](ECheckBoxState State){if(IsValid(Slab))Slab->SetDiagnosticHelpersVisible(State==ECheckBoxState::Checked);})
		 .ToolTipText(LOCTEXT("MasterAnalysisTip","3D 분석 표시를 한 번에 숨기거나 복원합니다. 아래의 개별 표시 선택은 그대로 유지됩니다."))
		 [SNewSensorTool(STextBlock).Text(LOCTEXT("MasterAnalysis","3D 분석 표시"))];
		auto Display = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12, 8));
		auto Toggle = [this, Display](FText Label, bool FSlabAnalysisDisplaySettings::* Field)
		{
			Display->AddSlot()[SNew(SCheckBox).IsEnabled_Lambda([this]() { return Slab != nullptr; })
			 .IsChecked_Lambda([this, Field]() { return Slab && Slab->GetAnalysisDisplaySettings().*Field ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			 .OnCheckStateChanged_Lambda([this, Field](ECheckBoxState State) { if (Slab) { auto Settings = Slab->GetAnalysisDisplaySettings(); Settings.*Field = State == ECheckBoxState::Checked; Slab->SetAnalysisDisplaySettings(Settings); } })
			 [SNewSensorTool(STextBlock).Text(Label)]];
		};
		Toggle(LOCTEXT("Outline", "실제 윤곽"), &FSlabAnalysisDisplaySettings::bOutline);
		Toggle(LOCTEXT("Cross", "중심·십자선"), &FSlabAnalysisDisplaySettings::bCross);
		Toggle(LOCTEXT("Reference", "0° 기준 윤곽"), &FSlabAnalysisDisplaySettings::bReferencePose);
		Toggle(LOCTEXT("Centerline", "진행 중심선"), &FSlabAnalysisDisplaySettings::bCenterline);
		Toggle(LOCTEXT("Yaw", "기울기"), &FSlabAnalysisDisplaySettings::bYaw);
		Toggle(LOCTEXT("Margins", "좌우 간격"), &FSlabAnalysisDisplaySettings::bMargins);
		Toggle(LOCTEXT("Status", "3D 상태"), &FSlabAnalysisDisplaySettings::bStatus);
		Display->AddSlot()[SNew(SCheckBox).IsEnabled_Lambda([this](){return IsValid(Slab);})
		 .IsChecked_Lambda([this](){return IsValid(Slab)&&Slab->GetDetailedDiagnosticLabels()?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
		 .OnCheckStateChanged_Lambda([this](ECheckBoxState State){if(IsValid(Slab))Slab->SetDetailedDiagnosticLabels(State==ECheckBoxState::Checked);})
		 .ToolTipText(LOCTEXT("DetailLabelsTip","좌/우 여유·중심 이탈·원본 행을 3D에 표시합니다. 카드의 상세 선택과 함께 바뀌며 이번 세션에만 적용됩니다."))
		 [SNewSensorTool(STextBlock).Text(LOCTEXT("DetailLabels","3D 상세 라벨"))]];
		for(bool Text:{false,true})
		{
			Display->AddSlot()[SNew(SHorizontalBox)
			 +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNewSensorTool(STextBlock).Text(Text?LOCTEXT("TextSize","문자 크기 (px)"):LOCTEXT("LineSize","선 굵기 (px)"))]
			 +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SSpinBox<float>).Font_Lambda([this](){return FCoreStyle::GetDefaultFontStyle("Regular",FMath::RoundToInt(16*GetSensorToolFontScale()));}).MinValue(Text?18.f:2.f).MaxValue(Text?56.f:12.f).Delta(1.f).MinDesiredWidth(80)
			  .IsEnabled_Lambda([this](){return IsValid(Slab);}).Value_Lambda([this,Text](){const auto S=Slab?Slab->GetAnalysisDisplaySettings():FSlabAnalysisDisplaySettings();return Text?S.TextHeightPixels:S.LineWidthPixels;})
			  .OnValueChanged_Lambda([this,Text](float V){if(Slab){auto S=Slab->GetAnalysisDisplaySettings();if(Text)S.TextHeightPixels=V;else S.LineWidthPixels=V;Slab->SetAnalysisDisplaySettings(S);}})]];
		}
		Display->AddSlot()[SNewSensorTool(SButton).Text(LOCTEXT("ReadableReset","가독성 초기화")).OnClicked_Lambda([this](){if(Slab){auto S=Slab->GetAnalysisDisplaySettings();S.LineWidthPixels=4;S.TextHeightPixels=28;Slab->SetAnalysisDisplaySettings(S);}return FReply::Handled();})];
		return BuildToolPanelFrame(SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(12)
		[SNew(SVerticalBox)
		 + SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(SensorToolUiText::ScenarioProgress())]
		 + SVerticalBox::Slot().FillHeight(1)[SNew(SVerticalBox).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
		   // Controls are before all variable-length content: status wrapping cannot move them.
		   + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)[Controls]
		   + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SNew(SProgressBar).Percent_Lambda([this]() { return TOptional<float>(Progress); }).FillColorAndOpacity(GetToolAccentColor())]
		   + SVerticalBox::Slot().FillHeight(1)[SAssignNew(OwnedScenarioScroll,SScrollBox)
		     + SScrollBox::Slot()[SNew(SVerticalBox)
		       + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,10)[SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(GetToolSectionColor()).Padding(8)[SNew(SVerticalBox)
		         +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).Text(SensorToolUiText::SlabInformation())]
		         +SVerticalBox::Slot().AutoHeight().Padding(0,4)[InformationGrid]]]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolAccentColor()).Text_Lambda([this]() { return OwnedSummaryLabel; })]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return OwnedStatusLabel; })]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SAssignNew(OwnedDisplayArea,SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(SensorToolUiText::DisplaySettings())].BodyContent()[SNew(SVerticalBox)
		        +SVerticalBox::Slot().AutoHeight()[MasterDisplay]+SVerticalBox::Slot().AutoHeight().Padding(0,8)[Display]+SVerticalBox::Slot().AutoHeight()[Config]]]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SExpandableArea).InitiallyCollapsed(true)
		         .OnAreaExpansionChanged_Lambda([this](bool Open){bOwnedDetailsExpanded=Open;RefreshAccumulator=1;})
		         .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Details", "진단·맵 연결 점검"))].BodyContent()
		         [SNew(SVerticalBox)
		          + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return OwnedSourceInformationLabel;})
		           .ToolTipText(LOCTEXT("SourceCardTip","원본 행 위치·속도는 입력 데이터 값이며 현재 표시 값과 다를 수 있습니다. Y 입력은 현재 이동에 적용되지 않습니다. 중심 이탈·여유는 해당 원본 행의 Track 기하 계산이며 임계값 판정이 아닙니다. 중량은 원본 단위입니다."))]
		          + SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this]() { return OwnedDetailLabel; })]
		          + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNewSensorTool(SButton).Text(LOCTEXT("Validate", "연결 상태 점검"))
		            .OnClicked_Lambda([this]() { SetupStatus.Empty(); if (Slab) { const auto V = Slab->ValidateSlabSetup(); SetupStatus = V.bCanSimulate ? TEXT("시뮬레이션 연결 정상") : TEXT("시뮬레이션 연결 확인 필요"); for (const auto& E : V.Errors) SetupStatus += TEXT("\n오류: ") + E; for (const auto& W : V.Warnings) SetupStatus += TEXT("\n안내: ") + W; } else SetupStatus = TEXT("Slab Actor 연결 필요"); return FReply::Handled(); })]
		          + SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this]() { return FText::FromString(SetupStatus); })]
		         ]]
		     ]]
		 ]
		]);
	}
	return SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(12)
	[SNew(SVerticalBox)
	 + SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("Title", "Slab 진행"))]
	 + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); }) + SScrollBox::Slot()[SNew(SVerticalBox)
	   + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolAccentColor()).Text_Lambda([this]() { return FText::FromString(Summary); })]
	   + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNew(SProgressBar).Percent_Lambda([this]() { return TOptional<float>(Progress); }).FillColorAndOpacity(GetToolAccentColor())]
	   + SVerticalBox::Slot().AutoHeight()[Controls]
	   + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(SensorStatus); })]
	   + SVerticalBox::Slot().AutoHeight()[Config]
	   + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this]() { return FText::FromString(Detail); })]
	   + SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(LOCTEXT("Notice", "움직임 종료와 센서의 마지막 데이터 전송 완료는 별도 상태입니다. 외형은 열 물리 시뮬레이션이 아닙니다."))]
	 ]]
	];
}
#undef LOCTEXT_NAMESPACE
