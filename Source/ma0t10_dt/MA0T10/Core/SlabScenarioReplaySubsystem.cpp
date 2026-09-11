#include "SlabScenarioReplaySubsystem.h"
#include "VirtualSensorSlabContextSubsystem.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Json.h"

namespace {
FString CanonicalId(const FString& Text) { FGuid Id; return FGuid::Parse(Text,Id)&&Id.IsValid()?Id.ToString(EGuidFormats::DigitsWithHyphensLower):FString(); }
bool SessionBusy(const UWorld* World)
{
	if (!World) return false;
	const auto S=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus().State;
	return S==EVirtualSlabSessionState::Ready||S==EVirtualSlabSessionState::Running||S==EVirtualSlabSessionState::Paused||S==EVirtualSlabSessionState::Draining;
}
}
void USlabScenarioReplaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection); bInitialized=true; ++Generation;
	TickerHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::Poll),0.2f);
	CleanupHandle=FWorldDelegates::OnWorldCleanup.AddUObject(this,&ThisClass::OnWorldCleanup);
}
void USlabScenarioReplaySubsystem::Deinitialize()
{
	AbortReplay(TEXT("프로그램 종료")); bInitialized=false; ++Generation;
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
	PendingJson.Reset(); Entries.Reset(); PlaybackAdapter.Reset(); PlaybackWorld.Reset(); bParsing=false; bLivePlaybackActive=false;
	Super::Deinitialize();
}
bool USlabScenarioReplaySubsystem::ValidateScenario(const FString& Json,FSlabScenarioSummary& Summary,FString& Error)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root.IsValid()) { Error=TEXT("JSON 형식 오류"); return false; }
	const TSharedPtr<FJsonObject>* Meta=nullptr; const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
	FString UUID, MessageId;
	if (!Root->TryGetObjectField(TEXT("_meta"),Meta)||!(*Meta)->TryGetStringField(TEXT("UUID"),UUID)||CanonicalId(UUID).IsEmpty())
	{ Error=TEXT("_meta.UUID에 유효한 UUID가 필요합니다."); return false; }
	Summary.UUID=CanonicalId(UUID);
	if (!(*Meta)->TryGetStringField(TEXT("scenario"),Summary.Name)||Summary.Name.IsEmpty()||
		!Root->TryGetStringField(TEXT("CREATE_TIMESTAMP"),Summary.CreateTimestamp)||Summary.CreateTimestamp.IsEmpty()||
		!Root->TryGetStringField(TEXT("MESSAGE_ID"),MessageId)||MessageId!=TEXT("IFactory-agent")||
		!Root->TryGetArrayField(TEXT("DATA_MAP"),Rows)||Rows->IsEmpty())
	{ Error=TEXT("시나리오 이름, 생성 시각, MESSAGE_ID 또는 DATA_MAP이 잘못되었습니다."); return false; }
	double LastFrame=-1,LastTime=-1; TArray<FString> Materials;
	const TCHAR* NumericFields[]={TEXT("slab_thk"),TEXT("slab_wth"),TEXT("slab_len"),TEXT("slab_wt"),TEXT("left_skew_angle"),TEXT("right_skew_angle"),TEXT("mov_pos"),TEXT("center_x"),TEXT("center_y")};
	for (int32 I=0;I<Rows->Num();++I)
	{
		const TSharedPtr<FJsonObject>* Row=nullptr; FString Material; double Frame=0,Time=0; bool Meandering=false;
		if (!(*Rows)[I].IsValid()||!(*Rows)[I]->TryGetObject(Row)||!Row||!Row->IsValid()||
			!(*Row)->TryGetStringField(TEXT("mtl_no"),Material)||Material.TrimStartAndEnd().IsEmpty()||Material.Len()>256||Material.Contains(TEXT("\n"))||Material.Contains(TEXT("\r"))||
			!(*Row)->TryGetNumberField(TEXT("frame_no"),Frame)||!FMath::IsFinite(Frame)||Frame<0||Frame>9007199254740991.0||Frame!=FMath::FloorToDouble(Frame)||Frame<=LastFrame||
			!(*Row)->TryGetNumberField(TEXT("elapsed_sec"),Time)||!FMath::IsFinite(Time)||Time<0||Time<LastTime||
			!(*Row)->TryGetBoolField(TEXT("is_meandering"),Meandering))
		{ Error=FString::Printf(TEXT("DATA_MAP[%d] 소재/프레임/시각/타입 오류"),I); return false; }
		for (const TCHAR* Field:NumericFields) { double V; if(!(*Row)->TryGetNumberField(Field,V)||!FMath::IsFinite(V)) { Error=FString::Printf(TEXT("DATA_MAP[%d].%s 숫자 오류"),I,Field); return false; } }
		LastFrame=Frame; LastTime=Time; Materials.AddUnique(Material);
	}
	Summary.RowCount=Rows->Num(); Summary.LastElapsedSec=LastTime;
	Summary.MaterialSummary=Materials.Num()>3?FString::Printf(TEXT("%s 외 %d개"),*Materials[0],Materials.Num()-1):FString::Join(Materials,TEXT(", "));
	Summary.ReceivedUtc=FDateTime::UtcNow(); return true;
}
bool USlabScenarioReplaySubsystem::RegisterScenarioJson(const FString& Json)
{
	if (!IsInGameThread()||!bInitialized) return false;
	if (Json.Len()>8*1024*1024||FTCHARToUTF8(*Json).Length()>8*1024*1024) { RegistrationMessage=TEXT("등록 거부: JSON 8MiB 제한 초과"); return false; }
	if (PendingJson.Num()>=2) { RegistrationMessage=TEXT("등록 거부: 대기 2건 초과"); return false; }
	PendingJson.Add(Json); RegistrationMessage=TEXT("시나리오 등록 중"); StartNextRegistration(); return true;
}
void USlabScenarioReplaySubsystem::StartNextRegistration()
{
	if (bParsing||PendingJson.IsEmpty()||!bInitialized) return;
	FString Json=MoveTemp(PendingJson[0]); PendingJson.RemoveAt(0); bParsing=true;
	const uint64 Epoch=Generation; TWeakObjectPtr<ThisClass> Weak(this);
	Async(EAsyncExecution::ThreadPool,[Weak,Epoch,Json=MoveTemp(Json)]() mutable {
		FSlabScenarioSummary Summary; FString Error; const bool Valid=ValidateScenario(Json,Summary,Error);
		AsyncTask(ENamedThreads::GameThread,[Weak,Epoch,Valid,Summary=MoveTemp(Summary),Error=MoveTemp(Error),Json=MoveTemp(Json)]() mutable {
			if (!Weak.IsValid()||!Weak->bInitialized||Weak->Generation!=Epoch) return;
			Weak->bParsing=false;
			if (Valid) Weak->StoreValidated(MoveTemp(Summary),MoveTemp(Json));
			else { Weak->RegistrationMessage=Error; Weak->OnRegistrationFinished.Broadcast(Summary.UUID,false,Error); }
			Weak->StartNextRegistration();
		});
	});
}
void USlabScenarioReplaySubsystem::StoreValidated(FSlabScenarioSummary Summary,FString Json)
{
	if (Entries.ContainsByPredicate([&](const auto& E){return E.Summary.UUID==Summary.UUID;}))
	{ RegistrationMessage=TEXT("같은 UUID가 이미 있어 추가하지 않았습니다."); OnRegistrationFinished.Broadcast(Summary.UUID,false,RegistrationMessage); return; }
	if (Entries.Num()>=10)
	{
		for (int32 I=Entries.Num()-1;I>=0;--I) if (!IsReplayBusy()||Entries[I].Summary.UUID!=Status.ScenarioUUID) { Entries.RemoveAt(I); break; }
	}
	const FString UUID=Summary.UUID; Entries.Insert({MoveTemp(Summary),MoveTemp(Json)},0);
	RegistrationMessage=TEXT("시나리오 등록 완료 · 현재 실행 중에만 보관"); OnScenariosChanged.Broadcast(); OnRegistrationFinished.Broadcast(UUID,true,RegistrationMessage);
}
TArray<FSlabScenarioSummary> USlabScenarioReplaySubsystem::GetScenarios() const
{ TArray<FSlabScenarioSummary> Result; for(const auto& E:Entries) Result.Add(E.Summary); return Result; }
bool USlabScenarioReplaySubsystem::GetScenarioJson(const FString& UUID,FString& Json) const
{ const FString Key=CanonicalId(UUID); for(const auto& E:Entries) if(E.Summary.UUID==Key) { Json=E.Json; return true; } return false; }
bool USlabScenarioReplaySubsystem::IsReplayBusy() const
{ return Status.State==ESlabScenarioReplayState::Starting||Status.State==ESlabScenarioReplayState::Playing||Status.State==ESlabScenarioReplayState::Draining; }
bool USlabScenarioReplaySubsystem::CanReplay() const
{ return bInitialized&&GetWorld()&&!bLivePlaybackActive&&!IsReplayBusy()&&PlaybackAdapter.IsValid()&&PlaybackAdapter->GetWorld()==GetWorld()&&!SessionBusy(GetWorld()); }
bool USlabScenarioReplaySubsystem::RegisterPlaybackAdapter(UObject* Adapter)
{
	if (!IsInGameThread()||!GetWorld()||IsReplayBusy()||!IsValid(Adapter)||!Adapter->GetClass()->ImplementsInterface(USlabScenarioPlaybackAdapter::StaticClass())||Adapter->GetWorld()!=GetWorld()) return false;
	PlaybackAdapter=Adapter; return true;
}
void USlabScenarioReplaySubsystem::UnregisterPlaybackAdapter(UObject* Adapter)
{ if(IsInGameThread()&&PlaybackAdapter.Get()==Adapter) { AbortReplay(TEXT("재생 adapter 해제")); PlaybackAdapter.Reset(); } }
bool USlabScenarioReplaySubsystem::SetLivePlaybackActive(bool Active)
{ if(!IsInGameThread()||(Active&&IsReplayBusy())) return false; bLivePlaybackActive=Active; return true; }
bool USlabScenarioReplaySubsystem::RequestScenarioReplay(const FString& UUID,bool SendPcd,const TArray<FString>& Ids)
{
	if (!IsInGameThread()) return false;
	if (!CanReplay()) { Status.Message=TEXT("현재 실행 종료와 adapter 연결을 확인하십시오."); return false; }
	FString Json; if(!GetScenarioJson(UUID,Json)) { Status.Message=TEXT("저장된 시나리오가 없습니다."); return false; }
	const FString Run=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	if (Session->BeginReplaySensorSession(Run,Ids,CanonicalId(UUID),SendPcd).IsEmpty()) { Status.Message=Session->GetSlabSensorSessionStatus().Message; return false; }
	Status=FSlabScenarioReplayStatus(); Status.State=ESlabScenarioReplayState::Starting; Status.ScenarioUUID=CanonicalId(UUID); Status.RunUUID=Run; Status.bSendPcd=SendPcd;
	Status.Message=TEXT("재생 준비 중"); PlaybackWorld=GetWorld(); StartRequestedSeconds=FPlatformTime::Seconds();
	const bool Accepted=ISlabScenarioPlaybackAdapter::Execute_StartScenarioPlayback(PlaybackAdapter.Get(),Json,Status.ScenarioUUID,Run);
	if (!Accepted && Status.RunUUID==Run) { AbortReplay(TEXT("기존 재생기가 시작을 거절했습니다.")); return false; }
	return true;
}
bool USlabScenarioReplaySubsystem::NotifyPlaybackStarted(const FString& Run)
{
	if(!IsInGameThread()||Run!=Status.RunUUID||Status.State!=ESlabScenarioReplayState::Starting||!PlaybackWorld.IsValid()) return false;
	Status.State=ESlabScenarioReplayState::Playing; Status.Message=Status.bSendPcd?TEXT("재생 중 · PCD 송신"):TEXT("재생 중 · 관찰 전용"); return true;
}
bool USlabScenarioReplaySubsystem::NotifyPlaybackFinished(const FString& Run,bool Aborted)
{
	if(!IsInGameThread()||Run!=Status.RunUUID||!IsReplayBusy()||!PlaybackWorld.IsValid()) return false;
	if(Status.State==ESlabScenarioReplayState::Draining) return true;
	PlaybackWorld->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->EndSlabSensorSession(Run,Aborted);
	Status.State=ESlabScenarioReplayState::Draining; Status.Message=TEXT("종료 및 송신 정리 중"); return true;
}
void USlabScenarioReplaySubsystem::AbortReplay(const FString& Reason)
{
	if (IsReplayBusy()&&PlaybackWorld.IsValid()) PlaybackWorld->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->EndSlabSensorSession(Status.RunUUID,true);
	if (IsReplayBusy()) { Status.State=ESlabScenarioReplayState::Failed; Status.Message=Reason; }
}
bool USlabScenarioReplaySubsystem::Poll(float Delta)
{
	if (!bInitialized) return false;
	if (IsReplayBusy()&&(!PlaybackAdapter.IsValid()||!PlaybackWorld.IsValid())) AbortReplay(TEXT("재생 월드 또는 adapter가 종료되었습니다."));
	if(Status.State==ESlabScenarioReplayState::Starting&&FPlatformTime::Seconds()-StartRequestedSeconds>5) AbortReplay(TEXT("실제 재생 시작 알림 시간 초과 (5초)"));
	if (Status.State==ESlabScenarioReplayState::Draining&&PlaybackWorld.IsValid())
	{
		const auto S=PlaybackWorld->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus();
		if(S.State==EVirtualSlabSessionState::Completed||S.State==EVirtualSlabSessionState::Incomplete)
		{ Status.State=S.State==EVirtualSlabSessionState::Completed?ESlabScenarioReplayState::Completed:ESlabScenarioReplayState::Failed; Status.Message=S.Message; }
	}
	return true;
}
void USlabScenarioReplaySubsystem::OnWorldCleanup(UWorld* World,bool,bool)
{
	if (PlaybackWorld.Get()==World) { AbortReplay(TEXT("재생 월드 종료")); PlaybackWorld.Reset(); }
	if (PlaybackAdapter.IsValid()&&PlaybackAdapter->GetWorld()==World) PlaybackAdapter.Reset();
	if (World && World->GetGameInstance()==GetGameInstance()) bLivePlaybackActive=false;
}
