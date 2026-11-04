#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/UnrealType.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorCaptureExportPanel.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorSettingsPanel.h"
#include "ma0t10_dt/MA0T10/UI/Main/Sensor/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorUiPreferences.h"
#include "DeviceUI/DxDeviceSensorMonitorPanel.h"
#include "Device/Camera/VirtualCameraSensorActor.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreExtCaptureNavigationTest, "MA0T10.DTCoreExt09.CaptureNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreExtCaptureNavigationTest::RunTest(const FString& Parameters)
{
	auto* Panel = NewObject<UDxWidgetVirtualSensorCaptureExportPanel>();
	Panel->SetActiveTab(EVirtualSensorCaptureExportTab::LiveStream);
	Panel->SetActiveTab(EVirtualSensorCaptureExportTab::Capture);
	TestEqual(TEXT("legacy tab API routes through DTCore"), Panel->GetCurrentSubTarget(), FName(TEXT("Capture")));
	TestTrue(TEXT("export navigation"), Panel->NavigateTo(TEXT("Export")));
	TestEqual(TEXT("new navigation updates legacy tab"), Panel->GetActiveTab(), EVirtualSensorCaptureExportTab::Export);
	TestTrue(TEXT("DTCore back"), Panel->NavigateBack());
	TestEqual(TEXT("back restores capture"), Panel->GetActiveTab(), EVirtualSensorCaptureExportTab::Capture);
	const auto* Saved = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
	TestEqual(TEXT("unchanged uint8 capture value"), Saved->CaptureExportActiveTab, static_cast<uint8>(1));
	TestNotNull(TEXT("saved field remains uint8"), FindFProperty<FByteProperty>(UVirtualSensorUiPreferencesSaveGame::StaticClass(), TEXT("CaptureExportActiveTab")));
	TestEqual(TEXT("live enum byte"), static_cast<uint8>(EVirtualSensorCaptureExportTab::LiveStream), static_cast<uint8>(0));
	TestEqual(TEXT("export enum byte"), static_cast<uint8>(EVirtualSensorCaptureExportTab::Export), static_cast<uint8>(2));
	TestEqual(TEXT("connection enum byte"), static_cast<uint8>(EVirtualSensorCaptureExportTab::ConnectionLog), static_cast<uint8>(3));
	TestFalse(TEXT("unknown target rejected"), Panel->NavigateTo(TEXT("UnknownDataTab")));
	TestEqual(TEXT("invalid navigation preserves current"), Panel->GetCurrentSubTarget(), FName(TEXT("Capture")));
	Panel->ClearOwnedTabHistory();
	TestFalse(TEXT("legacy clear empties DTCore history"), Panel->CanNavigateBack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreExtSidebarNavigationTest, "MA0T10.DTCoreExt09.SidebarNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreExtSidebarNavigationTest::RunTest(const FString& Parameters)
{
	const ESensorToolDestination Destinations[] = {ESensorToolDestination::Placement, ESensorToolDestination::Preview, ESensorToolDestination::Scenario, ESensorToolDestination::PcdTransfer, ESensorToolDestination::Results};
	const FName Expected[] = {TEXT("Placement"), TEXT("Preview"), TEXT("Scenario"), TEXT("Transfer"), TEXT("Results")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		TestEqual(TEXT("sidebar SubTarget"), USensorToolToolbarWidget::NavigationSubTarget(Destinations[Index]), Expected[Index]);
	TestEqual(TEXT("details SubTarget"), USensorToolToolbarWidget::NavigationSubTarget(ESensorToolDestination::SensorSettings), FName(TEXT("EquipmentDetails")));
	struct FHost final : IDevicePanelHostBridge
	{
		mutable FName Selected;
		bool IsOwned() const override { return true; }
		float PixelScale() const override { return 1.0f; }
		bool NavigateBack() override { return false; }
		FText BackLabel() const override { return FText::GetEmpty(); }
		bool NavigateToSensor(FName SensorId) const override
		{
			Selected = SensorId;
			return true;
		}
	};
	auto* Owner = NewObject<UGameInstance>();
	auto* Panel = NewObject<UDxWidgetVirtualSensorSettingsPanel>();
	auto Host = MakeShared<FHost>();
	Panel->BindDevicePanelHost(Owner, Host);
	TestTrue(TEXT("base sensor navigation delegates to explicit host"), Panel->NavigateTo(TEXT("Sensor:one")));
	TestEqual(TEXT("host receives sensor id"), Host->Selected, FName(TEXT("one")));
	TestTrue(TEXT("second sensor navigation"), Panel->NavigateTo(TEXT("Sensor:two")));
	Panel->BindDevicePanelHost(nullptr, nullptr);
	TestFalse(TEXT("unbound host cannot apply history"), Panel->NavigateBack());
	TestEqual(TEXT("failed back preserves current"), Panel->GetCurrentSubTarget(), FName(TEXT("Sensor:two")));
	TestTrue(TEXT("failed back preserves history"), Panel->CanNavigateBack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreExtMonitorNavigationTest, "MA0T10.DTCoreExt09.MonitorNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreExtMonitorNavigationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Camera = World->SpawnActor<AVirtualCameraSensorActor>();
	auto* Lidar = World->SpawnActor<AVirtualLidarSensorActor>();
	auto* Panel = NewObject<UDxDeviceSensorMonitorPanel>(World);
	if (!Camera || !Lidar || !Panel)
	{
		AddError(TEXT("navigation fixture creation"));
		World->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("camera selected through DTCore"), Panel->NavigateTo(FName(*(TEXT("Sensor:") + Camera->GetSensorId()))));
	TestEqual(TEXT("camera identity"), Panel->GetSelectedSensor(), static_cast<AVirtualSensorActorBase*>(Camera));
	TestTrue(TEXT("lidar selected through DTCore"), Panel->NavigateTo(FName(*(TEXT("Sensor:") + Lidar->GetSensorId()))));
	TestTrue(TEXT("monitor back"), Panel->NavigateBack());
	TestEqual(TEXT("monitor back restores camera"), Panel->GetSelectedSensor(), static_cast<AVirtualSensorActorBase*>(Camera));
	const FName Previous = Panel->GetCurrentSubTarget();
	TestFalse(TEXT("missing sensor rejected"), Panel->NavigateTo(TEXT("Sensor:missing")));
	TestEqual(TEXT("missing sensor preserves navigation"), Panel->GetCurrentSubTarget(), Previous);
	World->DestroyWorld(false);
	return true;
}
#endif
