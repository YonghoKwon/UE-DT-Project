#if WITH_DEV_AUTOMATION_TESTS

#include "EngineGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorRecorderComp.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorManagerPointCloudOnlyPolicyTest, "M7AT10.SensorManager.PointCloudOnlyPreservesPayloadPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorManagerSharedServicesTest, "M7AT10.SensorManager.SharedServicesAssigned", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSensorManagerPointCloudOnlyPolicyTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    AVirtualSensorManager* Manager = World->SpawnActor<AVirtualSensorManager>();
    AActor* LidarOwnerA = World->SpawnActor<AActor>();
    AActor* LidarOwnerB = World->SpawnActor<AActor>();
    TestNotNull(TEXT("sensor manager"), Manager);
    TestNotNull(TEXT("lidar owner A"), LidarOwnerA);
    TestNotNull(TEXT("lidar owner B"), LidarOwnerB);
    if (!Manager || !LidarOwnerA || !LidarOwnerB)
    {
        return false;
    }

    UVirtualLidarSensorComp* LidarA = NewObject<UVirtualLidarSensorComp>(LidarOwnerA);
    UVirtualLidarSensorComp* LidarB = NewObject<UVirtualLidarSensorComp>(LidarOwnerB);
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

    AVirtualSensorManager* Manager = World->SpawnActor<AVirtualSensorManager>();
    AActor* CameraOwner = World->SpawnActor<AActor>();
    AActor* LidarOwner = World->SpawnActor<AActor>();
    TestNotNull(TEXT("sensor manager"), Manager);
    TestNotNull(TEXT("camera owner"), CameraOwner);
    TestNotNull(TEXT("lidar owner"), LidarOwner);
    if (!Manager || !CameraOwner || !LidarOwner)
    {
        return false;
    }

    UVirtualCameraComp* CameraComp = NewObject<UVirtualCameraComp>(CameraOwner);
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>(LidarOwner);
    ULidarCsvReplaySourceComp* RealSourceComp = NewObject<ULidarCsvReplaySourceComp>(LidarOwner);
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

    UVirtualSensorDataTransportComp* SharedTransport = Manager->SharedTransportComponent;
    UVirtualSensorRecorderComp* SharedRecorder = Manager->SharedRecorderComponent;
    TestNotNull(TEXT("manager shared transport"), SharedTransport);
    TestNotNull(TEXT("manager shared recorder"), SharedRecorder);

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
    TestEqual(TEXT("selected real sensor source matches selected lidar"), Manager->GetSelectedRealSensorSource(), Cast<URealSensorSourceComp>(RealSourceComp));
    TestTrue(TEXT("real sensor source summary exposes source id"), Manager->GetRealSensorSourceSummaries()[0].SensorId.Contains(TEXT("TEST-MANAGER-REAL-SOURCE")));

    const FVirtualSensorHealthSummary Health = Manager->GetHealthSummary();
    TestEqual(TEXT("health counts real sensor sources"), Health.RealSensorSourceCount, 1);
    TestEqual(TEXT("health counts replay as no external evidence required"), Health.ExternalEvidenceRequiredRealSensorSourceCount, 0);
    TestEqual(TEXT("health reports no real source errors"), Health.ErrorRealSensorSourceCount, 0);
    TestTrue(TEXT("health summary exposes real source counts"), Health.Summary.Contains(TEXT("RealSources=1")) && Health.Summary.Contains(TEXT("ExternalEvidenceRequired=0")));

    UVirtualSensorMonitorWidget* MonitorWidget = NewObject<UVirtualSensorMonitorWidget>();
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

    CameraOwner->Destroy();
    LidarOwner->Destroy();
    Manager->Destroy();
    return true;
}

#endif
