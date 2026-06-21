#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorHostActor.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorHostFallbackTest, "M7AT10.SensorMonitor.HostNativeFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorCameraStatusTextTest, "M7AT10.SensorMonitor.CameraStatusTextContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorLidarStatusTextTest, "M7AT10.SensorMonitor.LidarStatusTextContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorPerformanceWarningStatusTest, "M7AT10.SensorMonitor.PerformanceWarningStatusText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorServerPayloadExportTest, "M7AT10.SensorMonitor.ServerPayloadExport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorMonitorHostFallbackTest::RunTest(const FString& Parameters)
{
    AVirtualSensorMonitorHostActor* HostActor = NewObject<AVirtualSensorMonitorHostActor>();
    TestNotNull(TEXT("monitor host actor"), HostActor);

    HostActor->MonitorWidgetClass = nullptr;
    HostActor->bUseNativeMonitorWidgetFallback = true;
    TestEqual(TEXT("native fallback class"), HostActor->GetEffectiveMonitorWidgetClass(), TSubclassOf<UVirtualSensorMonitorWidget>(UVirtualSensorMonitorWidget::StaticClass()));

    HostActor->bUseNativeMonitorWidgetFallback = false;
    TestNull(TEXT("fallback disabled returns null"), HostActor->GetEffectiveMonitorWidgetClass().Get());

    HostActor->MonitorWidgetClass = UVirtualSensorMonitorWidget::StaticClass();
    TestEqual(TEXT("explicit class wins"), HostActor->GetEffectiveMonitorWidgetClass(), TSubclassOf<UVirtualSensorMonitorWidget>(UVirtualSensorMonitorWidget::StaticClass()));
    return true;
}

bool FVirtualSensorMonitorCameraStatusTextTest::RunTest(const FString& Parameters)
{
    UVirtualCameraComp* CameraComp = NewObject<UVirtualCameraComp>();
    UVirtualSensorMonitorWidget* MonitorWidget = NewObject<UVirtualSensorMonitorWidget>();
    TestNotNull(TEXT("camera component"), CameraComp);
    TestNotNull(TEXT("monitor widget"), MonitorWidget);
    if (!CameraComp || !MonitorWidget)
    {
        return false;
    }

    CameraComp->SensorId = TEXT("TEST-CAMERA-MONITOR-STATUS");
    CameraComp->CaptureResolution = FIntPoint(800, 450);
    CameraComp->CaptureInterval = 0.2f;
    CameraComp->CaptureMode = EVirtualCameraCaptureMode::Payload;
    CameraComp->ApplySimulationQuality(EVirtualSensorSimulationQuality::Debug);
    CameraComp->CaptureResolution = FIntPoint(800, 450);
    CameraComp->CaptureInterval = 0.2f;

    MonitorWidget->BindVirtualCamera(CameraComp);
    MonitorWidget->ShowCameraView();

    const FString TitleText = MonitorWidget->GetMonitorTitleText();
    const FString StatusText = MonitorWidget->GetMonitorStatusText();

    TestTrue(TEXT("title shows camera view"), TitleText.Contains(TEXT("Camera")));
    TestFalse(TEXT("monitor reports camera mode"), MonitorWidget->IsShowingLidar());
    TestTrue(TEXT("monitor reports bound camera"), MonitorWidget->HasBoundCamera());
    TestFalse(TEXT("monitor reports no bound lidar"), MonitorWidget->HasBoundLidar());
    TestTrue(TEXT("camera status includes selected sensor id"), StatusText.Contains(TEXT("TEST-CAMERA-MONITOR-STATUS")));
    TestTrue(TEXT("camera status includes schema version"), StatusText.Contains(TEXT("Schema: virtual-camera.v1")));
    TestTrue(TEXT("camera status includes resolution"), StatusText.Contains(TEXT("Resolution: 800x450")));
    TestTrue(TEXT("camera status includes capture mode"), StatusText.Contains(TEXT("Capture: Mode=")));
    TestTrue(TEXT("camera status includes cached payload flag"), StatusText.Contains(TEXT("Cached=false")));
    TestTrue(TEXT("camera status includes export path hint"), StatusText.Contains(TEXT("ServerPayload")));
    TestTrue(TEXT("camera selected sensor getter exposes sensor id"), MonitorWidget->GetSelectedSensorIdText().Contains(TEXT("TEST-CAMERA-MONITOR-STATUS")));
    TestTrue(TEXT("camera frame summary getter exposes frame"), MonitorWidget->GetFrameSummaryText().Contains(TEXT("Frame:")));
    TestTrue(TEXT("camera measurement getter exposes resolution"), MonitorWidget->GetMeasurementSummaryText().Contains(TEXT("Resolution: 800x450")));
    TestTrue(TEXT("camera payload getter exposes cached flag"), MonitorWidget->GetServerPayloadSummaryText().Contains(TEXT("Cached=false")));
    TestTrue(TEXT("camera view mode getter exposes render target"), MonitorWidget->GetViewModeSummaryText().Contains(TEXT("Camera View")));
    TestTrue(TEXT("camera acceptance gate getter exposes pending evidence"), MonitorWidget->GetAcceptanceGateSummaryText().Contains(TEXT("Acceptance Gates:")) && MonitorWidget->GetAcceptanceGateSummaryText().Contains(TEXT("PayloadPending")));
    TestTrue(TEXT("camera real sensor getter exposes unbound deployment state"), MonitorWidget->GetRealSensorDeploymentSummaryText().Contains(TEXT("source not bound")));
    const FVirtualSensorMonitorDisplayData CameraDisplayData = MonitorWidget->GetMonitorDisplayData();
    TestFalse(TEXT("camera display data marks camera mode"), CameraDisplayData.bShowingLidar);
    TestTrue(TEXT("camera display data exposes sensor"), CameraDisplayData.SelectedSensorText.Contains(TEXT("TEST-CAMERA-MONITOR-STATUS")));
    TestTrue(TEXT("camera display data exposes payload"), CameraDisplayData.ServerPayloadText.Contains(TEXT("Cached=false")));
    TestTrue(TEXT("camera display data exposes acceptance gates"), CameraDisplayData.AcceptanceGateText.Contains(TEXT("RealSensor=DeploymentEvidencePending")));
    TestTrue(TEXT("camera display data exposes real sensor deployment state"), CameraDisplayData.RealSensorText.Contains(TEXT("source not bound")));
    return true;
}

