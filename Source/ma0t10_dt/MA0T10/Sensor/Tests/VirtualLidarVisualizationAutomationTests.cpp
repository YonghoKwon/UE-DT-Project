#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarColorModeTest, "MA0T10.SensorV2.LidarVisualization.ColorModes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarProjectionMathTest, "MA0T10.SensorV2.LidarVisualization.ProjectionMath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarLegacyViewMappingTest, "MA0T10.SensorV2.LidarVisualization.LegacyMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarProjectionNavigationTest, "MA0T10.SensorV2.LidarVisualization.ProjectionNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarFramePoseCoherenceTest, "MA0T10.SensorV2.LidarVisualization.FramePoseCoherence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarPreviewPolicyReversibleTest, "MA0T10.SensorV2.LidarVisualization.PreviewPolicyReversible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualLidarAcquisitionNotificationTest, "MA0T10.SensorV2.LidarVisualization.AcquisitionNotification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
	bool bInsideSlice = false;
	const FIntPoint Slice = UVirtualLidarVisualizationComponent::ProjectForwardSlice(FVector(500.0, 20.0, 100.0), 0.0f, 1000.0f, -100.0f, 100.0f, 101, 101, bInsideSlice, 100.0f);
	TestTrue(TEXT("point inside slice thickness is accepted"), bInsideSlice);
	TestEqual(TEXT("forward slice maps local X to horizontal axis"), Slice.X, 50);
	UVirtualLidarVisualizationComponent::ProjectForwardSlice(FVector(500.0, 80.0, 0.0), 0.0f, 1000.0f, -100.0f, 100.0f, 101, 101, bInsideSlice, 100.0f);
	TestFalse(TEXT("point outside slice thickness is rejected"), bInsideSlice);

	TArray<FVirtualLidarPoint> WorldPoints;
	FVirtualLidarPoint WorldA;
	WorldA.bHit = true;
	WorldA.WorldLocation = FVector(1000.0, 2000.0, 300.0);
	WorldPoints.Add(WorldA);
	FVirtualLidarPoint WorldB = WorldA;
	WorldB.WorldLocation = FVector(3000.0, 6000.0, 800.0);
	WorldPoints.Add(WorldB);
	FVector2D WorldCenter;
	FVector2D WorldHalfExtent;
	TestTrue(TEXT("world top-down auto-fit finds hit bounds"),
		UVirtualLidarVisualizationComponent::CalculateWorldTopDownView(WorldPoints, FVector::ZeroVector, 10000.0f, WorldCenter, WorldHalfExtent));
	TestEqual(TEXT("world top-down horizontal center uses World Y"), WorldCenter.X, 4000.0);
	TestEqual(TEXT("world top-down vertical center uses World X"), WorldCenter.Y, 2000.0);
	const FIntPoint WorldCenterPixel = UVirtualLidarVisualizationComponent::ProjectWorldTopDown(
		FVector(2000.0, 4000.0, -999.0), WorldCenter, WorldHalfExtent, 0.0f, 101, 101);
	TestEqual(TEXT("world Z does not affect floor-plan X"), WorldCenterPixel.X, 50);
	TestEqual(TEXT("world Z does not affect floor-plan Y"), WorldCenterPixel.Y, 50);
	TestEqual(TEXT("existing ForwardSlice value stays serialized-compatible"), static_cast<uint8>(ELidarMonitorProjectionMode::ForwardSlice), static_cast<uint8>(4));
	TestEqual(TEXT("world top-down is appended after legacy modes"), static_cast<uint8>(ELidarMonitorProjectionMode::WorldTopDown), static_cast<uint8>(5));
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

	Visualization->PanProjectionView(ELidarMonitorProjectionMode::WorldTopDown, FVector2D(40.0f, 20.0f), FVector2D(400.0f, 200.0f));
	Visualization->RotateProjectionView(ELidarMonitorProjectionMode::WorldTopDown, 10.0f);
	Visualization->ZoomProjectionView(ELidarMonitorProjectionMode::WorldTopDown, 2.0f);
	const FVirtualLidarVisualizationSettings& WorldTopDown = Visualization->GetVisualizationSettings();
	TestNotEqual(TEXT("world top-down drag changes independent pan"), WorldTopDown.WorldTopDownPanCm, FVector2D::ZeroVector);
	TestEqual(TEXT("world top-down right drag changes rotation"), WorldTopDown.WorldTopDownRotationDegrees, 10.0f);
	TestEqual(TEXT("world top-down wheel changes zoom"), WorldTopDown.WorldTopDownZoom, 2.0f);

    Visualization->PanProjectionView(ELidarMonitorProjectionMode::Elevation, FVector2D(30.0f, -10.0f), FVector2D(300.0f, 150.0f));
    Visualization->RotateProjectionView(ELidarMonitorProjectionMode::Elevation, -5.0f);
    Visualization->ZoomProjectionView(ELidarMonitorProjectionMode::Elevation, 1.5f);
    const FVirtualLidarVisualizationSettings& Elevation = Visualization->GetVisualizationSettings();
    TestNotEqual(TEXT("elevation drag changes pan"), Elevation.ElevationPanCm, FVector2D::ZeroVector);
    TestEqual(TEXT("elevation right drag changes rotation"), Elevation.ElevationRotationDegrees, -5.0f);
    TestEqual(TEXT("elevation wheel changes zoom"), Elevation.ElevationZoom, 1.5f);

    Visualization->ResetProjectionView(ELidarMonitorProjectionMode::TopDown);
	Visualization->ResetProjectionView(ELidarMonitorProjectionMode::WorldTopDown);
    Visualization->ResetProjectionView(ELidarMonitorProjectionMode::Elevation);
    const FVirtualLidarVisualizationSettings& Reset = Visualization->GetVisualizationSettings();
    TestEqual(TEXT("top-down reset clears pan"), Reset.TopDownPanCm, FVector2D::ZeroVector);
    TestEqual(TEXT("top-down reset restores zoom"), Reset.TopDownZoom, 1.0f);
	TestEqual(TEXT("world top-down reset clears pan"), Reset.WorldTopDownPanCm, FVector2D::ZeroVector);
	TestEqual(TEXT("world top-down reset restores zoom"), Reset.WorldTopDownZoom, 1.0f);
	TestTrue(TEXT("world top-down reset restores auto-fit"), Reset.bWorldTopDownAutoFit);
    TestEqual(TEXT("elevation reset clears pan"), Reset.ElevationPanCm, FVector2D::ZeroVector);
    TestEqual(TEXT("elevation reset restores zoom"), Reset.ElevationZoom, 1.0f);
    return true;
}

