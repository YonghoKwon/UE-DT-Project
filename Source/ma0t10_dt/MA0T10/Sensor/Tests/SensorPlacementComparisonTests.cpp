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
#endif