bool FVirtualSensorMonitorLidarStatusTextTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    UVirtualSensorMonitorWidget* MonitorWidget = NewObject<UVirtualSensorMonitorWidget>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    TestNotNull(TEXT("monitor widget"), MonitorWidget);
    if (!ReplayComp || !LidarComp || !MonitorWidget)
    {
        return false;
    }

    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");
    ReplayComp->SourceId = TEXT("TEST-REAL-SENSOR-REPLAY");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads before monitor status test"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-MONITOR-STATUS");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->ScanInterval = 0.125f;
    LidarComp->SlabSemanticLabel = TEXT("Slab");
    LidarComp->MinSlabPointsForAnalysis = 3;
    LidarComp->SetServerPayloadPolicy(2, 8, false);
    LidarComp->SetPreviewPolicy(3, 5, true);
    LidarComp->bPointCloudPreviewEnabled = true;
    LidarComp->InjectPointCloudFrame(Points, false);

    MonitorWidget->BindVirtualLidar(LidarComp);
    MonitorWidget->BindRealSensorSource(ReplayComp);
    MonitorWidget->ShowLidarView();

    const FString TitleText = MonitorWidget->GetMonitorTitleText();
    const FString StatusText = MonitorWidget->GetMonitorStatusText();

    TestTrue(TEXT("title shows LiDAR view"), TitleText.Contains(TEXT("LIDAR")));
    TestTrue(TEXT("monitor reports lidar mode"), MonitorWidget->IsShowingLidar());
    TestTrue(TEXT("monitor reports bound lidar"), MonitorWidget->HasBoundLidar());
    TestFalse(TEXT("monitor reports no bound camera"), MonitorWidget->HasBoundCamera());
    TestTrue(TEXT("status includes selected sensor id"), StatusText.Contains(TEXT("TEST-LIDAR-MONITOR-STATUS")));
    TestTrue(TEXT("status includes frame id"), StatusText.Contains(TEXT("Frame:")));
    TestTrue(TEXT("status includes scan interval"), StatusText.Contains(TEXT("Scan: 0.125s")));
    TestTrue(TEXT("status includes ray count"), StatusText.Contains(TEXT("Rays=24")));
    TestTrue(TEXT("status includes measured hit count"), StatusText.Contains(TEXT("Measured Points/Hits: 24/24")));
    TestTrue(TEXT("status includes server payload policy"), StatusText.Contains(TEXT("Server Payload: Points=8 Bytes=")) && StatusText.Contains(TEXT("Stride=2 Max=8 IncludeMiss=false")));
    TestTrue(TEXT("status includes preview policy"), StatusText.Contains(TEXT("Preview: On Points=5 Stride=3 Max=5 HitOnly=true")));
    TestTrue(TEXT("status includes slab analysis"), StatusText.Contains(TEXT("Slab: Valid Points=24 Angle=")));
    TestTrue(TEXT("status includes laz export telemetry row"), StatusText.Contains(TEXT("LAZ Export: Attempted=false")));
    TestTrue(TEXT("status includes transport warning row"), StatusText.Contains(TEXT("Transport/Warning:")));
    TestTrue(TEXT("status includes view mode"), StatusText.Contains(TEXT("LiDAR View:")));
    TestTrue(TEXT("status includes acceptance gate row"), StatusText.Contains(TEXT("Acceptance Gates:")) && StatusText.Contains(TEXT("LAZ=TrueCompressionPending")));
    TestTrue(TEXT("status includes real sensor deployment row"), StatusText.Contains(TEXT("Deployment Gate: Source=TEST-REAL-SENSOR-REPLAY")));
    TestTrue(TEXT("status includes export CSV contract"), StatusText.Contains(TEXT("CSV: row,col,returnIndex,x,y,z")));
    TestTrue(TEXT("lidar selected sensor getter exposes sensor id"), MonitorWidget->GetSelectedSensorIdText().Contains(TEXT("TEST-LIDAR-MONITOR-STATUS")));
    TestTrue(TEXT("lidar frame summary getter exposes scan interval"), MonitorWidget->GetFrameSummaryText().Contains(TEXT("Scan: 0.125s")));
    TestTrue(TEXT("lidar measurement getter exposes rays and hits"), MonitorWidget->GetMeasurementSummaryText().Contains(TEXT("Rays: 24")) && MonitorWidget->GetMeasurementSummaryText().Contains(TEXT("Points/Hits: 24/24")));
    TestTrue(TEXT("lidar payload getter exposes server policy"), MonitorWidget->GetServerPayloadSummaryText().Contains(TEXT("Points=8")) && MonitorWidget->GetServerPayloadSummaryText().Contains(TEXT("Stride=2 Max=8")));
    TestTrue(TEXT("lidar preview getter exposes preview policy"), MonitorWidget->GetPreviewPolicySummaryText().Contains(TEXT("Points=5")) && MonitorWidget->GetPreviewPolicySummaryText().Contains(TEXT("HitOnly=true")));
    TestTrue(TEXT("lidar slab getter exposes slab analysis"), MonitorWidget->GetSlabAnalysisSummaryText().Contains(TEXT("Slab: Valid")) && MonitorWidget->GetSlabAnalysisSummaryText().Contains(TEXT("Points=24")));
    TestTrue(TEXT("lidar laz getter exposes initial state"), MonitorWidget->GetLazExportSummaryText().Contains(TEXT("Attempted=false")));
    TestTrue(TEXT("lidar warning getter exposes warning row"), MonitorWidget->GetTransportWarningText().Contains(TEXT("Transport/Warning:")));
    TestTrue(TEXT("lidar view getter exposes view mode"), MonitorWidget->GetViewModeSummaryText().Contains(TEXT("LiDAR View:")));
    TestTrue(TEXT("lidar acceptance gate getter exposes cached server payload"), MonitorWidget->GetAcceptanceGateSummaryText().Contains(TEXT("Server=PayloadCached")));
    TestTrue(TEXT("lidar real sensor getter exposes replay baseline"), MonitorWidget->GetRealSensorDeploymentSummaryText().Contains(TEXT("Replay baseline")));
    const FVirtualSensorMonitorDisplayData LidarDisplayData = MonitorWidget->GetMonitorDisplayData();
    TestTrue(TEXT("lidar display data marks lidar mode"), LidarDisplayData.bShowingLidar);
    TestTrue(TEXT("lidar display data exposes sensor"), LidarDisplayData.SelectedSensorText.Contains(TEXT("TEST-LIDAR-MONITOR-STATUS")));
    TestTrue(TEXT("lidar display data exposes payload"), LidarDisplayData.ServerPayloadText.Contains(TEXT("Points=8")));
    TestTrue(TEXT("lidar display data exposes slab"), LidarDisplayData.SlabText.Contains(TEXT("Slab: Valid")));
    TestTrue(TEXT("lidar display data exposes laz export"), LidarDisplayData.LazExportText.Contains(TEXT("LAZ Export:")));
    TestTrue(TEXT("lidar display data exposes acceptance gates"), LidarDisplayData.AcceptanceGateText.Contains(TEXT("WBP=ManualPIEPending")));
    TestTrue(TEXT("lidar display data exposes real sensor deployment"), LidarDisplayData.RealSensorText.Contains(TEXT("TEST-REAL-SENSOR-REPLAY")));

    TestTrue(TEXT("monitor laz export telemetry updates after placeholder export"), LidarComp->ExportLastPointCloudLaz(TEXT("automation_monitor_laz")));
    const FString LazExportText = MonitorWidget->GetLazExportSummaryText();
    TestTrue(TEXT("monitor laz getter exposes placeholder success"), LazExportText.Contains(TEXT("Attempted=true")) && LazExportText.Contains(TEXT("Placeholder=true")));
    TestTrue(TEXT("monitor laz getter preserves true validation boundary"), LazExportText.Contains(TEXT("TrueValidated=false")));

    MonitorWidget->ToggleLidarPreviewHitOnly();
    const FString ToggledStatusText = MonitorWidget->GetMonitorStatusText();
    TestFalse(TEXT("preview hit-only toggles off"), LidarComp->bPointCloudPreviewHitOnly);
    TestTrue(TEXT("status reflects preview hit-only toggle"), ToggledStatusText.Contains(TEXT("Preview: On")) && ToggledStatusText.Contains(TEXT("Stride=3 Max=5 HitOnly=false")));
    TestTrue(TEXT("preview getter reflects hit-only toggle"), MonitorWidget->GetPreviewPolicySummaryText().Contains(TEXT("HitOnly=false")));
    TestEqual(TEXT("server payload stride unchanged after preview hit-only toggle"), LidarComp->ServerPayloadStride, 2);
    TestEqual(TEXT("server payload max unchanged after preview hit-only toggle"), LidarComp->MaxServerPayloadPoints, 8);
    return true;
}

