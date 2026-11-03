#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/VirtualSensorPanelHostComp.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorUiHostActor.h"
#include "Device/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2PanelCollapseGeometryTest,
	"MA0T10.SensorV2.UI.CollapseGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2PanelHostDeduplicationTest,
	"MA0T10.SensorV2.UI.HostDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2DualCameraSelectionTest,
	"MA0T10.SensorV2.UI.DualCameraSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSensorV2PanelCollapseGeometryTest::RunTest(const FString& Parameters)
{
	UDxWidgetVirtualSensorMonitorPanel* Panel = NewObject<UDxWidgetVirtualSensorMonitorPanel>();
	const FVector2D ExpandedSize = Panel->GetEffectivePanelSize();
	Panel->TogglePanelCollapsed();
	const FVector2D CollapsedSize = Panel->GetEffectivePanelSize();

	TestEqual(TEXT("collapse keeps panel width"), CollapsedSize.X, ExpandedSize.X);
	TestTrue(TEXT("collapse reduces actual panel height"), CollapsedSize.Y < ExpandedSize.Y);
	TestTrue(TEXT("collapsed title remains usable"), CollapsedSize.Y >= Panel->DragHandleHeight);

	Panel->TogglePanelCollapsed();
	TestEqual(TEXT("expand restores exact size"), Panel->GetEffectivePanelSize(), ExpandedSize);
	TestEqual(TEXT("drag delta is applied in logical DPI coordinates"),
		UDxWidgetVirtualSensorPanelBase::CalculateDraggedPanelPosition(FVector2D(100.0f, 80.0f), FVector2D(40.0f, 20.0f), 2.0f),
		FVector2D(120.0f, 90.0f));
	TestEqual(TEXT("resize delta is DPI corrected"),
		UDxWidgetVirtualSensorPanelBase::CalculateResizedPanelSize(
			FVector2D(820.0f, 430.0f), FVector2D(200.0f, 100.0f), 2.0f,
			FVector2D(480.0f, 300.0f), FVector2D(1200.0f, 900.0f)),
		FVector2D(920.0f, 480.0f));
	TestEqual(TEXT("resize clamps to minimum"),
		UDxWidgetVirtualSensorPanelBase::CalculateResizedPanelSize(
			FVector2D(500.0f, 320.0f), FVector2D(-500.0f, -500.0f), 1.0f,
			FVector2D(480.0f, 300.0f), FVector2D(1200.0f, 900.0f)),
		FVector2D(480.0f, 300.0f));
	return true;
}

bool FSensorV2PanelHostDeduplicationTest::RunTest(const FString& Parameters)
{
	AVirtualSensorUiHostActor* HostActor = NewObject<AVirtualSensorUiHostActor>();
	TestNotNull(TEXT("V2 UI host owns panel host component"), HostActor->PanelHostComp.Get());
	UVirtualSensorPanelHostComp* Host = HostActor->PanelHostComp;
	UDxWidgetVirtualSensorMonitorPanel* Panel = NewObject<UDxWidgetVirtualSensorMonitorPanel>();
	Host->bAllowViewportFallback = false;
	Host->RegisterPanel(Panel, 10);
	Host->RegisterPanel(Panel, 10);
	TestEqual(TEXT("same panel instance is hosted once"), Host->GetHostedPanelCount(), 1);
	Host->UnregisterAllPanels();
	TestEqual(TEXT("unregister clears hosted panels"), Host->GetHostedPanelCount(), 0);
	return true;
}

bool FSensorV2DualCameraSelectionTest::RunTest(const FString& Parameters)
{
	AVirtualSensorCoordinator* Coordinator = NewObject<AVirtualSensorCoordinator>();
	AVirtualCameraSensorActor* CameraA = NewObject<AVirtualCameraSensorActor>();
	AVirtualCameraSensorActor* CameraB = NewObject<AVirtualCameraSensorActor>();
	CameraA->CaptureComp->SensorId = TEXT("VCAM-A");
	CameraB->CaptureComp->SensorId = TEXT("VCAM-B");
	Coordinator->RegisterSensorActor(CameraA);
	Coordinator->RegisterSensorActor(CameraB);
	Coordinator->RegisterCamera(CameraA->CaptureComp);
	Coordinator->RegisterCamera(CameraB->CaptureComp);
	TestEqual(TEXT("camera count API exposes both cameras"), Coordinator->GetCameraCount(), 2);
	TestEqual(TEXT("SensorId lookup finds second camera"), Coordinator->FindCameraIndexBySensorId(TEXT("VCAM-B")), 1);

	UDxWidgetVirtualSensorMonitorPanel* Monitor = NewObject<UDxWidgetVirtualSensorMonitorPanel>();
	Monitor->BindSensorManager(Coordinator);
	Monitor->SetDualCameraModeEnabled(true);
	TestTrue(TEXT("dual camera mode enables when two cameras exist"), Monitor->IsDualCameraModeEnabled());
	TestTrue(TEXT("primary camera can be selected by SensorId"), Monitor->SelectPrimaryCameraBySensorId(TEXT("VCAM-B")));
	TestEqual(TEXT("primary selection synchronizes coordinator"), Coordinator->GetSelectedCamera(), CameraB->CaptureComp.Get());
	TestFalse(TEXT("same camera cannot be selected as secondary"), Monitor->SelectSecondaryCameraBySensorId(TEXT("VCAM-B")));
	TestTrue(TEXT("other camera can be selected as secondary"), Monitor->SelectSecondaryCameraBySensorId(TEXT("VCAM-A")));
	return true;
}

#endif
