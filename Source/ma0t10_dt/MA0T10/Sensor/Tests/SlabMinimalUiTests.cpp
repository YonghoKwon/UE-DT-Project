#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "Widgets/SWidget.h"
namespace { int32 ComboCount(const TSharedRef<SWidget>& W){int32 N=W->GetTypeAsString().StartsWith(TEXT("SComboBox"))?1:0;auto* C=W->GetChildren();for(int32 I=0;C&&I<C->Num();++I)N+=ComboCount(C->GetChildAt(I));return N;} }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabMinimalUiTest,"MA0T10.SensorUi.MinimalMonitorAndScenarioDefault",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabMinimalUiTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Manager->DiscoverSensorsInLevel();Manager->SetViewMode(EVirtualSensorViewMode::Lidar);
	auto* Monitor=NewObject<UVirtualSensorMonitorPanelWidget>(World);Monitor->bPersistMonitorPreferences=false;Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Monitor);Monitor->BindSensorManager(Manager);
	// Editor-only NewObject fixture has no player/UMG wrapper. Inspect the actual
	// native tree directly; runtime TakeWidget/rehosting is covered by PIE tests.
	const auto Slate=Monitor->RebuildWidget();Slate->SlatePrepass(1);
	TestEqual(TEXT("no inline projection/color selectors in default monitor"),ComboCount(Slate),0);
	const auto Summary=Monitor->BuildCompactStatusText();TestTrue(TEXT("minimal status identifies selected sensor"),Summary.Contains(Lidar->GetSensorId()));
	TestFalse(TEXT("point counts removed from default status"),Summary.Contains(TEXT("광선"))||Summary.Contains(TEXT("검출점"))||Summary.Contains(TEXT("\n")));
	const auto Menu=Monitor->BuildOwnedAdvancedViewMenu();Menu->SlatePrepass(1);TestTrue(TEXT("view selectors remain available in menu"),ComboCount(Menu)>=3);
	auto* Data=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Data,Data);
	Data->ActiveTab=EVirtualSensorCaptureExportTab::Export;Data->InitializeOwnedEntryView(Data->ActiveTab);
	TestEqual(TEXT("fresh owned instance ignores persisted file tab for entry"),Data->ActiveTab,EVirtualSensorCaptureExportTab::LiveStream);
	TestTrue(TEXT("fresh entry is scenario view"),Data->bOwnedScenarioStreamView);
	Data->bOwnedScenarioStreamView=false;Data->ActiveTab=EVirtualSensorCaptureExportTab::ConnectionLog;
	Data->InitializeOwnedEntryView(Data->ActiveTab);
	TestEqual(TEXT("rehosting does not reset chosen tab"),Data->ActiveTab,EVirtualSensorCaptureExportTab::ConnectionLog);
	TestFalse(TEXT("reopening preserves chosen subview"),Data->bOwnedScenarioStreamView);
	auto* Foreign=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);Foreign->ActiveTab=EVirtualSensorCaptureExportTab::Export;Foreign->InitializeOwnedEntryView(EVirtualSensorCaptureExportTab::LiveStream);
	TestEqual(TEXT("unowned panel entry unchanged"),Foreign->ActiveTab,EVirtualSensorCaptureExportTab::Export);
	Workspace->UnregisterPanel(Data);Workspace->UnregisterPanel(Monitor);Manager->Destroy();Lidar->Destroy();return true;
}
#endif
