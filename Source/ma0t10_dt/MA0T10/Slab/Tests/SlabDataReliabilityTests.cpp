#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

namespace
{
class FSlabDataPreparationCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();int Stage=0;
    TWeakObjectPtr<ASlabActor> Slab;TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;TWeakObjectPtr<AVirtualSensorCoordinator> Manager;
    FTransform Placement;FString Run;
    FSlabScenarioDataPtr Data(){FSlabScenarioDataPtr D;FString E;FSlabScenarioCodec::Parse(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)),D,E,true);return D;}
public:
    explicit FSlabDataPreparationCheck(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!W->GetGameInstance()){if(FPlatformTime::Seconds()-Started>15){Test->AddError(TEXT("Preparation PIE timeout"));return true;}return false;}
        auto* Session=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();auto* Raw=W->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
        if(Stage==0)
        {
            Manager=W->SpawnActor<AVirtualSensorCoordinator>();Lidar=W->SpawnActor<AVirtualLidarSensorActor>();Manager->DiscoverSensorsInLevel();Lidar->StopSensor();
            auto Profile=Manager->SharedTransportComponent->GetTransportProfile();Profile.BrokerUrl=TEXT("tcp://127.0.0.1:1");Manager->SharedTransportComponent->ConfigureTransportProfile(Profile);Manager->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
            auto Config=Manager->StreamPublisherComponent->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());Config.bEnabled=true;Manager->StreamPublisherComponent->ConfigureStream(Config);
            Slab=W->SpawnActor<ASlabActor>(FVector(313,77,44),FRotator(3,17,0));Placement=Slab->GetActorTransform();FSlabExecutionOptions O;O.Policy=ESlabExecutionPolicy::RequireData;O.PreparationTimeoutSeconds=1;Slab->SetExecutionOptions(O);
            Test->TestTrue(TEXT("required request admitted"),Slab->ReceiveScenario(Data()));Run=Slab->GetSimulationStatus().RunUUID;
            Test->TestEqual(TEXT("preparation does not mean playing"),Slab->GetSimulationStatus().State,ESlabSimulationState::Preparing);
            Test->TestTrue(TEXT("worker may exist while broker is not ready"),Raw->IsHighThroughputTransportRunning());Test->TestFalse(TEXT("worker presence is not connection readiness"),Raw->IsTransportConnected());
            FString Reason;Test->TestFalse(TEXT("preparing reset blocked"),Slab->CanResetToInitialPlacement(Reason));Test->TestFalse(TEXT("required configuration locked"),Lidar->CanEditSensorConfiguration(Reason));
            Test->TestFalse(TEXT("required manipulation blocked"),Lidar->BeginInteractiveManipulation(FVirtualSensorInteractionRequest()));
            Started=FPlatformTime::Seconds();Stage=1;return false;
        }
        if(Stage==1)
        {
            Test->TestTrue(TEXT("no motion before connection"),Slab->GetActorTransform().Equals(Placement));
            for(const auto& T:Raw->GetStreamTelemetry())Test->TestEqual(TEXT("no run submission before ready"),T.SubmittedCount,int64(0));
            if(Slab->GetSimulationStatus().State==ESlabSimulationState::Preparing){if(FPlatformTime::Seconds()-Started>5){Test->AddError(TEXT("Preparation did not timeout"));return true;}return false;}
            Test->TestEqual(TEXT("timeout records failed execution"),Slab->GetSimulationStatus().State,ESlabSimulationState::Failed);
            Test->TestTrue(TEXT("independent stream preserved"),Manager->StreamPublisherComponent->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId()));
            FString Reason;Test->TestTrue(TEXT("failed preparation permits reset"),Slab->ResetToInitialPlacement(Reason));
            Test->TestTrue(TEXT("next fresh request admitted"),Slab->ReceiveScenario(Data()));Slab->StopSimulation();
            Test->TestTrue(TEXT("cancel leaves pose unchanged"),Slab->GetActorTransform().Equals(Placement));Test->TestTrue(TEXT("cancel unlocks configuration"),Lidar->CanEditSensorConfiguration(Reason));
            auto Changed=Manager->SharedTransportComponent->GetTransportProfile();Changed.BrokerUrl=TEXT("tcp://127.0.0.1:2");Manager->SharedTransportComponent->TransportProfile=Changed;
            Test->TestFalse(TEXT("independent sender prevents shared profile replacement"),Slab->ReceiveScenario(Data()));
            Test->TestTrue(TEXT("conflict keeps independent sender enabled"),Manager->StreamPublisherComponent->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId()));
            Manager->StreamPublisherComponent->StopAllStreams(FString());
            Test->TestTrue(TEXT("idle transport may adopt the requested profile"),Slab->ReceiveScenario(Data()));Slab->StopSimulation();
            Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());Test->TestFalse(TEXT("required request without output rejected"),Slab->ReceiveScenario(Data()));
            Slab->Destroy();Lidar->Destroy();Manager->Destroy();return true;
        }
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabDataPreparationTest,"MA0T10.SlabReliability.PreparationTimeoutCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabDataPreparationTest::RunTest(const FString&)
{FAutomationEditorCommonUtils::CreateNewMap();ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FSlabDataPreparationCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
