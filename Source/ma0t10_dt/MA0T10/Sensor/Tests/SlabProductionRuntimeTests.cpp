#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
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
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
#include "FinalAcceptanceTestSession.h"
namespace
{
class FSlabProductionRuntimeCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Start=FPlatformTime::Seconds();
    int32 Stage=0;
    int32 CompletedRuns=0;
    const bool bRepeated=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_REPEAT_RHI"))==TEXT("1");
    TSet<FString> RunIds;
    int64 WorkerSerial=0,ConnectionRevision=0;
    TWeakObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> ObservedRaw;
    FDelegateHandle ReceiveHandle;
    TSet<FString> ActualConsumerRequests;
    FString ArchiveId,FirstRun;
    int64 FirstCount=0;
	int64 FirstAllSubmitted=0;
	FTransform InitialPlacement;
	double ResetAt=0;
	bool bResetBetweenRuns=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_RESET_RHI"))==TEXT("1");
	bool bAllOutputs=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_OUTPUT_ALL"))==TEXT("1");
    bool OldThrottle=false,OldMonitor=false;
public:
    explicit FSlabProductionRuntimeCheck(FAutomationTestBase* T):Test(T)
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
        if(FPlatformTime::Seconds()-Start>(bRepeated?370:115)){Test->AddError(TEXT("Production Slab runtime timed out"));return true;}
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
            InitialPlacement=Slab->GetActorTransform();
            ObservedRaw=Raw;ReceiveHandle=Raw->OnTransportObservation.AddLambda([this](const FVirtualSensorTransportObservation& Data){if(Data.Phase==EVirtualSensorTransportObservationPhase::Consumed&&!Data.RunId.IsEmpty()&&ActualConsumerRequests.Num()<100000)ActualConsumerRequests.Add(Data.RunId+TEXT("|")+Data.RequestId);});
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
		if(Stage==20)
		{
			if(FPlatformTime::Seconds()-ResetAt<1)return false;
			int64 Count=0;for(const auto& T:Raw->GetStreamTelemetry())Count+=T.SubmittedCount;
			Test->TestEqual(TEXT("reset introduces zero socket submissions"),Count,FirstAllSubmitted);
			Test->TestEqual(TEXT("reset remains idle"),S.State,ESlabSimulationState::Idle);
			Test->TestTrue(TEXT("reset remains at level placement"),Slab->GetActorLocation().Equals(InitialPlacement.GetLocation(),.1));
			FVirtualSlabSensorOutputSelection Outputs=Slab->GetSensorOutputs();
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
            Rig->SaveEvidence(false); // Persist the first run before the second replaces its status.
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
        if(bRepeated)
        {
            Test->TestFalse(TEXT("every replay has unique run UUID"),RunIds.Contains(S.RunUUID));RunIds.Add(S.RunUUID);++CompletedRuns;
            Rig->SaveEvidence(false);
            if(CompletedRuns<10)
            {
                FirstCount=Receive;FirstAllSubmitted=0;for(const auto& T:Raw->GetStreamTelemetry())FirstAllSubmitted+=T.SubmittedCount;
                FString Reason;Test->TestTrue(TEXT("repeated run reset accepted"),Slab->ResetToInitialPlacement(Reason));ResetAt=FPlatformTime::Seconds();Stage=20;return false;
            }
            Test->TestEqual(TEXT("ten distinct completed runs"),RunIds.Num(),10);return true;
        }
        // Screenshots/readback are deliberately outside both measured runs.
        Rig->SaveEvidence();
        return true;
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
    ADD_LATENT_AUTOMATION_COMMAND(FSlabProductionRuntimeCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabRepeatedRuntimeTest,"MA0T10.SlabReliability.RepeatedRuntime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabRepeatedRuntimeTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_REPEAT_RHI"))!=TEXT("1")){AddInfo(TEXT("Skipped opt-in same-worker thirty-second ten-run test."));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FSlabProductionRuntimeCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
