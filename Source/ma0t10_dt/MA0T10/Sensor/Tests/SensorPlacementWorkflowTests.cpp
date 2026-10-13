#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementMotion.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#include "Json.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabRunPlacementSnapshotTest,"MA0T10.Placement.RunPlacementSnapshot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabRunPlacementSnapshotTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* S=W->SpawnActor<AVirtualLidarSensorActor>();S->ScanComponent->SensorId=TEXT("run-pose-test");S->StopSensor();
    auto* P=W->GetSubsystem<UVirtualSensorPlacementSubsystem>();P->StorageRootOverrideForTests=FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementWorkflow/run-fixtures")/FGuid::NewGuid().ToString();
    const FTransform A(FRotator(-45,15,5),FVector(120,240,800),FVector(1,2,3));FString Reason;S->ApplyEditableTransform(A,Reason);
    const FString Request=P->RequestSaveSensorPose(S,TEXT("Run A"));P->FinishWrite(P->PendingWrite->Future.Get(),false);const FString Bookmark=P->GetPoseSaveStatus(Request).BookmarkId;P->ApplySensorPoseBookmark(S,Bookmark,Reason);
    auto* R=W->GetSubsystem<USlabRunResultsSubsystem>();R->ReportDirectoryOverrideForTests=FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementWorkflow/run-fixtures/reports");
    const FString RunA=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),Scenario=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>> Sensors;Sensors.Add(S->GetSensorId(),S);
    R->BeginRun(RunA,Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());R->SetSensors(RunA,Sensors,nullptr);
    FSlabRunDeliverySummary Snapshot;R->GetRunResult(RunA,Snapshot);
    TestTrue(TEXT("run starts with immutable actual world pose"),Snapshot.Sensors[0].Placement.WorldTransform.Equals(A));
    TestEqual(TEXT("run remembers applied bookmark"),Snapshot.Sensors[0].Placement.BookmarkId,Bookmark);
    TestTrue(TEXT("run map is canonical"),!Snapshot.Sensors[0].Placement.MapPackage.Contains(TEXT("UEDPIE_")));
    S->SetActorLocation(A.GetLocation()+FVector(25,0,0));R->GetRunResult(RunA,Snapshot);
    TestTrue(TEXT("external movement is recorded"),Snapshot.Sensors[0].Placement.bChangedDuringRun);
    TestTrue(TEXT("external movement never overwrites start pose"),Snapshot.Sensors[0].Placement.WorldTransform.Equals(A));
    FVirtualSlabSessionStatus Session;Session.RunId=RunA;Session.bObservationOnly=true;Session.Outputs=FVirtualSlabSensorOutputSelection::ObservationOnly();R->Finalize(Session);
    R->GetRunResult(RunA,Snapshot);const auto Finished=Snapshot;TestTrue(TEXT("external movement warning persists at finalization"),Snapshot.Warning.Contains(TEXT("배치")));
    const FString Json=R->SerializeSummary(Snapshot);TSharedPtr<FJsonObject> Object;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object);
    TestEqual(TEXT("existing run wire schema retained"),Object->GetStringField(TEXT("schema")),FString(TEXT("ma0t10.slab-run.v1")));
    TestTrue(TEXT("run JSON contains optional placement snapshot"),Json.Contains(TEXT("placement"))&&Json.Contains(Bookmark));
    S->SetActorLocation(FVector(999,888,777));R->SetSensors(RunA,Sensors,nullptr);R->GetRunResult(RunA,Snapshot);
    TestTrue(TEXT("late old-run setup cannot mutate finalized placement"),Snapshot.Sensors[0].Placement.WorldTransform.Equals(Finished.Sensors[0].Placement.WorldTransform));
    const FString RunB=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);R->BeginRun(RunB,Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());R->SetSensors(RunB,Sensors,nullptr);
    R->GetRunResult(RunB,Snapshot);TestTrue(TEXT("next run captures position B independently"),Snapshot.Sensors[0].Placement.WorldTransform.GetLocation().Equals(FVector(999,888,777)));
    TestTrue(TEXT("direct pose has no stale bookmark"),Snapshot.Sensors[0].Placement.bDirectPlacement&&Snapshot.Sensors[0].Placement.BookmarkId.IsEmpty());
    TestEqual(TEXT("old bindings are retired when next run starts"),R->PlacementBindings.Num(),1);S->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPoseLibraryTest,"MA0T10.Placement.PoseLibraryPersistenceAndHistory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPoseLibraryTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* S=W->SpawnActor<AVirtualLidarSensorActor>();S->StopSensor();S->ScanComponent->SensorId=TEXT("bookmark-test");
    auto* P=W->GetSubsystem<UVirtualSensorPlacementSubsystem>();
    P->StorageRootOverrideForTests=FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementWorkflow/fixtures")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const auto Root=P->StorageRootOverrideForTests;
    // Wait for file workers only. Workers never require a game-thread callback.
    auto Complete=[&](const FString& Request){if(P->PendingWrite)P->FinishWrite(P->PendingWrite->Future.Get(),false);return P->GetPoseSaveStatus(Request);};
    const FTransform A(FRotator(-25,40,8),FVector(111,222,333));FString Reason;S->ApplyEditableTransform(A,Reason);
    const FString RequestA=P->RequestSaveSensorPose(S,TEXT("위치 A"));TestEqual(TEXT("save request is pending until disk completion"),P->GetPoseSaveStatus(RequestA).State,EVirtualSensorPoseSaveState::Writing);
    auto Saved=Complete(RequestA);TestEqual(TEXT("real file save succeeds"),Saved.State,EVirtualSensorPoseSaveState::Saved);const FString IdA=Saved.BookmarkId;
    S->SetActorScale3D(FVector(2,3,4));P->BeginGesture(S);auto B=S->GetActorTransform();B.SetLocation(FVector(555,666,777));B.SetRotation(FRotator(10,-35,0).Quaternion());S->ApplyEditableTransform(B,Reason);P->CommitGesture(S);
    const FString IdB=Complete(P->RequestSaveSensorPose(S,TEXT("위치 B"))).BookmarkId;
    TestTrue(TEXT("gesture undo restores position"),P->UndoSensorPlacement(S,Reason)&&UVirtualSensorPlacementSubsystem::SamePose(S->GetActorTransform(),A));
    TestTrue(TEXT("undo preserves current live scale"),S->GetActorScale3D().Equals(FVector(2,3,4)));
    TestTrue(TEXT("redo restores position B"),P->RedoSensorPlacement(S,Reason)&&UVirtualSensorPlacementSubsystem::SamePose(S->GetActorTransform(),B));
    TestTrue(TEXT("bookmark applies instantly"),P->ApplySensorPoseBookmark(S,IdA,Reason));
    TestFalse(TEXT("stopped sensor remains stopped"),S->IsSensorRunning());TestEqual(TEXT("bookmark never acquires a frame"),S->GetSensorRuntimeStatus().FrameId,int64(0));
    TestTrue(TEXT("recall preserves current scale"),S->GetActorScale3D().Equals(FVector(2,3,4)));
    const auto Snapshot=P->GetCurrentPlacementSnapshot(S);TestEqual(TEXT("snapshot links applied bookmark"),Snapshot.BookmarkId,IdA);TestFalse(TEXT("snapshot is named placement"),Snapshot.bDirectPlacement);
    auto* Reload=NewObject<UVirtualSensorPlacementSubsystem>(W);Reload->StorageRootOverrideForTests=Root;
    const auto PoseBeforeLoad=S->GetActorTransform();TestEqual(TEXT("new instance loads persisted library"),Reload->GetSensorPoseBookmarks(S).Num(),2);
    TestTrue(TEXT("loading never applies a saved pose"),S->GetActorTransform().Equals(PoseBeforeLoad));TestTrue(TEXT("new instance can apply persisted B"),Reload->ApplySensorPoseBookmark(S,IdB,Reason));
    TestEqual(TEXT("duplicate name is explicitly rejected"),P->GetPoseSaveStatus(P->RequestSaveSensorPose(S,TEXT("위치 A"))).State,EVirtualSensorPoseSaveState::Rejected);
    auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>(S);S->SetSharedOutputServices(nullptr,nullptr,Publisher);Publisher->StartStream(EVirtualSensorStreamKind::PointCloud,S->GetSensorId());
    const int32 Index=P->HistoryFor(S).Index;TestFalse(TEXT("locked undo is refused"),P->UndoSensorPlacement(S,Reason));TestEqual(TEXT("rejected undo preserves index"),P->HistoryFor(S).Index,Index);
    TestFalse(TEXT("locked bookmark is refused"),P->ApplySensorPoseBookmark(S,IdA,Reason));Publisher->StopAllStreams(S->GetSensorId());
    FString Original;const FString Path=P->StoragePath();FFileHelper::LoadFileToString(Original,*Path);
    IFileManager::Get().MakeDirectory(*Root,true);const FString Blocker=Root/TEXT("not-a-directory");FFileHelper::SaveStringToFile(TEXT("owned test blocker"),*Blocker);
    P->StorageRootOverrideForTests=Blocker;const auto Failed=Complete(P->RequestSaveSensorPose(S,TEXT("실패 시험")));
    TestEqual(TEXT("actual bad-directory write fails"),Failed.State,EVirtualSensorPoseSaveState::Failed);P->StorageRootOverrideForTests=Root;
    FString Preserved;FFileHelper::LoadFileToString(Preserved,*Path);TestEqual(TEXT("write failure preserves original file"),Preserved,Original);TestEqual(TEXT("failed save does not enter memory library"),P->GetSensorPoseBookmarks(S).Num(),2);
    for(int32 I=2;I<20;++I)TestEqual(TEXT("bounded library admits up to twenty"),Complete(P->RequestSaveSensorPose(S,FString::Printf(TEXT("위치-%02d"),I))).State,EVirtualSensorPoseSaveState::Saved);
    TestEqual(TEXT("twenty-first bookmark rejected"),P->GetPoseSaveStatus(P->RequestSaveSensorPose(S,TEXT("overflow"))).State,EVirtualSensorPoseSaveState::Rejected);
    TestEqual(TEXT("rename writes same bookmark"),Complete(P->RenameSensorPoseBookmark(S,IdA,TEXT("이름 변경 A"))).State,EVirtualSensorPoseSaveState::Saved);
    TestEqual(TEXT("delete writes updated file"),Complete(P->DeleteSensorPoseBookmark(S,IdB)).State,EVirtualSensorPoseSaveState::Saved);TestEqual(TEXT("delete only removes selected bookmark"),P->GetSensorPoseBookmarks(S).Num(),19);
    auto* Duplicate=W->SpawnActor<AVirtualLidarSensorActor>();Duplicate->ScanComponent->SensorId=S->GetSensorId();
    TestFalse(TEXT("duplicate ID cannot recall"),P->ApplySensorPoseBookmark(S,IdA,Reason));Duplicate->Destroy();
    S->ApplyEditableTransform(A,Reason);P->Histories.Reset();
    for(int32 I=0;I<60;++I){const auto Before=S->GetActorTransform();auto After=Before;After.AddToTranslation(FVector(1,0,0));S->ApplyEditableTransform(After,Reason);P->RecordPoseChange(S,Before,After);}
    TestEqual(TEXT("history is bounded to fifty"),P->HistoryFor(S).Commands.Num(),50);
    for(int32 I=0;I<50;++I)TestTrue(TEXT("bounded history undo succeeds"),P->UndoSensorPlacement(S,Reason));TestFalse(TEXT("past history boundary rejected"),P->UndoSensorPlacement(S,Reason));
    const FString OldId=S->GetSensorId();S->ScanComponent->SensorId=TEXT("new-id");TestEqual(TEXT("renaming ID never guesses old library target"),P->GetSensorPoseBookmarks(S).Num(),0);S->ScanComponent->SensorId=OldId;
    FFileHelper::SaveStringToFile(TEXT("{\"version\":999}"),*Path);auto* Bad=NewObject<UVirtualSensorPlacementSubsystem>(W);Bad->StorageRootOverrideForTests=Root;
    TestEqual(TEXT("unsupported file returns no entries"),Bad->GetSensorPoseBookmarks(S).Num(),0);TestEqual(TEXT("unsupported file prevents overwrite"),Bad->GetPoseSaveStatus(Bad->RequestSaveSensorPose(S,TEXT("x"))).State,EVirtualSensorPoseSaveState::Rejected);
    TestTrue(TEXT("canonical map strips PIE prefix"),UWorld::RemovePIEPrefix(TEXT("/Game/Maps/UEDPIE_2_Test"))==TEXT("/Game/Maps/Test"));
    S->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPlacementMotionTest,"MA0T10.Placement.ContinuousCadence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPlacementMotionTest::RunTest(const FString&)
{
    for(int32 Fps:{30,60,120})
    {
        FTransform Pose=FTransform::Identity;
        for(int32 I=0;I<Fps*2;++I)Pose=AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(Pose,EVirtualSensorCoordinateSpace::World,
            VirtualSensorPlacementMotion::Translation(FVector(1,0,0),125,1.f/Fps),VirtualSensorPlacementMotion::Rotation(FRotator(0,1,0),62.5f,1.f/Fps));
        TestTrue(TEXT("two-second travel is frame independent"),FMath::Abs(Pose.GetLocation().X-250)<.01);
        TestTrue(TEXT("two-second rotation is frame independent"),FMath::Abs(FMath::FindDeltaAngleDegrees(Pose.Rotator().Yaw,125))<.01);
    }
    TestTrue(TEXT("diagonal translation has same speed"),FMath::Abs(VirtualSensorPlacementMotion::Translation(FVector(1,1,1),125,.04f).Size()-5)<.001);
    TestTrue(TEXT("hitch travel is bounded to 50ms"),FMath::Abs(VirtualSensorPlacementMotion::Translation(FVector(1,0,0),125,1).Size()-6.25)<.001);
    TestTrue(TEXT("shift multiplier retained"),FMath::Abs(VirtualSensorPlacementMotion::Translation(FVector(1,0,0),125,.04f,5).Size()-25)<.001);
    TestTrue(TEXT("control multiplier retained"),FMath::Abs(VirtualSensorPlacementMotion::Translation(FVector(1,0,0),125,.04f,.2f).Size()-1)<.001);
    const FTransform Rotated(FRotator(0,90,0),FVector::ZeroVector);
    const auto Local=AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(Rotated,EVirtualSensorCoordinateSpace::Local,FVector(10,0,0),FRotator::ZeroRotator);
    TestTrue(TEXT("local forward follows sensor rotation"),Local.GetLocation().Equals(FVector(0,10,0),.001));
    return true;
}

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
