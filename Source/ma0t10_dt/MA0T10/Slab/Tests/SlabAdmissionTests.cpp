#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

namespace
{
class FSlabAdmissionCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;double Began=FPlatformTime::Seconds();
public:
	explicit FSlabAdmissionCommand(FAutomationTestBase* T):Test(T){}
	virtual bool Update() override
	{
		UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){World=C.World();break;}
		if(!World||!World->GetGameInstance()){if(FPlatformTime::Seconds()-Began>15){Test->AddError(TEXT("PIE initialization timed out"));return true;}return false;}
		auto* Slab=World->SpawnActor<ASlabActor>();auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
		auto* Archive=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
		auto NewData=[&](){FSlabScenarioDataPtr D;FString Error;Test->TestTrue(TEXT("synthetic input parses"),FSlabScenarioCodec::Parse(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString()),D,Error,true));return D;};
		auto End=[&](){Slab->StopSimulation();Session->Tick(0);};
		const auto First=NewData();Test->TestTrue(TEXT("missing Coordinator does not block movement"),Slab->ReceiveScenario(First));
		Test->TestEqual(TEXT("fallback is explicit"),Slab->GetLastScenarioAdmissionStatus().Result,ESlabScenarioAdmission::StartedWithoutTransmission);
		Test->TestTrue(TEXT("requested PCD preserved"),Slab->GetSensorOutputs().bPointCloud);
		Test->TestFalse(TEXT("fallback has no automatic output"),Session->GetSlabSensorSessionStatus().Outputs.HasAnyOutput());
		const FString Warning=Slab->GetSimulationStatus().TransmissionWarning;Slab->AdvanceSimulation(.025);
		Test->TestTrue(TEXT("subframe interpolation occurs"),Slab->GetSimulationStatus().ElapsedSec>.02);
		Test->TestEqual(TEXT("warning survives pose"),Slab->GetSimulationStatus().TransmissionWarning,Warning);
		Test->TestFalse(TEXT("busy input is archive-only"),Slab->ReceiveScenario(NewData()));
		Test->TestEqual(TEXT("busy reason exposed"),Slab->GetLastScenarioAdmissionStatus().Result,ESlabScenarioAdmission::StoredOnlyBusy);
		Test->TestTrue(TEXT("pause allowed without sensors"),Slab->SetSimulationPaused(true));Slab->AdvanceSimulation(1);
		Test->TestTrue(TEXT("resume allowed without sensors"),Slab->SetSimulationPaused(false));Slab->AdvanceSimulation(30);Session->Tick(0);
		Test->TestEqual(TEXT("30 second observation reaches end"),Slab->GetSimulationStatus().State,ESlabSimulationState::Completed);
		Test->TestFalse(TEXT("duplicate never automatically replays"),Slab->ReceiveScenario(First));
		Test->TestEqual(TEXT("duplicate distinguished"),Slab->GetLastScenarioAdmissionStatus().Result,ESlabScenarioAdmission::Duplicate);
		const auto PreRegistered=NewData();Archive->RegisterValidatedScenario(PreRegistered);
		Test->TestFalse(TEXT("pre-registered input not silently executed twice"),Slab->ReceiveScenario(PreRegistered));
		Test->TestEqual(TEXT("pre-registration diagnostic"),Slab->GetLastScenarioAdmissionStatus().Result,ESlabScenarioAdmission::Duplicate);
		auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Manager->RegisterSensorActor(Lidar);
		auto* Publisher=Manager->StreamPublisherComponent.Get();Publisher->SetTransportComponent(Manager->SharedTransportComponent);
		Manager->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::LogOnly;
		Publisher->StartStream(EVirtualSensorStreamKind::LidarPayload,Lidar->GetSensorId());
		const bool Running=Lidar->IsSensorRunning();FString Reason;
		Test->TestFalse(TEXT("unapplied STOMP configuration fails readonly readiness"),Session->ValidateScenarioOutputs({},Slab->GetSensorOutputs(),Reason));
		Test->TestTrue(TEXT("readiness cannot disable independent stream"),Publisher->IsStreamEnabled(EVirtualSensorStreamKind::LidarPayload,Lidar->GetSensorId()));
		Test->TestTrue(TEXT("bad output configuration starts unbound observation"),Slab->ReceiveScenario(NewData()));
		Test->TestFalse(TEXT("fallback owns no sensor"),Session->ControlsSensor(Lidar->GetSensorId()));
		Test->TestEqual(TEXT("fallback does not start sensor"),Lidar->IsSensorRunning(),Running);
		Test->TestTrue(TEXT("independent stream survives fallback start"),Publisher->IsStreamEnabled(EVirtualSensorStreamKind::LidarPayload,Lidar->GetSensorId()));
		Manager->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
		Slab->AdvanceSimulation(.1);Test->TestFalse(TEXT("configuration recovery cannot enable mid-run output"),Session->GetSlabSensorSessionStatus().Outputs.HasAnyOutput());End();
		Test->TestTrue(TEXT("independent stream survives fallback finish"),Publisher->IsStreamEnabled(EVirtualSensorStreamKind::LidarPayload,Lidar->GetSensorId()));
		Slab->TargetSensorIds={TEXT("MissingSensor")};Test->TestTrue(TEXT("missing target falls back"),Slab->ReceiveScenario(NewData()));End();
		Slab->TargetSensorIds={Lidar->GetSensorId()};Test->TestTrue(TEXT("configured PCD run starts"),Slab->ReceiveScenario(NewData()));
		Test->TestEqual(TEXT("normal admission distinguished"),Slab->GetLastScenarioAdmissionStatus().Result,ESlabScenarioAdmission::Started);
		Test->TestTrue(TEXT("PCD remains selected"),Session->GetSlabSensorSessionStatus().Outputs.bPointCloud);
		Slab->StopSimulation();Test->TestFalse(TEXT("drain cannot be bypassed by fallback"),Slab->ReceiveScenario(NewData()));
		Test->TestEqual(TEXT("drain input stored only"),Slab->GetLastScenarioAdmissionStatus().Result,ESlabScenarioAdmission::StoredOnlyBusy);Session->Tick(0);
		Test->TestTrue(TEXT("invalid observation UUID rejected"),Session->BeginUnboundObservationSession(TEXT("bad"),First->ScenarioUUID).IsEmpty());
		Slab->Destroy();Lidar->StopSensor();Publisher->StopAllStreams(FString());Manager->Destroy();Lidar->Destroy();return true;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabAdmissionTest,"MA0T10.SlabScenario.AdmissionAndUnboundObservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabAdmissionTest::RunTest(const FString&)
{
	FAutomationEditorCommonUtils::CreateNewMap();ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FSlabAdmissionCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
