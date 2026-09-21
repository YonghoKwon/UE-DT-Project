#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"

namespace
{
int32 CountVisibleCheckboxesAfterPrepass(const TSharedRef<SWidget>& Widget)
{
	if(!Widget->GetVisibility().IsVisible()) return 0;
	int32 Count=Widget->GetType()==FName(TEXT("SCheckBox"))&&Widget->GetVisibility().IsVisible()?1:0;
	if(auto* Children=Widget->GetChildren()) for(int32 I=0;I<Children->Num();++I) Count+=CountVisibleCheckboxesAfterPrepass(Children->GetChildAt(I));
	return Count;
}
int32 CountVisibleCheckboxes(const TSharedRef<SWidget>& Widget)
{
	// UE 5.3 caches registered visibility attributes. A newly constructed widget's
	// GetVisibility() is not evidence of what Slate will arrange until its prepass.
	Widget->Invalidate(EInvalidateWidgetReason::Prepass);
	Widget->SlatePrepass(1.0f);
	return CountVisibleCheckboxesAfterPrepass(Widget);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabUiSimplificationTest,"MA0T10.SensorUi.SlabOwnedSimplification",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabUiSimplificationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap(); auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Replay=NewObject<USlabScenarioReplayPanelWidget>(World); Replay->SelectedUUID=TEXT("selected");
	TestTrue(TEXT("unregistered replay retains actions for all rows"),Replay->AreRowActionsVisible(TEXT("other")));
	TestTrue(TEXT("replay explicitly registered"),Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Replay,Replay));
	TestTrue(TEXT("owned selected row exposes actions"),Replay->AreRowActionsVisible(TEXT("selected")));
	TestFalse(TEXT("owned nonselected row hides repeated actions"),Replay->AreRowActionsVisible(TEXT("other")));
	TestFalse(TEXT("replay remains observation-only by default"),Replay->GetReplayOutputs().HasAnyOutput());
	auto Additional=StaticCastSharedRef<SExpandableArea>(Replay->BuildAdditionalOutputOptions(true));
	TestFalse(TEXT("Camera and LiDAR extra options start collapsed"),Additional->IsExpanded());
	TestEqual(TEXT("legacy output options retain both checkboxes"),CountVisibleCheckboxes(Replay->BuildAdditionalOutputOptions(false)),2);
	Replay->WidgetTree=NewObject<UWidgetTree>(Replay); Replay->WidgetTree->RootWidget=Replay->WidgetTree->ConstructWidget<UCanvasPanel>();
	TestFalse(TEXT("designer replay does not opt into native rearrangement"),Replay->UsesSimplifiedNativeLayout());
	TestTrue(TEXT("designer replay preserves row actions behavior"),Replay->AreRowActionsVisible(TEXT("other")));
	Workspace->UnregisterPanel(Replay);

