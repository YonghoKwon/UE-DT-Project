#include "SlabProgressPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "VirtualSensorUiStyle.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SlabProgressPanel"

namespace
{
	FText SlabUnitText(ESlabInputUnit U) { return FText::FromString(U == ESlabInputUnit::Millimeters ? TEXT("mm") : U == ESlabInputUnit::Meters ? TEXT("m") : TEXT("cm")); }
	ESlabInputUnit NextUnit(ESlabInputUnit U) { return U == ESlabInputUnit::Centimeters ? ESlabInputUnit::Millimeters : U == ESlabInputUnit::Millimeters ? ESlabInputUnit::Meters : ESlabInputUnit::Centimeters; }
}
void USlabProgressPanelWidget::BindSlabActor(ASlabActor* InSlab) { Slab = IsValid(InSlab) && InSlab->GetWorld() == GetWorld() ? InSlab : nullptr; RefreshAccumulator = 1; }
void USlabProgressPanelWidget::NativeTick(const FGeometry& G, float D)
{
	Super::NativeTick(G, D);
	if (!IsValid(Slab)) Slab = nullptr;
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden || IsPanelCollapsed()) return;
	RefreshAccumulator += D; if (RefreshAccumulator < .2f) return; RefreshAccumulator = 0;
	if (!Slab) { Summary = TEXT("Slab Actor 연결 필요"); Detail.Empty(); SensorStatus.Empty(); Progress = 0; return; }
	const auto S = Slab->GetSimulationStatus(); Progress = S.Progress;
	const TCHAR* State = S.State == ESlabSimulationState::Playing ? TEXT("실행 중") : S.State == ESlabSimulationState::Paused ? TEXT("일시정지") : S.State == ESlabSimulationState::Completed ? TEXT("움직임 완료") : S.State == ESlabSimulationState::Failed ? TEXT("실패") : TEXT("대기");
	Summary = FString::Printf(TEXT("%s · %s\n원본 frame %lld · 행 %d/%d · %.2f / %.2f초 (%.1f%%)"), State, *S.MtlNo, S.FrameNo, S.RowCount > 0 ? S.RowIndex + 1 : 0, S.RowCount, S.ElapsedSec, S.DurationSec, S.Progress * 100);
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
	if (auto* C = GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()) { const auto T = C->GetSlabSensorSessionStatus(); if (!S.RunUUID.IsEmpty() && T.RunId == S.RunUUID) SensorStatus = FString::Printf(TEXT("센서 송신: %s · 미완료 %lld"), *T.Message, T.UnfinishedFrames); }
}
TSharedRef<SWidget> USlabProgressPanelWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget) return Super::RebuildWidget();
	auto Controls = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 6));
	Controls->AddSlot()[SNewSensorTool(SButton).Text_Lambda([this]() { return Slab && Slab->GetSimulationStatus().State == ESlabSimulationState::Paused ? LOCTEXT("Resume", "재개") : LOCTEXT("Pause", "일시정지"); }).IsEnabled_Lambda([this]() { if (!Slab) return false; auto S = Slab->GetSimulationStatus().State; return S == ESlabSimulationState::Playing || S == ESlabSimulationState::Paused; }).OnClicked_Lambda([this]() { if (Slab) Slab->SetSimulationPaused(Slab->GetSimulationStatus().State != ESlabSimulationState::Paused); return FReply::Handled(); })];
	Controls->AddSlot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle(true)).Text(LOCTEXT("Stop", "중단")).IsEnabled_Lambda([this]() { if (!Slab) return false; auto S = Slab->GetSimulationStatus().State; return S == ESlabSimulationState::Playing || S == ESlabSimulationState::Paused; }).OnClicked_Lambda([this]() { if (Slab) Slab->StopSimulation(); return FReply::Handled(); })];
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
