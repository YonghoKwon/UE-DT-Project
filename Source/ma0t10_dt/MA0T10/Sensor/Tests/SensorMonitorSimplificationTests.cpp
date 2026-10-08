#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorMonitorRateStateTest,"MA0T10.ResourceLifecycle.MonitorCurrentRateState",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorMonitorRateStateTest::RunTest(const FString&)
{
    using M=UVirtualSensorMonitorPanelWidget;
    FVirtualSensorRuntimeStatus History;History.FrameId=717;History.MeasuredCompletionRateHz=20;
    auto Rate=UVirtualSensorSchedulerSubsystem::EvaluateRate(3,0,1,20,.05f,true,false,false,0);
    auto Text=M::FormatOwnedSensorRate(TEXT("LiDAR"),TEXT("rate-test"),History,true,&Rate);
    TestTrue(TEXT("stale positive historical rate is shown as zero and explicitly stalled"),Text.Contains(TEXT("0.00 Hz"))&&Text.Contains(TEXT("정체")));
    Rate=UVirtualSensorSchedulerSubsystem::EvaluateRate(3,0,2.95,20,.05f,true,false,false,0);
    Text=M::FormatOwnedSensorRate(TEXT("LiDAR"),TEXT("rate-test"),History,true,&Rate);
    TestTrue(TEXT("recovery shows current twenty Hz"),Text.Contains(TEXT("20.00 Hz"))&&Text.Contains(TEXT("측정 중")));
    Rate=UVirtualSensorSchedulerSubsystem::EvaluateRate(3,0,1,20,.05f,true,true,false,0);
    TestTrue(TEXT("pause is not labelled stalled"),M::FormatOwnedSensorRate(TEXT("Camera"),TEXT("rate-test"),History,true,&Rate).Contains(TEXT("일시정지")));
    Rate=UVirtualSensorSchedulerSubsystem::EvaluateRate(.5,0,-1,20,.05f,true,false,false,0);
    TestTrue(TEXT("warmup is distinct"),M::FormatOwnedSensorRate(TEXT("Camera"),TEXT("rate-test"),History,true,&Rate).Contains(TEXT("준비 중")));
    Rate=UVirtualSensorSchedulerSubsystem::EvaluateRate(5,0,4.8,4,.25f,true,false,true,0);
    TestTrue(TEXT("interaction preview is distinct"),M::FormatOwnedSensorRate(TEXT("LiDAR"),TEXT("rate-test"),History,true,&Rate).Contains(TEXT("조작 미리보기")));
    Text=M::FormatOwnedSensorRate(TEXT("Camera"),TEXT("rate-test"),History,false,nullptr);
    TestTrue(TEXT("unregistered stopped sensor never retains twenty Hz"),Text.Contains(TEXT("0.00 Hz"))&&Text.Contains(TEXT("정지")));
    TestEqual(TEXT("presentation never mutates stored completion history"),History.MeasuredCompletionRateHz,20.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorMonitorOptionApplicabilityTest,"MA0T10.SensorWorkspace.MonitorOptionApplicability",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorMonitorOptionApplicabilityTest::RunTest(const FString&)
{
	using M=UVirtualSensorMonitorPanelWidget;
	TestTrue(TEXT("range image has image overlays"),M::SupportsRangeOverlays(ELidarMonitorProjectionMode::RangeImage));
	TestTrue(TEXT("split retains range image overlays"),M::SupportsRangeOverlays(ELidarMonitorProjectionMode::Split));
	for(auto P:{ELidarMonitorProjectionMode::TopDown,ELidarMonitorProjectionMode::WorldTopDown,ELidarMonitorProjectionMode::Elevation,ELidarMonitorProjectionMode::ForwardSlice})TestFalse(TEXT("non-range projection hides inapplicable overlay controls"),M::SupportsRangeOverlays(P));
	for(auto C:{ELidarColorMode::DistanceTurbo,ELidarColorMode::DistanceViridis,ELidarColorMode::DistanceGray})TestTrue(TEXT("distance colors allow adaptive normalization"),M::SupportsAdaptiveDistance(C));
	for(auto C:{ELidarColorMode::RelativeHeight,ELidarColorMode::SemanticLabel,ELidarColorMode::GeometrySeparation,ELidarColorMode::HitMask,ELidarColorMode::VerticalChannel,ELidarColorMode::ReturnIndex})TestFalse(TEXT("non-distance colors do not present a no-effect distance option"),M::SupportsAdaptiveDistance(C));
	TestNotNull(TEXT("legacy export API retained"),M::StaticClass()->FindFunctionByName(TEXT("ExportSelectedSensorServerPayload")));
	TestNotNull(TEXT("legacy capture API retained"),M::StaticClass()->FindFunctionByName(TEXT("CaptureConfiguredOutputsOnce")));
	TestNotNull(TEXT("legacy point visibility API retained"),M::StaticClass()->FindFunctionByName(TEXT("SetLidarWorldPointCloudEnabled")));
	TestNotNull(TEXT("existing settings map-save API retained"),UVirtualSensorSettingsPanelWidget::StaticClass()->FindFunctionByName(TEXT("QueuePendingStateForSensorTestMap")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorMonitorPresentationTest,"MA0T10.SensorWorkspace.MonitorPresentationRoundtrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorMonitorPresentationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();
	// CreateNewMap is an editor world and does not run the Coordinator's BeginPlay discovery.
	// RegisterSensorActor alone registers output services, not its selectable component list.
	Manager->DiscoverSensorsInLevel();Manager->SetViewMode(EVirtualSensorViewMode::Lidar);Manager->bPointCloudOnlyHideWorld=false;
	auto* M=NewObject<UVirtualSensorMonitorPanelWidget>(World);M->bPersistMonitorPreferences=false;M->BindSensorManager(Manager);
	const bool Selected=Manager->GetSelectedLidar()==Lidar->ScanComponent.Get();
	TestTrue(TEXT("fixture discovers selected LiDAR component as runtime BeginPlay does"),Selected);
	TestTrue(TEXT("monitor actually bound to selected LiDAR before presentation changes"),M->HasBoundLidar()&&M->IsShowingLidar());
	if(!Selected||!M->HasBoundLidar()){Manager->Destroy();Lidar->Destroy();return false;}
	auto* V=Lidar->VisualizationComponent.Get();
	auto S=V->GetVisualizationSettings();S.ProjectionMode=ELidarMonitorProjectionMode::WorldTopDown;S.ColorMode=ELidarColorMode::RelativeHeight;S.bShowGrid=true;S.bShowDepthEdges=true;S.bAutoHeightRange=false;S.HeightMinMeters=-2;S.HeightMaxMeters=3;V->SetVisualizationSettings(S);
	M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::TwoDimensional);
	TestFalse(TEXT("2D does not render world points"),V->GetVisualizationSettings().bShowWorldPointCloud);
	M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::PointCloudOnly);
	TestTrue(TEXT("point-only delegates to existing coordinator state"),Manager->IsPointCloudOnlyModeEnabled());
	TestEqual(TEXT("getter reflects existing point-only API"),M->GetMonitorPresentation(),EVirtualSensorMonitorPresentation::PointCloudOnly);
	M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::WorldOverlay);
	TestFalse(TEXT("overlay exits point-only mode"),Manager->IsPointCloudOnlyModeEnabled());
	TestTrue(TEXT("overlay keeps world points enabled"),V->GetVisualizationSettings().bShowWorldPointCloud);
	M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::TwoDimensional);
	const auto After=V->GetVisualizationSettings();
	TestEqual(TEXT("presentation preserves projection"),After.ProjectionMode,S.ProjectionMode);
	TestEqual(TEXT("presentation preserves color"),After.ColorMode,S.ColorMode);
	TestTrue(TEXT("hidden overlays retain prior values"),After.bShowGrid&&After.bShowDepthEdges);
	TestEqual(TEXT("hidden height minimum retained"),After.HeightMinMeters,-2.f);
	TestEqual(TEXT("hidden height maximum retained"),After.HeightMaxMeters,3.f);
	Manager->Destroy();Lidar->Destroy();return true;
}
#endif
