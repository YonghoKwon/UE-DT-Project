#include "SlabScenarioReplayTestAdapter.h"
#include "VirtualSlabSensorTestDriver.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayUiHostActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Json.h"
ASlabScenarioReplayTestAdapter::ASlabScenarioReplayTestAdapter()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PrePhysics;
	Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReplayFixtureSlab")); SetRootComponent(Mesh);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Tags.Add(TEXT("SlabReplayTransientFixture")); SetActorHiddenInGame(true);
}
void ASlabScenarioReplayTestAdapter::BeginPlay()
{ Super::BeginPlay(); GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>()->RegisterPlaybackAdapter(this); }
FString ASlabScenarioReplayTestAdapter::MakeBulkJson(const FString& UUID)
{
	TSharedPtr<FJsonObject> Root; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(AVirtualSlabSensorTestDriver::MakeSyntheticBulkJson()),Root);
	Root->GetObjectField(TEXT("_meta"))->SetStringField(TEXT("UUID"),UUID);
	FString Result; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Result)); return Result;
}
bool ASlabScenarioReplayTestAdapter::StartScenarioPlayback_Implementation(const FString& Json,const FString& ScenarioUUID,const FString& RunUUID)
{
	if(bPlaying) return false;
	TSharedPtr<FJsonObject> Root; if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)) return false;
	Rows=Root->GetArrayField(TEXT("DATA_MAP")); if(Rows.Num()!=600) return false;
	LastReceivedJson=Json; LastScenarioUUID=ScenarioUUID; LastRunUUID=RunUUID;
	StartedSeconds=FPlatformTime::Seconds(); LastIndex=-1; AppliedCount=0; bPlaying=true;
	SetActorHiddenInGame(false);
	if(!GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>()->NotifyPlaybackStarted(RunUUID)) { bPlaying=false; return false; }
	Tick(0); return true;
}
void ASlabScenarioReplayTestAdapter::Tick(float Delta)
{
	Super::Tick(Delta); if(!bPlaying||!GetGameInstance()) return;
	auto* Replay=GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
	if(!Replay->IsReplayBusy()||Replay->GetReplayStatus().RunUUID!=LastRunUUID) { bPlaying=false; return; }
	const double Elapsed=FPlatformTime::Seconds()-StartedSeconds;
	const int32 Index=FMath::Clamp(FMath::FloorToInt(Elapsed/0.05),0,599);
	if(Index!=LastIndex)
	{
		const auto Row=Rows[Index]->AsObject();
		// Fixture units only. The production adapter keeps the colleague's exact mapping.
		SetActorScale3D(FVector(Row->GetNumberField(TEXT("slab_len"))/1000,Row->GetNumberField(TEXT("slab_wth"))/1000,Row->GetNumberField(TEXT("slab_thk"))/1000));
		SetActorLocation(FVector(250+(Row->GetNumberField(TEXT("mov_pos"))-1210)*0.1,Row->GetNumberField(TEXT("center_y"))*0.1,60));
		SetActorRotation(FRotator(0,Row->GetNumberField(TEXT("right_skew_angle")),0));
		bFailed|=!GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->NotifySlabFrameApplied(LastRunUUID,Row->GetStringField(TEXT("mtl_no")),static_cast<int64>(Row->GetNumberField(TEXT("frame_no"))),Row->GetNumberField(TEXT("elapsed_sec")));
		LastIndex=Index; ++AppliedCount;
	}
	if(Elapsed>=30) { bPlaying=false; ++CompletedRuns; Replay->NotifyPlaybackFinished(LastRunUUID,bFailed); UE_LOG(LogTemp,Display,TEXT("[ScenarioReplayFixture] finished scenario=%s run=%s elapsed=%.3f applied=%d"),*LastScenarioUUID,*LastRunUUID,Elapsed,AppliedCount); }
}
void ASlabScenarioReplayTestAdapter::EndPlay(const EEndPlayReason::Type Reason)
{ if(GetGameInstance()) GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>()->UnregisterPlaybackAdapter(this); Super::EndPlay(Reason); }
#if WITH_EDITOR
static FAutoConsoleCommandWithWorld GReplayDemo(TEXT("ma0t10.ScenarioReplayDemo"),TEXT("Install a transient scenario replay adapter, memory sample and UI in PIE."),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World){
	if(!World||!World->IsGameWorld()||!World->GetGameInstance()) return;
	if(!TActorIterator<ASlabScenarioReplayTestAdapter>(World)) World->SpawnActor<ASlabScenarioReplayTestAdapter>();
	if(!TActorIterator<ASlabScenarioReplayUiHostActor>(World)) World->SpawnActor<ASlabScenarioReplayUiHostActor>();
	auto* Manager=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
	Manager->RegisterScenarioJson(ASlabScenarioReplayTestAdapter::MakeBulkJson(TEXT("8c667bbb-2480-4d9e-8c43-70440a8376e0")));
}));
#endif
