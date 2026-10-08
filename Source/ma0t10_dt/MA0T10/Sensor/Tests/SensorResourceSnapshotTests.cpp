#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorResourceSnapshotTest,"MA0T10.ResourceLifecycle.SnapshotIsReadOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraOrderedFailureRecoveryTest,"MA0T10.ResourceLifecycle.CameraFailedHeadReleasesFollowers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCameraOrderedFailureRecoveryTest::RunTest(const FString&)
{
    auto* C=NewObject<UVirtualCameraCaptureComponent>();C->OutputMode=EVirtualCameraOutputMode::None;
    auto QueueFollower=[&](){C->EncodeOrder={1,2};auto& Done=C->CompletedEncodes.FindOrAdd(2);Done.JpegBytes={0xff,0xd8,0xff,0xd9};Done.Width=1;Done.Height=1;
        FVirtualCameraPayloadSnapshot Meta;Meta.FrameId=2;Meta.SensorId=TEXT("ordered-test");Meta.TimestampUtc=FDateTime::UtcNow();C->ScheduledAcquisitionSnapshots.Add(2,Meta);};
    QueueFollower();C->CompleteSlabAcquisition(1,false);
    TestTrue(TEXT("failed head drains already-completed follower without waiting for another encode"),C->EncodeOrder.IsEmpty()&&C->CompletedEncodes.IsEmpty());
    TestEqual(TEXT("follower retains its own acquisition identity"),C->GetLastCompletedAcquisition().FrameId,int64(2));
    const auto Before=C->GetLastJpegSnapshot();QueueFollower();C->StopCapture();
    TestTrue(TEXT("Stop clears followers without publishing canceled work"),C->GetLastJpegSnapshot()==Before);
    TestTrue(TEXT("Stop removes order and completion buffers"),C->EncodeOrder.IsEmpty()&&C->CompletedEncodes.IsEmpty());
    return true;
}
bool FSensorResourceSnapshotTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Camera=W->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=W->SpawnActor<AVirtualLidarSensorActor>();
    Camera->StopSensor();Lidar->StopSensor();const auto CameraId=Camera->GetSensorRuntimeStatus().FrameId,LidarId=Lidar->GetSensorRuntimeStatus().FrameId;
    for(int32 I=0;I<25;++I)
    {
        const auto C=Camera->CaptureComponent->GetResourceSnapshot();const auto L=Lidar->ScanComponent->GetResourceSnapshot();
        TestEqual(TEXT("stopped camera has no encode jobs"),C.Counts.FindRef(TEXT("active_encodes")),int64(0));
        TestEqual(TEXT("stopped camera has no acquisition metadata"),C.Counts.FindRef(TEXT("acquisition_snapshots")),int64(0));
        TestEqual(TEXT("stopped lidar has no scheduled scan"),L.Counts.FindRef(TEXT("scan_in_flight")),int64(0));
        TestTrue(TEXT("GPU estimates are clearly separate"),C.EstimatedGpuBytes.Contains(TEXT("render_target_unpadded")));
        Lidar->GpuDepthProjectionComponent->GetResourceSnapshot();
    }
    TestEqual(TEXT("diagnostics never capture camera"),Camera->GetSensorRuntimeStatus().FrameId,CameraId);
    TestEqual(TEXT("diagnostics never scan lidar"),Lidar->GetSensorRuntimeStatus().FrameId,LidarId);
    TestFalse(TEXT("diagnostics never restart camera"),Camera->IsSensorRunning());TestFalse(TEXT("diagnostics never restart lidar"),Lidar->IsSensorRunning());
    Camera->Destroy();Lidar->Destroy();return true;
}
#endif
