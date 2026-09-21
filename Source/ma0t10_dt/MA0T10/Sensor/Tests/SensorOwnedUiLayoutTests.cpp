#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorWorkspaceForeignFixture.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"
#include "Widgets/Input/SButton.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorOwnedUiLayoutTest,"MA0T10.SensorUi.LayoutAndLegacyTabs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorOwnedUiLayoutTest::RunTest(const FString&)
{
	TestEqual(TEXT("one toolbar row reserves measured height plus margins"),UVirtualSensorToolWorkspaceSubsystem::CalculateToolbarReservedTop(48,720),64.0f);
	TestEqual(TEXT("wrapped toolbar reserves two rows"),UVirtualSensorToolWorkspaceSubsystem::CalculateToolbarReservedTop(96,720),112.0f);
	TestTrue(TEXT("tiny view keeps room for panel bodies"),UVirtualSensorToolWorkspaceSubsystem::CalculateToolbarReservedTop(500,300)<150);
	TestEqual(TEXT("capture enum maps to file tab"),UVirtualSensorCaptureExportPanelWidget::ResolveOwnedTabIndex(EVirtualSensorCaptureExportTab::Capture),1);
	TestEqual(TEXT("export enum maps to same file tab"),UVirtualSensorCaptureExportPanelWidget::ResolveOwnedTabIndex(EVirtualSensorCaptureExportTab::Export),1);
	TestEqual(TEXT("connection ordinal stays compatible"),static_cast<uint8>(EVirtualSensorCaptureExportTab::ConnectionLog),uint8(3));
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Foreign=NewObject<USensorWorkspaceForeignFixture>(World);auto* ForeignToolbar=NewObject<USensorToolToolbarWidget>(World);
	const auto ForeignSize=Foreign->GetEffectivePanelSize();
	TestFalse(TEXT("common toolbar class alone does not grant ownership"),Workspace->OwnsToolbar(ForeignToolbar));
	auto* Panel=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);
	TestTrue(TEXT("data panel explicit registration"),Workspace->RegisterOwnedPanel(ESensorToolPanelRole::Data,Panel));
	const SButton::FArguments Args=SButton::FArguments().ForegroundColor_Lambda([](){return FSlateColor(FLinearColor::Green);});
	auto Button=SNew(SButton).ForegroundColor_Lambda([](){return FSlateColor(FLinearColor::Green);});
	Panel->RegisterSensorNativeFont(Button,Args);
	Button->SlatePrepass(1.0f); // Bound Slate attributes are evaluated during the real layout pass.
	TestTrue(TEXT("owned styling preserves explicit foreground color"),Button->GetForegroundColor().GetSpecifiedColor().Equals(FLinearColor::Green));
	Workspace->SetOwnedPanelFontScale(1.5f);
	TestEqual(TEXT("foreign font scale unchanged"),Foreign->GetSensorToolFontScale(),1.0f);
	TestEqual(TEXT("foreign geometry unchanged"),Foreign->GetEffectivePanelSize(),ForeignSize);
	TestTrue(TEXT("foreign shared colors unchanged"),Foreign->GetToolPanelColor().Equals(FVirtualSensorUiStyle::PanelBackground));
	const auto Id=Panel->RequestFileSave(EVirtualSensorFileSaveMode::NewFrame);
	TestFalse(TEXT("rejected request still has diagnostic ID"),Id.IsEmpty());
	TestEqual(TEXT("no actor fails explicitly"),Panel->GetActiveFileSaveStatus().State,EVirtualSensorFileSaveState::Failed);
	TestEqual(TEXT("one terminal result only"),Panel->GetRecentResults().Num(),1);
	TestFalse(TEXT("request admission is not reported as successful file"),Panel->GetRecentResults()[0].bSucceeded);
	TestTrue(TEXT("failed request never fabricates path"),Panel->GetRecentResults()[0].AbsolutePath.IsEmpty());
	Workspace->UnregisterPanel(Panel);return true;
}
#endif
