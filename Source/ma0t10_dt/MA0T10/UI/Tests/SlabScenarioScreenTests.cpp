#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Widgets/Layout/SExpandableArea.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioScreenTest,"MA0T10.UXR04.ScenarioScreenAndChartToggle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioScreenTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* Slab=World->SpawnActor<ASlabActor>();auto* Progress=NewObject<USlabProgressPanelWidget>(World);auto* Charts=NewObject<USlabChartsPanelWidget>(World);
    W->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress,Progress);W->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts,Charts);
    Progress->BindSlabActor(Slab);Progress->Initialize();const auto Slate=Progress->TakeWidget();Slate->SlatePrepass(1);
    TestTrue(TEXT("display settings are one initially collapsed area"),Progress->OwnedDisplayArea.IsValid()&&!Progress->OwnedDisplayArea->IsExpanded());
    W->SetPanelOpen(ESensorToolPanelRole::SlabCharts,false);
    TestTrue(TEXT("chart button opens the existing registered panel"),Progress->ToggleOwnedCharts()&&W->IsPanelOpen(ESensorToolPanelRole::SlabCharts));
    TestTrue(TEXT("same chart button hides it on the second click"),Progress->ToggleOwnedCharts()&&!W->IsPanelOpen(ESensorToolPanelRole::SlabCharts));
    TestEqual(TEXT("chart toggle has no scenario side effect"),Slab->GetSimulationStatus().State,ESlabSimulationState::Idle);
    Progress->Detail=TEXT("hidden detail sentinel");Progress->RefreshOwnedStatus();
    TestEqual(TEXT("collapsed run details are not assembled"),Progress->Detail,FString(TEXT("hidden detail sentinel")));
    Progress->bOwnedDetailsExpanded=true;Progress->RefreshOwnedStatus();TestTrue(TEXT("expanded details remain available"),Progress->Detail.Contains(TEXT("실행 UUID")));
    Progress->SetVisibility(ESlateVisibility::Hidden);Progress->Summary=TEXT("hidden summary sentinel");Progress->NativeTick(FGeometry(),.3f);
    TestEqual(TEXT("hidden scenario panel stops display updates"),Progress->Summary,FString(TEXT("hidden summary sentinel")));
    auto* Replay=NewObject<USlabScenarioReplayPanelWidget>(World);TestEqual(TEXT("default output summary does not duplicate observation policy"),Replay->GetOwnedReplayOutputSummary().ToString(),FString(TEXT("이번 재생 출력: 관찰 전용")));
    TestFalse(TEXT("screen changes do not enable replay output"),Replay->GetReplayOutputs().HasAnyOutput());
    W->UnregisterPanel(Progress);W->UnregisterPanel(Charts);Slab->Destroy();return true;
}
#endif
