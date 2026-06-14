#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorHostActor.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorHostFallbackTest, "M7AT10.SensorMonitor.HostNativeFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorLidarStatusTextTest, "M7AT10.SensorMonitor.LidarStatusTextContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorMonitorPerformanceWarningStatusTest, "M7AT10.SensorMonitor.PerformanceWarningStatusText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
    MonitorWidget->ShowLidarView();

    const FString TitleText = MonitorWidget->GetMonitorTitleText();
    const FString StatusText = MonitorWidget->GetMonitorStatusText();

    TestTrue(TEXT("title shows LiDAR view"), TitleText.Contains(TEXT("LIDAR")));
    TestTrue(TEXT("status includes selected sensor id"), StatusText.Contains(TEXT("TEST-LIDAR-MONITOR-STATUS")));
    TestTrue(TEXT("status includes frame id"), StatusText.Contains(TEXT("Frame:")));
    TestTrue(TEXT("status includes scan interval"), StatusText.Contains(TEXT("Scan: 0.125s")));
    TestTrue(TEXT("status includes ray count"), StatusText.Contains(TEXT("Rays=24")));
    TestTrue(TEXT("status includes measured hit count"), StatusText.Contains(TEXT("Measured Points/Hits: 24/24")));
    TestTrue(TEXT("status includes server payload policy"), StatusText.Contains(TEXT("Server Payload: Points=8 Stride=2 Max=8 IncludeMiss=false")));
    TestTrue(TEXT("status includes preview policy"), StatusText.Contains(TEXT("Preview: On Points=5 Stride=3 Max=5 HitOnly=true")));
    TestTrue(TEXT("status includes slab analysis"), StatusText.Contains(TEXT("Slab: Valid Points=24 Angle=")));
    TestTrue(TEXT("status includes transport warning row"), StatusText.Contains(TEXT("Transport/Warning:")));
    TestTrue(TEXT("status includes view mode"), StatusText.Contains(TEXT("LiDAR View:")));
    TestTrue(TEXT("status includes export CSV contract"), StatusText.Contains(TEXT("CSV: row,col,x,y,z")));
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

#endif
