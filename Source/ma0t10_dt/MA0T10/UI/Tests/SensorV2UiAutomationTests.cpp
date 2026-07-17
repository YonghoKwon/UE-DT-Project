#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelHostComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2PanelCollapseGeometryTest,
	"MA0T10.SensorV2.UI.CollapseGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSensorV2PanelHostDeduplicationTest,
	"MA0T10.SensorV2.UI.HostDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSensorV2PanelCollapseGeometryTest::RunTest(const FString& Parameters)
{
	UVirtualSensorMonitorPanelWidget* Panel = NewObject<UVirtualSensorMonitorPanelWidget>();
	const FVector2D ExpandedSize = Panel->GetEffectivePanelSize();
	Panel->TogglePanelCollapsed();
	const FVector2D CollapsedSize = Panel->GetEffectivePanelSize();

	TestEqual(TEXT("collapse keeps panel width"), CollapsedSize.X, ExpandedSize.X);
	TestTrue(TEXT("collapse reduces actual panel height"), CollapsedSize.Y < ExpandedSize.Y);
	TestTrue(TEXT("collapsed title remains usable"), CollapsedSize.Y >= Panel->DragHandleHeight);

	Panel->TogglePanelCollapsed();
	TestEqual(TEXT("expand restores exact size"), Panel->GetEffectivePanelSize(), ExpandedSize);
	TestEqual(TEXT("drag delta is applied in logical DPI coordinates"),
		UVirtualSensorPanelWidgetBase::CalculateDraggedPanelPosition(FVector2D(100.0f, 80.0f), FVector2D(40.0f, 20.0f), 2.0f),
		FVector2D(120.0f, 90.0f));
	return true;
}

bool FSensorV2PanelHostDeduplicationTest::RunTest(const FString& Parameters)
{
	AVirtualSensorUiHostActor* HostActor = NewObject<AVirtualSensorUiHostActor>();
	TestNotNull(TEXT("V2 UI host owns panel host component"), HostActor->PanelHostComponent.Get());
	UVirtualSensorPanelHostComponent* Host = HostActor->PanelHostComponent;
	UVirtualSensorMonitorPanelWidget* Panel = NewObject<UVirtualSensorMonitorPanelWidget>();
	Host->bAllowViewportFallback = false;
	Host->RegisterPanel(Panel, 10);
	Host->RegisterPanel(Panel, 10);
	TestEqual(TEXT("same panel instance is hosted once"), Host->GetHostedPanelCount(), 1);
	Host->UnregisterAllPanels();
	TestEqual(TEXT("unregister clears hosted panels"), Host->GetHostedPanelCount(), 0);
	return true;
}

#endif
