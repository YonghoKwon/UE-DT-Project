#include "SlabProgressPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "VirtualSensorUiStyle.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SExpandableArea.h"
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
void USlabProgressPanelWidget::BindSlabActor(ASlabActor* InSlab) { Slab = IsValid(InSlab) && InSlab->GetWorld() == GetWorld() ? InSlab : nullptr; RefreshAccumulator = 1; }
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
	Controls->AddSlot()[SNewSensorTool(SButton).Text_Lambda([this]() { return Slab && Slab->GetSimulationStatus().State == ESlabSimulationState::Paused ? LOCTEXT("Resume", "재개") : LOCTEXT("Pause", "일시정지"); }).IsEnabled_Lambda([this]() { if (!Slab) return false; auto S = Slab->GetSimulationStatus().State; return S == ESlabSimulationState::Playing || S == ESlabSimulationState::Paused; }).OnClicked_Lambda([this]() { if (Slab) Slab->SetSimulationPaused(Slab->GetSimulationStatus().State != ESlabSimulationState::Paused); return FReply::Handled(); })];
	Controls->AddSlot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle(true)).Text(LOCTEXT("Stop", "중단")).IsEnabled_Lambda([this]() { return Slab&&Slab->IsSimulationActive(); }).OnClicked_Lambda([this]() { if (Slab) Slab->StopSimulation(); return FReply::Handled(); })];
	Controls->AddSlot()[SNewSensorTool(SButton).Text(LOCTEXT("ResetPlacement","초기 위치로 복귀"))
	 .IsEnabled_Lambda([this](){FString Reason;return CanResetSlabToInitialPlacement(Reason);})
	 .ToolTipText_Lambda([this](){FString Reason;return FText::FromString(CanResetSlabToInitialPlacement(Reason)?TEXT("레벨 배치 위치·회전으로 즉시 복귀합니다. 현재 형상·보관 목록은 유지하고 진행 표시만 초기화합니다."):Reason);})
	 .OnClicked_Lambda([this](){FString Error;ResetSlabToInitialPlacement(Error);return FReply::Handled();})];
	auto Config = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6));
	for (bool Dimensions : {true, false}) Config->AddSlot()[SNewSensorTool(SButton)
	 .Text_Lambda([this, Dimensions]() { return FText::FromString((Dimensions ? FString(TEXT("치수 단위: ")) : FString(TEXT("위치 단위: "))) + (Slab ? SlabUnitText(Dimensions ? Slab->GetDimensionUnit() : Slab->GetPositionUnit()).ToString() : TEXT("cm"))); })
	 .ToolTipText(LOCTEXT("UnitsTip", "cm → mm → m 순환. 실행 전 선택하며 원본 JSON은 변경하지 않습니다."))
	 .IsEnabled_Lambda([this]() { if (!Slab) return false; auto S = Slab->GetSimulationStatus().State; return S != ESlabSimulationState::Playing && S != ESlabSimulationState::Paused; })
	 .OnClicked_Lambda([this, Dimensions]() { if (Slab) { if (Dimensions) Slab->SetDimensionUnit(NextUnit(Slab->GetDimensionUnit())); else Slab->SetPositionUnit(NextUnit(Slab->GetPositionUnit())); } return FReply::Handled(); })];
	Config->AddSlot()[SNew(SCheckBox).IsEnabled_Lambda([this]() { return Slab != nullptr; }).IsChecked_Lambda([this]() { return Slab && Slab->GetHotAppearance() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState S) { if (Slab) Slab->SetHotAppearance(S == ECheckBoxState::Checked); })[SNewSensorTool(STextBlock).Text(LOCTEXT("Hot", "고온 Slab 외형 (해제: 냉각 철강)"))]];
	if (IsWorkspaceOwned())
	{
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
		for(bool Text:{false,true})
		{
			Display->AddSlot()[SNew(SHorizontalBox)
			 +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNewSensorTool(STextBlock).Text(Text?LOCTEXT("TextSize","문자 크기 (px)"):LOCTEXT("LineSize","선 굵기 (px)"))]
			 +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SSpinBox<float>).Font_Lambda([this](){return FCoreStyle::GetDefaultFontStyle("Regular",FMath::RoundToInt(16*GetSensorToolFontScale()));}).MinValue(Text?18.f:2.f).MaxValue(Text?56.f:12.f).Delta(1.f).MinDesiredWidth(80)
			  .IsEnabled_Lambda([this](){return IsValid(Slab);}).Value_Lambda([this,Text](){const auto S=Slab?Slab->GetAnalysisDisplaySettings():FSlabAnalysisDisplaySettings();return Text?S.TextHeightPixels:S.LineWidthPixels;})
			  .OnValueChanged_Lambda([this,Text](float V){if(Slab){auto S=Slab->GetAnalysisDisplaySettings();if(Text)S.TextHeightPixels=V;else S.LineWidthPixels=V;Slab->SetAnalysisDisplaySettings(S);}})]];
		}
		Display->AddSlot()[SNewSensorTool(SButton).Text(LOCTEXT("ReadableReset","가독성 초기화")).OnClicked_Lambda([this](){if(Slab){auto S=Slab->GetAnalysisDisplaySettings();S.LineWidthPixels=4;S.TextHeightPixels=28;Slab->SetAnalysisDisplaySettings(S);}return FReply::Handled();})];
		return SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(12)
		[SNew(SVerticalBox)
		 + SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("Title", "Slab 진행"))]
		 + SVerticalBox::Slot().FillHeight(1)[SNew(SVerticalBox).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
		   // Controls are before all variable-length content: status wrapping cannot move them.
		   + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)[Controls]
		   + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SNew(SProgressBar).Percent_Lambda([this]() { return TOptional<float>(Progress); }).FillColorAndOpacity(GetToolAccentColor())]
		   + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[MasterDisplay]
		   + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)
		     + SScrollBox::Slot()[SNew(SVerticalBox)
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolAccentColor()).Text_Lambda([this]() { return FText::FromString(Summary); })]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(SensorStatus); })]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("AnalysisDetails", "세부 표시 선택"))].BodyContent()[Display]]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Appearance", "단위·표면 설정"))].BodyContent()[Config]]
		       + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Details", "실행 정보·맵 연결 점검"))].BodyContent()
		         [SNew(SVerticalBox)
		          + SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this]() { return FText::FromString(Detail); })]
		          + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNewSensorTool(SButton).Text(LOCTEXT("Validate", "연결 상태 점검"))
		            .OnClicked_Lambda([this]() { SetupStatus.Empty(); if (Slab) { const auto V = Slab->ValidateSlabSetup(); SetupStatus = V.bCanSimulate ? TEXT("시뮬레이션 연결 정상") : TEXT("시뮬레이션 연결 확인 필요"); for (const auto& E : V.Errors) SetupStatus += TEXT("\n오류: ") + E; for (const auto& W : V.Warnings) SetupStatus += TEXT("\n안내: ") + W; } else SetupStatus = TEXT("Slab Actor 연결 필요"); return FReply::Handled(); })]
		          + SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this]() { return FText::FromString(SetupStatus); })]
		         ]]
		     ]]
		 ]
		];
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
