#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
class FCpuSnapshotParityRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds();
    TWeakObjectPtr<AVirtualLidarSensorActor> Sensor;
    TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> Reference;
    IConsoleVariable* Switch=nullptr;
    int Old=1,Phase=0;
    EConsoleVariableFlags Priority=ECVF_SetByConstructor;
    int64 Baseline=0;
public:
    explicit FCpuSnapshotParityRun(FAutomationTestBase* T):Test(T){}
    ~FCpuSnapshotParityRun(){if(Switch)Switch->Set(Old,Priority);if(Sensor.IsValid())Sensor->StopSensor();}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>30){Test->AddError(TEXT("CPU snapshot parity timed out"));return true;}
        if(Phase==0)
        {
            UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
            if(!World||!FinalAcceptanceViewportReady(World))return false;
            AShipHatchSimulation* Sim=nullptr;for(TActorIterator<AShipHatchSimulation>I(World);I;++I)Sim=*I;
            if(!Sim||Sim->IsGenerating()||!Sim->HasFreshMeasurement())return false;
            for(int I=0;I<3;++I)if(auto* A=Sim->SensorAt(I))A->StopSensor();
            Sensor=Sim->SensorAt(0);if(!Test->TestTrue(TEXT("CPU parity fixture sensor exists"),Sensor.IsValid()))return true;
            Switch=IConsoleManager::Get().FindConsoleVariable(TEXT("ma0t10.LidarCpuSnapshotNormalize"));
            if(!Test->TestNotNull(TEXT("CPU normalization diagnostic switch"),Switch))return true;
            Old=Switch->GetInt();Priority=EConsoleVariableFlags(Switch->GetFlags()&ECVF_SetByMask);Switch->Set(0,Priority);
            auto* Scan=Sensor->ScanComponent.Get();Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
            Scan->HorizontalSamples=256;Scan->VerticalChannels=16;Scan->bEnableRangeNoise=false;
            Scan->bEnableMaterialResponse=false;Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
            Scan->bUseRollingPointTimestamps=false;Scan->bEnableMotionDistortion=false;
            Baseline=Scan->GetRuntimeStatus().FrameId;Sensor->StartSensor();Scan->RequestImmediateScheduledScan();Phase=1;return false;
        }
        auto* Scan=Sensor->ScanComponent.Get();auto Frame=Scan->GetLastFrameSnapshot();
        if(!Frame||Frame->FrameId<=Baseline)return false;
        if(Phase==1)
        {
            Reference=Frame;Sensor->StopSensor();Baseline=Frame->FrameId;Switch->Set(1,Priority);
            Sensor->StartSensor();Scan->RequestImmediateScheduledScan();Phase=2;return false;
        }
        Test->TestEqual(TEXT("worker preserves complete ray count"),Frame->Points->Num(),Reference->Points->Num());
        Test->TestTrue(TEXT("completed frame IDs advance in order"),Frame->FrameId>Reference->FrameId);
        const int Count=FMath::Min(Frame->Points->Num(),Reference->Points->Num());
        for(int I=0;I<Count;++I)
        {
            const auto& A=(*Reference->Points)[I];const auto& B=(*Frame->Points)[I];
            if(A.bHit!=B.bHit||A.RangeMillimeters!=B.RangeMillimeters||A.RawIntensity!=B.RawIntensity||A.Validity!=B.Validity||A.Confidence!=B.Confidence||A.SemanticLabel!=B.SemanticLabel||A.EchoCount!=B.EchoCount||A.HitActorName!=B.HitActorName||!A.WorldLocation.Equals(B.WorldLocation,.001))
            {Test->AddError(FString::Printf(TEXT("CPU worker/GT snapshot parity differs at ray%d"),I));break;}
        }
        Sensor->StopSensor();return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpuSnapshotWorkerParityTest,"MA0T10.GpuLidar.CpuSnapshotWorkerParityRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCpuSnapshotWorkerParityTest::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());
    ADD_LATENT_AUTOMATION_COMMAND(FCpuSnapshotParityRun(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
