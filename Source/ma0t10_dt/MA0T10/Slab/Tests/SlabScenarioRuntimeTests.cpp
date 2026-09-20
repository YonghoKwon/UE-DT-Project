#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DxDataSubsystem.h"
#include "Core/DxProcessSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabDataSyncComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabTrackReferenceActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"

namespace
{
class FSlabProductionLifecycleCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Started=FPlatformTime::Seconds(),PauseAt=0;
	int32 Stage=0;
	TWeakObjectPtr<ASlabActor> Slab;
	FString SourceJson,FirstRun,ScenarioId;
	double PausedTime=0;
	FTransform PausedPose;
public:
	explicit FSlabProductionLifecycleCommand(FAutomationTestBase* InTest):Test(InTest){}
	bool Update() override
	{
		if(FPlatformTime::Seconds()-Started>25) { Test->AddError(TEXT("Slab production lifecycle timed out.")); return true; }
		UWorld* World=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::PIE) {World=C.World();break;}
		if(!World||!World->GetGameInstance()||FPlatformTime::Seconds()-Started<3) return false;
		auto* Catalog=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
		auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
		if(Stage==0)
		{
			for(TActorIterator<ASlabActor> It(World);It;++It) {Slab=*It;break;}
			if(!Slab.IsValid()) {Test->AddError(TEXT("Validation map must contain production ASlabActor.")); return true;}
			Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
			ScenarioId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower); SourceJson=FSlabScenarioCodec::MakeSyntheticJson(ScenarioId);
			Test->TestEqual(TEXT("DTCore component registry resolves Slab DataSync"),World->GetGameInstance()->GetSubsystem<UDxProcessSubsystem>()->FindComponent(Slab->ReceiverId),static_cast<UActorComponent*>(Slab->GetDataSyncComponent()));
			World->GetGameInstance()->GetSubsystem<UDxDataSubsystem>()->EnqueueWebSocketData(SourceJson); Stage=1; return false;
		}
		if(!Slab.IsValid()) {Test->AddError(TEXT("Slab destroyed during lifecycle test."));return true;}
		if(Stage==1)
		{
			if(!Slab->IsSimulationActive()) return false;
			if(Slab->GetSimulationStatus().ElapsedSec<0.15) return false;
			const auto S=Slab->GetSimulationStatus(); FirstRun=S.RunUUID;
			Test->TestEqual(TEXT("DTCore TC reached production movement"),S.MtlNo,FString(TEXT("SQ83521 047")));
			Test->TestEqual(TEXT("raw source retained"),Slab->GetScenario()->OriginalJson,SourceJson);
			Test->TestTrue(TEXT("observation suppresses automatic sensor output"),Session->GetSlabSensorSessionStatus().bObservationOnly);
			Test->TestFalse(TEXT("live units cannot change midrun"),Slab->SetDimensionUnit(ESlabInputUnit::Meters));
			Test->TestTrue(TEXT("next-run output selection remains configurable"),Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection()));
			Test->TestFalse(TEXT("changing next outputs cannot mutate current observation"),Session->GetSlabSensorSessionStatus().Outputs.HasAnyOutput());
			TArray<UStaticMeshComponent*> Meshes; Slab->GetComponents(Meshes);
			int32 Helpers=0; for(auto* Mesh:Meshes) if(Mesh!=Slab->SlabMesh)
			{++Helpers;Test->TestEqual(TEXT("helpers never enter CPU sensor traces"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);Test->TestTrue(TEXT("helpers excluded from camera/GPU sensor captures"),bool(Mesh->bHiddenInSceneCapture));}
			Test->TestEqual(TEXT("outline four edges, cross two axes, centre marker"),Helpers,7);
			Test->TestTrue(TEXT("pause accepted"),Slab->SetSimulationPaused(true));
			PausedTime=Slab->GetSimulationStatus().ElapsedSec; PausedPose=Slab->GetActorTransform(); PauseAt=FPlatformTime::Seconds(); Stage=2; return false;
		}
		if(Stage==2)
		{
			if(FPlatformTime::Seconds()-PauseAt<0.2) return false;
			Slab->AdvanceSimulation(1); Test->TestEqual(TEXT("paused input cannot advance timeline"),Slab->GetSimulationStatus().ElapsedSec,PausedTime);
			Test->TestTrue(TEXT("paused transform unchanged"),Slab->GetActorTransform().Equals(PausedPose));
			Test->TestTrue(TEXT("resume accepted"),Slab->SetSimulationPaused(false));
			// Accelerate only this deterministic boundary assertion. This is not a 30s performance claim.
			Slab->AdvanceSimulation(30);
			Test->TestEqual(TEXT("30s lifecycle completion"),Slab->GetSimulationStatus().State,ESlabSimulationState::Completed);
			Test->TestEqual(TEXT("last source frame 599 applied before finish"),Slab->GetSimulationStatus().FrameNo,int64(599));
			Test->TestEqual(TEXT("last source frame context separate from continuous time"),Session->GetSlabSensorSessionStatus().CurrentSlab.ElapsedSec,29.95);
			Test->TestEqual(TEXT("explicit duration 30 seconds"),Slab->GetSimulationStatus().ElapsedSec,30.0);
			Session->Tick(0); Stage=3; return false;
		}
		if(Stage==3)
		{
			if(!Catalog->CanReplay()) return false;
			Test->TestFalse(TEXT("duplicate UUID does not autoexecute"),Slab->ReceiveScenario(Slab->GetScenario()));
			Test->TestTrue(TEXT("same immutable scenario replays through real adapter"),Catalog->RequestScenarioReplayWithOutputs(ScenarioId,FVirtualSlabSensorOutputSelection::ObservationOnly(),{}));
			Test->TestNotEqual(TEXT("fresh execution identity"),Slab->GetSimulationStatus().RunUUID,FirstRun);
			Test->TestEqual(TEXT("original scenario identity reused"),Slab->GetSimulationStatus().ScenarioUUID,ScenarioId);
			Slab->AdvanceSimulation(0.025);
			Test->TestEqual(TEXT("render midpoint uses lower source frame"),Slab->GetSimulationStatus().FrameNo,int64(0));
			Test->TestTrue(TEXT("interpolated angle exactly between source samples"),FMath::IsNearlyEqual(Slab->GetCurrentMetrics().LeftAngle,-0.1925,1e-6));
			Slab->StopSimulation(); Session->Tick(0);
			Test->TestFalse(TEXT("stop ends production motion"),Slab->IsSimulationActive()); return true;
		}
		return false;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabProductionLifecycleTest,"MA0T10.SlabScenario.ProductionLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabProductionLifecycleTest::RunTest(const FString&)
{
	if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_RUNTIME"))!=TEXT("1")) { AddInfo(TEXT("Skipped actual PIE lifecycle; set MA0T10_SLAB_RUNTIME=1 after validation map and TC registration are generated.")); return true; }
	if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FSlabProductionLifecycleCommand(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