	auto* Charts=NewObject<USlabChartsPanelWidget>(World); Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts,Charts); Charts->InitializeCharts(true);
	TestTrue(TEXT("owned native charts enable compact configuration"),Charts->UsesSimplifiedNativeLayout());
	Charts->SetChartSlotMetric(0,ESlabChartMetric::Angles);
	const auto OwnedControls=Charts->BuildChartControls(0,true);
	TestEqual(TEXT("paired metric exposes two selectors only inside config"),CountVisibleCheckboxes(OwnedControls),2);
	Charts->bShowSeries[0][0]=false;
	Charts->SetChartSlotMetric(0,ESlabChartMetric::CenterOffset);
	TestFalse(TEXT("single series has no redundant display selector"),Charts->AreSeriesSelectorsVisible(0));
	TestEqual(TEXT("same live control tree hides single-series checkboxes after Slate prepass"),CountVisibleCheckboxes(OwnedControls),0);
	TestFalse(TEXT("switching to single series preserves stored dual preference"),Charts->bShowSeries[0][0]);
	Charts->SetChartSlotMetric(0,ESlabChartMetric::RailMargins);
	TestTrue(TEXT("return to paired metric restores selectors"),Charts->AreSeriesSelectorsVisible(0));
	TestEqual(TEXT("same live control tree restores both paired-series checkboxes"),CountVisibleCheckboxes(OwnedControls),2);
	TestFalse(TEXT("dual preference was not silently reset"),Charts->bShowSeries[0][0]);
	for(int32 I=0;I<3;++I) TestNotNull(TEXT("three chart instances retained"),Charts->GetChartWidget(I));
	Charts->WidgetTree=NewObject<UWidgetTree>(Charts); Charts->WidgetTree->RootWidget=Charts->WidgetTree->ConstructWidget<UCanvasPanel>();
	TestFalse(TEXT("designer charts preserve their layout"),Charts->UsesSimplifiedNativeLayout());
	Charts->SetChartSlotMetric(0,ESlabChartMetric::CenterOffset);
	TestEqual(TEXT("legacy/native-unowned single-series checkbox behavior retained"),CountVisibleCheckboxes(Charts->BuildChartControls(0,false)),1);
	Workspace->UnregisterPanel(Charts);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabDiagnosticMasterTest,"MA0T10.SensorUi.SlabDiagnosticMasterPreservesFlags",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabDiagnosticMasterTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap(); auto* Slab=World->SpawnActor<ASlabActor>();
	FSlabAnalysisDisplaySettings S; S.bOutline=false;S.bCross=true;S.bReferencePose=false;S.bCenterline=true;S.bYaw=false;S.bMargins=true;S.bStatus=false;
	Slab->SetAnalysisDisplaySettings(S);
	TestTrue(TEXT("master initially enabled"),Slab->GetDiagnosticHelpersVisible());
	auto* Visual=Slab->FindComponentByClass<USlabVisualizationComponent>(); Visual->UpdateGeometry(FVector(1083,110,25));
	const int32 HelperCount=Visual->GetOwnedHelperCount();
	TestEqual(TEXT("bounded analysis helper and background pool created once"),HelperCount,USlabVisualizationComponent::MaxLineHelpers+USlabVisualizationComponent::MaxTextHelpers+USlabVisualizationComponent::MaxLabelBackgrounds);
	Slab->SetDiagnosticHelpersVisible(false); TestFalse(TEXT("master disabled through Actor API"),Slab->GetDiagnosticHelpersVisible());
	S.bYaw=true; Slab->SetAnalysisDisplaySettings(S);
	TestFalse(TEXT("editing a detail cannot override disabled master"),Slab->GetDiagnosticHelpersVisible());
	Slab->SetDiagnosticHelpersVisible(true); TestTrue(TEXT("master restored"),Slab->GetDiagnosticHelpersVisible());
	const auto Restored=Slab->GetAnalysisDisplaySettings();
	TestEqual(TEXT("outline preference retained"),Restored.bOutline,S.bOutline); TestEqual(TEXT("cross preference retained"),Restored.bCross,S.bCross);
	TestEqual(TEXT("reference preference retained"),Restored.bReferencePose,S.bReferencePose); TestEqual(TEXT("centerline preference retained"),Restored.bCenterline,S.bCenterline);
	TestEqual(TEXT("yaw preference retained"),Restored.bYaw,S.bYaw); TestEqual(TEXT("margin preference retained"),Restored.bMargins,S.bMargins); TestEqual(TEXT("status preference retained"),Restored.bStatus,S.bStatus);
	TestEqual(TEXT("master toggles do not allocate more render helpers"),Visual->GetOwnedHelperCount(),HelperCount);
	Slab->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabProgressOutcomeTest,"MA0T10.SensorUi.SlabProgressHonestOutcome",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabProgressOutcomeTest::RunTest(const FString&)
{
	FSlabSimulationStatus Simulation;Simulation.State=ESlabSimulationState::Completed;Simulation.RunUUID=TEXT("run-a");Simulation.DurationSec=30;Simulation.ElapsedSec=21.4;
	FVirtualSlabSessionStatus Session;Session.RunId=Simulation.RunUUID;Session.State=EVirtualSlabSessionState::Draining;Session.bAborted=true;
	TestEqual(TEXT("early stop while draining is not normal completion"),USlabProgressPanelWidget::ResolveOwnedMovementState(Simulation,Session).ToString(),FString(TEXT("중단됨")));
	Session.State=EVirtualSlabSessionState::Completed;Session.EndReason=EVirtualSlabSessionEndReason::Aborted;
	TestEqual(TEXT("drained stop keeps stopped label"),USlabProgressPanelWidget::ResolveOwnedMovementState(Simulation,Session).ToString(),FString(TEXT("중단됨")));
	Session.RunId=TEXT("another-run");
	TestEqual(TEXT("new session cannot turn an earlier partial run into completed"),USlabProgressPanelWidget::ResolveOwnedMovementState(Simulation,Session).ToString(),FString(TEXT("중단됨")));
	Simulation.ElapsedSec=30;
	TestEqual(TEXT("unrelated abort cannot change complete movement"),USlabProgressPanelWidget::ResolveOwnedMovementState(Simulation,Session).ToString(),FString(TEXT("움직임 완료")));
	Session.RunId=Simulation.RunUUID;
	TestEqual(TEXT("explicit abort at end still reported honestly"),USlabProgressPanelWidget::ResolveOwnedMovementState(Simulation,Session).ToString(),FString(TEXT("중단됨")));
	Session.bAborted=false;Session.State=EVirtualSlabSessionState::Incomplete;Session.EndReason=EVirtualSlabSessionEndReason::StreamFailure;
	TestEqual(TEXT("network failure is separate from completed movement"),USlabProgressPanelWidget::ResolveOwnedMovementState(Simulation,Session).ToString(),FString(TEXT("움직임 완료")));
	Simulation.State=ESlabSimulationState::Failed;
	TestEqual(TEXT("actual movement failure retains priority"),USlabProgressPanelWidget::ResolveOwnedMovementState(Simulation,Session).ToString(),FString(TEXT("실패")));
	return true;
}
#endif
