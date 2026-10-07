#include "SlabScenarioReplaySubsystem.h"
#include "VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Json.h"

namespace {
FString CanonicalId(const FString& Text) { FGuid Id; return FGuid::Parse(Text,Id)&&Id.IsValid()?Id.ToString(EGuidFormats::DigitsWithHyphensLower):FString(); }
FSlabScenarioSummary MakeSummary(const FSlabScenarioData& Data)
{
	FSlabScenarioSummary S; S.UUID=Data.ScenarioUUID; S.Name=Data.Name; S.CreateTimestamp=Data.CreateTimestamp;
	S.bGeneratedArchiveId=Data.bGeneratedArchiveId; S.RowCount=Data.Rows.Num(); S.ReceivedUtc=FDateTime::UtcNow();
	if(!Data.Rows.IsEmpty()) { S.MaterialSummary=Data.Rows[0].MtlNo; S.LastElapsedSec=Data.Rows.Last().ElapsedSec; }
	return S;
}
bool SessionBusy(const UWorld* World)
{
	if (!World) return false;
	const auto S=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus().State;
	return S==EVirtualSlabSessionState::Preparing||S==EVirtualSlabSessionState::Ready||S==EVirtualSlabSessionState::Running||S==EVirtualSlabSessionState::Paused||S==EVirtualSlabSessionState::Draining;
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
	PendingJson.Reset(); Entries.Reset(); PlaybackAdapter.Reset(); PlaybackWorld.Reset(); bParsing=false; bLivePlaybackActive=false; LiveScenarioUUID.Reset();
	ActiveRegistrationSerial=0; DeletedRegistrationCutoffs.Reset();
	Super::Deinitialize();
}
bool USlabScenarioReplaySubsystem::ValidateScenario(const FString& Json,FSlabScenarioSummary& Summary,FString& Error)
{
	FSlabScenarioDataPtr Data;
	if(!FSlabScenarioCodec::Parse(Json,Data,Error,false)) return false;
	Summary=MakeSummary(*Data); return true;
}
bool USlabScenarioReplaySubsystem::RegisterScenarioJson(const FString& Json)
{
	return RegisterScenarioJsonWithMissingUuidPolicy(Json,false);
}
bool USlabScenarioReplaySubsystem::RegisterScenarioJsonWithMissingUuidPolicy(const FString& Json,bool AllowMissingUuid)
{
	if (!IsInGameThread()||!bInitialized) return false;
	if (Json.Len()>8*1024*1024||FTCHARToUTF8(*Json).Length()>8*1024*1024) { RegistrationMessage=TEXT("등록 거부: JSON 8MiB 제한 초과"); return false; }
	if (PendingJson.Num()>=2) { RegistrationMessage=TEXT("등록 거부: 대기 2건 초과"); return false; }
	PendingJson.Add({Json,AllowMissingUuid,++NextRegistrationSerial}); RegistrationMessage=TEXT("시나리오 등록 중"); StartNextRegistration(); return true;
}
void USlabScenarioReplaySubsystem::StartNextRegistration()
{
	if (bParsing||PendingJson.IsEmpty()||!bInitialized) return;
	FPendingScenario Pending=MoveTemp(PendingJson[0]); PendingJson.RemoveAt(0); bParsing=true; ActiveRegistrationSerial=Pending.RegistrationSerial;
	const uint64 Epoch=Generation; TWeakObjectPtr<ThisClass> Weak(this);
	Async(EAsyncExecution::ThreadPool,[Weak,Epoch,Pending=MoveTemp(Pending)]() mutable {
		FSlabScenarioDataPtr Data; FString Error; const bool Valid=FSlabScenarioCodec::Parse(Pending.Json,Data,Error,Pending.bAllowMissingUuid);
		const uint64 Serial=Pending.RegistrationSerial;
		AsyncTask(ENamedThreads::GameThread,[Weak,Epoch,Serial,Valid,Data=MoveTemp(Data),Error=MoveTemp(Error)]() mutable {
			if (!Weak.IsValid()||!Weak->bInitialized||Weak->Generation!=Epoch) return;
			Weak->bParsing=false; Weak->ActiveRegistrationSerial=0;
			if (Valid) Weak->RegisterValidatedScenarioAtSerial(MoveTemp(Data),Serial);
			else { Weak->RegistrationMessage=Error; Weak->OnRegistrationFinished.Broadcast(FString(),false,Error); }
			Weak->StartNextRegistration();
			Weak->PruneDeletedRegistrationCutoffs();
		});
	});
}
void USlabScenarioReplaySubsystem::StoreValidated(FSlabScenarioSummary Summary,FString Json)
{
	// Compatibility helper retained for existing native catalog tests.
	FSlabScenarioDataPtr Data; FString Error;
	if(FSlabScenarioCodec::Parse(Json,Data,Error,true)) RegisterValidatedScenario(MoveTemp(Data));
}
bool USlabScenarioReplaySubsystem::RegisterValidatedScenario(FSlabScenarioDataPtr Data)
{
	if(!IsInGameThread())return false;
	if(!bInitialized){RegistrationMessage=TEXT("시나리오 보관 서비스가 초기화되지 않았습니다.");return false;}
	return RegisterValidatedScenarioAtSerial(MoveTemp(Data),++NextRegistrationSerial);
}
bool USlabScenarioReplaySubsystem::RegisterValidatedScenarioAtSerial(FSlabScenarioDataPtr Data,uint64 Serial)
{
	if(!IsInGameThread())return false;
	if(!bInitialized||!Data.IsValid()||Data->Rows.IsEmpty()||CanonicalId(Data->ScenarioUUID).IsEmpty())
	{RegistrationMessage=TEXT("유효한 UUID·시나리오 행과 준비된 보관 서비스가 필요합니다.");return false;}
	FSlabScenarioSummary Summary=MakeSummary(*Data);
	if(const uint64* Cutoff=DeletedRegistrationCutoffs.Find(Summary.UUID)) if(Serial<=*Cutoff)
	{ RegistrationMessage=TEXT("삭제 이전에 접수된 시나리오 등록 결과를 폐기했습니다."); OnRegistrationFinished.Broadcast(Summary.UUID,false,RegistrationMessage); return false; }
	if (Entries.ContainsByPredicate([&](const auto& E){return E.Summary.UUID==Summary.UUID;}))
	{ RegistrationMessage=TEXT("같은 UUID가 이미 있어 추가하지 않았습니다."); OnRegistrationFinished.Broadcast(Summary.UUID,false,RegistrationMessage); return false; }
	if (Entries.Num()>=10)
	{
		bool Removed=false;
		for (int32 I=Entries.Num()-1;I>=0;--I)
			if (!IsScenarioProtected(Entries[I].Summary.UUID)) { Entries.RemoveAt(I); Removed=true; break; }
		if(!Removed) { RegistrationMessage=TEXT("재생 중인 시나리오는 정리할 수 없어 등록하지 못했습니다."); OnRegistrationFinished.Broadcast(Summary.UUID,false,RegistrationMessage); return false; }
	}
	const FString UUID=Summary.UUID; Entries.Insert({MoveTemp(Summary),MoveTemp(Data)},0);
	RegistrationMessage=TEXT("시나리오 등록 완료 · 현재 실행 중에만 보관"); OnScenariosChanged.Broadcast(); OnRegistrationFinished.Broadcast(UUID,true,RegistrationMessage);
	return true;
}
TArray<FSlabScenarioSummary> USlabScenarioReplaySubsystem::GetScenarios() const
{ TArray<FSlabScenarioSummary> Result; for(const auto& E:Entries) Result.Add(E.Summary); return Result; }
bool USlabScenarioReplaySubsystem::GetScenarioJson(const FString& UUID,FString& Json) const
{ const auto Data=GetValidatedScenario(UUID); if(!Data.IsValid()) return false; Json=Data->OriginalJson; return true; }
FSlabScenarioDataPtr USlabScenarioReplaySubsystem::GetValidatedScenario(const FString& UUID) const
{ const FString Key=CanonicalId(UUID); for(const auto& E:Entries) if(E.Summary.UUID==Key) return E.Data; return nullptr; }
bool USlabScenarioReplaySubsystem::IsScenarioProtected(const FString& UUID,FString* OutReason) const
{
	auto Protect=[OutReason](const TCHAR* Reason){ if(OutReason) *OutReason=Reason; return true; };
	if(IsReplayBusy()&&Status.ScenarioUUID==UUID) return Protect(TEXT("재생 준비·재생·송신 정리 중인 시나리오는 삭제할 수 없습니다."));
	if(bLivePlaybackActive&&(LiveScenarioUUID.IsEmpty()||LiveScenarioUUID==UUID))
		return Protect(LiveScenarioUUID.IsEmpty()?TEXT("진행 중인 실시간 재생의 UUID를 확인할 수 없어 삭제를 보류합니다."):TEXT("실시간 재생 중인 시나리오는 삭제할 수 없습니다."));
	// Live playback releases its flag before sensor drain; an aborted replay can also be Failed while draining.
	const UWorld* Worlds[]={GetWorld(),PlaybackWorld.Get()};
	for(const UWorld* World:Worlds) if(World) if(const auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>())
	{
		const auto S=Session->GetSlabSensorSessionStatus();
		if(S.CurrentSlab.ScenarioUUID==UUID&&(S.State==EVirtualSlabSessionState::Ready||S.State==EVirtualSlabSessionState::Running||S.State==EVirtualSlabSessionState::Paused||S.State==EVirtualSlabSessionState::Draining))
			return Protect(TEXT("센서 세션 준비·진행·일시정지·송신 정리가 끝난 뒤 삭제할 수 있습니다."));
	}
	return false;
}
bool USlabScenarioReplaySubsystem::CanDeleteScenario(const FString& UUID,FString& OutReason) const
{
	OutReason.Reset();
	if(!IsInGameThread()||!bInitialized) { OutReason=TEXT("시나리오 보관 관리자가 준비되지 않았습니다."); return false; }
	const FString Key=CanonicalId(UUID);
	if(Key.IsEmpty()||!Entries.ContainsByPredicate([&](const auto& E){return E.Summary.UUID==Key;}))
	{ OutReason=TEXT("보관 목록에서 시나리오를 찾을 수 없습니다."); return false; }
	return !IsScenarioProtected(Key,&OutReason);
}
bool USlabScenarioReplaySubsystem::DeleteScenario(const FString& UUID,FString& OutReason)
{
	if(!CanDeleteScenario(UUID,OutReason)) return false;
	const FString Key=CanonicalId(UUID);
	DeletedRegistrationCutoffs.Add(Key,NextRegistrationSerial);
	Entries.RemoveAll([&](const auto& E){return E.Summary.UUID==Key;});
	RegistrationMessage=TEXT("보관 목록에서 삭제했습니다. 새로 수신하면 다시 보관할 수 있습니다.");
	OnScenariosChanged.Broadcast(); PruneDeletedRegistrationCutoffs(); return true;
}
void USlabScenarioReplaySubsystem::PruneDeletedRegistrationCutoffs()
{
	uint64 Earliest=ActiveRegistrationSerial?ActiveRegistrationSerial:MAX_uint64;
	for(const auto& Pending:PendingJson) Earliest=FMath::Min(Earliest,Pending.RegistrationSerial);
	for(auto It=DeletedRegistrationCutoffs.CreateIterator();It;++It) if(Earliest>It.Value()) It.RemoveCurrent();
}
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
{ return SetLiveScenarioPlaybackActive(Active,FString()); }
bool USlabScenarioReplaySubsystem::SetLiveScenarioPlaybackActive(bool Active,const FString& UUID)
{ if(!IsInGameThread()||(Active&&IsReplayBusy())) return false; bLivePlaybackActive=Active; LiveScenarioUUID=Active?CanonicalId(UUID):FString(); return true; }
bool USlabScenarioReplaySubsystem::RequestScenarioReplay(const FString& UUID,bool SendPcd,const TArray<FString>& Ids)
{
	FVirtualSlabSensorOutputSelection Outputs=FVirtualSlabSensorOutputSelection::ObservationOnly(); Outputs.bPointCloud=SendPcd;
	return RequestScenarioReplayWithOutputs(UUID,Outputs,Ids);
}
bool USlabScenarioReplaySubsystem::RequestScenarioReplayWithOutputs(const FString& UUID,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& Ids)
{return RequestScenarioReplayWithExecutionOptions(UUID,Outputs,Ids,FSlabExecutionOptions());}
bool USlabScenarioReplaySubsystem::RequestScenarioReplayWithExecutionOptions(const FString& UUID,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& Ids,const FSlabExecutionOptions& Options)
{
	if (!IsInGameThread()) return false;
	if (!CanReplay()) { Status.Message=TEXT("현재 실행 종료와 adapter 연결을 확인하십시오."); return false; }
	FString Json; if(!GetScenarioJson(UUID,Json)) { Status.Message=TEXT("저장된 시나리오가 없습니다."); return false; }
	const FString Run=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	FString Error;bWaitingForTransmission=Options.Policy==ESlabExecutionPolicy::RequireData;
	if(bWaitingForTransmission)
	{if(!Session->PrepareScenarioTransmission(Run,Ids,CanonicalId(UUID),Outputs,Options,Error)){bWaitingForTransmission=false;Status.Message=Error;return false;}}
	else if (Session->BeginScenarioSensorSession(Run,Ids,CanonicalId(UUID),Outputs).IsEmpty()) { Status.Message=Session->GetSlabSensorSessionStatus().Message; return false; }
	Status=FSlabScenarioReplayStatus(); Status.State=ESlabScenarioReplayState::Starting; Status.ScenarioUUID=CanonicalId(UUID); Status.RunUUID=Run; Status.bSendPcd=Outputs.bPointCloud; Status.Outputs=Outputs;
	Status.Message=TEXT("재생 준비 중"); PlaybackWorld=GetWorld(); StartRequestedSeconds=FPlatformTime::Seconds();
	if(!bWaitingForTransmission)InvokePlaybackAdapter();
	return Status.State!=ESlabScenarioReplayState::Failed;
}
void USlabScenarioReplaySubsystem::InvokePlaybackAdapter()
{
	FString Json;if(!PlaybackAdapter.IsValid()||!GetScenarioJson(Status.ScenarioUUID,Json)){AbortReplay(TEXT("재생 adapter 또는 원본 데이터가 없습니다."));return;}
	bWaitingForTransmission=false;StartRequestedSeconds=FPlatformTime::Seconds();const FString Run=Status.RunUUID;
	if(!ISlabScenarioPlaybackAdapter::Execute_StartScenarioPlayback(PlaybackAdapter.Get(),Json,Status.ScenarioUUID,Run)&&Status.RunUUID==Run)AbortReplay(TEXT("기존 재생기가 시작을 거절했습니다."));
}
bool USlabScenarioReplaySubsystem::NotifyPlaybackStarted(const FString& Run)
{
	if(!IsInGameThread()||Run!=Status.RunUUID||Status.State!=ESlabScenarioReplayState::Starting||!PlaybackWorld.IsValid()) return false;
	Status.State=ESlabScenarioReplayState::Playing; Status.Message=Status.Outputs.HasAnyOutput()?TEXT("재생 중 · 선택 출력 송신"):TEXT("재생 중 · 관찰 전용"); return true;
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
	if(!IsReplayBusy()) return;
	bWaitingForTransmission=false;
	const FString Run=Status.RunUUID;
	if (PlaybackWorld.IsValid()) PlaybackWorld->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->EndSlabSensorSession(Run,true);
	Status.State=ESlabScenarioReplayState::Failed; Status.Message=Reason;
	if(PlaybackAdapter.IsValid()) ISlabScenarioPlaybackAdapter::Execute_StopScenarioPlayback(PlaybackAdapter.Get(),Run);
}
bool USlabScenarioReplaySubsystem::Poll(float Delta)
{
	if (!bInitialized) return false;
	if (IsReplayBusy()&&(!PlaybackAdapter.IsValid()||!PlaybackWorld.IsValid())) AbortReplay(TEXT("재생 월드 또는 adapter가 종료되었습니다."));
	if(Status.State==ESlabScenarioReplayState::Starting&&bWaitingForTransmission&&PlaybackWorld.IsValid())
	{
		const auto S=PlaybackWorld->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus();
		if(S.RunId!=Status.RunUUID||S.State==EVirtualSlabSessionState::Incomplete)AbortReplay(S.Message);
		else if(S.State==EVirtualSlabSessionState::Ready)InvokePlaybackAdapter();
	}
	if(Status.State==ESlabScenarioReplayState::Starting&&!bWaitingForTransmission&&FPlatformTime::Seconds()-StartRequestedSeconds>5) AbortReplay(TEXT("실제 재생 시작 알림 시간 초과 (5초)"));
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
	if (World && World->GetGameInstance()==GetGameInstance()) { bLivePlaybackActive=false; LiveScenarioUUID.Reset(); }
}
