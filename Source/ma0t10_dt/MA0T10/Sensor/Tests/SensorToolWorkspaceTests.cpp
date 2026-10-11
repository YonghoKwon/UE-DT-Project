#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorWorkspaceForeignFixture.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "Components/TextBlock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkspaceIsolationTest,"MA0T10.SensorWorkspace.IsolationAndStorage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkspaceIsolationTest::RunTest(const FString&)
{
    // ARCH-03: exact legacy literals stay independent of the implementation constants.
    struct FDefaultCase { const TCHAR* Name; const TCHAR* Actual; const TCHAR* Expected; };
    const FDefaultCase Defaults[] = {
        { TEXT("CameraTopic"), VirtualSensorDefaults::CameraTopic, TEXT("topic.virtual.sensor.camera.0") },
        { TEXT("LidarTopic"), VirtualSensorDefaults::LidarTopic, TEXT("topic.virtual.sensor.lidar.0") },
        { TEXT("PointCloudTopic"), VirtualSensorDefaults::PointCloudTopic, TEXT("topic.virtual.sensor.export.0") },
        { TEXT("UiSlotV1"), VirtualSensorDefaults::UiSlotV1, TEXT("MA0T10_VirtualSensorUI_v1") },
        { TEXT("UiSlotV2"), VirtualSensorDefaults::UiSlotV2, TEXT("MA0T10_VirtualSensorUI_v2") },
        { TEXT("UiSlotV3"), VirtualSensorDefaults::UiSlotV3, TEXT("MA0T10_VirtualSensorUI_v3") },
        { TEXT("UiSlotV4"), VirtualSensorDefaults::UiSlotV4, TEXT("MA0T10_VirtualSensorUI_v4") },
        { TEXT("UiSlotV5"), VirtualSensorDefaults::UiSlotV5, TEXT("MA0T10_VirtualSensorUI_v5") },
        { TEXT("UiSlotV6"), VirtualSensorDefaults::UiSlotV6, TEXT("MA0T10_VirtualSensorUI_v6") },
        { TEXT("WorkspaceSlot"), VirtualSensorDefaults::WorkspaceSlot, TEXT("MA0T10_SensorToolWorkspace_v1") },
        { TEXT("AppearanceSlot"), VirtualSensorDefaults::AppearanceSlot, TEXT("MA0T10_SensorToolAppearance_v1") },
        { TEXT("CaptureDirectory"), VirtualSensorDefaults::CaptureDirectory, TEXT("SensorCaptures") },
        { TEXT("RecordingDirectory"), VirtualSensorDefaults::RecordingDirectory, TEXT("SensorRecordings") },
        { TEXT("PlacementDirectory"), VirtualSensorDefaults::PlacementDirectory, TEXT("SensorPlacements") },
        { TEXT("LayoutDirectory"), VirtualSensorDefaults::LayoutDirectory, TEXT("SensorLayouts") },
        { TEXT("StreamTempDirectory"), VirtualSensorDefaults::StreamTempDirectory, TEXT("SensorStreamTemp") },
        { TEXT("ReportsDirectory"), VirtualSensorDefaults::ReportsDirectory, TEXT("Reports") },
        { TEXT("ScenarioRunReportsDirectory"), VirtualSensorDefaults::ScenarioRunReportsDirectory, TEXT("Reports/ScenarioRuns") },
        { TEXT("TimedCaptureDirectory"), VirtualSensorDefaults::TimedCaptureDirectory, TEXT("SensorCaptures/LocalTimedCapture") },
        { TEXT("TimedCaptureAutomationDirectory"), VirtualSensorDefaults::TimedCaptureAutomationDirectory, TEXT("SensorCaptures/LocalTimedCapture/Automation") },
        { TEXT("PointCloudSubdirectory"), VirtualSensorDefaults::PointCloudSubdirectory, TEXT("PointCloud") },
        { TEXT("ServerPayloadSubdirectory"), VirtualSensorDefaults::ServerPayloadSubdirectory, TEXT("ServerPayload") },
    };
    for (const FDefaultCase& Item : Defaults)
        TestEqual(FString::Printf(TEXT("literal default %s"), Item.Name), FString(Item.Actual), FString(Item.Expected));

    USensorToolWorkspaceSaveGame* Saved = NewObject<USensorToolWorkspaceSaveGame>();
    TestEqual(TEXT("workspace schema stays v1"), Saved->Version, 1);
    Saved->FontScale = 1.25f;
    FSensorToolWorkspacePanelState& PanelState = Saved->Panels.Add(ESensorToolPanelRole::Settings);
    PanelState.bOpen = true;
    PanelState.Layout.NormalizedPosition = FVector2D(0.2f, 0.3f);
    PanelState.Layout.bHasSavedPosition = true;
    PanelState.Layout.ExpandedSize = FVector2D(520.0f, 600.0f);
    PanelState.Layout.bHasSavedSize = true;
    TArray<uint8> WorkspaceBytes;
    TestTrue(TEXT("workspace v1 serializes to memory"), UGameplayStatics::SaveGameToMemory(Saved, WorkspaceBytes));
    USensorToolWorkspaceSaveGame* Reloaded = Cast<USensorToolWorkspaceSaveGame>(UGameplayStatics::LoadGameFromMemory(WorkspaceBytes));
    TestNotNull(TEXT("workspace v1 deserializes from memory"), Reloaded);
    if (!Reloaded) return false;
    TestEqual(TEXT("workspace version survives roundtrip"), Reloaded->Version, 1);
    TestEqual(TEXT("workspace scale survives roundtrip"), Reloaded->FontScale, 1.25f);
    const FSensorToolWorkspacePanelState* ReloadedPanel = Reloaded->Panels.Find(ESensorToolPanelRole::Settings);
    TestNotNull(TEXT("workspace panel survives roundtrip"), ReloadedPanel);
    if (!ReloadedPanel) return false;
    TestTrue(TEXT("workspace open state survives roundtrip"), ReloadedPanel->bOpen);
    TestTrue(TEXT("workspace saved flags survive roundtrip"), ReloadedPanel->Layout.bHasSavedPosition && ReloadedPanel->Layout.bHasSavedSize);
    TestEqual(TEXT("workspace position survives roundtrip"), ReloadedPanel->Layout.NormalizedPosition, FVector2D(0.2f, 0.3f));
    TestEqual(TEXT("workspace size survives roundtrip"), ReloadedPanel->Layout.ExpandedSize, FVector2D(520.0f, 600.0f));
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Foreign=NewObject<USensorWorkspaceForeignFixture>(World);
	auto* Monitor=NewObject<UVirtualSensorMonitorPanelWidget>(World);
	auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);
	auto* Placement=NewObject<UVirtualSensorPlacementPanelWidget>(World);
	auto* ForeignText=NewObject<UTextBlock>(Foreign);ForeignText->SetText(FText::FromString(TEXT("동료 차트")));
	const auto OriginalFont=ForeignText->GetFont();const auto OriginalSize=Foreign->GetEffectivePanelSize();const auto OriginalPosition=Foreign->GetCurrentPanelPosition();
	TArray<uint8> Before,After,WorkspaceBefore;
	const bool HadLegacy=UGameplayStatics::LoadDataFromSlot(Before,UVirtualSensorUiPreferencesSaveGame::SlotName,0);
	const bool HadWorkspace=UGameplayStatics::LoadDataFromSlot(WorkspaceBefore,UVirtualSensorToolWorkspaceSubsystem::SlotName,0);
	TestFalse(TEXT("common parent alone cannot register"),W->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Foreign));
	TestTrue(TEXT("explicit monitor registration"),W->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Monitor));
	TestTrue(TEXT("explicit settings registration"),W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings));
	TestTrue(TEXT("placement appended without renumbering existing roles"),int32(ESensorToolPanelRole::SlabProgress)==5&&int32(ESensorToolPanelRole::Placement)==6);
	TestTrue(TEXT("explicit placement registration"),W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,Placement));
	TestFalse(TEXT("wrong role rejected"),W->RegisterOwnedPanel(ESensorToolPanelRole::Data,Settings));
	Monitor->RegisterSensorTextControl(ForeignText); // must not cross UserWidget ownership
	W->SetOwnedPanelFontScale(1.5f);
	TestEqual(TEXT("foreign nested text font unchanged"),ForeignText->GetFont().Size,OriginalFont.Size);
	TestEqual(TEXT("foreign base scale unchanged"),Foreign->GetSensorToolFontScale(),1.0f);
	TestEqual(TEXT("owned scale applied"),Monitor->GetSensorToolFontScale(),1.5f);
	TestEqual(TEXT("new owned placement receives font scale"),Placement->GetSensorToolFontScale(),1.5f);
	W->SetPanelOpen(ESensorToolPanelRole::Settings,false);
	TestTrue(TEXT("hide keeps same instance"),W->GetOwnedPanel(ESensorToolPanelRole::Settings)==Settings);
	W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
	W->ResetOwnedWorkspaceLayout();
	TestTrue(TEXT("monitor is default open"),W->IsPanelOpen(ESensorToolPanelRole::Monitor));
	TestFalse(TEXT("tools default closed"),W->IsPanelOpen(ESensorToolPanelRole::Settings));
	TestEqual(TEXT("foreign geometry size unchanged"),Foreign->GetEffectivePanelSize(),OriginalSize);
	TestEqual(TEXT("foreign geometry position unchanged"),Foreign->GetCurrentPanelPosition(),OriginalPosition);
	TestEqual(TEXT("foreign text unchanged"),ForeignText->GetText().ToString(),FString(TEXT("동료 차트")));
	const bool HasLegacy=UGameplayStatics::LoadDataFromSlot(After,UVirtualSensorUiPreferencesSaveGame::SlotName,0);TestTrue(TEXT("legacy SaveGame bytes untouched"),Before==After&&HadLegacy==HasLegacy);
	W->UnregisterPanel(Monitor);W->UnregisterPanel(Settings);W->UnregisterPanel(Placement);
	if(HadWorkspace)UGameplayStatics::SaveDataToSlot(WorkspaceBefore,UVirtualSensorToolWorkspaceSubsystem::SlotName,0);else UGameplayStatics::DeleteGameInSlot(UVirtualSensorToolWorkspaceSubsystem::SlotName,0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkspaceSelectionSyncTest,"MA0T10.SensorWorkspace.SelectionSync",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkspaceSelectionSyncTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();
	auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();
	auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();
	Manager->RegisterSensorActor(Camera);Manager->RegisterSensorActor(Lidar);W->SetCoordinator(Manager);
	auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);
	W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings);Settings->BindSensorManager(Manager);
	auto* Toolbar=NewObject<USensorToolToolbarWidget>(World);Toolbar->RefreshSensors();
	TestTrue(TEXT("selector includes LiDAR"),Toolbar->SensorOptions.ContainsByPredicate([&](const auto& Id){return *Id==Lidar->GetSensorId();}));
	Toolbar->SelectSensor(Lidar->GetSensorId());
	TestTrue(TEXT("toolbar LiDAR updates settings kind immediately"),Settings->GetPendingState().TargetKind==EVirtualSensorTargetKind::Lidar);
	TestEqual(TEXT("toolbar LiDAR updates settings ID"),Settings->GetPendingState().SensorId,Lidar->GetSensorId());
	TestTrue(TEXT("gizmo follows selected LiDAR"),Settings->GetTransformGizmoActor()->GetBoundTargetActor()==Lidar);
	Toolbar->SelectSensor(Camera->GetSensorId());
	TestTrue(TEXT("camera selection remains reversible"),Settings->GetPendingState().TargetKind==EVirtualSensorTargetKind::Camera);
	W->SetPanelOpen(ESensorToolPanelRole::Settings,false);
	Toolbar->SelectSensor(Lidar->GetSensorId());
	TestTrue(TEXT("hidden settings also follow toolbar"),Settings->GetPendingState().TargetKind==EVirtualSensorTargetKind::Lidar);
	W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
	TestEqual(TEXT("reopening reads selected LiDAR"),Settings->GetPendingState().SensorId,Lidar->GetSensorId());
	W->UnregisterPanel(Settings);Settings->GetTransformGizmoActor()->Destroy();
	Manager->Destroy();Camera->Destroy();Lidar->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkspacePlacementEntryTest,"MA0T10.SensorWorkspace.PlacementEntry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkspacePlacementEntryTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    W->Preferences->Panels.Reset();
    TestTrue(TEXT("first use opens placement and preview"),W->IsPanelOpen(ESensorToolPanelRole::Placement)&&W->IsPanelOpen(ESensorToolPanelRole::Monitor));
    W->Preferences->Panels.FindOrAdd(ESensorToolPanelRole::Placement).bOpen=false;
    TestFalse(TEXT("explicit saved closed state wins"),W->IsPanelOpen(ESensorToolPanelRole::Placement));
    W->SetPanelOpen(ESensorToolPanelRole::Placement,true);
    TestTrue(TEXT("missing host has visible failure reason"),!W->GetPanelOpenMessage().IsEmpty());
    TestFalse(TEXT("failed open does not save fake open state"),W->IsPanelOpen(ESensorToolPanelRole::Placement));
    auto* Host=World->SpawnActor<AVirtualSensorUiHostActor>();Host->PanelHostComponent=nullptr;FString Reason;
    TestFalse(TEXT("missing dependencies do not create panel"),Host->EnsurePlacementPanel(Reason));
    TestTrue(TEXT("missing dependencies explained"),!Reason.IsEmpty());
    auto* C=World->SpawnActor<AVirtualSensorCoordinator>();Host->SensorManager=C;
    auto* PC=World->SpawnActor<APlayerController>();World->AddController(PC);
    TestTrue(TEXT("late dependencies can initialize same host"),Host->EnsurePlacementPanel(Reason));
    auto* Panel=Host->GetPlacementWidget();TestNotNull(TEXT("placement exists"),Panel);
    if(Panel)
    {
        Panel->TogglePanelCollapsed();
        TestTrue(TEXT("explicit open succeeds"),Host->ShowPlacementPanel(Reason));
        TestFalse(TEXT("explicit open expands body"),Panel->IsPanelCollapsed());
        Host->EnsurePlacementPanel(Reason);
        TestTrue(TEXT("repeated ensure reuses same panel"),Panel==Host->GetPlacementWidget());
    }
    Host->DestroyMonitorWidget();W->UnregisterPlacementHost(Host);Host->Destroy();
    auto* Disabled=World->SpawnActor<AVirtualSensorUiHostActor>();Disabled->PanelHostComponent=nullptr;Disabled->SensorManager=C;Disabled->bUseNativeToolWidgetFallback=false;
    TestFalse(TEXT("disabled fallback respects host configuration"),Disabled->EnsurePlacementPanel(Reason));
    TestTrue(TEXT("fallback failure is actionable"),Reason.Contains(TEXT("fallback")));
    W->UnregisterPlacementHost(Disabled);Disabled->Destroy();PC->Destroy();C->Destroy();return true;
}
#endif
