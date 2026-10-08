#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Misc/Paths.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPlacementIdentityUiTest,"MA0T10.Placement.ResetAndIdentityUi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPlacementIdentityUiTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Sensor=W->SpawnActor<AVirtualLidarSensorActor>();Sensor->StopSensor();Sensor->ScanComponent->SensorId=TEXT("pose-ui-A");
    auto* Manager=W->SpawnActor<AVirtualSensorCoordinator>();Manager->RegisterSensorActor(Sensor);Manager->RegisterLidar(Sensor->ScanComponent);
    auto* Placement=W->GetSubsystem<UVirtualSensorPlacementSubsystem>();
    Placement->SetAutomationStorageRoot(FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementComparison/identity")/FGuid::NewGuid().ToString());
    auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(W);
    Settings->SetToolWorkspace(W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>(),ESensorToolPanelRole::Settings);
    Settings->BindSensorManager(Manager);Settings->SelectTargetKind(EVirtualSensorTargetKind::Lidar);
    const auto Initial=Sensor->GetActorTransform();
    Settings->PendingState.ActorTransform.AddToTranslation(FVector(120,20,10));
    TestTrue(TEXT("numeric pose applies"),Settings->ApplyPendingTransform());const auto Moved=Sensor->GetActorTransform();
    Settings->ResetPendingStateToMapValue();
    TestTrue(TEXT("reset returns to PIE start pose"),UVirtualSensorPlacementSubsystem::SamePose(Sensor->GetActorTransform(),Initial));
    FString Reason;
    TestTrue(TEXT("reset is one undoable pose change"),Placement->UndoSensorPlacement(Sensor,Reason)&&UVirtualSensorPlacementSubsystem::SamePose(Sensor->GetActorTransform(),Moved));
    TestTrue(TEXT("reset redo returns to start"),Placement->RedoSensorPlacement(Sensor,Reason)&&UVirtualSensorPlacementSubsystem::SamePose(Sensor->GetActorTransform(),Initial));
    Settings->PendingState.ActorTransform.AddToTranslation(FVector(80,0,0));Settings->ApplyPendingTransform();
    auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>(Sensor);Sensor->SetSharedOutputServices(nullptr,nullptr,Publisher);
    Publisher->StartStream(EVirtualSensorStreamKind::PointCloud,Sensor->GetSensorId());
    const auto Locked=Sensor->GetActorTransform();const int32 HistoryIndex=Placement->HistoryFor(Sensor).Index;
    Settings->ResetPendingStateToMapValue();
    TestTrue(TEXT("locked reset preserves actual pose"),Sensor->GetActorTransform().Equals(Locked));
    TestEqual(TEXT("locked reset preserves history index"),Placement->HistoryFor(Sensor).Index,HistoryIndex);
    TestTrue(TEXT("reset retains failure instead of overwriting with success"),Settings->LastControlMessage.Contains(TEXT("실패")));
    Publisher->StopAllStreams(Sensor->GetSensorId());
    auto Complete=[&](){if(Placement->PendingWrite)Placement->FinishWrite(Placement->PendingWrite->Future.Get(),false);};
    const FString SaveA=Placement->RequestSaveSensorPose(Sensor,TEXT("A only"));Complete();const FString A=Placement->GetPoseSaveStatus(SaveA).BookmarkId;
    Settings->RefreshPoseRows();TestEqual(TEXT("A identity list visible"),Settings->PoseRows.Num(),1);
    Settings->SelectedPoseId=A;Settings->PendingPoseDeleteId=A;
    Settings->PendingState.SensorId=TEXT("pose-ui-B");TestTrue(TEXT("same actor can change id"),Settings->ApplyPendingState());Settings->RefreshPoseRows();
    TestEqual(TEXT("old identity rows disappear without library write"),Settings->PoseRows.Num(),0);
    TestTrue(TEXT("old selection and delete confirmation cleared"),Settings->SelectedPoseId.IsEmpty()&&Settings->PendingPoseDeleteId.IsEmpty());
    TestFalse(TEXT("old identity history is not reused"),Placement->CanUndoSensorPlacement(Sensor));
    Settings->LastPoseRequest=Settings->RequestSaveSelectedSensorPose(TEXT("B pending"));
    Settings->PendingState.SensorId=TEXT("pose-ui-A");Settings->ApplyPendingState();Settings->RefreshPoseRows();Complete();Settings->RefreshPoseRows();
    TestEqual(TEXT("returning A reloads preserved A list"),Settings->PoseRows.Num(),1);
    TestTrue(TEXT("late B save cannot select B on A"),Settings->SelectedPoseId.IsEmpty()&&Settings->LastPoseRequest.IsEmpty());
    Settings->PendingState.SensorId=TEXT("pose-ui-B");Settings->ApplyPendingState();Settings->RefreshPoseRows();
    TestEqual(TEXT("B saved file entry remains available under B"),Settings->PoseRows.Num(),1);
    TestEqual(TEXT("B identity gets only B entry"),Settings->PoseRows[0].Name,FString(TEXT("B pending")));
    Sensor->Destroy();Settings->RefreshPoseRows();TestFalse(TEXT("deleted sensor cannot undo"),Placement->CanUndoSensorPlacement(Sensor));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPlacementPreviewTest,"MA0T10.Placement.FreshPosePreview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPlacementPreviewTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* P=W->GetSubsystem<UVirtualSensorPlacementSubsystem>();
    for(bool Camera:{true,false})
    {
        auto* S=Camera?static_cast<AVirtualSensorActorBase*>(W->SpawnActor<AVirtualCameraSensorActor>()):static_cast<AVirtualSensorActorBase*>(W->SpawnActor<AVirtualLidarSensorActor>());
        S->StopSensor();P->BeginPlacementPreviewRefresh(S);
        TestEqual(TEXT("stopped pose never starts acquisition"),P->GetPlacementPreviewStatus(S).State,EVirtualSensorPlacementPreviewState::Stopped);
        S->StartSensor();P->BeginPlacementPreviewRefresh(S);const auto Request=P->PreviewRequests.FindChecked(S);const int64 F=Request.BaselineFrame+10;
        auto Observe=[&](const FTransform& Pose,double At,int32 Gen,int32 Width){P->ObservePreviewFrame(S,F,Pose,At,Gen,Width,Request.Height);};
        auto OldPose=Request.AcquisitionPose;OldPose.AddToTranslation(FVector(-100,0,0));
        Observe(OldPose,Request.StartedMonotonic-1,Request.Generation,Request.Width);
        TestTrue(TEXT("old in-flight pose cannot make preview ready"),P->IsPreviewRefreshPending(S));
        Observe(Request.AcquisitionPose,Request.StartedMonotonic-1,Request.Generation,Request.Width);
        TestTrue(TEXT("matching old pose still needs new acquisition start"),P->IsPreviewRefreshPending(S));
        Observe(Request.AcquisitionPose,Request.StartedMonotonic+1,Request.Generation+1,Request.Width);
        TestTrue(TEXT("different generation cannot complete request"),P->IsPreviewRefreshPending(S));
        Observe(Request.AcquisitionPose,Request.StartedMonotonic+1,Request.Generation,Request.Width+1);
        TestTrue(TEXT("different acquisition shape cannot complete request"),P->IsPreviewRefreshPending(S));
        Observe(Request.AcquisitionPose,Request.StartedMonotonic+1,Request.Generation,Request.Width);
        TestEqual(TEXT("only new matching completed frame makes ready"),P->GetPlacementPreviewStatus(S).State,EVirtualSensorPlacementPreviewState::Ready);
        P->BeginPlacementPreviewRefresh(S);TestTrue(TEXT("new request supersedes ready result"),P->GetPlacementPreviewStatus(S).RequestId>Request.Status.RequestId);
        auto& Timeout=P->PreviewRequests.FindChecked(S);Timeout.StartedWorld-=Timeout.Timeout+1;P->Tick(0);
        TestEqual(TEXT("bounded wait reports delay"),P->GetPlacementPreviewStatus(S).State,EVirtualSensorPlacementPreviewState::Delayed);
        TestTrue(TEXT("delay does not stop running sensor"),S->IsSensorRunning());
        P->BeginPlacementPreviewRefresh(S);S->SetActorLocation(S->GetActorLocation()+FVector(1,0,0));P->Tick(0);
        TestEqual(TEXT("external new pose cancels old request"),P->GetPlacementPreviewStatus(S).State,EVirtualSensorPlacementPreviewState::Cancelled);
        P->CancelPlacementPreviewRefresh(S);TestEqual(TEXT("selection cancellation clears status"),P->GetPlacementPreviewStatus(S).State,EVirtualSensorPlacementPreviewState::Idle);
        S->Destroy();P->Tick(0);
    }
    return true;
}
#endif
