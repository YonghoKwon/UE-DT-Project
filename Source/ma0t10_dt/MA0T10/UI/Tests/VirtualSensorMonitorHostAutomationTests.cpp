#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorHostFallbackTest, "MA0T10.SensorMonitor.HostNativeFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorCameraStatusTextTest, "MA0T10.SensorMonitor.CameraStatusTextContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorLidarStatusTextTest, "MA0T10.SensorMonitor.LidarStatusTextContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorPerformanceWarningStatusTest, "MA0T10.SensorMonitor.PerformanceWarningStatusText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorServerPayloadExportTest, "MA0T10.SensorMonitor.ServerPayloadExport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorThemeAndLidarModesTest, "MA0T10.SensorMonitor.ThemeAndLidarModes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorMonitorHostFallbackTest::RunTest(const FString& Parameters)
{
    AVirtualSensorUiHostActor* HostActor = NewObject<AVirtualSensorUiHostActor>();
    TestNotNull(TEXT("monitor host actor"), HostActor);

    HostActor->MonitorWidgetClass = nullptr;
    HostActor->bUseNativeMonitorWidgetFallback = true;
    TestEqual(TEXT("native fallback class"), HostActor->GetEffectiveMonitorWidgetClass(), TSubclassOf<UVirtualSensorMonitorPanelWidget>(UVirtualSensorMonitorPanelWidget::StaticClass()));

    HostActor->bUseNativeMonitorWidgetFallback = false;
    TestNull(TEXT("fallback disabled returns null"), HostActor->GetEffectiveMonitorWidgetClass().Get());

    HostActor->MonitorWidgetClass = UVirtualSensorMonitorPanelWidget::StaticClass();
    TestEqual(TEXT("explicit class wins"), HostActor->GetEffectiveMonitorWidgetClass(), TSubclassOf<UVirtualSensorMonitorPanelWidget>(UVirtualSensorMonitorPanelWidget::StaticClass()));
    return true;
}

