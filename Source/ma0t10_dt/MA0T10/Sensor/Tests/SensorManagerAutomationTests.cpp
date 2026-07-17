#if WITH_DEV_AUTOMATION_TESTS

#include "EngineGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/RealSensorSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorManagerPointCloudOnlyPolicyTest, "MA0T10.SensorManager.PointCloudOnlyPreservesPayloadPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorManagerSharedServicesTest, "MA0T10.SensorManager.SharedServicesAssigned", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSensorManagerPointCloudOnlyPolicyTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    AVirtualSensorCoordinator* Manager = World->SpawnActor<AVirtualSensorCoordinator>();
    AActor* LidarOwnerA = World->SpawnActor<AActor>();
    AActor* LidarOwnerB = World->SpawnActor<AActor>();
    TestNotNull(TEXT("sensor manager"), Manager);
    TestNotNull(TEXT("lidar owner A"), LidarOwnerA);
    TestNotNull(TEXT("lidar owner B"), LidarOwnerB);
    if (!Manager || !LidarOwnerA || !LidarOwnerB)
    {
        return false;
    }

    UVirtualLidarScanComponent* LidarA = NewObject<UVirtualLidarScanComponent>(LidarOwnerA);
    UVirtualLidarScanComponent* LidarB = NewObject<UVirtualLidarScanComponent>(LidarOwnerB);
    TestNotNull(TEXT("lidar A"), LidarA);
    TestNotNull(TEXT("lidar B"), LidarB);
    if (!LidarA || !LidarB)
    {
        return false;
    }

    LidarOwnerA->AddInstanceComponent(LidarA);
    LidarOwnerB->AddInstanceComponent(LidarB);
    LidarA->RegisterComponent();
    LidarB->RegisterComponent();

    Manager->bPointCloudOnlyHideWorld = false;
    Manager->bPointCloudOnlyAutoSelectLidarView = false;

    LidarA->SetServerPayloadPolicy(1, 0, false);
    LidarA->SetPreviewPolicy(1, 6000, false);
    LidarA->SetPointCloudPreviewEnabled(false);
    LidarA->bDrawDebugRays = true;
    LidarA->bDrawPointCloudPreviewDebugPoints = true;

    LidarB->SetServerPayloadPolicy(4, 9, true);
    LidarB->SetPreviewPolicy(3, 12000, false);
    LidarB->SetPointCloudPreviewEnabled(true);
    LidarB->bDrawDebugRays = true;
    LidarB->bDrawPointCloudPreviewDebugPoints = true;

    Manager->RegisterLidar(LidarA);
    Manager->RegisterLidar(LidarB);
    Manager->SelectLidarByIndex(0);

    Manager->SetPointCloudOnlyMode(true);

    TestTrue(TEXT("point cloud only mode enabled"), Manager->IsPointCloudOnlyModeEnabled());
    TestEqual(TEXT("view mode is point cloud only"), Manager->GetViewMode(), EVirtualSensorViewMode::PointCloudOnly);

    TestEqual(TEXT("lidar A server stride unchanged in point cloud only"), LidarA->ServerPayloadStride, 1);
    TestEqual(TEXT("lidar A server max unchanged in point cloud only"), LidarA->MaxServerPayloadPoints, 0);
    TestFalse(TEXT("lidar A include miss unchanged in point cloud only"), LidarA->bIncludeMissPointsInServerPayload);
    TestEqual(TEXT("lidar B server stride unchanged in point cloud only"), LidarB->ServerPayloadStride, 4);
    TestEqual(TEXT("lidar B server max unchanged in point cloud only"), LidarB->MaxServerPayloadPoints, 9);
    TestTrue(TEXT("lidar B include miss unchanged in point cloud only"), LidarB->bIncludeMissPointsInServerPayload);

    TestTrue(TEXT("selected lidar preview enabled"), LidarA->IsPointCloudPreviewEnabled());
    TestEqual(TEXT("selected lidar preview stride clamped for point cloud only"), LidarA->PreviewPointStride, 2);
    TestEqual(TEXT("selected lidar preview max clamped for point cloud only"), LidarA->MaxPreviewPoints, 3000);
    TestTrue(TEXT("selected lidar preview hit only"), LidarA->bPointCloudPreviewHitOnly);
    TestFalse(TEXT("selected lidar debug rays disabled"), LidarA->bDrawDebugRays);
    TestFalse(TEXT("selected lidar debug point preview disabled"), LidarA->bDrawPointCloudPreviewDebugPoints);

    TestFalse(TEXT("unselected lidar preview disabled"), LidarB->IsPointCloudPreviewEnabled());
    TestEqual(TEXT("unselected lidar preview stride preserved when already >= 2"), LidarB->PreviewPointStride, 3);
    TestEqual(TEXT("unselected lidar preview max clamped for point cloud only"), LidarB->MaxPreviewPoints, 3000);
    TestTrue(TEXT("unselected lidar preview hit only"), LidarB->bPointCloudPreviewHitOnly);

    Manager->SelectNextLidar();
    TestFalse(TEXT("previous selected preview disabled after lidar selection changes"), LidarA->IsPointCloudPreviewEnabled());
    TestTrue(TEXT("new selected preview enabled after lidar selection changes"), LidarB->IsPointCloudPreviewEnabled());

    TestEqual(TEXT("lidar A server stride unchanged after selection change"), LidarA->ServerPayloadStride, 1);
    TestEqual(TEXT("lidar B server stride unchanged after selection change"), LidarB->ServerPayloadStride, 4);
    TestEqual(TEXT("lidar B server max unchanged after selection change"), LidarB->MaxServerPayloadPoints, 9);
    TestTrue(TEXT("lidar B include miss unchanged after selection change"), LidarB->bIncludeMissPointsInServerPayload);

    Manager->SetPointCloudOnlyMode(false);

    TestFalse(TEXT("point cloud only mode disabled"), Manager->IsPointCloudOnlyModeEnabled());
    TestEqual(TEXT("view mode restored"), Manager->GetViewMode(), EVirtualSensorViewMode::Camera);

    TestEqual(TEXT("lidar A preview stride restored"), LidarA->PreviewPointStride, 1);
    TestEqual(TEXT("lidar A preview max restored"), LidarA->MaxPreviewPoints, 6000);
    TestFalse(TEXT("lidar A preview hit only restored"), LidarA->bPointCloudPreviewHitOnly);
    TestFalse(TEXT("lidar A preview enabled restored"), LidarA->IsPointCloudPreviewEnabled());
    TestTrue(TEXT("lidar A debug rays restored"), LidarA->bDrawDebugRays);
    TestTrue(TEXT("lidar A debug point preview restored"), LidarA->bDrawPointCloudPreviewDebugPoints);

    TestEqual(TEXT("lidar B preview stride restored"), LidarB->PreviewPointStride, 3);
    TestEqual(TEXT("lidar B preview max restored"), LidarB->MaxPreviewPoints, 12000);
    TestFalse(TEXT("lidar B preview hit only restored"), LidarB->bPointCloudPreviewHitOnly);
    TestTrue(TEXT("lidar B preview enabled restored"), LidarB->IsPointCloudPreviewEnabled());
    TestTrue(TEXT("lidar B debug rays restored"), LidarB->bDrawDebugRays);
    TestTrue(TEXT("lidar B debug point preview restored"), LidarB->bDrawPointCloudPreviewDebugPoints);

    TestEqual(TEXT("lidar A server stride restored unchanged"), LidarA->ServerPayloadStride, 1);
    TestEqual(TEXT("lidar A server max restored unchanged"), LidarA->MaxServerPayloadPoints, 0);
    TestFalse(TEXT("lidar A include miss restored unchanged"), LidarA->bIncludeMissPointsInServerPayload);
    TestEqual(TEXT("lidar B server stride restored unchanged"), LidarB->ServerPayloadStride, 4);
    TestEqual(TEXT("lidar B server max restored unchanged"), LidarB->MaxServerPayloadPoints, 9);
    TestTrue(TEXT("lidar B include miss restored unchanged"), LidarB->bIncludeMissPointsInServerPayload);

    LidarOwnerA->Destroy();
    LidarOwnerB->Destroy();
    Manager->Destroy();
    return true;
}

bool FSensorManagerSharedServicesTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    AVirtualSensorCoordinator* Manager = World->SpawnActor<AVirtualSensorCoordinator>();
    AActor* CameraOwner = World->SpawnActor<AActor>();
    AActor* LidarOwner = World->SpawnActor<AActor>();
    TestNotNull(TEXT("sensor manager"), Manager);
    TestNotNull(TEXT("camera owner"), CameraOwner);
    TestNotNull(TEXT("lidar owner"), LidarOwner);
    if (!Manager || !CameraOwner || !LidarOwner)
    {
        return false;
    }

    UVirtualCameraCaptureComponent* CameraComp = NewObject<UVirtualCameraCaptureComponent>(CameraOwner);
    UVirtualLidarScanComponent* LidarComp = NewObject<UVirtualLidarScanComponent>(LidarOwner);
    ULidarCsvReplaySourceComponent* RealSourceComp = NewObject<ULidarCsvReplaySourceComponent>(LidarOwner);
    TestNotNull(TEXT("camera component"), CameraComp);
    TestNotNull(TEXT("lidar component"), LidarComp);
    TestNotNull(TEXT("real sensor replay source"), RealSourceComp);
    if (!CameraComp || !LidarComp || !RealSourceComp)
    {
        return false;
    }

    CameraOwner->AddInstanceComponent(CameraComp);
    LidarOwner->AddInstanceComponent(LidarComp);
    LidarOwner->AddInstanceComponent(RealSourceComp);
    CameraComp->RegisterComponent();
    LidarComp->RegisterComponent();
    RealSourceComp->RegisterComponent();
    RealSourceComp->SourceId = TEXT("TEST-MANAGER-REAL-SOURCE");
    RealSourceComp->TargetLidar = LidarComp;
    RealSourceComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    RealSourceComp->ReplaySemanticLabel = TEXT("Slab");
    RealSourceComp->bSendTransportByDefault = false;

    UVirtualSensorTransportComponent* SharedTransport = Manager->SharedTransportComponent;
    UVirtualSensorRecorderComponent* SharedRecorder = Manager->SharedRecorderComponent;
    TestNotNull(TEXT("manager shared transport"), SharedTransport);
    TestNotNull(TEXT("manager shared recorder"), SharedRecorder);
    SharedTransport->TransportMode = EVirtualSensorTransportMode::HttpPost;
    SharedTransport->MaxInFlightHttpRequests = 3;
    SharedTransport->InFlightHttpRequestCount = 2;
    SharedTransport->BackpressureRejectedRequestCount = 4;
    SharedTransport->LastResult.bSubmitted = true;
    SharedTransport->LastResult.bAccepted = false;
    SharedTransport->LastResult.HttpStatusCode = 503;

    Manager->RegisterCamera(CameraComp);
    Manager->RegisterLidar(LidarComp);
    Manager->RegisterRealSensorSource(RealSourceComp);

    TestEqual(TEXT("camera receives shared transport"), CameraComp->TransportComponent.Get(), SharedTransport);
    TestEqual(TEXT("camera receives shared recorder"), CameraComp->RecorderComponent.Get(), SharedRecorder);
    TestEqual(TEXT("lidar receives shared transport"), LidarComp->TransportComponent.Get(), SharedTransport);
    TestEqual(TEXT("lidar receives shared recorder"), LidarComp->RecorderComponent.Get(), SharedRecorder);

    TestEqual(TEXT("camera summary count"), Manager->GetCameraSummaries().Num(), 1);
    TestEqual(TEXT("lidar summary count"), Manager->GetLidarSummaries().Num(), 1);
    TestEqual(TEXT("real sensor source summary count"), Manager->GetRealSensorSourceSummaries().Num(), 1);
    TestEqual(TEXT("selected camera"), Manager->GetSelectedCamera(), CameraComp);
    TestEqual(TEXT("selected lidar"), Manager->GetSelectedLidar(), LidarComp);
    TestEqual(TEXT("selected real sensor source matches selected lidar"), Manager->GetSelectedRealSensorSource(), Cast<URealSensorSourceComponent>(RealSourceComp));
    TestTrue(TEXT("real sensor source summary exposes source id"), Manager->GetRealSensorSourceSummaries()[0].SensorId.Contains(TEXT("TEST-MANAGER-REAL-SOURCE")));

    const FVirtualSensorHealthSummary Health = Manager->GetHealthSummary();
    TestEqual(TEXT("health counts real sensor sources"), Health.RealSensorSourceCount, 1);
    TestEqual(TEXT("health counts replay as no external evidence required"), Health.ExternalEvidenceRequiredRealSensorSourceCount, 0);
    TestEqual(TEXT("health reports no real source errors"), Health.ErrorRealSensorSourceCount, 0);
    TestTrue(TEXT("health summary exposes real source counts"), Health.Summary.Contains(TEXT("RealSources=1")) && Health.Summary.Contains(TEXT("ExternalEvidenceRequired=0")));

    UVirtualSensorMonitorPanelWidget* MonitorWidget = NewObject<UVirtualSensorMonitorPanelWidget>();
    TestNotNull(TEXT("monitor widget"), MonitorWidget);
    if (!MonitorWidget)
    {
        CameraOwner->Destroy();
        LidarOwner->Destroy();
        Manager->Destroy();
        return false;
    }
    Manager->BindMonitorWidget(MonitorWidget);
    TestTrue(TEXT("manager binds selected real source to monitor"), MonitorWidget->GetRealSensorDeploymentSummaryText().Contains(TEXT("TEST-MANAGER-REAL-SOURCE")));
    TestTrue(TEXT("monitor exposes transport in-flight telemetry"), MonitorWidget->GetTransportStatusSummaryText().Contains(TEXT("InFlight=2/3")));
    TestTrue(TEXT("monitor exposes transport backpressure telemetry"), MonitorWidget->GetTransportStatusSummaryText().Contains(TEXT("BackpressureRejected=4")));
    TestTrue(TEXT("monitor display data exposes transport status"), MonitorWidget->GetMonitorDisplayData().TransportText.Contains(TEXT("LastCode=503")));
    TestTrue(TEXT("monitor pushes selected real source frame"), MonitorWidget->PushSelectedRealSensorSourceOnce(false));
    TestTrue(TEXT("selected real source push updates target lidar"), LidarComp->GetLastPoints().Num() > 0);
    TestTrue(TEXT("monitor records successful push"), MonitorWidget->GetLastRealSensorControlMessage().Contains(TEXT("push succeeded")));
    TestEqual(TEXT("monitor starts all real sources"), MonitorWidget->StartRealSensorSources(), 1);
    TestTrue(TEXT("real source reports running after manager start"), RealSourceComp->IsSourceRunning());
    TestEqual(TEXT("health counts running real source"), Manager->GetHealthSummary().RunningRealSensorSourceCount, 1);
    TestTrue(TEXT("monitor status exposes real source start result"), MonitorWidget->GetMonitorStatusText().Contains(TEXT("started 1 source")));
    MonitorWidget->StopRealSensorSources();
    TestEqual(TEXT("real source reports stopped after manager stop"), RealSourceComp->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
    TestTrue(TEXT("monitor records real source stop"), MonitorWidget->GetLastRealSensorControlMessage().Contains(TEXT("stop requested")));

    CameraOwner->Destroy();
    LidarOwner->Destroy();
    Manager->Destroy();
    return true;
}

#endif
