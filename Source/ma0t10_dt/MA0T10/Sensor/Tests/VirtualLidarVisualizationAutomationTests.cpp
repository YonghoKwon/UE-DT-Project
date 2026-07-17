#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarColorModeTest, "MA0T10.SensorV2.LidarVisualization.ColorModes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarProjectionMathTest, "MA0T10.SensorV2.LidarVisualization.ProjectionMath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarLegacyViewMappingTest, "MA0T10.SensorV2.LidarVisualization.LegacyMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarProjectionNavigationTest, "MA0T10.SensorV2.LidarVisualization.ProjectionNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualLidarColorModeTest::RunTest(const FString& Parameters)
{
    UVirtualLidarScanComponent* Scan = NewObject<UVirtualLidarScanComponent>();
    Scan->VerticalChannels = 4;
    FVirtualLidarPoint Point;
    Point.bHit = true;
    Point.Row = 3;
    Point.ReturnIndex = 2;
    Point.SemanticLabel = TEXT("Unknown");

    const FColor TurboNear = UVirtualLidarVisualizationComponent::ResolveDisplayColor(Scan, ELidarColorMode::DistanceTurbo, Point, 0.0f, 0.5f);
    const FColor TurboFar = UVirtualLidarVisualizationComponent::ResolveDisplayColor(Scan, ELidarColorMode::DistanceTurbo, Point, 1.0f, 0.5f);
    TestNotEqual(TEXT("Turbo distance endpoints differ"), TurboNear, TurboFar);
    TestNotEqual(TEXT("Viridis differs from Turbo"), UVirtualLidarVisualizationComponent::ResolveDisplayColor(Scan, ELidarColorMode::DistanceViridis, Point, 0.5f, 0.5f), UVirtualLidarVisualizationComponent::ResolveDisplayColor(Scan, ELidarColorMode::DistanceTurbo, Point, 0.5f, 0.5f));
    TestEqual(TEXT("hit mask is white for a hit"), UVirtualLidarVisualizationComponent::ResolveDisplayColor(Scan, ELidarColorMode::HitMask, Point, 0.5f, 0.5f), FColor::White);
    Point.bHit = false;
    TestEqual(TEXT("hit mask is black for a miss"), UVirtualLidarVisualizationComponent::ResolveDisplayColor(Scan, ELidarColorMode::HitMask, Point, 1.0f, 0.5f), FColor::Black);
    return true;
}

bool FVirtualLidarProjectionMathTest::RunTest(const FString& Parameters)
{
    const FIntPoint CenterForward = UVirtualLidarVisualizationComponent::ProjectTopDown(FVector(500.0, 0.0, 0.0), 1000.0f, 101);
    TestEqual(TEXT("top-down forward point is centered horizontally"), CenterForward.X, 50);
    TestEqual(TEXT("top-down half range is halfway up"), CenterForward.Y, 50);
    const FIntPoint Elevated = UVirtualLidarVisualizationComponent::ProjectElevation(FVector(500.0, 0.0, 100.0), 1000.0f, -100.0f, 100.0f, 101, 101);
    TestEqual(TEXT("elevation half range maps to center X"), Elevated.X, 50);
    TestEqual(TEXT("maximum height maps to top row"), Elevated.Y, 0);
    return true;
}

bool FVirtualLidarLegacyViewMappingTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("legacy depth mode maps to Turbo"), UVirtualLidarVisualizationComponent::MapLegacyViewMode(EVirtualLidarViewMode::DepthGradient), ELidarColorMode::DistanceTurbo);
    TestEqual(TEXT("legacy semantic mode maps to semantic color"), UVirtualLidarVisualizationComponent::MapLegacyViewMode(EVirtualLidarViewMode::ActorClassColor), ELidarColorMode::SemanticLabel);
    TestEqual(TEXT("legacy grayscale round-trips without the old range bug"), UVirtualLidarVisualizationComponent::MapColorModeToLegacy(ELidarColorMode::DistanceGray), EVirtualLidarViewMode::IntensityGray);
    return true;
}

bool FVirtualLidarProjectionNavigationTest::RunTest(const FString& Parameters)
{
    UVirtualLidarVisualizationComponent* Visualization = NewObject<UVirtualLidarVisualizationComponent>();
    Visualization->PanProjectionView(ELidarMonitorProjectionMode::TopDown, FVector2D(40.0f, 20.0f), FVector2D(400.0f, 200.0f));
    Visualization->RotateProjectionView(ELidarMonitorProjectionMode::TopDown, 15.0f);
    Visualization->ZoomProjectionView(ELidarMonitorProjectionMode::TopDown, 2.0f);
    const FVirtualLidarVisualizationSettings& TopDown = Visualization->GetVisualizationSettings();
    TestNotEqual(TEXT("top-down drag changes pan"), TopDown.TopDownPanCm, FVector2D::ZeroVector);
    TestEqual(TEXT("top-down right drag changes rotation"), TopDown.TopDownRotationDegrees, 15.0f);
    TestEqual(TEXT("top-down wheel changes zoom"), TopDown.TopDownZoom, 2.0f);

    Visualization->PanProjectionView(ELidarMonitorProjectionMode::Elevation, FVector2D(30.0f, -10.0f), FVector2D(300.0f, 150.0f));
    Visualization->RotateProjectionView(ELidarMonitorProjectionMode::Elevation, -5.0f);
    Visualization->ZoomProjectionView(ELidarMonitorProjectionMode::Elevation, 1.5f);
    const FVirtualLidarVisualizationSettings& Elevation = Visualization->GetVisualizationSettings();
    TestNotEqual(TEXT("elevation drag changes pan"), Elevation.ElevationPanCm, FVector2D::ZeroVector);
    TestEqual(TEXT("elevation right drag changes rotation"), Elevation.ElevationRotationDegrees, -5.0f);
    TestEqual(TEXT("elevation wheel changes zoom"), Elevation.ElevationZoom, 1.5f);

    Visualization->ResetProjectionView(ELidarMonitorProjectionMode::TopDown);
    Visualization->ResetProjectionView(ELidarMonitorProjectionMode::Elevation);
    const FVirtualLidarVisualizationSettings& Reset = Visualization->GetVisualizationSettings();
    TestEqual(TEXT("top-down reset clears pan"), Reset.TopDownPanCm, FVector2D::ZeroVector);
    TestEqual(TEXT("top-down reset restores zoom"), Reset.TopDownZoom, 1.0f);
    TestEqual(TEXT("elevation reset clears pan"), Reset.ElevationPanCm, FVector2D::ZeroVector);
    TestEqual(TEXT("elevation reset restores zoom"), Reset.ElevationZoom, 1.0f);
    return true;
}

#endif
