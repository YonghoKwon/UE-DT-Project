#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Components/StaticMeshComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "FinalAcceptanceTestSession.h"
namespace
{
struct FSlabWorldResourceWitness { TArray<TWeakObjectPtr<UObject>> Objects; };
class FSlabResourceAfterEndPlayCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;TSharedRef<FSlabWorldResourceWitness> Witness;double Started=-1;
public:
    FSlabResourceAfterEndPlayCheck(FAutomationTestBase* In,TSharedRef<FSlabWorldResourceWitness> W):Test(In),Witness(W){}
    bool Update() override
    {
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)return false;
        if(Started<0)Started=FPlatformTime::Seconds();
        if(FPlatformTime::Seconds()-Started<2||UVirtualCameraCaptureComponent::GetGlobalPendingWorkCountForDiagnostics()>0)
        {if(FPlatformTime::Seconds()-Started>10){Test->AddError(TEXT("Camera encode jobs did not return after PIE shutdown"));return true;}return false;}
        // Post-performance diagnostic only: never force GC between scenario runs.
        CollectGarbage(RF_NoFlags);
        for(const auto& Weak:Witness->Objects)Test->TestFalse(TEXT("old PIE owner not retained after cleanup/GC"),Weak.IsValid());
        Test->AddInfo(FString::Printf(TEXT("Post-PIE weak owner checks=%d global camera jobs=0"),Witness->Objects.Num()));return true;
    }
};
class FSlabProductionRuntimeCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Start=FPlatformTime::Seconds();
    int32 Stage=0;
    int32 CompletedRuns=0;
    const bool bRepeated=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_REPEAT_RHI"))==TEXT("1");
    const FString RequestedRuns=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_REPEAT_COUNT"));
    const int32 TargetRuns=RequestedRuns.IsEmpty()?10:FMath::Clamp(FCString::Atoi(*RequestedRuns),2,100);
    TSet<FString> RunIds;
    int64 WorkerSerial=0,ConnectionRevision=0;
    TWeakObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> ObservedRaw;
    FDelegateHandle ReceiveHandle;
    TSet<FString> ActualConsumerRequests;
    TSet<FString> AuditedRuns;
    bool bAuditOverflow=false;
    double StoppedAt=0;
    TSharedPtr<FSlabWorldResourceWitness> Witness;
    FString ArchiveId,FirstRun;
    int64 FirstCount=0;
	int64 FirstAllSubmitted=0;
	FTransform InitialPlacement;
	double ResetAt=0;
	bool bResetBetweenRuns=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_RESET_RHI"))==TEXT("1");
	bool bAllOutputs=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_OUTPUT_ALL"))==TEXT("1");
    bool OldThrottle=false,OldMonitor=false;
    const bool bPlacementAB=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PLACEMENT_AB_RHI"))==TEXT("1");
    int32 PoseSetup=0;FString PoseRequest,BookmarkA,BookmarkB;FTransform PoseA,PoseB;bool AppliedB=false;
