#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <limits>
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorPanelClampTest, "MA0T10.SensorControl.PanelClamp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMapApplyQueueTest, "MA0T10.SensorControl.MapApplyQueue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorRealisticProfileTest, "MA0T10.SensorControl.RealisticProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorEditableStateValidationTest, "MA0T10.SensorControl.EditableStateValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorStorageSummaryTest, "MA0T10.SensorExport.StorageSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorGizmoMathTest, "MA0T10.SensorControl.GizmoMath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorDebugBudgetTest, "MA0T10.SensorDebug.ProjectionBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorUiPreferencesSerializationTest, "MA0T10.SensorControl.UiPreferencesSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorCapturePanelResizeTest, "MA0T10.SensorControl.CapturePanelResize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorPanelClampTest::RunTest(const FString& Parameters)
{
    const FVector2D Viewport(1280.0f, 720.0f);
    const FVector2D Panel(450.0f, 640.0f);
    TestEqual(TEXT("negative position clamps to margin"), UVirtualSensorPanelWidgetBase::ClampPanelPosition(FVector2D(-100, -100), Panel, Viewport, 18.0f), FVector2D(18.0f, 18.0f));
    const FVector2D Clamped = UVirtualSensorPanelWidgetBase::ClampPanelPosition(FVector2D(2000, 2000), Panel, Viewport, 18.0f);
    TestTrue(TEXT("right edge clamps"), FMath::IsNearlyEqual(Clamped.X, 812.0));
    TestTrue(TEXT("title remains reachable at bottom"), FMath::IsNearlyEqual(Clamped.Y, 638.0));
    return true;
}

bool FVirtualSensorMapApplyQueueTest::RunTest(const FString& Parameters)
{
    AVirtualSensorUiHostActor* Host = NewObject<AVirtualSensorUiHostActor>();
    TestNotNull(TEXT("host"), Host);
    FVirtualSensorEditableState State;
    State.PersistentActorTag = TEXT("SensorTestPersistent_PrimaryCamera");
    State.SensorId = TEXT("CAM-A");
    Host->QueueSensorStateForMapApply(State);
    TestTrue(TEXT("queue becomes pending"), Host->HasPendingMapApplySnapshot());
    TestEqual(TEXT("one state queued"), Host->GetPendingMapApplySnapshot().SensorStates.Num(), 1);
    State.SensorId = TEXT("CAM-B");
    Host->QueueSensorStateForMapApply(State);
    TestEqual(TEXT("same persistent tag replaces instead of duplicating"), Host->GetPendingMapApplySnapshot().SensorStates.Num(), 1);
    TestEqual(TEXT("replacement value is retained"), Host->GetPendingMapApplySnapshot().SensorStates[0].SensorId, FString(TEXT("CAM-B")));
    return true;
}

bool FVirtualSensorRealisticProfileTest::RunTest(const FString& Parameters)
{
    UVirtualCameraCaptureComponent* Camera = NewObject<UVirtualCameraCaptureComponent>();
    UVirtualLidarScanComponent* Lidar = NewObject<UVirtualLidarScanComponent>();
    Camera->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);
    Lidar->ApplyDeviceProfile(EVirtualLidarDeviceProfile::LivoxMid360S);
    const FVirtualSensorDeviceSpec& CameraSpec = Camera->GetDeviceSpec();
    const FVirtualSensorDeviceSpec& LidarSpec = Lidar->GetDeviceSpec();
    TestEqual(TEXT("D455 horizontal depth FOV"), CameraSpec.HorizontalFovDegrees, 87.0f);
    TestEqual(TEXT("D455 max-resolution Min-Z cm"), CameraSpec.MinRangeCm, 52.0f);
    TestEqual(TEXT("D455 ideal max range cm"), CameraSpec.MaxRangeCm, 600.0f);
    TestEqual(TEXT("Mid-360S horizontal FOV"), LidarSpec.HorizontalFovDegrees, 360.0f);
    TestEqual(TEXT("Mid-360S close blind zone cm"), LidarSpec.MinRangeCm, 10.0f);
    TestEqual(TEXT("Mid-360S 10 percent range cm"), LidarSpec.TypicalRangeCm, 4000.0f);
    TestEqual(TEXT("Mid-360S cutoff cm"), LidarSpec.MaxRangeCm, 10000.0f);
    TestEqual(TEXT("Mid-360S point rate"), LidarSpec.PointRate, 200000);
    Camera->ApplySimulationQuality(EVirtualSensorSimulationQuality::RealTimePreview);
    Lidar->ApplySimulationQuality(EVirtualSensorSimulationQuality::RealTimePreview);
    TestEqual(TEXT("D455 real-time preview resolution"), Camera->CaptureResolution, FIntPoint(640, 360));
    TestEqual(TEXT("Mid-360S real-time horizontal ray budget"), Lidar->HorizontalSamples, 120);
    TestEqual(TEXT("Mid-360S real-time vertical ray budget"), Lidar->VerticalChannels, 24);
    TestEqual(TEXT("Mid-360S real-time range remains realistic"), Lidar->MaxDistance, 4000.0f);
    return true;
}