bool FVirtualSensorMonitorCameraStatusTextTest::RunTest(const FString& Parameters)
{
    UVirtualCameraCaptureComponent* CameraComp = NewObject<UVirtualCameraCaptureComponent>();
    UVirtualSensorMonitorPanelWidget* MonitorWidget = NewObject<UVirtualSensorMonitorPanelWidget>();
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

    TestTrue(TEXT("title shows camera view"), TitleText.Contains(TEXT("카메라")));
    TestFalse(TEXT("monitor reports camera mode"), MonitorWidget->IsShowingLidar());
    TestTrue(TEXT("monitor reports bound camera"), MonitorWidget->HasBoundCamera());
    TestFalse(TEXT("monitor reports no bound lidar"), MonitorWidget->HasBoundLidar());
    TestTrue(TEXT("camera status includes selected sensor id"), StatusText.Contains(TEXT("TEST-CAMERA-MONITOR-STATUS")));
    TestTrue(TEXT("camera status includes schema version"), StatusText.Contains(TEXT("스키마: virtual-camera.v1")));
    TestTrue(TEXT("camera status includes resolution"), StatusText.Contains(TEXT("해상도:")) && StatusText.Contains(TEXT("800")) && StatusText.Contains(TEXT("450")));
    TestTrue(TEXT("camera status includes capture mode"), StatusText.Contains(TEXT("캡처: 모드=")));
    TestTrue(TEXT("camera status includes cached payload flag"), StatusText.Contains(TEXT("Payload 캐시: 없음")));
    TestFalse(TEXT("monitor status omits capture/export controls"), StatusText.Contains(TEXT("Export Payload")) || StatusText.Contains(TEXT("CaptureOnce")));
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
    ULidarCsvReplaySourceComponent* ReplayComp = NewObject<ULidarCsvReplaySourceComponent>();
    UVirtualLidarScanComponent* LidarComp = NewObject<UVirtualLidarScanComponent>();
    UVirtualSensorMonitorPanelWidget* MonitorWidget = NewObject<UVirtualSensorMonitorPanelWidget>();
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

    TestTrue(TEXT("title shows LiDAR view"), TitleText.Contains(TEXT("LiDAR")));
    TestTrue(TEXT("monitor reports lidar mode"), MonitorWidget->IsShowingLidar());
    TestTrue(TEXT("monitor reports bound lidar"), MonitorWidget->HasBoundLidar());
    TestFalse(TEXT("monitor reports no bound camera"), MonitorWidget->HasBoundCamera());
    TestTrue(TEXT("status includes selected sensor id"), StatusText.Contains(TEXT("TEST-LIDAR-MONITOR-STATUS")));
    TestTrue(TEXT("status includes frame id"), StatusText.Contains(TEXT("프레임:")));
    TestTrue(TEXT("status includes scan interval"), StatusText.Contains(TEXT("스캔 주기: 0.125초")));
    TestTrue(TEXT("status includes ray count"), StatusText.Contains(TEXT("광선=24")));
    TestTrue(TEXT("status includes measured hit count"), StatusText.Contains(TEXT("측정점/검출점: 24 / 24")));
    TestTrue(TEXT("status includes server payload policy"), StatusText.Contains(TEXT("서버 Payload: 점=8 바이트=")) && StatusText.Contains(TEXT("간격=2 최대=8 미검출점=아니요")));
    TestTrue(TEXT("status includes preview policy"), StatusText.Contains(TEXT("미리보기: 켜짐 점=5 간격=3 최대=5 검출점만=예")));
    TestTrue(TEXT("status includes slab analysis"), StatusText.Contains(TEXT("Slab 분석: 유효 점=24 각도=")));
    TestTrue(TEXT("status includes laz export telemetry row"), StatusText.Contains(TEXT("LAZ Export: Attempted=false")));
    TestTrue(TEXT("status includes transport warning row"), StatusText.Contains(TEXT("전송/경고:")));
    TestTrue(TEXT("status includes view mode"), StatusText.Contains(TEXT("LiDAR 표시:")));
    TestTrue(TEXT("status includes acceptance gate row"), StatusText.Contains(TEXT("Acceptance Gates:")) && StatusText.Contains(TEXT("LAZ=TrueCompressionPending")));
    TestTrue(TEXT("status includes real sensor deployment row"), StatusText.Contains(TEXT("Deployment Gate: Source=TEST-REAL-SENSOR-REPLAY")));
    TestTrue(TEXT("status includes export CSV contract"), StatusText.Contains(TEXT("CSV 열: row,col,returnIndex,x,y,z")));
    TestTrue(TEXT("lidar selected sensor getter exposes sensor id"), MonitorWidget->GetSelectedSensorIdText().Contains(TEXT("TEST-LIDAR-MONITOR-STATUS")));
    TestTrue(TEXT("lidar frame summary getter exposes scan interval"), MonitorWidget->GetFrameSummaryText().Contains(TEXT("Scan: 0.125s")));
    TestTrue(TEXT("lidar measurement getter exposes rays and hits"), MonitorWidget->GetMeasurementSummaryText().Contains(TEXT("Rays: 24")) && MonitorWidget->GetMeasurementSummaryText().Contains(TEXT("Points/Hits: 24/24")));
    TestTrue(TEXT("lidar payload getter exposes server policy"), MonitorWidget->GetServerPayloadSummaryText().Contains(TEXT("Points=8")) && MonitorWidget->GetServerPayloadSummaryText().Contains(TEXT("Stride=2 Max=8")));
    TestTrue(TEXT("lidar preview getter exposes preview policy"), MonitorWidget->GetPreviewPolicySummaryText().Contains(TEXT("Points=5")) && MonitorWidget->GetPreviewPolicySummaryText().Contains(TEXT("HitOnly=true")));
    TestTrue(TEXT("lidar slab getter exposes slab analysis"), MonitorWidget->GetSlabAnalysisSummaryText().Contains(TEXT("Slab: Valid")) && MonitorWidget->GetSlabAnalysisSummaryText().Contains(TEXT("Points=24")));
    TestTrue(TEXT("lidar laz getter exposes initial state"), MonitorWidget->GetLazExportSummaryText().Contains(TEXT("Attempted=false")));
    TestTrue(TEXT("lidar warning getter exposes Korean warning row"), MonitorWidget->GetTransportWarningText().Contains(TEXT("상태/경고:")));
    TestTrue(TEXT("lidar view getter exposes view mode"), MonitorWidget->GetViewModeSummaryText().Contains(TEXT("LiDAR View:")));
    TestTrue(TEXT("lidar acceptance gate getter exposes cached server payload"), MonitorWidget->GetAcceptanceGateSummaryText().Contains(TEXT("Server=PayloadCached")));
    TestTrue(TEXT("lidar real sensor getter exposes replay baseline"), MonitorWidget->GetRealSensorDeploymentSummaryText().Contains(TEXT("Replay baseline")));
    const FVirtualSensorMonitorDisplayData LidarDisplayData = MonitorWidget->GetMonitorDisplayData();
    TestTrue(TEXT("lidar display data marks lidar mode"), LidarDisplayData.bShowingLidar);
    TestTrue(TEXT("lidar display data exposes sensor"), LidarDisplayData.SelectedSensorText.Contains(TEXT("TEST-LIDAR-MONITOR-STATUS")));
    TestTrue(TEXT("lidar display data exposes payload"), LidarDisplayData.ServerPayloadText.Contains(TEXT("Points=8")));
    TestTrue(TEXT("lidar display data exposes preview independently"), LidarDisplayData.PreviewText.Contains(TEXT("Points=5")));
    TestTrue(TEXT("full status simultaneously exposes server and preview counts"),
        LidarDisplayData.FullStatusText.Contains(TEXT("서버 Payload: 점=8")) &&
        LidarDisplayData.FullStatusText.Contains(TEXT("미리보기: 켜짐 점=5")));
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
    TestTrue(TEXT("status reflects preview hit-only toggle"), ToggledStatusText.Contains(TEXT("미리보기: 켜짐")) && ToggledStatusText.Contains(TEXT("간격=3 최대=5 검출점만=아니요")));
    TestTrue(TEXT("preview getter reflects hit-only toggle"), MonitorWidget->GetPreviewPolicySummaryText().Contains(TEXT("HitOnly=false")));
    TestEqual(TEXT("server payload stride unchanged after preview hit-only toggle"), LidarComp->ServerPayloadStride, 2);
    TestEqual(TEXT("server payload max unchanged after preview hit-only toggle"), LidarComp->MaxServerPayloadPoints, 8);
    return true;
}

