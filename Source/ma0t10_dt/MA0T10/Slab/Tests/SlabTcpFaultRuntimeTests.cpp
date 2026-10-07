#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
namespace
{
class FSlabTcpFaultCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;FString Mode,Run;double Started=FPlatformTime::Seconds();int Stage=0;bool WasPlaying=false;
    TWeakObjectPtr<ASlabActor> Slab;TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;TWeakObjectPtr<AVirtualSensorCoordinator> Manager;FTransform Placement;
public:
    FSlabTcpFaultCheck(FAutomationTestBase* T,FString M):Test(T),Mode(M){}
    bool Update() override
    {
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!W->GetGameInstance())return false;
        if(FPlatformTime::Seconds()-Started>55){Test->AddError(TEXT("TCP fault runtime timeout"));return true;}
        auto* Context=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();auto* Results=W->GetSubsystem<USlabRunResultsSubsystem>();
        if(Stage==0)
        {
            Manager=W->SpawnActor<AVirtualSensorCoordinator>();Lidar=W->SpawnActor<AVirtualLidarSensorActor>();Lidar->StopSensor();Manager->DiscoverSensorsInLevel();
            auto P=Manager->SharedTransportComponent->GetTransportProfile();P.BrokerUrl=TEXT("tcp://127.0.0.1:18679");P.UserName=TEXT("fixture");P.TimeoutSeconds=2;Manager->SharedTransportComponent->TransportProfile=P;Manager->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
            FVirtualSensorEditableState State;Lidar->ReadEditableState(State);State.SimulationQuality=EVirtualSensorSimulationQuality::Custom;State.LidarScanInterval=Mode==TEXT("no_receipt")?2:.05f;State.LidarHorizontalSamples=4;State.LidarVerticalChannels=2;FString Error;Test->TestTrue(TEXT("small CPU fixture configured"),Lidar->ApplyEditableState(State,Error));Lidar->ScanComponent->AcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
            Slab=W->SpawnActor<ASlabActor>(FVector(123,70,42),FRotator(0,15,0));Slab->DimensionUnit=ESlabInputUnit::Millimeters;Placement=Slab->GetActorTransform();
            FSlabExecutionOptions Options;Options.Policy=ESlabExecutionPolicy::RequireData;Options.PreparationTimeoutSeconds=Mode==TEXT("auth")?2:5;Slab->SetExecutionOptions(Options);
            TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString())),Root);Root->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("duration_sec"),60);
            FString Json;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));FSlabScenarioDataPtr Data;FSlabScenarioCodec::Parse(Json,Data,Error,true);
            Test->TestTrue(TEXT("fault scenario admitted"),Slab->ReceiveScenario(Data));Run=Slab->GetSimulationStatus().RunUUID;Started=FPlatformTime::Seconds();Stage=1;return false;
        }
        const auto S=Slab->GetSimulationStatus();
        if(Stage==1)
        {
            if(S.State==ESlabSimulationState::Preparing){Test->TestTrue(TEXT("connection preparation cannot move slab"),Slab->GetActorTransform().Equals(Placement));return false;}
            if(S.State==ESlabSimulationState::Playing)
            {WasPlaying=true;if(Mode==TEXT("delay")&&S.ElapsedSec>.3){Slab->StopSimulation();Stage=2;}else if(Mode==TEXT("delete")&&S.ElapsedSec>.3&&Lidar.IsValid())Lidar->Destroy();return false;}
            Stage=2;
        }
        const auto Session=Context->GetSlabSensorSessionStatus();if(Session.State==EVirtualSlabSessionState::Draining||Session.State==EVirtualSlabSessionState::Running)return false;
        FSlabRunDeliverySummary R;if(!Results->GetRunResult(Run,R)||!R.bFinalized||R.SaveState==TEXT("저장 요청 접수"))return false;
        if(Mode==TEXT("auth"))
        {Test->TestFalse(TEXT("authentication rejection never starts movement"),WasPlaying);Test->TestEqual(TEXT("auth has zero accepted frames"),R.Accepted,int64(0));Test->TestEqual(TEXT("auth reported as preparation failure"),R.Outcome,ESlabRunDeliveryOutcome::PreparationFailed);}
        else if(Mode==TEXT("delay"))
        {Test->TestTrue(TEXT("delayed connection eventually starts once"),WasPlaying);Test->TestTrue(TEXT("connected frames accepted"),R.Accepted>0);Test->TestEqual(TEXT("every accepted frame receipted"),R.Receipts,R.Accepted);Test->TestEqual(TEXT("manual stop distinct from delivery fault"),R.Outcome,ESlabRunDeliveryOutcome::UserStopped);}
        else
        {Test->TestTrue(TEXT("fault happens after actual start"),WasPlaying);Test->TestEqual(TEXT("fault stops movement"),Slab->GetSimulationStatus().State,ESlabSimulationState::Failed);Test->TestTrue(TEXT("movement stopped before end"),S.ElapsedSec<60);Test->TestEqual(TEXT("network failure recorded as partial"),R.Outcome,ESlabRunDeliveryOutcome::Partial);}
        Test->TestFalse(TEXT("report saved without sensor body"),R.ReportPath.IsEmpty());FString Reason;Test->TestTrue(TEXT("drain completion permits initial reset"),Slab->ResetToInitialPlacement(Reason));
        FSlabRunDeliverySummary After;Test->TestTrue(TEXT("reset retains run result"),Results->GetRunResult(Run,After));Test->TestEqual(TEXT("reset preserves prior outcome"),After.Outcome,R.Outcome);
        Slab->Destroy();if(Lidar.IsValid())Lidar->Destroy();Manager->Destroy();return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabTcpFaultTest,"MA0T10.SlabReliability.TcpFaultRuntime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabTcpFaultTest::RunTest(const FString&)
{
    const FString Mode=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SCENARIO_FAULT_MODE"));if(Mode.IsEmpty()){AddInfo(TEXT("Skipped TCP fault fixture; requires isolated scenario_fault_server.mjs."));return true;}
    FAutomationEditorCommonUtils::CreateNewMap();ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FSlabTcpFaultCheck(this,Mode));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
