#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorWorkspaceForeignFixture.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "Components/TextBlock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkspaceIsolationTest,"MA0T10.SensorWorkspace.IsolationAndStorage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkspaceIsolationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Foreign=NewObject<USensorWorkspaceForeignFixture>(World);
	auto* Monitor=NewObject<UVirtualSensorMonitorPanelWidget>(World);
	auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);
	auto* ForeignText=NewObject<UTextBlock>(Foreign);ForeignText->SetText(FText::FromString(TEXT("동료 차트")));
	const auto OriginalFont=ForeignText->GetFont();const auto OriginalSize=Foreign->GetEffectivePanelSize();const auto OriginalPosition=Foreign->GetCurrentPanelPosition();
	TArray<uint8> Before,After,WorkspaceBefore;
	const bool HadLegacy=UGameplayStatics::LoadDataFromSlot(Before,UVirtualSensorUiPreferencesSaveGame::SlotName,0);
	const bool HadWorkspace=UGameplayStatics::LoadDataFromSlot(WorkspaceBefore,UVirtualSensorToolWorkspaceSubsystem::SlotName,0);
	TestFalse(TEXT("common parent alone cannot register"),W->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Foreign));
	TestTrue(TEXT("explicit monitor registration"),W->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Monitor));
	TestTrue(TEXT("explicit settings registration"),W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings));
	TestFalse(TEXT("wrong role rejected"),W->RegisterOwnedPanel(ESensorToolPanelRole::Data,Settings));
	Monitor->RegisterSensorTextControl(ForeignText); // must not cross UserWidget ownership
	W->SetOwnedPanelFontScale(1.5f);
	TestEqual(TEXT("foreign nested text font unchanged"),ForeignText->GetFont().Size,OriginalFont.Size);
	TestEqual(TEXT("foreign base scale unchanged"),Foreign->GetSensorToolFontScale(),1.0f);
	TestEqual(TEXT("owned scale applied"),Monitor->GetSensorToolFontScale(),1.5f);
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
	W->UnregisterPanel(Monitor);W->UnregisterPanel(Settings);
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
#endif
