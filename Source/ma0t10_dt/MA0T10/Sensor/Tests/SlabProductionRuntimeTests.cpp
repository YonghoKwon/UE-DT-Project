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
namespace
{
class FSlabProductionRuntimeCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Start=FPlatformTime::Seconds();
    int32 Stage=0;
    FString ArchiveId,FirstRun;
    int64 FirstCount=0;
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
        auto* Settings=GetMutableDefault<UEditorPerformanceSettings>();
        Settings->bThrottleCPUWhenNotForeground=OldThrottle;Settings->bMonitorEditorPerformance=OldMonitor;
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>115){Test->AddError(TEXT("Production Slab runtime timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){World=C.World();break;}
        if(!World||!World->GetGameInstance())return false;
        ASlabScenarioValidationRig* Rig=nullptr;for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It){Rig=*It;break;}
        if(!Rig||!Rig->SlabActor)return false;
        auto* Slab=Rig->SlabActor.Get();auto* Catalog=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
        auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
        auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
        if(Stage==0)
        {
            if(FPlatformTime::Seconds()-Start<10)return false;
            Rig->SubmitSynthetic(true);Stage=1;return false;
        }
        const auto S=Slab->GetSimulationStatus();
        if(S.State==ESlabSimulationState::Failed){Test->AddError(S.Message);return true;}
        if(Stage==1)
        {
            if(S.State!=ESlabSimulationState::Playing)return false;
            ArchiveId=S.ScenarioUUID;FirstRun=S.RunUUID;
            Test->TestFalse(TEXT("live input has unique archive/run identity"),ArchiveId==FirstRun);
            Test->TestTrue(TEXT("live TC result archived"),Catalog->GetValidatedScenario(ArchiveId).IsValid());
            Test->TestTrue(TEXT("concurrent replay rejected"),!Catalog->CanReplay());
            Stage=2;return false;
        }
        if(S.State==ESlabSimulationState::Playing||S.State==ESlabSimulationState::Paused)return false;
        const auto SS=Session->GetSlabSensorSessionStatus();
        if(SS.State==EVirtualSlabSessionState::Draining)return false;
        Test->TestEqual(TEXT("exact last source frame"),S.FrameNo,int64(599));
        Test->TestTrue(TEXT("30 second timeline"),FMath::IsNearlyEqual(S.ElapsedSec,30.0,1.e-6));
        Test->TestEqual(TEXT("session drained"),SS.State,EVirtualSlabSessionState::Completed);
        int64 Submit=0,Receipt=0,Receive=0;
        for(const auto& T:Raw->GetStreamTelemetry())
        {
            if(T.StreamKind==EVirtualSensorStreamKind::PointCloud)
            {Submit+=T.SubmittedCount;Receipt+=T.ReceiptCount;Receive+=T.ConsumerReceivedCount;}
            else Test->TestEqual(TEXT("unselected Topic submissions zero"),T.SubmittedCount,int64(0));
            Test->TestEqual(TEXT("consumer valid"),T.ValidationFailureCount,int64(0));
            Test->TestEqual(TEXT("queue bounded"),T.OverloadCount,int64(0));
            Test->TestEqual(TEXT("frame gap zero"),T.FrameGapCount,int64(0));
            Test->TestEqual(TEXT("duplicate zero"),T.DuplicateCount,int64(0));
        }
        if(Receive!=Submit||Receipt!=Submit)return false;
        if(Stage==2)
        {
            Rig->SaveEvidence(false); // Persist the first run before the second replaces its status.
            FirstCount=Receive;Test->TestTrue(TEXT("first run real PCD data"),FirstCount>=570);
            FVirtualSlabSensorOutputSelection Outputs;
            Test->TestTrue(TEXT("production adapter replay accepted"),Catalog->RequestScenarioReplayWithOutputs(ArchiveId,Outputs,{}));
            Test->TestNotEqual(TEXT("replay gets new RunUUID"),Catalog->GetReplayStatus().RunUUID,FirstRun);
            Stage=3;return false;
        }
        Test->TestTrue(TEXT("second run real PCD data"),Receive-FirstCount>=570);
        Test->TestEqual(TEXT("replay preserves archive ID"),Slab->GetSimulationStatus().ScenarioUUID,ArchiveId);
        Test->TestEqual(TEXT("two runs all receipts"),Receipt,Submit);Test->TestEqual(TEXT("two runs all real consumers"),Receive,Submit);
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
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FSlabProductionRuntimeCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