bool FVirtualSensorMonitorPerformanceWarningStatusTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComponent* ReplayComp = NewObject<ULidarCsvReplaySourceComponent>();
    UVirtualLidarScanComponent* LidarComp = NewObject<UVirtualLidarScanComponent>();
    UVirtualSensorMonitorPanelWidget* MonitorWidget = NewObject<UVirtualSensorMonitorPanelWidget>();
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
    TestTrue(TEXT("monitor transport warning row includes warning"), StatusText.Contains(TEXT("전송/경고:")) && StatusText.Contains(Warning));
    return true;
}

bool FVirtualSensorMonitorServerPayloadExportTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComponent* ReplayComp = NewObject<ULidarCsvReplaySourceComponent>();
    UVirtualLidarScanComponent* LidarComp = NewObject<UVirtualLidarScanComponent>();
    UVirtualSensorMonitorPanelWidget* MonitorWidget = NewObject<UVirtualSensorMonitorPanelWidget>();
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
    TestFalse(TEXT("monitor status excludes capture/export result ownership"), MonitorWidget->GetMonitorStatusText().Contains(TEXT("LiDAR Server Payload Export: saved")));

    IFileManager::Get().Delete(*ExportPath, false, true);
    return true;
}

bool FVirtualSensorMonitorThemeAndLidarModesTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("panel is intentionally translucent"), FVirtualSensorUiStyle::PanelBackground.A > 0.9f && FVirtualSensorUiStyle::PanelBackground.A < 1.0f);
    TestTrue(TEXT("primary text meets contrast target"), FVirtualSensorUiStyle::ContrastRatio(FVirtualSensorUiStyle::PrimaryText, FVirtualSensorUiStyle::PanelBackground) >= 4.5f);

    UVirtualLidarScanComponent* Lidar = NewObject<UVirtualLidarScanComponent>();
    Lidar->SemanticClassRules.Reset();
    FVirtualLidarSemanticClassRule Rule;
    Rule.Label = TEXT("Slab");
    Rule.DisplayColor = FLinearColor(0.8f, 0.1f, 0.2f, 1.0f);
    Lidar->SemanticClassRules.Add(Rule);

    FVirtualLidarPoint Point;
    Point.bHit = true;
    Point.SemanticLabel = TEXT("Slab");
    const FColor SemanticColor = UVirtualSensorMonitorPanelWidget::ResolveLidarPointDisplayColor(Lidar, EVirtualLidarViewMode::ActorClassColor, Point, 0.5f);
    TestEqual(TEXT("semantic mode uses configured class color"), SemanticColor, Rule.DisplayColor.ToFColor(true));
    TestEqual(TEXT("hit mask shows a hit as white"), UVirtualSensorMonitorPanelWidget::ResolveLidarPointDisplayColor(Lidar, EVirtualLidarViewMode::HitMask, Point, 0.5f), FColor::White);
    Point.bHit = false;
    TestEqual(TEXT("hit mask shows a miss as black"), UVirtualSensorMonitorPanelWidget::ResolveLidarPointDisplayColor(Lidar, EVirtualLidarViewMode::HitMask, Point, 1.0f), FColor::Black);

    UVirtualSensorMonitorPanelWidget* Monitor = NewObject<UVirtualSensorMonitorPanelWidget>();
    Monitor->BindVirtualLidar(Lidar);
    Monitor->SetLidarViewMode(EVirtualLidarViewMode::ActorClassColor);
    TestTrue(TEXT("semantic mode has Korean help"), Monitor->GetLidarViewModeDescription().Contains(TEXT("SemanticLabel")));
    TestTrue(TEXT("semantic legend lists configured label"), Monitor->GetLidarViewLegendText().Contains(TEXT("Slab")));
    return true;
}

#endif
