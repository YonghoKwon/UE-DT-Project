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
#include "ma0t10_dt/MA0T10/Core/SlabRunComparison.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"

namespace
{
FSlabRunDeliverySummary ComparisonFixture(const FString& Run)
{
    FSlabRunDeliverySummary R;R.RunUUID=Run;R.ScenarioUUID=TEXT("same-scenario");R.ScenarioContentHash=TEXT("sha1-immutable-source");R.bFinalized=true;R.Outcome=ESlabRunDeliveryOutcome::BrokerAccepted;R.LastSlabFrame=599;R.ElapsedSec=29.95;R.AppliedOutputs=R.Outputs;
    auto& S=R.Sensors.AddDefaulted_GetRef();S.SensorId=TEXT("lidar-1");S.Placement.SensorKind=EVirtualSensorKind::Lidar;S.Placement.MapPackage=TEXT("/Game/Test");S.Configuration=TEXT("{\"n\":1,\"nested\":{\"enabled\":true}}");S.SettingsRevision=1;S.RequestedHz=20;S.MeasuredHz=19.9;
    return R;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabRunComparisonMathTest,"MA0T10.Placement.RunComparisonValues",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabRunComparisonMathTest::RunTest(const FString&)
{
    auto A=ComparisonFixture(TEXT("run-A")),B=ComparisonFixture(TEXT("run-B"));
    const FString Json=FSlabScenarioCodec::MakeSyntheticJson();FSlabScenarioDataPtr SourceA,SourceB;FString SourceReason;
    TestTrue(TEXT("six-hundred-row source parses"),FSlabScenarioCodec::Parse(Json,SourceA,SourceReason));
    TestTrue(TEXT("same UUID changed original bytes parses"),FSlabScenarioCodec::Parse(Json+TEXT("\n"),SourceB,SourceReason));
    if(SourceA&&SourceB)
    {TestTrue(TEXT("source provenance distinguishes same UUID with different original bytes"),SourceA->ScenarioUUID==SourceB->ScenarioUUID&&SourceA->OriginalContentSha1!=SourceB->OriginalContentSha1);TestEqual(TEXT("hashing never rewrites source"),SourceA->OriginalJson,Json);}
    A.Accepted=A.Submitted=A.Receipts=600;B.Accepted=B.Submitted=B.Receipts=599;
    B.Sensors[0].Configuration=TEXT("{ \"nested\": { \"enabled\": true }, \"n\": 1.0 }");B.Sensors[0].SettingsRevision=99;
    B.Sensors[0].Placement.WorldTransform.SetLocation(FVector(300,400,0));B.Sensors[0].Placement.WorldTransform.SetRotation(FRotator(0,90,0).Quaternion());B.Sensors[0].Placement.WorldTransform.SetScale3D(FVector(2,1,1));
    FSlabRunComparison C;FString Reason;TestTrue(TEXT("finalized same-scenario results compare"),BuildRunComparison(A,B,C,Reason));
    TestTrue(TEXT("JSON order/number spelling/revision alone do not change settings"),C.bRecordedConditionsMatch&&C.Sensors[0].ChangedSettings.IsEmpty());
    TestTrue(TEXT("distance is measured in world centimeters"),FMath::Abs(C.Sensors[0].TranslationDeltaCm-500)<1.e-6);
    TestTrue(TEXT("rotation uses shortest quaternion angle"),FMath::Abs(C.Sensors[0].RotationDeltaDegrees-90)<1.e-6);
    TestTrue(TEXT("scale differences retained separately"),C.Sensors[0].ScaleDelta.Equals(FVector(1,0,0)));
    const auto Text=DescribeRunComparison(C);TestTrue(TEXT("whole-run counters remain explicitly whole-run"),Text.Contains(TEXT("실행 전체 카운터"))&&Text.Contains(TEXT("600/599")));
    TestTrue(TEXT("recorded Hz is not an average claim"),Text.Contains(TEXT("기록 시점 측정 Hz")));
    B.Sensors[0].Placement.WorldTransform=A.Sensors[0].Placement.WorldTransform;auto Q=B.Sensors[0].Placement.WorldTransform.GetRotation();B.Sensors[0].Placement.WorldTransform.SetRotation(FQuat(-Q.X,-Q.Y,-Q.Z,-Q.W));
    BuildRunComparison(A,B,C,Reason);TestTrue(TEXT("q and minus q are same rotation"),C.Sensors[0].RotationDeltaDegrees<.0001);
    auto Other=A.Sensors[0];Other.SensorId=TEXT("camera-1");Other.Placement.SensorKind=EVirtualSensorKind::Camera;A.Sensors.Add(Other);B.Sensors.Insert(Other,0);
    BuildRunComparison(A,B,C,Reason);TestTrue(TEXT("different array order matches by kind/id"),C.Sensors.Num()==2&&C.Sensors[0].bPresentA&&C.Sensors[0].bPresentB&&C.Sensors[1].bPresentA&&C.Sensors[1].bPresentB);
    B.Sensors[1].Configuration=TEXT("{\"n\":2,\"nested\":{\"enabled\":true}}");BuildRunComparison(A,B,C,Reason);
    TestTrue(TEXT("real setting difference marks reference comparison"),!C.bRecordedConditionsMatch&&!C.ConditionDifferences.IsEmpty());
    B.Sensors[1].Placement.MapPackage=TEXT("/Game/Other");BuildRunComparison(A,B,C,Reason);
    TestTrue(TEXT("different maps suppress world-pose delta"),C.Sensors.ContainsByPredicate([](const auto& S){return S.SensorId==TEXT("lidar-1")&&!S.bPoseDeltaAvailable;}));
    B.ScenarioContentHash.Reset();B.Accepted=B.Receipts=0;B.Outcome=ESlabRunDeliveryOutcome::Partial;B.Sensors[0].Placement.bChangedDuringRun=true;BuildRunComparison(A,B,C,Reason);
    TestTrue(TEXT("unknown source, partial and external motion remain visible"),!C.UnknownConditions.IsEmpty()&&!C.Warnings.IsEmpty()&&!C.ConditionDifferences.IsEmpty());
    TestTrue(TEXT("zero denominator is N/A"),DescribeRunComparison(C).Contains(TEXT("N/A")));
    B.ScenarioUUID=TEXT("other");TestFalse(TEXT("different scenario rejected"),BuildRunComparison(A,B,C,Reason));B.ScenarioUUID=A.ScenarioUUID;B.RunUUID=A.RunUUID;
    TestFalse(TEXT("same run rejected"),BuildRunComparison(A,B,C,Reason));B.RunUUID=TEXT("run-B");B.bFinalized=false;
    TestFalse(TEXT("draining result rejected"),BuildRunComparison(A,B,C,Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabRunComparisonUiTest,"MA0T10.Placement.RunComparisonSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabRunComparisonUiTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* Results=W->GetSubsystem<USlabRunResultsSubsystem>();
    auto A=ComparisonFixture(TEXT("A")),B=ComparisonFixture(TEXT("B"));Results->Results={A,B};Results->Changed();
    auto* Panel=NewObject<UVirtualSensorCaptureExportPanelWidget>(W);FString Reason;
    TestTrue(TEXT("A/B selected explicitly"),Panel->SelectScenarioComparisonRun(TEXT("A"),true,Reason)&&Panel->SelectScenarioComparisonRun(TEXT("B"),false,Reason));
    TestTrue(TEXT("comparison text identifies runs"),Panel->GetScenarioRunComparisonText().Contains(TEXT("A: A"))&&Panel->GetScenarioRunComparisonText().Contains(TEXT("B: B")));
    auto* Sensor=W->SpawnActor<AVirtualLidarSensorActor>();const auto Pose=Sensor->GetActorTransform();const auto Frame=Sensor->GetSensorRuntimeStatus().FrameId;
    for(int32 I=0;I<4;++I){Panel->CachedRunResultsRevision=-1;Panel->RefreshRunResults();}
    TestTrue(TEXT("comparison never changes sensor pose or acquires a frame"),Sensor->GetActorTransform().Equals(Pose)&&Sensor->GetSensorRuntimeStatus().FrameId==Frame);
    Results->Results.Add(ComparisonFixture(TEXT("C")));Results->Changed();Panel->RefreshRunResults();
    TestTrue(TEXT("new result never replaces selected A/B"),Panel->ComparisonRunA==TEXT("A")&&Panel->ComparisonRunB==TEXT("B"));
    Results->Results.RemoveAt(0);Results->Changed();Panel->RefreshRunResults();
    TestTrue(TEXT("evicted comparison stops with explicit reason"),Panel->GetScenarioRunComparisonText().Contains(TEXT("보관 종료"))&&Panel->ComparisonRunA==TEXT("A"));
    Panel->ClearScenarioRunComparison();TestTrue(TEXT("clear does not select another run"),Panel->ComparisonRunA.IsEmpty()&&Panel->ComparisonRunB.IsEmpty());
    Sensor->Destroy();return true;
}

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
