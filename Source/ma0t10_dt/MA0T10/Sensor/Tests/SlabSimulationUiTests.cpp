#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorWorkspaceForeignFixture.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioChartWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabChartDecimationTest, "MA0T10.Slab.Chart.NativeDecimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlabChartDecimationTest::RunTest(const FString&)
{
	TArray<FSlabChartSample> Samples;
	for (int32 I = 0; I < 600; ++I)
	{
		auto& S = Samples.AddDefaulted_GetRef(); S.Time = I * .05; S.Frame = I; S.LeftAngle = -.2; S.RightAngle = .2;
		S.MarginLeftCm = 100; S.MarginRightCm = 100; S.bMarginsValid = true;
	}
	Samples[211].LeftAngle = -9; Samples[212].LeftAngle = 7; Samples[400].MarginLeftCm = -2;
	const auto Result = USlabScenarioChartWidget::BuildMinMaxIndices(Samples, ESlabChartMetric::Angles, 0, false, 0, 29.95, 60);
	TestTrue(TEXT("bounded by pixel bins"), Result.Num() <= 120);
	TestTrue(TEXT("negative spike retained"), Result.Contains(211));
	TestTrue(TEXT("positive spike retained"), Result.Contains(212));
	for (int32 I = 1; I < Result.Num(); ++I) TestTrue(TEXT("indices stay ordered"), Result[I] > Result[I - 1]);
	const auto FrameResult = USlabScenarioChartWidget::BuildMinMaxIndices(Samples, ESlabChartMetric::RailMargins, 0, true, 300, 500, 20);
	TestTrue(TEXT("signed rail violation retained"), FrameResult.Contains(400));
	for (int32 I : FrameResult) TestTrue(TEXT("frame zoom filters outside values"), I >= 300 && I <= 500);
	for (auto& S : Samples) S.bMarginsValid = false;
	TestEqual(TEXT("unset rails produce no invented zero curve"), USlabScenarioChartWidget::BuildMinMaxIndices(Samples, ESlabChartMetric::RailMargins, 0, false, 0, 30, 400).Num(), 0);
	TestTrue(TEXT("original rows retained"), Samples.Num() == 600 && Samples[211].LeftAngle == -9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabPanelIsolationTest, "MA0T10.Slab.UI.OwnershipAndOutputDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlabPanelIsolationTest::RunTest(const FString&)
{
	auto* World = FAutomationEditorCommonUtils::CreateNewMap();
	auto* Workspace = World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Foreign = NewObject<USensorWorkspaceForeignFixture>(World);
	auto* Text = NewObject<UTextBlock>(Foreign);
	const auto OriginalFont = Text->GetFont(); const auto Size = Foreign->GetEffectivePanelSize();
	auto* Charts = NewObject<USlabChartsPanelWidget>(World);
	auto* Progress = NewObject<USlabProgressPanelWidget>(World);
	auto* Replay = NewObject<USlabScenarioReplayPanelWidget>(World);
	TestEqual(TEXT("existing enum ordinal preserved"), static_cast<uint8>(ESensorToolPanelRole::Replay), uint8(3));
	TestTrue(TEXT("new roles appended"), static_cast<uint8>(ESensorToolPanelRole::SlabCharts) > 3);
	TestFalse(TEXT("shared parent does not imply ownership"), Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts, Foreign));
	TestFalse(TEXT("wrong role denied"), Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress, Charts));
	TestTrue(TEXT("charts explicitly owned"), Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabCharts, Charts));
	TestTrue(TEXT("progress explicitly owned"), Workspace->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress, Progress));
	Charts->RegisterSensorTextControl(Text);
	Workspace->SetOwnedPanelFontScale(1.5f);
	TestEqual(TEXT("registered chart receives scale"), Charts->GetSensorToolFontScale(), 1.5f);
	TestEqual(TEXT("foreign text unmodified"), Text->GetFont().Size, OriginalFont.Size);
	TestEqual(TEXT("foreign geometry unmodified"), Foreign->GetEffectivePanelSize(), Size);
	TestEqual(TEXT("foreign font scale unmodified"), Foreign->GetSensorToolFontScale(), 1.0f);
	Workspace->SetPanelOpen(ESensorToolPanelRole::SlabCharts, false);
	TestTrue(TEXT("hide retains exact instance"), Workspace->GetOwnedPanel(ESensorToolPanelRole::SlabCharts) == Charts);
	const auto Default = Replay->GetReplayOutputs();
	TestFalse(TEXT("replay defaults to observation only"), Default.bPointCloud || Default.bCameraImage || Default.bLidarTelemetry);
	FVirtualSlabSensorOutputSelection Outputs; Outputs.bCameraImage = true;
	Replay->SetReplayOutputs(Outputs);
	TestTrue(TEXT("PCD compatibility property follows new API"), Replay->bSendPcd);
	TestTrue(TEXT("camera independently enabled"), Replay->GetReplayOutputs().bCameraImage);
	TestFalse(TEXT("lidar remains independently disabled"), Replay->GetReplayOutputs().bLidarTelemetry);
	Workspace->UnregisterPanel(Charts); Workspace->UnregisterPanel(Progress);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabWorkspaceDeferredLayoutTest, "MA0T10.Slab.UI.DeferredOwnedLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlabWorkspaceDeferredLayoutTest::RunTest(const FString&)
{
	auto* World = FAutomationEditorCommonUtils::CreateNewMap();
	auto* Workspace = World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
	auto* Progress = NewObject<USlabProgressPanelWidget>(World);
	const auto Role = ESensorToolPanelRole::SlabProgress;
	const auto OriginalPreferences = Workspace->Preferences->Panels;
	auto& Saved = Workspace->Preferences->Panels.FindOrAdd(Role);
	Saved.bOpen = false; Saved.Layout.bHasSavedSize = true; Saved.Layout.bHasSavedPosition = true;
	Saved.Layout.ExpandedSize = FVector2D(650, 440); Saved.Layout.NormalizedPosition = FVector2D(.25, .4);
	TestTrue(TEXT("registration accepted"), Workspace->RegisterOwnedPanel(Role, Progress));
	TestTrue(TEXT("registration schedules first layout"), Workspace->PendingInitialLayouts.Contains(Role));
	TestFalse(TEXT("unattached panel is not layout-ready"), Workspace->IsOwnedPanelLayoutReady(Role));
	Progress->ConfigurePanelLayout(EVirtualSensorPanelPlacement::LeftCenter, FVector2D(520, 420));
	Progress->SetPanelResizeLimits(FVector2D(360, 280), FVector2D::ZeroVector);
	Workspace->RestorePanel(Role);
	Workspace->SavePanel(Role);
	TestTrue(TEXT("startup does not lose pending layout"), Workspace->PendingInitialLayouts.Contains(Role));
	TestEqual(TEXT("provisional minimum cannot overwrite saved width/height"), Workspace->Preferences->Panels[Role].Layout.ExpandedSize, FVector2D(650, 440));
	TestEqual(TEXT("provisional position cannot overwrite saved placement"), Workspace->Preferences->Panels[Role].Layout.NormalizedPosition, FVector2D(.25, .4));
	Workspace->SetPanelOpen(Role, true);
	TestTrue(TEXT("first show still waits for actual Canvas arrangement"), Workspace->PendingInitialLayouts.Contains(Role));
	TestEqual(TEXT("first show preserves user size"), Workspace->Preferences->Panels[Role].Layout.ExpandedSize, FVector2D(650, 440));
	Workspace->UnregisterPanel(Progress);
	TestFalse(TEXT("pending entry cleaned up"), Workspace->PendingInitialLayouts.Contains(Role));
	TestEqual(TEXT("shutdown before first layout cannot save transient size"), Workspace->Preferences->Panels[Role].Layout.ExpandedSize, FVector2D(650, 440));
	Workspace->Preferences->Panels = OriginalPreferences;
	return true;
}
#endif