bool FVirtualSensorMonitorPerformanceWarningStatusTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    UVirtualSensorMonitorWidget* MonitorWidget = NewObject<UVirtualSensorMonitorWidget>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    TestNotNull(TEXT("monitor widget"), MonitorWidget);
    if (!ReplayComp || !LidarComp || !MonitorWidget)
    {
        return false;
    }

    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads before monitor warning status test"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-MONITOR-WARNING");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->SimulationQuality = EVirtualSensorSimulationQuality::FullSpec;
    LidarComp->bUseMultiHit = true;
    LidarComp->bExportCsvOnScan = true;
    LidarComp->bPointCloudPreviewEnabled = true;
    LidarComp->SetPreviewPolicy(1, 0, true);
    LidarComp->InjectPointCloudFrame(Points, false);

    MonitorWidget->BindVirtualLidar(LidarComp);
    MonitorWidget->ShowLidarView();

    const FString Warning = LidarComp->GetPerformanceWarning();
    const FString StatusText = MonitorWidget->GetMonitorStatusText();

    TestFalse(TEXT("lidar warning is populated"), Warning.IsEmpty());
    TestTrue(TEXT("monitor status includes fullspec multihit warning"), StatusText.Contains(TEXT("FullSpec+MultiHit")));
    TestTrue(TEXT("monitor status includes export warning"), StatusText.Contains(TEXT("FullSpec export-on-scan")));
    TestTrue(TEXT("monitor status includes uncapped preview warning"), StatusText.Contains(TEXT("Preview is uncapped")));
    TestTrue(TEXT("monitor transport warning row includes warning"), StatusText.Contains(TEXT("Transport/Warning:")) && StatusText.Contains(Warning));
    return true;
}

