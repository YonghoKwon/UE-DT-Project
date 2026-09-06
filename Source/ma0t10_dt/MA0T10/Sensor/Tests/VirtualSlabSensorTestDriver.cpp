#include "VirtualSlabSensorTestDriver.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"

AVirtualSlabSensorTestDriver::AVirtualSlabSensorTestDriver()
{
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.TickGroup=TG_PrePhysics;
	SlabMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TransientSlab"));
	SetRootComponent(SlabMesh);
	SlabMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	SlabMesh->SetMobility(EComponentMobility::Movable);
	SlabMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlabMesh->SetCollisionResponseToAllChannels(ECR_Block);
	Tags.Add(TEXT("SlabSensorTransientFixture"));
}
FString AVirtualSlabSensorTestDriver::MakeSyntheticBulkJson()
{
	auto Root=MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("CREATE_TIMESTAMP"),TEXT("20260727174504137"));
	Root->SetStringField(TEXT("MESSAGE_ID"),TEXT("IFactory-agent"));
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (int32 I=0; I<600; ++I)
	{
		auto Row=MakeShared<FJsonObject>();
		const double A=static_cast<double>(I)/599.0;
		Row->SetNumberField(TEXT("frame_no"),I); Row->SetStringField(TEXT("mtl_no"),TEXT("SQ83521 047"));
		Row->SetNumberField(TEXT("slab_thk"),250); Row->SetNumberField(TEXT("slab_wth"),1100);
		Row->SetNumberField(TEXT("slab_len"),10830); Row->SetNumberField(TEXT("slab_wt"),23290);
		Row->SetNumberField(TEXT("left_skew_angle"),-0.2-4.8*A); Row->SetNumberField(TEXT("right_skew_angle"),0.2+4.8*A);
		Row->SetNumberField(TEXT("mov_pos"),1210+910*A); Row->SetNumberField(TEXT("center_x"),909*A);
		Row->SetNumberField(TEXT("center_y"),0.1+9.8*A); Row->SetNumberField(TEXT("elapsed_sec"),I*0.05);
		Row->SetBoolField(TEXT("is_meandering"),A>0.6);
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	Root->SetArrayField(TEXT("DATA_MAP"),Rows);
	auto Meta=MakeShared<FJsonObject>(); Meta->SetStringField(TEXT("scenario"),TEXT("Synthetic slab 30s - mm/kg/degrees"));
	Meta->SetNumberField(TEXT("sample_period_sec"),0.05); Meta->SetNumberField(TEXT("duration_sec"),30);
	Meta->SetNumberField(TEXT("frame_count"),600); Root->SetObjectField(TEXT("_meta"),Meta);
	FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json)); return Json;
}
bool AVirtualSlabSensorTestDriver::StartTest(int32 Runs,const TArray<FString>& SensorIds)
{
	if (!Frames.IsEmpty()) return false;
	const FString Bulk=MakeSyntheticBulkJson();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Bulk),Root)) return false;
	Frames=Root->GetArrayField(TEXT("DATA_MAP")); Targets=SensorIds; RunCount=FMath::Clamp(Runs,1,2);
	const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Reports/SlabSensorSession"));
	IFileManager::Get().MakeDirectory(*Directory,true);
	FFileHelper::SaveStringToFile(Bulk,*FPaths::Combine(Directory,TEXT("synthetic_slab_30s.json")));
	return BeginRun();
}
bool AVirtualSlabSensorTestDriver::BeginRun()
{
	auto* Adapter=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	RunId=Adapter->BeginSlabSensorSession(FString(),Targets);
	if (RunId.IsEmpty()) { bFinished=bFailed=true; return false; }
	LastApplied=-1; AppliedCount=SkippedCount=0; bEndingRun=false; StartSeconds=FPlatformTime::Seconds();
	UE_LOG(LogTemp,Display,TEXT("[SlabSensorTest] begin uuid=%s frames=600 duration=30"),*RunId);
	Tick(0); return true;
}
void AVirtualSlabSensorTestDriver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bFinished || RunId.IsEmpty()) return;
	auto* Adapter=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	if (bEndingRun)
	{
		const auto Status=Adapter->GetSlabSensorSessionStatus();
		if (Status.State!=EVirtualSlabSessionState::Completed && Status.State!=EVirtualSlabSessionState::Incomplete) return;
		auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("run_uuid"),RunId);
		Report->SetNumberField(TEXT("applied_slab_frames"),AppliedCount); Report->SetNumberField(TEXT("skipped_slab_frames"),SkippedCount);
		Report->SetNumberField(TEXT("last_slab_frame_no"),LastApplied); Report->SetNumberField(TEXT("unfinished"),Status.UnfinishedFrames);
		Report->SetBoolField(TEXT("completed"),Status.State==EVirtualSlabSessionState::Completed);
		RunReports.Add(MakeShared<FJsonValueObject>(Report)); ++CompletedRuns;
		bFailed|=Status.State==EVirtualSlabSessionState::Incomplete;
		WriteReport();
		if (!bFailed && CompletedRuns<RunCount) BeginRun(); else bFinished=true;
		return;
	}
	const double Elapsed=FPlatformTime::Seconds()-StartSeconds;
	const int32 Index=FMath::Clamp(FMath::FloorToInt(Elapsed/0.05),0,599);
	if (Index!=LastApplied)
	{
		SkippedCount+=FMath::Max(0,Index-LastApplied-1);
		const auto Row=Frames[Index]->AsObject();
		// Fixture mapping only. Real installations keep their own Slab transforms and units.
		SetActorScale3D(FVector(Row->GetNumberField(TEXT("slab_len"))/1000.0,Row->GetNumberField(TEXT("slab_wth"))/1000.0,Row->GetNumberField(TEXT("slab_thk"))/1000.0));
		SetActorLocation(FVector(250+(Row->GetNumberField(TEXT("mov_pos"))-1210)*0.1,Row->GetNumberField(TEXT("center_y"))*0.1,60));
		SetActorRotation(FRotator(0,Row->GetNumberField(TEXT("right_skew_angle")),0));
		bFailed|=!Adapter->NotifySlabFrameApplied(RunId,Row->GetStringField(TEXT("mtl_no")),Index,Row->GetNumberField(TEXT("elapsed_sec")));
		LastApplied=Index; ++AppliedCount;
	}
	if (Elapsed>=30.0)
	{
		Adapter->EndSlabSensorSession(RunId,bFailed); bEndingRun=true;
		UE_LOG(LogTemp,Display,TEXT("[SlabSensorTest] movement-ended uuid=%s elapsed=%.3f lastFrame=%d"),*RunId,Elapsed,LastApplied);
	}
}
void AVirtualSlabSensorTestDriver::WriteReport()
{
	auto Root=MakeShared<FJsonObject>(); Root->SetArrayField(TEXT("runs"),RunReports);
	FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Reports/SlabSensorSession/session_runs.json")));
}
void AVirtualSlabSensorTestDriver::EndPlay(const EEndPlayReason::Type Reason)
{
	if (!RunId.IsEmpty() && !bFinished) if (auto* Adapter=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()) Adapter->EndSlabSensorSession(RunId,true);
	Super::EndPlay(Reason);
}
#if WITH_EDITOR
static FAutoConsoleCommandWithWorld GSlabSensorDemo(TEXT("ma0t10.SlabSensorTest"),TEXT("Run a transient 600-frame/30-second Slab integration fixture in PIE."),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
	if (!World || !World->IsGameWorld()) return;
	for (TActorIterator<AVirtualSlabSensorTestDriver> It(World);It;++It) if (!It->IsFinished()) return;
	World->SpawnActor<AVirtualSlabSensorTestDriver>()->StartTest(1,{});
}));
#endif
