#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
void FindTexts(const TSharedRef<SWidget>& Widget,TArray<FString>& Texts)
{
    if(Widget->GetType()==TEXT("STextBlock"))Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)FindTexts(Children->GetChildAt(I),Texts);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkflowToolbarTest,"MA0T10.UXR01.ToolbarRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkflowToolbarTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* C=World->SpawnActor<AVirtualSensorCoordinator>();W->SetCoordinator(C);
    auto* L=World->SpawnActor<AVirtualLidarSensorActor>();auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();
    C->RegisterSensorActor(L);C->RegisterSensorActor(Camera);
    auto* Placement=NewObject<UVirtualSensorPlacementPanelWidget>(World);auto* Monitor=NewObject<UVirtualSensorMonitorPanelWidget>(World);
    auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);auto* Data=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);
    auto* Progress=NewObject<USlabProgressPanelWidget>(World);auto* Replay=NewObject<USlabScenarioReplayPanelWidget>(World);auto* Charts=NewObject<USlabChartsPanelWidget>(World);
    W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,Placement);W->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Monitor);
    W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings);W->RegisterOwnedPanel(ESensorToolPanelRole::Data,Data);
    W->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress,Progress);W->RegisterOwnedPanel(ESensorToolPanelRole::Replay,Replay);W->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts,Charts);
    auto* Toolbar=NewObject<USensorToolToolbarWidget>(World);W->Toolbar=Toolbar;
    Settings->BindSensorManager(C);Data->BindSensorManager(C);Data->Initialize();const auto DataSlate=Data->TakeWidget();Toolbar->Initialize();
    const auto ToolbarSlate=Toolbar->TakeWidget();TArray<FString> BarTexts;FindTexts(ToolbarSlate,BarTexts);
    for(const FText& Text:{SensorToolUiText::Placement(),SensorToolUiText::Preview(),SensorToolUiText::Scenario(),SensorToolUiText::PcdTransfer(),SensorToolUiText::Results(),SensorToolUiText::Tools()})
        TestEqual(TEXT("each workflow entry appears once"),BarTexts.FilterByPredicate([&](const auto& S){return S==Text.ToString();}).Num(),1);
    TestTrue(TEXT("placement route"),Toolbar->OpenOwnedDestination(ESensorToolDestination::Placement)&&W->IsPanelOpen(ESensorToolPanelRole::Placement));
    TestTrue(TEXT("preview route"),Toolbar->OpenOwnedDestination(ESensorToolDestination::Preview)&&W->IsPanelOpen(ESensorToolPanelRole::Monitor));
    TestTrue(TEXT("scenario opens progress and archive"),Toolbar->OpenOwnedDestination(ESensorToolDestination::Scenario)&&W->IsPanelOpen(ESensorToolPanelRole::SlabProgress)&&W->IsPanelOpen(ESensorToolPanelRole::Replay));
    TestTrue(TEXT("pcd route"),Toolbar->OpenOwnedDestination(ESensorToolDestination::PcdTransfer));
    TestEqual(TEXT("pcd opens live screen"),Data->GetActiveTab(),EVirtualSensorCaptureExportTab::LiveStream);
    TestTrue(TEXT("same visible screen toggles hidden"),Toolbar->ToggleOwnedDestination(ESensorToolDestination::PcdTransfer)&&!W->IsPanelOpen(ESensorToolPanelRole::Data));
    TestTrue(TEXT("hidden screen toggles visible"),Toolbar->ToggleOwnedDestination(ESensorToolDestination::PcdTransfer)&&W->IsPanelOpen(ESensorToolPanelRole::Data));
    TestTrue(TEXT("other tab switches without hiding"),Toolbar->ToggleOwnedDestination(ESensorToolDestination::Results)&&W->IsPanelOpen(ESensorToolPanelRole::Data)&&Data->GetActiveTab()==EVirtualSensorCaptureExportTab::ConnectionLog);
    TestTrue(TEXT("results route"),Toolbar->OpenOwnedDestination(ESensorToolDestination::Results));
    TestEqual(TEXT("results opens connection screen"),Data->GetActiveTab(),EVirtualSensorCaptureExportTab::ConnectionLog);
    TestTrue(TEXT("results expands current run results"),Data->bRunResultsExpanded);
    TestTrue(TEXT("file route"),Toolbar->OpenOwnedDestination(ESensorToolDestination::FileSave));
    TestEqual(TEXT("file opens existing capture/file screen"),Data->GetActiveTab(),EVirtualSensorCaptureExportTab::Capture);
    Toolbar->OpenOwnedDestination(ESensorToolDestination::ServerSettings);
    TestTrue(TEXT("server expands settings not results"),Data->bOwnedServerExpanded&&!Data->bRunResultsExpanded&&!Data->bOwnedReceiverExpanded);
    Toolbar->OpenOwnedDestination(ESensorToolDestination::Diagnostics);
    TestTrue(TEXT("diagnostics routes to its independent tab"),Data->GetOwnedTab()==ESensorOwnedDataTab::Diagnostics&&!Data->bOwnedServerExpanded);
    Toolbar->OpenOwnedDestination(ESensorToolDestination::Compatibility);
    TestEqual(TEXT("compatibility opens existing advanced settings"),Settings->WorkspaceSettingsTab,2);
    Toolbar->OpenOwnedDestination(ESensorToolDestination::SensorSettings);
    TestEqual(TEXT("detail settings opens basic screen"),Settings->WorkspaceSettingsTab,0);
    TestTrue(TEXT("chart stays reachable before UXR-04"),Toolbar->OpenOwnedDestination(ESensorToolDestination::ScenarioCharts)&&W->IsPanelOpen(ESensorToolPanelRole::SlabCharts));
    TArray<FString> MenuTexts;FindTexts(Toolbar->BuildToolsMenu(),MenuTexts);
    for(const FText& Text:{SensorToolUiText::SensorSettings(),SensorToolUiText::FileSave(),SensorToolUiText::ServerSettings(),SensorToolUiText::Diagnostics(),SensorToolUiText::Compatibility(),SensorToolUiText::ScenarioCharts(),SensorToolUiText::Appearance()})
        TestTrue(TEXT("tool menu contains named destination"),MenuTexts.Contains(Text.ToString()));
    Toolbar->SelectSensor(L->GetSensorId());
    TestEqual(TEXT("owned selector synchronizes settings"),Settings->GetPendingState().SensorId,L->GetSensorId());
    TestTrue(TEXT("equipment label includes kind id and state"),Toolbar->SelectedSensorText().ToString().Contains(TEXT("LiDAR"))&&Toolbar->SelectedSensorText().ToString().Contains(TEXT("정지")));
    TestFalse(TEXT("equipment label has no redundant prefix"),Toolbar->SelectedSensorText().ToString().StartsWith(TEXT("장비")));
    TArray<FString> EquipmentTexts;FindTexts(Toolbar->BuildEquipmentMenu(),EquipmentTexts);
    TestTrue(TEXT("selector preserves kind groups and placement entry"),EquipmentTexts.Contains(TEXT("LiDAR"))&&EquipmentTexts.Contains(TEXT("Camera"))&&EquipmentTexts.Contains(SensorToolUiText::AddEquipment().ToString()));
    W->Toolbar=nullptr;
    for(const auto Role:{ESensorToolPanelRole::Placement,ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabProgress,ESensorToolPanelRole::SlabCharts})W->UnregisterPanel(W->GetOwnedPanel(Role));
    C->Destroy();L->Destroy();Camera->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkflowStatusTest,"MA0T10.UXR01.ReadOnlyStatus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkflowStatusTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* C=World->SpawnActor<AVirtualSensorCoordinator>();W->SetCoordinator(C);auto* L=World->SpawnActor<AVirtualLidarSensorActor>();
    C->RegisterSensorActor(L);auto* Toolbar=NewObject<USensorToolToolbarWidget>(World);W->Toolbar=Toolbar;
    const auto Transform=L->GetActorTransform();const auto Before=L->GetSensorRuntimeStatus();const bool Running=L->IsSensorRunning();
    const auto Streams=C->StreamPublisherComponent->GetStreamStatuses();const int32 Revision=L->GetConfigurationRevision();
    TArray<uint8> SaveBefore,SaveAfter;const bool HadSave=UGameplayStatics::LoadDataFromSlot(SaveBefore,UVirtualSensorUiPreferencesSaveGame::SlotName,0);
    Toolbar->RefreshOwnedStatus(1.0);const double Next=Toolbar->NextStatusRefresh;
    Toolbar->RefreshOwnedStatus(1.1);TestEqual(TEXT("status throttles to at most five hz"),Toolbar->NextStatusRefresh,Next);
    for(int32 I=0;I<100;++I)Toolbar->RefreshOwnedStatus(2.0+I*.2);
    TestEqual(TEXT("read-only device count"),Toolbar->GlobalStatus.EquipmentCount,1);
    TestEqual(TEXT("measurement running state untouched"),L->IsSensorRunning(),Running);
    TestTrue(TEXT("pose untouched"),Transform.Equals(L->GetActorTransform()));
    TestEqual(TEXT("settings revision untouched"),L->GetConfigurationRevision(),Revision);
    TestEqual(TEXT("no acquisition caused by display"),L->GetSensorRuntimeStatus().FrameId,Before.FrameId);
    TestEqual(TEXT("no derived output caused by display"),L->GetSensorRuntimeStatus().LastPayloadLength,Before.LastPayloadLength);
    TestEqual(TEXT("stream inventory untouched"),C->StreamPublisherComponent->GetStreamStatuses().Num(),Streams.Num());
    const bool HasSave=UGameplayStatics::LoadDataFromSlot(SaveAfter,UVirtualSensorUiPreferencesSaveGame::SlotName,0);
    TestTrue(TEXT("status does not save preferences"),HadSave==HasSave&&SaveBefore==SaveAfter);
    const double LastRefresh=Toolbar->NextStatusRefresh;
    W->Toolbar=nullptr;Toolbar->RefreshOwnedStatus(1000);TestEqual(TEXT("non-owned status is not updated"),Toolbar->NextStatusRefresh,LastRefresh);
    C->Destroy();L->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkflowLegacyTest,"MA0T10.UXR01.LegacyToolbar",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkflowLegacyTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Toolbar=NewObject<USensorToolToolbarWidget>(World);
    Toolbar->Initialize();TArray<FString> Texts;FindTexts(Toolbar->TakeWidget(),Texts);
    for(const TCHAR* Label:{TEXT("센서 없음"),TEXT("모니터"),TEXT("설정"),TEXT("데이터"),TEXT("재생"),TEXT("Slab 차트"),TEXT("Slab 진행"),TEXT("UI 표시")})
        TestTrue(TEXT("legacy entry unchanged"),Texts.Contains(Label));
    TestFalse(TEXT("legacy cannot use owned navigation"),Toolbar->OpenOwnedDestination(ESensorToolDestination::PcdTransfer));
    TestTrue(TEXT("legacy has no global status strip"),Toolbar->GetGlobalStatus().Equipment.IsEmpty());
    return true;
}
#endif
