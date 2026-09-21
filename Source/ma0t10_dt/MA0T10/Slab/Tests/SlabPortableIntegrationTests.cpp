#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Core/DxProcessSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabDataSyncComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabTrackReferenceActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/SlabSimulationUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelHostComponent.h"

namespace
{
/** Test-only ownership: no persistent map is saved and the user's workspace slot is restored after PIE. */
struct FSlabPortableState
{
	FAutomationTestBase* Test;
	FString SlotPath=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames/MA0T10_SensorToolWorkspace_v1.sav"));
	TArray<uint8> SlotBytes;
	bool bHadSlot=false,bCapturedSlot=false,bRestored=false,bControllerRemoved=false;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerController> Controller;
	explicit FSlabPortableState(FAutomationTestBase* InTest):Test(InTest)
	{
		bHadSlot=IFileManager::Get().FileExists(*SlotPath);
		bCapturedSlot=!bHadSlot||FFileHelper::LoadFileToArray(SlotBytes,*SlotPath);
	}
	void RestoreController()
	{
		if(bControllerRemoved&&World.IsValid()&&Controller.IsValid()&&Controller->GetWorld()==World.Get()) World->AddController(Controller.Get());
		bControllerRemoved=false;
	}
	void Restore()
	{
		if(bRestored) return; bRestored=true; RestoreController();
		if(!bCapturedSlot) return;
		const bool Ok=bHadSlot?FFileHelper::SaveArrayToFile(SlotBytes,*SlotPath):
			(!IFileManager::Get().FileExists(*SlotPath)||IFileManager::Get().Delete(*SlotPath));
		if(!Ok) Test->AddError(TEXT("Could not restore the sensor workspace SaveGame after portable-map test."));
	}
	~FSlabPortableState(){ Restore(); }
};
class FSlabPortableCommand final:public IAutomationLatentCommand
{
	TSharedRef<FSlabPortableState> Shared;
	double Started=FPlatformTime::Seconds(),DelayStarted=0;
	int32 Stage=0;
	TWeakObjectPtr<ASlabActor> Slab;
	TWeakObjectPtr<ASlabSimulationUiHostActor> Host;
	FString Json,ScenarioId,FirstRun;
public:
	explicit FSlabPortableCommand(TSharedRef<FSlabPortableState> InShared):Shared(InShared){}
	bool Update() override
	{
		auto* Test=Shared->Test;
		if(FPlatformTime::Seconds()-Started>25) { Test->AddError(TEXT("Portable Slab/UI lifecycle timed out.")); Shared->RestoreController(); return true; }
		UWorld* World=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::PIE) {World=C.World();break;}
		if(!World||!World->GetGameInstance()||FPlatformTime::Seconds()-Started<2) return false;
		auto* Catalog=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
		auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
		auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
		if(Stage==0)
		{
			Test->TestTrue(TEXT("portable map uses native GameModeBase instead of project validation GameMode"),World->GetAuthGameMode()&&World->GetAuthGameMode()->GetClass()==AGameModeBase::StaticClass());
			Test->TestFalse(TEXT("observation map contains no sensor Coordinator"),bool(TActorIterator<AVirtualSensorCoordinator>(World)));
			for(TActorIterator<ASlabActor> It(World);It;++It) {Slab=*It;break;}
			if(!Slab.IsValid()||!World->GetFirstPlayerController()) {Test->AddError(TEXT("Temporary map did not produce Slab and local player."));return true;}
			Test->TestNotNull(TEXT("track survives PIE duplication"),Slab->TrackReference.Get());
			Test->TestEqual(TEXT("DTCore DataSync registration needs no test rig"),World->GetGameInstance()->GetSubsystem<UDxProcessSubsystem>()->FindComponent(Slab->ReceiverId),static_cast<UActorComponent*>(Slab->GetDataSyncComponent()));
			Test->TestEqual(TEXT("production actor owns replay adapter"),Catalog->GetPlaybackAdapter(),static_cast<UObject*>(Slab.Get()));
			// Temporarily withdraw only this disposable PIE world's local controller; preserve the LocalPlayer itself.
			Shared->World=World; Shared->Controller=World->GetFirstPlayerController(); World->RemoveController(Shared->Controller.Get()); Shared->bControllerRemoved=true;
			Test->TestNull(TEXT("late-controller precondition"),World->GetFirstPlayerController());
			auto* NewHost=World->SpawnActorDeferred<ASlabSimulationUiHostActor>(ASlabSimulationUiHostActor::StaticClass(),FTransform::Identity);
			NewHost->SlabActor=Slab.Get(); NewHost->ChartsWidgetClass=USlabChartsPanelWidget::StaticClass(); NewHost->ProgressWidgetClass=USlabProgressPanelWidget::StaticClass();
			NewHost->FinishSpawning(FTransform::Identity); Host=NewHost;
			Test->TestNull(TEXT("host does not construct widgets before controller"),Host->ChartsWidget.Get());
			Test->TestTrue(TEXT("host clearly reports bounded initialization wait"),Host->GetInitializationMessage().Contains(TEXT("준비 대기")));
			DelayStarted=FPlatformTime::Seconds(); Stage=1; return false;
		}
		if(!Slab.IsValid()||!Host.IsValid()) {Test->AddError(TEXT("Portable fixture actor disappeared."));Shared->RestoreController();return true;}
		if(Stage==1)
		{
			if(FPlatformTime::Seconds()-DelayStarted<.35) return false;
			Test->TestNull(TEXT("retry leaves widgets absent while player is unavailable"),Host->ChartsWidget.Get());
			Shared->RestoreController(); DelayStarted=FPlatformTime::Seconds(); Stage=2; return false;
		}
		if(Stage==2)
		{
			if(!Host->ChartsWidget||!Host->ProgressWidget) return false;
			Test->TestEqual(TEXT("late retry binds charts to explicit actor"),Host->ChartsWidget->GetBoundSlabActor(),Slab.Get());
			Test->TestEqual(TEXT("late retry binds progress to explicit actor"),Host->ProgressWidget->GetBoundSlabActor(),Slab.Get());
			auto* OriginalChart=Host->ChartsWidget.Get(); Host->ShowSimulationPanels();
			Test->TestEqual(TEXT("repeat show reuses the same widget"),Host->ChartsWidget.Get(),OriginalChart);
			Test->TestEqual(TEXT("exactly two panels hosted"),Host->PanelHost->GetHostedPanelCount(),2);
			Workspace->SetPanelOpen(ESensorToolPanelRole::SlabCharts,true); Workspace->SetPanelOpen(ESensorToolPanelRole::SlabProgress,true);
			ScenarioId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower); Json=FSlabScenarioCodec::MakeSyntheticJson(ScenarioId);
			Test->TestTrue(TEXT("600-row production SubmitScenarioJson accepted without rig"),Slab->SubmitScenarioJson(Json));
			Stage=3; return false;
		}
		if(Stage==3)
		{
			if(!Slab->IsSimulationActive()||Slab->GetSimulationStatus().ElapsedSec<.2) return false;
			const auto S=Slab->GetSimulationStatus(); FirstRun=S.RunUUID;
			Test->TestEqual(TEXT("all 600 original rows available"),S.RowCount,600);
			Test->TestEqual(TEXT("same original JSON retained"),Slab->GetScenario()->OriginalJson,Json);
			Test->TestTrue(TEXT("no sensors needed for observation session"),Session->GetSlabSensorSessionStatus().bObservationOnly);
			for(int32 I=0;I<3;++I)
			{
				auto* Chart=Host->ChartsWidget->GetChartWidget(I); Test->TestNotNull(TEXT("three native charts created in unrelated GameMode"),Chart);
				if(Chart) Test->TestTrue(TEXT("chart shows reached prefix only"),Chart->GetRevealedSampleCount()>0&&Chart->GetRevealedSampleCount()<600);
			}
			// Deterministic end-boundary test, not a claim of a 30-second wall-clock performance run.
			Slab->AdvanceSimulation(30); Session->Tick(0);
			Test->TestEqual(TEXT("portable player finishes at explicit duration"),Slab->GetSimulationStatus().ElapsedSec,30.0);
			Test->TestEqual(TEXT("portable player applies last row"),Slab->GetSimulationStatus().FrameNo,int64(599));
			Stage=4; return false;
		}
		if(Stage==4)
		{
			if(!Catalog->CanReplay()) return false;
			Test->TestTrue(TEXT("same actor replays archived scenario without validation rig"),Catalog->RequestScenarioReplayWithOutputs(ScenarioId,FVirtualSlabSensorOutputSelection::ObservationOnly(),{}));
			Test->TestNotEqual(TEXT("replay receives a fresh run UUID"),Slab->GetSimulationStatus().RunUUID,FirstRun);
			Test->TestEqual(TEXT("replay keeps source identity"),Slab->GetSimulationStatus().ScenarioUUID,ScenarioId);
			Slab->StopSimulation(); Session->Tick(0);
			Host->Destroy();
			Test->TestNull(TEXT("host destruction unregisters charts"),Workspace->GetOwnedPanel(ESensorToolPanelRole::SlabCharts));
			Test->TestNull(TEXT("host destruction unregisters progress"),Workspace->GetOwnedPanel(ESensorToolPanelRole::SlabProgress));
			return true;
		}
		return false;
	}
};
class FRestoreSlabPortableState final:public IAutomationLatentCommand
{
	TSharedRef<FSlabPortableState> Shared;
public:
	explicit FRestoreSlabPortableState(TSharedRef<FSlabPortableState> InShared):Shared(InShared){}
	bool Update() override { Shared->Restore(); return true; }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabPortableIntegrationTest,"MA0T10.SlabScenario.PortableMapAndLatePlayer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabPortableIntegrationTest::RunTest(const FString&)
{
	if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_SLAB_PORTABLE"))!=TEXT("1"))
	{ AddInfo(TEXT("Skipped disposable-map PIE integration; set MA0T10_SLAB_PORTABLE=1. No production map or rig is used.")); return true; }
	if(GEditor&&GEditor->PlayWorld) { AddError(TEXT("End the existing PIE session before the isolated portable-map test.")); return false; }
	auto Shared=MakeShared<FSlabPortableState>(this);
	if(!Shared->bCapturedSlot) { AddError(TEXT("Could not back up the user's workspace slot; test was not started.")); return false; }
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
	auto* Track=World->SpawnActor<ASlabTrackReferenceActor>();
	auto* Slab=World->SpawnActor<ASlabActor>(); Slab->ReceiverId=TEXT("SlabScenario.PortableTest"); Slab->TrackReference=Track;
	Slab->DimensionUnit=ESlabInputUnit::Millimeters; Slab->PositionUnit=ESlabInputUnit::Centimeters;
	Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FSlabPortableCommand(Shared));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FRestoreSlabPortableState(Shared));
	return true;
}
#endif