bool FVirtualLidarFramePoseCoherenceTest::RunTest(const FString& Parameters)
{
	FVirtualLidarFrameSnapshot Frame;
	Frame.AcquisitionTransform = FTransform(FRotator::ZeroRotator, FVector(100.0, 20.0, 30.0));
	Frame.Points = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>();
	const FVector AcquiredWorldPoint(600.0, 20.0, 130.0);
	const FVector AcquiredLocalPoint = Frame.AcquisitionTransform.InverseTransformPosition(AcquiredWorldPoint);
	const FTransform SensorAfterMove(FRotator::ZeroRotator, FVector(350.0, 20.0, 30.0));
	TestEqual(TEXT("snapshot keeps the original local X"), AcquiredLocalPoint.X, 500.0);
	TestNotEqual(TEXT("moving actor would reinterpret an old point"), SensorAfterMove.InverseTransformPosition(AcquiredWorldPoint).X, AcquiredLocalPoint.X);
	return true;
}

bool FVirtualLidarPreviewPolicyReversibleTest::RunTest(const FString& Parameters)
{
	UVirtualLidarScanComponent* Scan = NewObject<UVirtualLidarScanComponent>();
	FVirtualLidarPreviewPolicy Policy = Scan->GetPreviewPolicyState();
	Policy.bHitOnly = true;
	Scan->SetPreviewPolicyState(Policy);
	TestTrue(TEXT("hit-only starts enabled"), Scan->GetPreviewPolicyState().bHitOnly);
	Policy.bHitOnly = false;
	Scan->SetPreviewPolicyState(Policy);
	TestFalse(TEXT("hit-only can be disabled"), Scan->GetPreviewPolicyState().bHitOnly);
	Policy.bHitOnly = true;
	Scan->SetPreviewPolicyState(Policy);
	TestTrue(TEXT("hit-only can be enabled again without a new scan"), Scan->GetPreviewPolicyState().bHitOnly);
	return true;
}

bool FVirtualLidarAcquisitionNotificationTest::RunTest(const FString& Parameters)
{
    UVirtualLidarScanComponent* Scan = NewObject<UVirtualLidarScanComponent>();
    bool bReceived = false;
    bool bPayloadWasStillPending = false;
    int64 ReceivedFrameId = INDEX_NONE;
    Scan->OnFrameAcquiredNative.AddLambda([&](int64 FrameId)
    {
        bReceived = true;
        ReceivedFrameId = FrameId;
        bPayloadWasStillPending = Scan->GetLastJsonPayload().IsEmpty();
    });

    FVirtualLidarPoint Point;
    Point.bHit = true;
    Point.Distance = 100.0f;
    Point.WorldLocation = FVector(100.0, 0.0, 0.0);
    Scan->InjectPointCloudFrame({ Point }, false);

    const TSharedPtr<const FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe> Snapshot = Scan->GetLastFrameSnapshot();
    TestTrue(TEXT("acquisition notification fires for an injected frame"), bReceived);
    TestTrue(TEXT("notification is independent of JSON post processing"), bPayloadWasStillPending);
    TestTrue(TEXT("immutable snapshot is available during notification"), Snapshot.IsValid());
    if (Snapshot.IsValid())
    {
        TestEqual(TEXT("notification frame matches snapshot"), ReceivedFrameId, Snapshot->FrameId);
    }
    return true;
}

#endif