bool FVirtualSensorMonitorServerPayloadExportTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    UVirtualSensorMonitorWidget* MonitorWidget = NewObject<UVirtualSensorMonitorWidget>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    TestNotNull(TEXT("monitor widget"), MonitorWidget);
    if (!ReplayComp || !LidarComp || !MonitorWidget)
    {
        return false;
    }

    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads before server payload export test"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-MONITOR-PAYLOAD-EXPORT");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->SlabSemanticLabel = TEXT("Slab");
    LidarComp->MinSlabPointsForAnalysis = 3;
    LidarComp->SetServerPayloadPolicy(2, 8, false);
    LidarComp->SetPreviewPolicy(3, 5, true);
    LidarComp->InjectPointCloudFrame(Points, false);

    MonitorWidget->BindVirtualLidar(LidarComp);
    MonitorWidget->ShowLidarView();

    TestTrue(TEXT("server payload export succeeds"), MonitorWidget->ExportSelectedLidarServerPayload(TEXT("automation_server_payload")));
    const FString ExportPath = MonitorWidget->GetLastManualExportPath();
    TestFalse(TEXT("server payload export path is populated"), ExportPath.IsEmpty());
    TestTrue(TEXT("server payload export message is populated"), MonitorWidget->GetLastManualExportMessage().Contains(TEXT("LiDAR Server Payload Export: saved")));
    TestTrue(TEXT("server payload export file exists"), FPaths::FileExists(ExportPath));

    FString SavedPayload;
    TestTrue(TEXT("server payload export file loads"), FFileHelper::LoadFileToString(SavedPayload, *ExportPath));
    TestEqual(TEXT("server payload export matches last payload"), SavedPayload, LidarComp->GetLastJsonPayload());
    TestTrue(TEXT("monitor status includes server payload export result"), MonitorWidget->GetMonitorStatusText().Contains(TEXT("LiDAR Server Payload Export: saved")));

    IFileManager::Get().Delete(*ExportPath, false, true);
    return true;
}

#endif
