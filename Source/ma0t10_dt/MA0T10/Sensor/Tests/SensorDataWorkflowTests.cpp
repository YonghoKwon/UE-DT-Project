#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"

namespace
{
int32 CheckCount(const TSharedRef<SWidget>& Widget)
{
    int32 Count=Widget->GetType()==TEXT("SCheckBox")?1:0;
    auto* Children=Widget->GetChildren();
    for(int32 I=0;I<Children->Num();++I)Count+=CheckCount(Children->GetChildAt(I));
    return Count;
}
void VisibleTexts(const TSharedRef<SWidget>& Widget,TArray<FString>& Out)
{
    if(!Widget->GetVisibility().IsVisible())return;
    if(Widget->GetWidgetClass().GetWidgetType()==TEXT("STextBlock"))Out.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    auto* Children=Widget->GetChildren();
    for(int32 I=0;I<Children->Num();++I)VisibleTexts(Children->GetChildAt(I),Out);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorDataWorkflowTest,"MA0T10.UXR03.SingleOutputEditorAndDefaults",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorDataWorkflowTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* Slab=World->SpawnActor<ASlabActor>();
    auto* Data=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);auto* Replay=NewObject<USlabScenarioReplayPanelWidget>(World);
    W->RegisterOwnedPanel(ESensorToolPanelRole::Data,Data);W->RegisterOwnedPanel(ESensorToolPanelRole::Replay,Replay);
    Data->BindScenarioSlabActor(Slab);Data->Initialize();Replay->Initialize();
    const auto DataSlate=Data->TakeWidget();const auto ReplaySlate=Replay->TakeWidget();
    const auto Default=Data->GetLiveScenarioOutputs();
    TestTrue(TEXT("new scenarios retain PCD-only default"),Default.bPointCloud&&!Default.bCameraImage&&!Default.bLidarTelemetry);
    TestFalse(TEXT("archive replay retains observation-only default"),Replay->GetReplayOutputs().HasAnyOutput());
    TestEqual(TEXT("new policy unchanged"),Data->GetLiveScenarioExecutionOptions().Policy,ESlabExecutionPolicy::ObservationAllowed);
    TestEqual(TEXT("replay policy unchanged"),Replay->ExecutionOptions.Policy,ESlabExecutionPolicy::ObservationAllowed);
    TestEqual(TEXT("owned replay has zero output editing checkboxes"),CheckCount(ReplaySlate),0);
    TestEqual(TEXT("two columns each contain PCD, two additional outputs and policy"),CheckCount(Data->BuildOwnedLiveTab()),8);
    if(TestTrue(TEXT("real replay PCD control constructed"),Data->OwnedReplayPcdControl.IsValid()))
    {
        Data->OwnedReplayPcdControl->ToggleCheckedState();
        TestTrue(TEXT("actual control delegate changes the existing replay options"),Replay->GetReplayOutputs().bPointCloud);
        TestFalse(TEXT("replay control does not change live extras"),Data->GetLiveScenarioOutputs().bCameraImage);
    }
    FVirtualSlabSensorOutputSelection Extra;Extra.bCameraImage=true;Extra.bLidarTelemetry=true;
    TestTrue(TEXT("replay adapter delegates all outputs"),Data->SetOwnedReplayOutputs(Extra));
    FSlabExecutionOptions Options;Options.Policy=ESlabExecutionPolicy::RequireData;
    TestTrue(TEXT("replay execution adapter retains actual owner"),Data->SetOwnedReplayExecutionOptions(Options));
    const auto Summary=Replay->GetOwnedReplayOutputSummary().ToString();
    for(const FText& Label:{SensorToolUiText::PcdOutput(),SensorToolUiText::CameraOutput(),SensorToolUiText::LidarOutput(),SensorToolUiText::RequireConnection()})TestTrue(TEXT("summary agrees with options"),Summary.Contains(Label.ToString()));
    TestTrue(TEXT("summary link opens transfer replay column"),Replay->OpenOwnedReplayOutputSettings()&&W->IsPanelOpen(ESensorToolPanelRole::Data)&&Data->GetOwnedTab()==ESensorOwnedDataTab::Transfer&&Data->bOwnedReplayPreflight);
    Data->FocusOwnedOutputSettings(false);TestFalse(TEXT("live no-output fix returns to the live column"),Data->bOwnedReplayPreflight);
    Data->FocusOwnedOutputSettings(true);TestTrue(TEXT("replay no-output fix returns to the replay column"),Data->bOwnedReplayPreflight);
    for(const auto Tab:{ESensorOwnedDataTab::Transfer,ESensorOwnedDataTab::Results,ESensorOwnedDataTab::FileSave,ESensorOwnedDataTab::ServerConnection,ESensorOwnedDataTab::Diagnostics})
    {
        Data->OpenOwnedTab(Tab);TestEqual(TEXT("five distinct owned tabs"),Data->GetOwnedTab(),Tab);
        TestTrue(TEXT("saved legacy enum remains bounded"),uint8(Data->GetActiveTab())<=3);
    }
    Data->OpenOwnedTab(ESensorOwnedDataTab::Transfer);Data->CachedPcdPreflight=TEXT("detail sentinel");
    Data->CachedTransportLog=TEXT("log sentinel");Data->OwnedSelectionText=TEXT("direct selection sentinel");Data->WorkspaceStreamCards.Add(EVirtualSensorStreamKind::PointCloud,TEXT("direct card sentinel"));Data->TickOwnedUi(1);
    TestEqual(TEXT("collapsed direct transfer does not format selection"),Data->OwnedSelectionText,FString(TEXT("direct selection sentinel")));
    TestEqual(TEXT("collapsed direct transfer does not format stream cards"),Data->WorkspaceStreamCards.FindRef(EVirtualSensorStreamKind::PointCloud),FString(TEXT("direct card sentinel")));
    TestEqual(TEXT("collapsed preflight detail not reformatted"),Data->CachedPcdPreflight,FString(TEXT("detail sentinel")));
    TestTrue(TEXT("explicit Blueprint preflight getter retains detail access"),Data->GetPcdPreflightText().Contains(TEXT("PCD")));
    TestEqual(TEXT("inactive diagnostic tab not reformatted"),Data->CachedTransportLog,FString(TEXT("log sentinel")));
    Data->TickOwnedUi(1.1);TestEqual(TEXT("UI updates capped at five Hz"),Data->LastNativeStatusRefreshSeconds,1.0);
    Data->SetVisibility(ESlateVisibility::Hidden);Data->TickOwnedUi(2);TestEqual(TEXT("hidden panel stops updates"),Data->LastNativeStatusRefreshSeconds,1.0);
    Data->OwnedFileStatusText=TEXT("file sentinel");Data->OwnedFileRequests.Add(TEXT("event-fixture"));
    FVirtualSensorFileSaveStatus FileEvent;FileEvent.RequestId=TEXT("event-fixture");FileEvent.State=EVirtualSensorFileSaveState::Saving;FileEvent.Message=TEXT("writing");
    Data->HandleFileSaveUpdated(FileEvent);
    TestEqual(TEXT("file events do not format a hidden file tab"),Data->OwnedFileStatusText,FString(TEXT("file sentinel")));
    TestEqual(TEXT("file events do not bypass UI rate limit"),Data->LastNativeStatusRefreshSeconds,1.0);
    Data->SetVisibility(ESlateVisibility::Visible);Data->OpenOwnedTab(ESensorOwnedDataTab::Transfer);Data->TickOwnedUi(3);
    // A standalone widget has no Slate window to evaluate the switcher attributes.
    // Inspect the actual transfer subtree; the opt-in PIE render covers visibility.
    const auto TransferSlate=Data->BuildOwnedLiveTab();TransferSlate->SlatePrepass(1);
    TArray<FString> Texts;VisibleTexts(TransferSlate,Texts);
    TestFalse(TEXT("transfer vocabulary scan is nonempty"),Texts.IsEmpty());
    TestTrue(TEXT("preflight choices identify what is selected"),Texts.Contains(SensorToolUiText::PreflightTarget().ToString()));
    TestTrue(TEXT("direct transfer remains discoverable while collapsed"),Texts.Contains(SensorToolUiText::DirectTransfer().ToString()));
    for(const auto& Text:Texts)for(const TCHAR* Word:{TEXT("Payload"),TEXT("Topic"),TEXT("receipt"),TEXT("Raw TCP"),TEXT("coherent"),TEXT("revision"),TEXT("p95"),TEXT("STOMP")})TestFalse(TEXT("default transfer has no developer vocabulary"),Text.Contains(Word,ESearchCase::IgnoreCase));
    auto* Legacy=NewObject<USlabScenarioReplayPanelWidget>(World);Legacy->Initialize();TestEqual(TEXT("native legacy replay retains all output controls"),CheckCount(Legacy->TakeWidget()),4);
    TestNotNull(TEXT("existing replay setter remains reflected"),Replay->FindFunction(TEXT("SetReplayOutputs")));
    for(const TCHAR* Api:{TEXT("SelectScenarioComparisonRun"),TEXT("GetScenarioRunComparisonText"),TEXT("RetrySelectedScenarioRunReport")})TestNotNull(TEXT("result APIs remain reflected"),Data->FindFunction(FName(Api)));
    W->UnregisterPanel(Data);W->UnregisterPanel(Replay);Slab->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorDataPreflightFormattingTest,"MA0T10.UXR03.PreflightDetailBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorDataPreflightFormattingTest::RunTest(const FString&)
{
    TestEqual(TEXT("technical transport blocker has an actionable user sentence"),SensorToolUiText::PreflightReason(TEXT("실시간 Topic 송신에는 STOMP 전송 모드가 필요합니다.")),SensorToolUiText::PcdServerRequired().ToString());
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();FVirtualSlabSensorOutputSelection Outputs;
    const auto Full=UVirtualSensorOutputPreflight::InspectForPurpose(World,Outputs,{},EVirtualSensorPreflightPurpose::ScenarioTransition);
    const auto Compact=UVirtualSensorOutputPreflight::InspectForPresentation(World,Outputs,{},EVirtualSensorPreflightPurpose::ScenarioTransition,false);
    TestTrue(TEXT("collapsed detail has no formatted summary"),Compact.Summary.IsEmpty());
    TestFalse(TEXT("legacy inspection retains formatted summary"),Full.Summary.IsEmpty());
    TestTrue(TEXT("blocking and warning decisions identical"),Compact.BlockingReasons==Full.BlockingReasons&&Compact.Warnings==Full.Warnings);
    TestEqual(TEXT("capacity identical"),Compact.TheoreticalSubmissionHz,Full.TheoreticalSubmissionHz);
    return true;
}
#endif