public:
    explicit FSlabProductionRuntimeCheck(FAutomationTestBase* T,TSharedPtr<FSlabWorldResourceWitness> W=nullptr):Test(T),Witness(W)
    {
        // UE's own PIE performance tests disable these independently of t.IdleWhenNotForeground.
        // This affects this test process only; never persist the user's Editor settings.
        auto* Settings=GetMutableDefault<UEditorPerformanceSettings>();
        OldThrottle=Settings->bThrottleCPUWhenNotForeground;OldMonitor=Settings->bMonitorEditorPerformance;
        Settings->bThrottleCPUWhenNotForeground=false;Settings->bMonitorEditorPerformance=false;
        Test->AddInfo(FString::Printf(TEXT("Test-only background throttle %d->0, editor performance monitor %d->0; restored at completion; no SaveConfig"),OldThrottle,OldMonitor));
    }
    ~FSlabProductionRuntimeCheck()
    {
        if(ObservedRaw.IsValid())ObservedRaw->OnTransportObservation.Remove(ReceiveHandle);
        auto* Settings=GetMutableDefault<UEditorPerformanceSettings>();
        Settings->bThrottleCPUWhenNotForeground=OldThrottle;Settings->bMonitorEditorPerformance=OldMonitor;
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>(bRepeated?TargetRuns*34+100:115)){Test->AddError(TEXT("Production Slab runtime timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){World=C.World();break;}
        if(!World||!World->GetGameInstance())return false;
        if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_EXACT_VIEWPORT"))==TEXT("1")&&!FinalAcceptanceViewportReady(World))return false;
        ASlabScenarioValidationRig* Rig=nullptr;for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It){Rig=*It;break;}
        if(!Rig||!Rig->SlabActor)return false;
        auto* Slab=Rig->SlabActor.Get();auto* Catalog=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
        auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
        auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
        if(Stage==0)
        {
            if(FPlatformTime::Seconds()-Start<10)return false;
            if(bPlacementAB)
            {
                auto* P=World->GetSubsystem<UVirtualSensorPlacementSubsystem>();auto* L=Rig->Lidar.Get();FString Error;
                if(PoseSetup==0)
                {Test->TestTrue(TEXT("A/B fixture uses isolated pose library"),P->SetAutomationStorageRoot(FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementWorkflow/ab-bookmarks")/FGuid::NewGuid().ToString()));PoseA=L->GetActorTransform();PoseB=PoseA;PoseB.AddToTranslation(FVector(500,0,0));PoseRequest=P->RequestSaveSensorPose(L,TEXT("배치 A"));PoseSetup=1;return false;}
                const auto Save=P->GetPoseSaveStatus(PoseRequest);if(Save.State==EVirtualSensorPoseSaveState::Writing)return false;
                if(Save.State!=EVirtualSensorPoseSaveState::Saved){Test->AddError(Save.Message);return true;}
                if(PoseSetup==1){BookmarkA=Save.BookmarkId;Test->TestTrue(TEXT("set B fixture pose"),L->ApplyEditableTransform(PoseB,Error));PoseRequest=P->RequestSaveSensorPose(L,TEXT("배치 B"));PoseSetup=2;return false;}
                if(PoseSetup==2){BookmarkB=Save.BookmarkId;Test->TestTrue(TEXT("first run recalls A"),P->ApplySensorPoseBookmark(L,BookmarkA,Error));PoseSetup=3;}
            }
            InitialPlacement=Slab->GetActorTransform();
            if(Witness){for(UObject* Object:{static_cast<UObject*>(World),static_cast<UObject*>(World->GetGameInstance()),static_cast<UObject*>(Raw),static_cast<UObject*>(Session),static_cast<UObject*>(Catalog),static_cast<UObject*>(Slab),static_cast<UObject*>(Rig),static_cast<UObject*>(Rig->Camera.Get()),static_cast<UObject*>(Rig->Lidar.Get()),static_cast<UObject*>(Rig->Camera->CaptureComponent.Get()),static_cast<UObject*>(Rig->Lidar->ScanComponent.Get()),static_cast<UObject*>(World->GetSubsystem<USlabRunResultsSubsystem>())})Witness->Objects.Add(Object);}
            ObservedRaw=Raw;ReceiveHandle=Raw->OnTransportObservation.AddLambda([this](const FVirtualSensorTransportObservation& Data){
                if(Data.Phase!=EVirtualSensorTransportObservationPhase::Consumed||Data.RunId.IsEmpty()||AuditedRuns.Contains(Data.RunId))return;
                const FString Key=Data.RunId+TEXT("|")+Data.RequestId;
                if(!ActualConsumerRequests.Contains(Key)&&ActualConsumerRequests.Num()>=65536){if(!bAuditOverflow)Test->AddError(TEXT("Per-run consumer audit overflow; validation cannot continue silently"));bAuditOverflow=true;return;}
                ActualConsumerRequests.Add(Key);});
            Rig->SaveResourceEvidence(TEXT("warmup"),0);
            if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SENSOR_RESOURCE_PREVIEW_OFF"))==TEXT("1"))
            {Rig->Lidar->VisualizationComponent->SetWorldPointCloudEnabled(false);Rig->Lidar->ScanComponent->SetPointCloudPreviewEnabled(false);}
			if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_REQUIRE_DATA"))==TEXT("1"))
			{FSlabExecutionOptions Options;Options.Policy=ESlabExecutionPolicy::RequireData;Test->TestTrue(TEXT("data-required options accepted"),Slab->SetExecutionOptions(Options));}
            Rig->SubmitSynthetic(true);
			if(bAllOutputs)
			{
				for(TActorIterator<AVirtualCameraSensorActor> It(World);It;++It){FVirtualSensorEditableState State,Applied;It->ReadEditableState(State);State.CameraProfile=EVirtualCameraDeviceProfile::IntelRealSenseD455;State.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;FString Error;Test->TestTrue(TEXT("fullspec camera fixture applied"),It->ApplyProfileAndSimulationQuality(State,Applied,Error));}
				auto Outputs=Slab->GetSensorOutputs();Outputs.bCameraImage=true;Outputs.bLidarTelemetry=true;Slab->SetSensorOutputs(Outputs);
			}
			Stage=1;return false;
        }
        const auto S=Slab->GetSimulationStatus();
        if(Stage==30)
        {
            if(FPlatformTime::Seconds()-StoppedAt<3)return false;
            Rig->SaveResourceEvidence(TEXT("sensors-stopped"),CompletedRuns);
            if(Rig->Camera){const auto C=Rig->Camera->CaptureComponent->GetResourceSnapshot();
                for(const auto* Key:{TEXT("pending_readbacks"),TEXT("pending_encodes"),TEXT("active_encodes"),TEXT("global_encode_jobs"),TEXT("completed_reorder"),TEXT("encode_order"),TEXT("acquisition_snapshots"),TEXT("slab_contexts"),TEXT("readbacks_in_flight")})
                    Test->TestEqual(FString::Printf(TEXT("Camera stopped %s returned"),Key),C.Counts.FindRef(Key),int64(0));}
            return true;
        }
		if(Stage==20)
		{
			if(FPlatformTime::Seconds()-ResetAt<1)return false;
			int64 Count=0;for(const auto& T:Raw->GetStreamTelemetry())Count+=T.SubmittedCount;
			Test->TestEqual(TEXT("reset introduces zero socket submissions"),Count,FirstAllSubmitted);
			Test->TestEqual(TEXT("reset remains idle"),S.State,ESlabSimulationState::Idle);
			Test->TestTrue(TEXT("reset remains at level placement"),Slab->GetActorLocation().Equals(InitialPlacement.GetLocation(),.1));
			FVirtualSlabSensorOutputSelection Outputs=Slab->GetSensorOutputs();
			if(bPlacementAB&&!AppliedB)
            {FString Error;auto* P=World->GetSubsystem<UVirtualSensorPlacementSubsystem>();Test->TestTrue(TEXT("drained run allows position B recall"),P->ApplySensorPoseBookmark(Rig->Lidar,BookmarkB,Error));Test->TestTrue(TEXT("B recall matches exact pose"),UVirtualSensorPlacementSubsystem::SamePose(Rig->Lidar->GetActorTransform(),PoseB));AppliedB=true;}
			Test->TestTrue(TEXT("same archived scenario replays after reset"),Catalog->RequestScenarioReplayWithExecutionOptions(ArchiveId,Outputs,{},Slab->GetExecutionOptions()));
			Test->TestNotEqual(TEXT("reset replay has fresh RunUUID"),Catalog->GetReplayStatus().RunUUID,FirstRun);
			Stage=3;return false;
		}
        if(S.State==ESlabSimulationState::Failed){Test->AddError(S.Message);return true;}
        if(Stage==1)
        {
            if(S.State!=ESlabSimulationState::Playing)return false;
            ArchiveId=S.ScenarioUUID;FirstRun=S.RunUUID;
            RunIds.Add(FirstRun);WorkerSerial=Raw->GetWorkerInstanceSerial();ConnectionRevision=Raw->GetConnectionRevision();
            Test->TestFalse(TEXT("live input has unique archive/run identity"),ArchiveId==FirstRun);
            Test->TestTrue(TEXT("live TC result archived"),Catalog->GetValidatedScenario(ArchiveId).IsValid());
            Test->TestTrue(TEXT("concurrent replay rejected"),!Catalog->CanReplay());
            Stage=2;return false;
        }
        if(S.State==ESlabSimulationState::Playing||S.State==ESlabSimulationState::Paused)return false;
        const auto SS=Session->GetSlabSensorSessionStatus();
		if(SS.State==EVirtualSlabSessionState::Preparing||Catalog->GetReplayStatus().State==ESlabScenarioReplayState::Starting)return false;
        if(SS.State==EVirtualSlabSessionState::Draining)return false;
        if(Catalog->IsReplayBusy())return false;
        Test->TestEqual(TEXT("exact last source frame"),S.FrameNo,int64(599));
        Test->TestTrue(TEXT("30 second timeline"),FMath::IsNearlyEqual(S.ElapsedSec,30.0,1.e-6));
        Test->TestEqual(TEXT("session drained"),SS.State,EVirtualSlabSessionState::Completed);
        int64 Submit=0,Receipt=0,Receive=0;
        for(const auto& T:Raw->GetStreamTelemetry())
        {
            if(T.StreamKind==EVirtualSensorStreamKind::PointCloud)
            {Submit+=T.SubmittedCount;Receipt+=T.ReceiptCount;Receive+=T.ConsumerReceivedCount;}
            else if(!bAllOutputs)Test->TestEqual(TEXT("unselected Topic submissions zero"),T.SubmittedCount,int64(0));
            Test->TestEqual(TEXT("consumer valid"),T.ValidationFailureCount,int64(0));
            Test->TestEqual(TEXT("queue bounded"),T.OverloadCount,int64(0));
            Test->TestEqual(TEXT("frame gap zero"),T.FrameGapCount,int64(0));
            Test->TestEqual(TEXT("duplicate zero"),T.DuplicateCount,int64(0));
        }
        if(Receive!=Submit||Receipt!=Submit)return false;
        const auto Audit=World->GetSubsystem<USlabRunResultsSubsystem>()->GetRunFrameAuditForTesting(S.RunUUID);
        for(const auto& A:Audit)
        {
            Test->TestTrue(TEXT("accepted request submitted and receipted"),(A.Flags&7)==7);
            Test->TestTrue(TEXT("accepted request actually consumed"),ActualConsumerRequests.Contains(S.RunUUID+TEXT("|")+A.RequestId));
        }
        Test->TestFalse(TEXT("consumer audit never exceeded its explicit bound"),bAuditOverflow);
        Test->TestEqual(TEXT("actual per-run MESSAGE identities exactly match accepted identities"),ActualConsumerRequests.Num(),Audit.Num());
        if(Raw->GetRunRetentionSnapshot().RetiringRuns>0)return false;
        if(bRepeated)
        {
            Test->TestEqual(TEXT("same worker retained across runs"),Raw->GetWorkerInstanceSerial(),WorkerSerial);
            Test->TestEqual(TEXT("same connection retained across runs"),Raw->GetConnectionRevision(),ConnectionRevision);
            const auto Retention=Raw->GetRunRetentionSnapshot();
            Test->TestEqual(TEXT("no active managed run after drain"),Retention.ActiveRuns,0);Test->TestEqual(TEXT("no queued body after drain"),Retention.OutstandingBytes,int64(0));
            Test->TestEqual(TEXT("closed run sequence returned"),Retention.SequenceEntries,0);Test->TestTrue(TEXT("closed diagnostics bounded"),Retention.CompletedRuns<=20);
        }
        if(Stage==2)
        {
            if(bPlacementAB)
            {FSlabRunDeliverySummary R;Test->TestTrue(TEXT("A run result exists"),World->GetSubsystem<USlabRunResultsSubsystem>()->GetRunResult(S.RunUUID,R));const auto* Entry=R.Sensors.FindByPredicate([&](const auto& E){return E.SensorId==Rig->Lidar->GetSensorId();});Test->TestTrue(TEXT("A run records applied bookmark and start pose"),Entry&&Entry->Placement.BookmarkId==BookmarkA&&UVirtualSensorPlacementSubsystem::SamePose(Entry->Placement.WorldTransform,PoseA)&&!Entry->Placement.bChangedDuringRun);}
            Rig->SaveEvidence(false); // Persist the first run before the second replaces its status.
            int64 AuditBytes=ActualConsumerRequests.GetAllocatedSize();for(const auto& K:ActualConsumerRequests)AuditBytes+=K.GetAllocatedSize();
            Rig->SaveResourceEvidence(TEXT("drained"),1,ActualConsumerRequests.Num(),AuditBytes);
            AuditedRuns.Add(S.RunUUID);ActualConsumerRequests.Empty();
            FirstCount=Receive;Test->TestTrue(TEXT("first run real PCD data"),FirstCount>=570);
            CompletedRuns=1;
			FirstAllSubmitted=0;for(const auto& T:Raw->GetStreamTelemetry())FirstAllSubmitted+=T.SubmittedCount;
			if(bResetBetweenRuns)
			{
				FString Reason;const auto Shape=Slab->SlabMesh->GetRelativeScale3D();
				Test->TestTrue(TEXT("drained live run permits reset"),Slab->ResetToInitialPlacement(Reason));
				Test->TestTrue(TEXT("reset uses initial level placement"),Slab->GetActorLocation().Equals(InitialPlacement.GetLocation(),.1));
				Test->TestEqual(TEXT("reset preserves active slab dimensions"),Slab->SlabMesh->GetRelativeScale3D(),Shape);
				Test->TestTrue(TEXT("reset preserves archived data"),Catalog->GetValidatedScenario(ArchiveId).IsValid());
				ResetAt=FPlatformTime::Seconds();Stage=20;return false;
			}
            FVirtualSlabSensorOutputSelection Outputs=Slab->GetSensorOutputs();
            Test->TestTrue(TEXT("production adapter replay accepted"),Catalog->RequestScenarioReplayWithExecutionOptions(ArchiveId,Outputs,{},Slab->GetExecutionOptions()));
            Test->TestNotEqual(TEXT("replay gets new RunUUID"),Catalog->GetReplayStatus().RunUUID,FirstRun);
            Stage=3;return false;
        }
        Test->TestTrue(TEXT("second run real PCD data"),Receive-FirstCount>=570);
        Test->TestEqual(TEXT("replay preserves archive ID"),Slab->GetSimulationStatus().ScenarioUUID,ArchiveId);
        Test->TestEqual(TEXT("two runs all receipts"),Receipt,Submit);Test->TestEqual(TEXT("two runs all real consumers"),Receive,Submit);
		FSlabRunDeliverySummary Summary;auto* Results=World->GetSubsystem<USlabRunResultsSubsystem>();
		Test->TestTrue(TEXT("run delivery summary retained"),Results->GetRunResult(S.RunUUID,Summary));
		Test->TestEqual(TEXT("run summary has all broker receipts"),Summary.Receipts,Summary.Accepted);
		Test->TestEqual(TEXT("run summary has no delivery failures"),Summary.DeliveryFailures,int64(0));
        Test->TestEqual(TEXT("run summary broker outcome"),Summary.Outcome,ESlabRunDeliveryOutcome::BrokerAccepted);
        Test->TestEqual(TEXT("completed snapshot has no unfinished work"),Summary.TerminalSnapshot.UniqueUnfinished,int64(0));
        if(bPlacementAB)
        {
            const auto* Entry=Summary.Sensors.FindByPredicate([&](const auto& E){return E.SensorId==Rig->Lidar->GetSensorId();});Test->TestTrue(TEXT("B run records applied bookmark and start pose"),Entry&&Entry->Placement.BookmarkId==BookmarkB&&UVirtualSensorPlacementSubsystem::SamePose(Entry->Placement.WorldTransform,PoseB)&&!Entry->Placement.bChangedDuringRun);
            FSlabRunComparison Comparison;FString Reason;const double ComparisonStart=FPlatformTime::Seconds();
            Test->TestTrue(TEXT("real finalized A/B runs compare"),Results->GetRunComparison(FirstRun,S.RunUUID,Comparison,Reason));
            Test->TestTrue(TEXT("same source hash captured at both run starts"),!Comparison.A.ScenarioContentHash.IsEmpty()&&Comparison.A.ScenarioContentHash==Comparison.B.ScenarioContentHash);
            const auto* Delta=Comparison.Sensors.FindByPredicate([&](const auto& C){return C.SensorId==Rig->Lidar->GetSensorId();});Test->TestTrue(TEXT("real A/B displacement matches planned 500cm"),Delta&&Delta->bPoseDeltaAvailable&&FMath::Abs(Delta->TranslationDeltaCm-500)<.1);
            auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Panel=Cast<UVirtualSensorCaptureExportPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Data));
            Test->TestNotNull(TEXT("actual owned results UI exists"),Panel);
            if(Panel)
            {const auto BeforePose=Rig->Lidar->GetActorTransform();const int64 BeforeSubmit=Submit;Panel->SetActiveTab(EVirtualSensorCaptureExportTab::ConnectionLog);
                Test->TestTrue(TEXT("actual UI accepts finalized A and B"),Panel->SelectScenarioComparisonRun(FirstRun,true,Reason)&&Panel->SelectScenarioComparisonRun(S.RunUUID,false,Reason));
                Test->TestTrue(TEXT("actual UI shows pose difference"),Panel->GetScenarioRunComparisonText().Contains(TEXT("500.00cm")));
                Test->TestTrue(TEXT("comparison leaves current pose unchanged"),Rig->Lidar->GetActorTransform().Equals(BeforePose));
                int64 AfterSubmit=0;for(const auto& T:Raw->GetStreamTelemetry())if(T.StreamKind==EVirtualSensorStreamKind::PointCloud)AfterSubmit+=T.SubmittedCount;
                Test->TestEqual(TEXT("comparison never submits sensor data"),AfterSubmit,BeforeSubmit);
            }
            const double BuildMs=(FPlatformTime::Seconds()-ComparisonStart)*1000;Test->AddInfo(FString::Printf(TEXT("real_ab_build_and_ui_ms=%.3f"),BuildMs));
            FFileHelper::SaveStringToFile(DescribeRunComparison(Comparison),*(FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementComparison/actual-ab.txt")));
        }
        if(bRepeated)
        {
            Test->TestFalse(TEXT("every replay has unique run UUID"),RunIds.Contains(S.RunUUID));RunIds.Add(S.RunUUID);++CompletedRuns;
            Rig->SaveEvidence(false);
            int64 AuditBytes=ActualConsumerRequests.GetAllocatedSize();for(const auto& K:ActualConsumerRequests)AuditBytes+=K.GetAllocatedSize();
            Rig->SaveResourceEvidence(TEXT("drained"),CompletedRuns,ActualConsumerRequests.Num(),AuditBytes);
            AuditedRuns.Add(S.RunUUID);ActualConsumerRequests.Empty();
            if(CompletedRuns<TargetRuns)
            {
                FirstCount=Receive;FirstAllSubmitted=0;for(const auto& T:Raw->GetStreamTelemetry())FirstAllSubmitted+=T.SubmittedCount;
                FString Reason;Test->TestTrue(TEXT("repeated run reset accepted"),Slab->ResetToInitialPlacement(Reason));ResetAt=FPlatformTime::Seconds();Stage=20;return false;
            }
            Test->TestEqual(TEXT("requested distinct completed runs"),RunIds.Num(),TargetRuns);
            Rig->Camera->StopSensor();Rig->Lidar->StopSensor();StoppedAt=FPlatformTime::Seconds();Stage=30;return false;
        }
        // Screenshots/readback are deliberately outside both measured runs.
        Rig->SaveEvidence();
        return true;
    }
};
/** Post-run UI cost only; separate from the two measured transport runs. */
class FSlabComparisonUiCostCheck:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double At=-1;int32 Phase=0;TArray<double> Hidden,Visible;bool WasOpen=false;
public:
    explicit FSlabComparisonUiCostCheck(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PLACEMENT_AB_RHI"))!=TEXT("1"))return true;
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!World)return false;
        auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Panel=Cast<UVirtualSensorCaptureExportPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Data));if(!Panel){Test->AddError(TEXT("No owned comparison panel for UI cost check"));return true;}
        if(At<0){WasOpen=Workspace->IsPanelOpen(ESensorToolPanelRole::Data);Workspace->SetPanelOpen(ESensorToolPanelRole::Data,false);At=FPlatformTime::Seconds();return false;}
        const double Age=FPlatformTime::Seconds()-At;
        if(Age>=1)(Phase==0?Hidden:Visible).Add(FApp::GetDeltaTime()*1000);
        if(Age<3)return false;
        if(Phase++==0){Panel->SetActiveTab(EVirtualSensorCaptureExportTab::ConnectionLog);Panel->SetScenarioRunResultsExpanded(true);Workspace->SetPanelOpen(ESensorToolPanelRole::Data,true);At=FPlatformTime::Seconds();return false;}
        auto P95=[](TArray<double> V){V.Sort();return V.IsEmpty()?0.:V[FMath::CeilToInt(V.Num()*.95)-1];};
        const double H=P95(Hidden),V=P95(Visible);auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("hidden_frame_p95_ms"),H);Root->SetNumberField(TEXT("visible_comparison_frame_p95_ms"),V);Root->SetNumberField(TEXT("hidden_samples"),Hidden.Num());Root->SetNumberField(TEXT("visible_samples"),Visible.Num());Root->SetStringField(TEXT("scope"),TEXT("post-run same scene, settled UI; not rated streaming acceptance"));
        FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementComparison/comparison-ui-cost.json")));
        Test->AddInfo(FString::Printf(TEXT("comparison UI post-run frame p95 hidden/visible=%.3f/%.3fms samples=%d/%d"),H,V,Hidden.Num(),Visible.Num()));
        Test->TestTrue(TEXT("comparison UI settled-frame p95 increase <=5 percent"),H>0&&V<=H*1.05);Workspace->SetPanelOpen(ESensorToolPanelRole::Data,WasOpen);return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabProductionRuntimeTest,"MA0T10.SlabScenario.ProductionRuntime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabProductionRuntimeTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_RHI"))!=TEXT("1"))
    {AddInfo(TEXT("Skipped opt-in actual RHI/Artemis test; set MA0T10_SLAB_RHI=1 with local broker."));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_EXACT_VIEWPORT"))==TEXT("1")){ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());}
    else {ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));}
    ADD_LATENT_AUTOMATION_COMMAND(FSlabProductionRuntimeCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FSlabComparisonUiCostCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabRepeatedRuntimeTest,"MA0T10.SlabReliability.RepeatedRuntime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabRepeatedRuntimeTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_REPEAT_RHI"))!=TEXT("1")){AddInfo(TEXT("Skipped opt-in same-worker thirty-second repeated-run test."));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    auto Witness=MakeShared<FSlabWorldResourceWitness>();
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FSlabProductionRuntimeCheck(this,Witness));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FSlabResourceAfterEndPlayCheck(this,Witness));return true;
}
#endif
