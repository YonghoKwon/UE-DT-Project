#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorPerformanceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorPerformanceTierTest, "MA0T10.SensorPerformance.AutomaticTier", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorPerformanceFullSpecContractTest, "MA0T10.SensorPerformance.FullSpecContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorPerformanceLoadCalculationTest, "MA0T10.SensorPerformance.LoadCalculation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorSettingHelpCoverageTest, "MA0T10.SensorControl.SettingHelpCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorPerformanceTierTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("2 cameras and 2 lidars use the 60 FPS tier"), UVirtualSensorPerformanceSubsystem::ResolveTargetFps(2, 2), 60);
    TestEqual(TEXT("3 cameras move to the 30 FPS tier"), UVirtualSensorPerformanceSubsystem::ResolveTargetFps(3, 2), 30);
    TestEqual(TEXT("4 cameras and 4 lidars remain supported at 30 FPS"), UVirtualSensorPerformanceSubsystem::ResolveTargetFps(4, 4), 30);
    TestEqual(TEXT("60 FPS tier uses a four millisecond lidar budget"), UVirtualSensorPerformanceSubsystem::ResolveLidarBudgetMs(60), 4.0f);
    TestEqual(TEXT("30 FPS tier uses an eight millisecond lidar budget"), UVirtualSensorPerformanceSubsystem::ResolveLidarBudgetMs(30), 8.0f);
    TestFalse(TEXT("four of each sensor is not best effort"), UVirtualSensorPerformanceSubsystem::IsBestEffortConfiguration(4, 4));
    TestTrue(TEXT("five cameras is best effort"), UVirtualSensorPerformanceSubsystem::IsBestEffortConfiguration(5, 2));
    TestTrue(TEXT("five lidars is best effort"), UVirtualSensorPerformanceSubsystem::IsBestEffortConfiguration(2, 5));
    return true;
}

bool FVirtualSensorPerformanceFullSpecContractTest::RunTest(const FString& Parameters)
{
    UVirtualCameraComp* Camera = NewObject<UVirtualCameraComp>();
    UVirtualLidarSensorComp* Lidar = NewObject<UVirtualLidarSensorComp>();
    Camera->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
    Lidar->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);

    TestEqual(TEXT("FullSpec camera width"), Camera->CaptureResolution.X, 1280);
    TestEqual(TEXT("FullSpec camera height"), Camera->CaptureResolution.Y, 720);
    TestTrue(TEXT("FullSpec camera runs at 30 Hz"), FMath::IsNearlyEqual(Camera->CaptureInterval, 1.0f / 30.0f));
    TestEqual(TEXT("FullSpec lidar horizontal samples"), Lidar->HorizontalSamples, 360);
    TestEqual(TEXT("FullSpec lidar vertical channels"), Lidar->VerticalChannels, 60);
    TestTrue(TEXT("FullSpec lidar runs at 10 Hz"), FMath::IsNearlyEqual(Lidar->ScanInterval, 0.1f));
    return true;
}

bool FVirtualSensorPerformanceLoadCalculationTest::RunTest(const FString& Parameters)
{
    FVirtualSensorEditableState State;
    State.CameraResolution = FIntPoint(1280, 720);
    State.CameraCaptureInterval = 1.0f / 30.0f;
    State.LidarHorizontalSamples = 360;
    State.LidarVerticalChannels = 60;
    State.LidarScanInterval = 0.1f;
    const double CameraMpPerSecond = UVirtualSensorSettingsWidget::CalculateCameraMegaPixelsPerSecond(State);
    const double LidarRaysPerSecond = UVirtualSensorSettingsWidget::CalculateLidarRaysPerSecond(State);
    TestTrue(TEXT("camera MP/s is calculated from pixels and interval"), FMath::IsNearlyEqual(CameraMpPerSecond, 27.648, 0.001));
    TestTrue(TEXT("lidar rays/s is calculated from samples, channels, and interval"), FMath::IsNearlyEqual(LidarRaysPerSecond, 216000.0, 0.1));
    State.CameraCaptureInterval = 0.0f;
    State.LidarScanInterval = 0.0f;
    TestEqual(TEXT("zero camera interval is safe"), UVirtualSensorSettingsWidget::CalculateCameraMegaPixelsPerSecond(State), 0.0);
    TestEqual(TEXT("zero lidar interval is safe"), UVirtualSensorSettingsWidget::CalculateLidarRaysPerSecond(State), 0.0);
    return true;
}

bool FVirtualSensorSettingHelpCoverageTest::RunTest(const FString& Parameters)
{
    UVirtualSensorSettingsWidget* Widget = NewObject<UVirtualSensorSettingsWidget>();
    const TArray<FVirtualSensorSettingHelpDescriptor> Descriptors = Widget->GetAllSettingHelpDescriptors();
    TSet<FName> Keys;
    for (const FVirtualSensorSettingHelpDescriptor& Descriptor : Descriptors)
    {
        TestFalse(*FString::Printf(TEXT("%s has a key"), *Descriptor.Label.ToString()), Descriptor.Key.IsNone());
        TestFalse(*FString::Printf(TEXT("%s has a Korean label"), *Descriptor.Key.ToString()), Descriptor.Label.IsEmpty());
        TestFalse(*FString::Printf(TEXT("%s has a description"), *Descriptor.Key.ToString()), Descriptor.Description.IsEmpty());
        TestFalse(*FString::Printf(TEXT("%s has a unit"), *Descriptor.Key.ToString()), Descriptor.Unit.IsEmpty());
        TestFalse(*FString::Printf(TEXT("%s has a performance impact"), *Descriptor.Key.ToString()), Descriptor.PerformanceImpact.IsEmpty());
        TestFalse(*FString::Printf(TEXT("%s has a recommendation"), *Descriptor.Key.ToString()), Descriptor.RecommendedValue.IsEmpty());
        TestFalse(*FString::Printf(TEXT("%s is unique"), *Descriptor.Key.ToString()), Keys.Contains(Descriptor.Key));
        Keys.Add(Descriptor.Key);
    }

    const TArray<FName> RequiredKeys = {
        TEXT("SimulationQuality"), TEXT("CameraWidth"), TEXT("CameraHeight"), TEXT("CameraInterval"), TEXT("CameraFov"), TEXT("JpegQuality"), TEXT("CaptureMode"),
        TEXT("HorizontalSamples"), TEXT("VerticalChannels"), TEXT("LidarInterval"), TEXT("HorizontalFov"), TEXT("VerticalMin"), TEXT("VerticalMax"),
        TEXT("ServerStride"), TEXT("ServerMax"), TEXT("IncludeMisses"), TEXT("PreviewStride"), TEXT("PreviewMax"), TEXT("PreviewHitOnly"),
        TEXT("MultiHit"), TEXT("MaxHits"), TEXT("AutoExport")
    };
    for (const FName RequiredKey : RequiredKeys)
    {
        TestTrue(*FString::Printf(TEXT("help exists for %s"), *RequiredKey.ToString()), Keys.Contains(RequiredKey));
    }
    return true;
}

#endif