bool FVirtualSensorEditableStateValidationTest::RunTest(const FString& Parameters)
{
    FVirtualSensorEditableState State;
    State.TargetKind = EVirtualSensorTargetKind::Camera;
    State.SensorId = TEXT("CAM-A");
    FString Error;
    TestTrue(TEXT("real-time camera state is accepted"), UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(State, {}, Error));

    TestFalse(TEXT("duplicate SensorId is rejected"), UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(State, { TEXT("CAM-A") }, Error));
    State.SensorId = TEXT("CAM-B");
    State.CameraResolution.X = 100;
    TestFalse(TEXT("out-of-range camera resolution is rejected"), UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(State, {}, Error));

    State.CameraResolution = FIntPoint(640, 360);
    State.ActorTransform.SetLocation(FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    TestFalse(TEXT("NaN transform is rejected"), UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(State, {}, Error));

    State.ActorTransform = FTransform::Identity;
    State.TargetKind = EVirtualSensorTargetKind::Lidar;
    State.SensorId = TEXT("LIDAR-A");
    State.LidarMinVerticalAngle = 52.0f;
    State.LidarMaxVerticalAngle = -7.0f;
    TestFalse(TEXT("inverted LiDAR vertical angles are rejected"), UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(State, {}, Error));
    return true;
}

bool FVirtualSensorStorageSummaryTest::RunTest(const FString& Parameters)
{
    UVirtualSensorCaptureExportPanelWidget* Widget = NewObject<UVirtualSensorCaptureExportPanelWidget>();
    const FString Summary = Widget->GetStorageSummaryText();
    TestTrue(TEXT("summary includes payload folder"), Summary.Contains(TEXT("ServerPayload")));
    TestTrue(TEXT("summary includes point cloud folder"), Summary.Contains(TEXT("PointCloud")));
    TestTrue(TEXT("summary includes timed camera and lidar"), Summary.Contains(TEXT("LocalTimedCapture")) && Summary.Contains(TEXT("Camera | Lidar")));
    TestTrue(TEXT("summary includes recorder folder"), Summary.Contains(TEXT("SensorRecordings")));
    TestFalse(TEXT("copy path fails clearly when no result exists"), Widget->CopyLastResultPath());
    TestTrue(TEXT("copy path failure is shown in summary"), Widget->GetStorageSummaryText().Contains(TEXT("복사할 최근 저장 경로가 없습니다")));
    return true;
}

bool FVirtualSensorGizmoMathTest::RunTest(const FString& Parameters)
{
    const FTransform Rotated(FRotator(0.0f, 90.0f, 0.0f), FVector(100.0f, 200.0f, 300.0f));
    const FVector LocalForward = AVirtualSensorTransformGizmoActor::ResolveAxisVector(Rotated, EVirtualSensorCoordinateSpace::Local, EAxis::X);
    TestTrue(TEXT("local X follows sensor yaw"), LocalForward.Equals(FVector::RightVector, 0.01f));
    const FVector WorldForward = AVirtualSensorTransformGizmoActor::ResolveAxisVector(Rotated, EVirtualSensorCoordinateSpace::World, EAxis::X);
    TestTrue(TEXT("world X ignores sensor yaw"), WorldForward.Equals(FVector::ForwardVector, 0.01f));

    const FTransform LocalMoved = AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(Rotated, EVirtualSensorCoordinateSpace::Local, FVector(10.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    TestTrue(TEXT("local W movement uses sensor forward"), LocalMoved.GetLocation().Equals(FVector(100.0f, 210.0f, 300.0f), 0.01f));
    const FTransform WorldMoved = AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(Rotated, EVirtualSensorCoordinateSpace::World, FVector(10.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    TestTrue(TEXT("world W movement uses world X"), WorldMoved.GetLocation().Equals(FVector(110.0f, 200.0f, 300.0f), 0.01f));

    const FTransform RotatedAgain = AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(FTransform::Identity, EVirtualSensorCoordinateSpace::World, FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f));
    TestTrue(TEXT("keyboard yaw rotates forward to world Y"), RotatedAgain.GetUnitAxis(EAxis::X).Equals(FVector::RightVector, 0.01f));
    return true;
}

bool FVirtualSensorDebugBudgetTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("projection debug ray budget remains performance safe"), AVirtualSensorTransformGizmoActor::ProjectionDebugRayBudget, 64);
    UVirtualLidarScanComponent* Lidar = NewObject<UVirtualLidarScanComponent>();
    TestFalse(TEXT("legacy per-ray debug remains off by default"), Lidar->bDrawDebugRays);
    return true;
}

bool FVirtualSensorUiPreferencesSerializationTest::RunTest(const FString& Parameters)
{
    UVirtualSensorUiPreferencesSaveGame* Preferences = NewObject<UVirtualSensorUiPreferencesSaveGame>();
    FVirtualSensorPanelUiState& Panel = Preferences->PanelStates.Add(TEXT("Monitor"));
    Panel.NormalizedPosition = FVector2D(0.75f, 0.25f);
    Panel.bHasSavedPosition = true;
    Panel.bCollapsed = true;
    Panel.ExpandedSize = FVector2D(1040.0f, 620.0f);
    Panel.bHasSavedSize = true;
    Preferences->LidarViewMode = static_cast<uint8>(EVirtualLidarViewMode::ActorClassColor);
    Preferences->LidarProjectionMode = ELidarMonitorProjectionMode::Split;
    Preferences->LidarColorMode = ELidarColorMode::DistanceViridis;
    Preferences->bShowWorldLidarPointCloud = false;
    Preferences->LidarPointSize = 3.5f;
    Preferences->bWorldTopDownAutoFit = false;
    Preferences->bMonitorDetailsExpanded = true;
    Preferences->bSettingsKeyboardHelpExpanded = true;
	Preferences->CaptureExportActiveTab = static_cast<uint8>(EVirtualSensorCaptureExportTab::ConnectionLog);
	Preferences->SensorStreamFrameStride = 3;
	Preferences->SensorStreamReceiptInterval = 10;

    TArray<uint8> Bytes;
    TestTrue(TEXT("UI preferences serialize to memory"), UGameplayStatics::SaveGameToMemory(Preferences, Bytes));
    UVirtualSensorUiPreferencesSaveGame* Loaded = Cast<UVirtualSensorUiPreferencesSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    TestNotNull(TEXT("UI preferences deserialize from memory"), Loaded);
    if (!Loaded) return false;
    const FVirtualSensorPanelUiState* LoadedPanel = Loaded->PanelStates.Find(TEXT("Monitor"));
    TestNotNull(TEXT("panel state survives serialization"), LoadedPanel);
    if (LoadedPanel)
    {
        TestEqual(TEXT("normalized position survives serialization"), LoadedPanel->NormalizedPosition, FVector2D(0.75f, 0.25f));
        TestTrue(TEXT("collapsed state survives serialization"), LoadedPanel->bCollapsed);
        TestTrue(TEXT("saved size flag survives serialization"), LoadedPanel->bHasSavedSize);
        TestEqual(TEXT("expanded size survives serialization"), LoadedPanel->ExpandedSize, FVector2D(1040.0f, 620.0f));
    }
    TestEqual(TEXT("LiDAR mode survives serialization"), Loaded->LidarViewMode, static_cast<uint8>(EVirtualLidarViewMode::ActorClassColor));
    TestEqual(TEXT("LiDAR projection survives serialization"), Loaded->LidarProjectionMode, ELidarMonitorProjectionMode::Split);
    TestEqual(TEXT("LiDAR color mode survives serialization"), Loaded->LidarColorMode, ELidarColorMode::DistanceViridis);
    TestFalse(TEXT("world point visibility survives serialization"), Loaded->bShowWorldLidarPointCloud);
    TestEqual(TEXT("point size survives serialization"), Loaded->LidarPointSize, 3.5f);
    TestFalse(TEXT("world top-down auto-fit survives serialization"), Loaded->bWorldTopDownAutoFit);
    TestTrue(TEXT("monitor detail state survives serialization"), Loaded->bMonitorDetailsExpanded);
    TestTrue(TEXT("keyboard help state survives serialization"), Loaded->bSettingsKeyboardHelpExpanded);
	TestEqual(TEXT("capture/export tab survives serialization"), Loaded->CaptureExportActiveTab, static_cast<uint8>(EVirtualSensorCaptureExportTab::ConnectionLog));
	TestEqual(TEXT("stream stride survives serialization"), Loaded->SensorStreamFrameStride, 3);
	TestEqual(TEXT("receipt sampling survives serialization"), Loaded->SensorStreamReceiptInterval, 10);
    return true;
}

bool FVirtualSensorCapturePanelResizeTest::RunTest(const FString& Parameters)
{
	const FVector2D Resized = UVirtualSensorPanelWidgetBase::CalculateResizedPanelSize(
		FVector2D(900.0f, 620.0f), FVector2D(200.0f, 100.0f), 1.0f,
		FVector2D(620.0f, 360.0f), FVector2D(1600.0f, 1000.0f));
	TestEqual(TEXT("capture panel grows from bottom-right grip"), Resized, FVector2D(1100.0f, 720.0f));
	const FVector2D Minimum = UVirtualSensorPanelWidgetBase::CalculateResizedPanelSize(
		FVector2D(900.0f, 620.0f), FVector2D(-1000.0f, -1000.0f), 1.0f,
		FVector2D(620.0f, 360.0f), FVector2D(1600.0f, 1000.0f));
	TestEqual(TEXT("capture panel respects its minimum size"), Minimum, FVector2D(620.0f, 360.0f));
	UVirtualSensorCaptureExportPanelWidget* Widget = NewObject<UVirtualSensorCaptureExportPanelWidget>();
	Widget->SetPanelResizable(true);
	TestTrue(TEXT("capture/export panel can opt into common resize behavior"), Widget->bPanelResizable);
	Widget->SetActiveTab(EVirtualSensorCaptureExportTab::Export);
	TestEqual(TEXT("four-tab selection is explicit"), Widget->GetActiveTab(), EVirtualSensorCaptureExportTab::Export);
	return true;
}

#endif
