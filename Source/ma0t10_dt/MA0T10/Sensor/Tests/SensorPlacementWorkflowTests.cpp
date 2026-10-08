#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPlacementGateTest,"MA0T10.Placement.CollectionPoseGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPlacementGateTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();
    auto* L=W->SpawnActor<AVirtualLidarSensorActor>();auto* Other=W->SpawnActor<AVirtualLidarSensorActor>();auto* C=W->SpawnActor<AVirtualCameraSensorActor>();
    L->ScanComponent->SensorId=TEXT("placement-A");Other->ScanComponent->SensorId=TEXT("placement-B");C->CaptureComponent->SensorId=TEXT("placement-camera");
    auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>(L);L->SetSharedOutputServices(nullptr,nullptr,Publisher);Other->SetSharedOutputServices(nullptr,nullptr,Publisher);C->SetSharedOutputServices(nullptr,nullptr,Publisher);
    L->StopSensor();Other->StopSensor();C->StopSensor();FString Reason;
    const auto Pose=L->GetActorTransform();auto Moved=Pose;Moved.AddToTranslation(FVector(10,20,30));
    TestTrue(TEXT("preview/idle allows pose editing"),L->CanEditSensorPlacement(Reason));
    const int32 H=L->ScanComponent->HorizontalSamples,V=L->ScanComponent->VerticalChannels;const float Interval=L->ScanComponent->ScanInterval;
    TestTrue(TEXT("manipulation starts without starting stopped sensor"),L->BeginInteractiveManipulation(FVirtualSensorInteractionRequest()));
    Publisher->StartStream(EVirtualSensorStreamKind::PointCloud,L->GetSensorId());
    TestFalse(TEXT("data admission finishes manipulation"),L->IsInteractiveManipulationActive());
    TestEqual(TEXT("data admission restores horizontal spec"),L->ScanComponent->HorizontalSamples,H);
    TestEqual(TEXT("data admission restores channels"),L->ScanComponent->VerticalChannels,V);
    TestEqual(TEXT("data admission restores period"),L->ScanComponent->ScanInterval,Interval);
    TestFalse(TEXT("manual stream never implicitly starts stopped actor"),L->IsSensorRunning());
    TestFalse(TEXT("exact stream rejects numeric transform"),L->ApplyEditableTransform(Moved,Reason));
    TestTrue(TEXT("rejection does not change pose"),L->GetActorTransform().Equals(Pose));
    TestTrue(TEXT("other sensor remains editable"),Other->CanEditSensorPlacement(Reason));
    FVirtualSensorEditableState Settings;L->ReadEditableState(Settings);Settings.SensorId=TEXT("renamed");
    TestFalse(TEXT("changing ID cannot bypass collection pose lock"),L->ApplyEditableState(Settings,Reason));
    Publisher->StopAllStreams(L->GetSensorId());
    TestTrue(TEXT("stopped empty stream releases lock"),L->CanEditSensorPlacement(Reason));
    Publisher->StartStream(EVirtualSensorStreamKind::PointCloud,TEXT(""));
    TestFalse(TEXT("wildcard locks matching lidar"),L->CanEditSensorPlacement(Reason));
    TestFalse(TEXT("wildcard locks other matching lidar"),Other->CanEditSensorPlacement(Reason));
    TestTrue(TEXT("lidar wildcard does not lock camera"),C->CanEditSensorPlacement(Reason));
    Publisher->StopAllStreams(TEXT(""));
    UVirtualSensorStreamPublisherComponent::FReceiptWait Wait;Wait.Message.SensorId=L->GetSensorId();Publisher->WaitingReceipts.Add(TEXT("pending-receipt"),Wait);
    TestFalse(TEXT("receipt drain remains locked after toggle stop"),L->CanEditSensorPlacement(Reason));
    TestTrue(TEXT("other ID is not locked by receipt"),Other->CanEditSensorPlacement(Reason));Publisher->WaitingReceipts.Reset();

    auto* Session=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();Session->Targets.Add(L->GetSensorId(),L);Session->Targets.Add(C->GetSensorId(),C);
    Session->Status.Outputs=FVirtualSlabSensorOutputSelection();Session->Status.bObservationOnly=false;
    for(auto State:{EVirtualSlabSessionState::Preparing,EVirtualSlabSessionState::Ready,EVirtualSlabSessionState::Running,EVirtualSlabSessionState::Paused,EVirtualSlabSessionState::Draining})
    {Session->Status.State=State;TestFalse(TEXT("selected scenario target stays locked"),L->CanEditSensorPlacement(Reason));TestTrue(TEXT("unselected scenario camera remains editable"),C->CanEditSensorPlacement(Reason));}
    Session->Status.bObservationOnly=true;TestTrue(TEXT("observation-only releases scenario pose lock"),L->CanEditSensorPlacement(Reason));Session->Targets.Reset();
    FVirtualSensorCaptureSelection Selection;Selection.bLidarPayload=false;Selection.bPointCloud=true;Selection.PointCloudFormat=EVirtualSensorExportKind::PointCloudPcd;
    auto* Periodic=W->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();const FString PeriodicId=Periodic->StartPeriodicSave(L,Selection);
    TestTrue(TEXT("periodic save admitted"),Periodic->GetPeriodicStatus(PeriodicId).bActive);
    TestFalse(TEXT("periodic save locks sensor even between files"),L->CanEditSensorPlacement(Reason));Periodic->StopPeriodicSave(PeriodicId);
    TestTrue(TEXT("stopped periodic with no pending file releases lock"),L->CanEditSensorPlacement(Reason));
    L->StartSensor();auto* Files=W->GetSubsystem<UVirtualSensorFileSaveSubsystem>();const FString Request=Files->RequestSave(EVirtualSensorFileSaveMode::NewFrame,L,Selection);
    TestTrue(TEXT("new-frame wait is an actual pending save"),Files->HasPendingSave(L));
    TestFalse(TEXT("file wait locks transform"),L->CanEditSensorPlacement(Reason));Files->CancelRequest(Request);
    TestTrue(TEXT("cancelled request releases pose lock"),L->CanEditSensorPlacement(Reason));
    L->StopSensor();L->Destroy();Other->Destroy();C->Destroy();return true;
}
#endif
