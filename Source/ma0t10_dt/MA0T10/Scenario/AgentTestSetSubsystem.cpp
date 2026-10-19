#include "AgentTestSetSubsystem.h"
#include "VirtualDatasetSubsystem.h"
#include "AgentTestSetPcdPreview.h"
#include "AgentTestSetExamples.h"
#include "ma0t10_dt/ma0t10_dt.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "SlabScenarioReplaySubsystem.h"
#include "VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Json.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "TimerManager.h"
#include <atomic>

namespace
{
constexpr int64 TestSetInputLimit=64ll*1024*1024;
struct FAgentTestSetPrepared
{
    FSlabScenarioDataPtr Data;
    TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Plan;
    FString Error;
    uint32 Configuration=0;
};
struct FAgentTestSetInput
{
    FSlabScenarioDataPtr Data;
    FString Hash;
    int64 Bytes=0;
    bool Stored=false;
    FSlabScenarioDataPtr Replacement;
    int64 ReplacementBytes=0;
    FString TemplatePath,TemplateHash;
};
struct FTestSetStored {bool Ok=false;FString Hash;};
struct FAgentTestSetSpool {int32 Index;TFuture<FTestSetStored> Result;};
FString NewTestSetId(){return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);}
FString EncodeTestSetJson(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
bool SaveTestSetJson(const FString& Path,const FString& Text)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    const FString Temp=Path+TEXT(".pending");
    if(!FFileHelper::SaveStringToFile(Text,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return false;
    return UVirtualSensorFileSaveSubsystem::CommitTestSetManifest(Path,Temp);
}
const TCHAR* TestSetStateName(EAgentTestSetItemState S)
{switch(S){case EAgentTestSetItemState::Generating:return TEXT("generating");case EAgentTestSetItemState::Completed:return TEXT("completed");case EAgentTestSetItemState::Failed:return TEXT("failed");default:return TEXT("waiting");}}
USlabScenarioReplaySubsystem* TestSetReplay(UWorld* W)
{return W&&W->GetGameInstance()?W->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>():nullptr;}
bool TestSetWorldBusy(UWorld* W)
{
    const auto S=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus().State;
    return S==EVirtualSlabSessionState::Preparing||S==EVirtualSlabSessionState::Ready||S==EVirtualSlabSessionState::Running||S==EVirtualSlabSessionState::Paused||S==EVirtualSlabSessionState::Draining;
}
template<class T> T* FindTestSetSensor(UWorld* W,const FString& Explicit)
{
    T* Found=nullptr;
    for(TActorIterator<T> It(W);It;++It)if(!It->IsActorBeingDestroyed()&&(Explicit.IsEmpty()||It->GetSensorId()==Explicit))
    {if(Found)return nullptr;Found=*It;}
    return Found;
}
}
struct FTestSetStoppedWork
{
    FString SensorId;
    TWeakObjectPtr<AVirtualSensorActorBase> Sensor;
    TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> Publisher;
    TWeakObjectPtr<UVirtualSensorRecorderComponent> Recorder;
    TArray<TPair<FString,uint64>> Periodic;
    uint64 RunRevision=0;
    bool Running=false,PublisherHeld=false,RecorderHeld=false;
};
struct FAgentTestSetState
{
    FString Root,SetId,Message,PreviewRun;
    TArray<FAgentTestSetItem> Items;
    TArray<FAgentTestSetInput> Inputs;
    TArray<FAgentTestSetSpool> Spools;
    TArray<TSharedPtr<FJsonValue>> Replaced;
    TFuture<bool> Replace;
    int32 Replacing=INDEX_NONE;
    FString ArchivePath;
    double NextReplace=0;
    TWeakObjectPtr<ASlabActor> Slab;
    TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    TArray<FTestSetStoppedWork> Stopped;
    bool StopRequested=false,Blocked=false;
    double NextReadiness=0;
    int32 Active=INDEX_NONE,Preparing=INDEX_NONE,Preview=INDEX_NONE;
    int64 Revision=1,ManifestRevision=0,PendingManifestRevision=0,InputBytes=0;
    int32 SavedCompleted=0,PendingManifestCompleted=0;
    bool Paused=false,Yielded=false,Reset=false,ManifestFailed=false,ResetManifestQueued=false;
    double Started=0,ActiveStarted=0;
    double WorkSeconds=0,LastPump=0;
    int32 GeneratedCount=0;
    TFuture<FAgentTestSetPrepared> Prepare;
    TOptional<FAgentTestSetPrepared> Prepared;
    TFuture<bool> Manifest;
    TFuture<FString> Verify;
    int32 Verifying=INDEX_NONE;
    TSharedRef<std::atomic<bool>,ESPMode::ThreadSafe> Cancelled=MakeShared<std::atomic<bool>,ESPMode::ThreadSafe>(false);
};

FAgentTestSetSettings FAgentTestSetSettings::Load()
{
    FAgentTestSetSettings S;const TCHAR* Section=TEXT("AgentTestSet");
    GConfig->GetBool(Section,TEXT("Enabled"),S.Enabled,GGameIni);GConfig->GetString(Section,TEXT("TriggerMtlNo"),S.Trigger,GGameIni);S.Trigger.TrimStartAndEndInline();
    GConfig->GetInt(Section,TEXT("TargetScenarios"),S.Target,GGameIni);GConfig->GetInt(Section,TEXT("SampleStride"),S.SampleStride,GGameIni);GConfig->GetInt(Section,TEXT("SettleFrames"),S.SettleFrames,GGameIni);
    GConfig->GetBool(Section,TEXT("Pipeline"),S.Pipeline,GGameIni);GConfig->GetBool(Section,TEXT("CaptureCamera"),S.CaptureCamera,GGameIni);GConfig->GetBool(Section,TEXT("CameraTruth"),S.CameraTruth,GGameIni);GConfig->GetBool(Section,TEXT("IncludeImu"),S.IncludeImu,GGameIni);
    GConfig->GetBool(Section,TEXT("AutoFillExamples"),S.AutoFillExamples,GGameIni);GConfig->GetInt(Section,TEXT("ExampleSeed"),S.ExampleSeed,GGameIni);GConfig->GetInt(Section,TEXT("ExampleMaxFrames"),S.ExampleMaxFrames,GGameIni);
    S.ExampleMaxFrames=FMath::Clamp(S.ExampleMaxFrames,1,10000);
    GConfig->GetString(Section,TEXT("LidarSensorId"),S.LidarId,GGameIni);GConfig->GetString(Section,TEXT("CameraSensorId"),S.CameraId,GGameIni);
    GConfig->GetBool(Section,TEXT("LidarFidelity"),S.LidarFidelity,GGameIni);GConfig->GetString(Section,TEXT("ExportFormat"),S.ExportFormat,GGameIni);GConfig->GetBool(Section,TEXT("ExportUprightEnvelope"),S.ExportUprightEnvelope,GGameIni);
    S.Target=FMath::Clamp(S.Target,1,1000);S.SampleStride=FMath::Clamp(S.SampleStride,1,10000);S.SettleFrames=FMath::Clamp(S.SettleFrames,1,600);
    S.Enabled&=!S.Trigger.IsEmpty();S.CaptureCamera|=S.CameraTruth;return S;
}
void UAgentTestSetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{Super::Initialize(Collection);Config=FAgentTestSetSettings::Load();}
bool UAgentTestSetSubsystem::Matches(const FSlabScenarioData& Data,const FString& Trigger)
{
    // The codec already enforces one material per immutable scenario. Admission stays O(1).
    const FString Key=Trigger.TrimStartAndEnd();return !Data.Rows.IsEmpty()&&!Key.IsEmpty()&&Data.Rows[0].MtlNo.TrimStartAndEnd()==Key;
}
bool UAgentTestSetSubsystem::IsTrigger(const FSlabScenarioData& Data) const {return Config.Enabled&&Matches(Data,Config.Trigger);}
bool UAgentTestSetSubsystem::Enqueue(FSlabScenarioDataPtr Data,ASlabActor* Slab,FString& Error)
{
    if(!Data||!IsTrigger(*Data)||!IsValid(Slab)||Slab->GetWorld()!=GetWorld()){Error=TEXT("테스트셋 입력과 대상 Slab을 확인하세요");return false;}
    if(!State)
    {
        State=MakeShared<FAgentTestSetState>();State->SetId=NewTestSetId();State->Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TestSets")/State->SetId);State->Slab=Slab;State->Started=FPlatformTime::Seconds();
    }
    auto& S=*State;
    if(S.Reset||S.Slab.Get()!=Slab){Error=TEXT("세트 초기화 또는 다른 Slab의 작업이 진행 중입니다");return false;}
    for(int32 I=0;I<S.Items.Num();++I)if(S.Items[I].ScenarioId==Data->ScenarioUUID||(S.Inputs[I].Replacement&&S.Inputs[I].Replacement->ScenarioUUID==Data->ScenarioUUID))
    {Error=S.Inputs[I].Hash==Data->OriginalContentSha1?TEXT("이미 등록된 테스트셋 시나리오입니다"):TEXT("같은 시나리오 ID의 원본이 다릅니다");return false;}
    const int64 Bytes=int64(Data->OriginalJson.Len())*sizeof(TCHAR)+int64(Data->Rows.Num())*(sizeof(FSlabScenarioRow)+64);
    int32 Replace=INDEX_NONE;
    if(Config.AutoFillExamples)for(int32 I=0;I<S.Items.Num();++I)if(S.Items[I].Example&&!S.Inputs[I].Replacement){Replace=I;break;}
    if((S.Items.Num()>=Config.Target&&Replace==INDEX_NONE)||Bytes>TestSetInputLimit-S.InputBytes){Error=TEXT("테스트셋 목표 개수 또는 입력 대기 64MiB 한도에 도달했습니다");return false;}
    FAgentTestSetItem Item;Item.ScenarioId=Data->ScenarioUUID;Item.RunId=NewTestSetId();Item.Frames=FMath::DivideAndRoundUp(Data->Rows.Num(),Config.SampleStride);
    if(Config.AutoFillExamples)Item.Frames=FMath::Min(Item.Frames,Config.ExampleMaxFrames);
    if(Item.Frames>10000){Error=TEXT("샘플 프레임은 시나리오당 10000개 이하여야 합니다 · SampleStride를 늘리세요");return false;}
    if(Replace!=INDEX_NONE)
    {
        // Reserve immediately, replace only after all owners of that slot have drained.
        StopPreview();S.Inputs[Replace].Replacement=Data;S.Inputs[Replace].ReplacementBytes=Bytes;S.InputBytes+=Bytes;
        S.SavedCompleted=0;S.Message=TEXT("원본 수신 · 예시를 원본으로 바꾸는 중");++S.Revision;EnsurePump();return true;
    }
    Item.SourceUUID=Data->ScenarioUUID;
    Item.Directory=S.Root/FString::Printf(TEXT("scenario_%03d_%s"),S.Items.Num()+1,*Item.ScenarioId);Item.Message=TEXT("대기");
    S.Items.Add(MoveTemp(Item));S.Inputs.Add({Data,Data->OriginalContentSha1,Bytes,false});S.InputBytes+=Bytes;
    if(Config.AutoFillExamples&&S.Items.Num()==1)
    {
        const int32 Seed=AgentTestSetExamples::SeedFor(Data->OriginalContentSha1,Config.ExampleSeed);
        for(int32 N=1;N<Config.Target;++N)
        {
            FAgentTestSetItem Example;Example.Example=true;Example.SourceUUID=Data->ScenarioUUID;Example.Seed=Seed;Example.ScenarioId=NewTestSetId();Example.RunId=NewTestSetId();
            Example.VariationJson=AgentTestSetExamples::Variation(Seed,N,Slab->DimensionUnit,Slab->PositionUnit);
            Example.Frames=FMath::Min(FMath::DivideAndRoundUp(Data->Rows.Num(),Config.SampleStride),Config.ExampleMaxFrames);
            Example.Directory=S.Root/FString::Printf(TEXT("scenario_%03d_%s"),N+1,*Example.ScenarioId);Example.Message=TEXT("대기");
            S.Items.Add(MoveTemp(Example));
            // Derive on a bounded worker from the first disk source. Examples never reserve
            // 99 input copies or keep the received source's admission bytes pinned in memory.
            FAgentTestSetInput Input;Input.TemplatePath=S.Items[0].Directory/TEXT("original.json");Input.TemplateHash=Data->OriginalContentSha1;S.Inputs.Add(MoveTemp(Input));
        }
    }
    S.Message=TEXT("테스트셋 시나리오 대기");++S.Revision;EnsurePump();return true;
}
void UAgentTestSetSubsystem::EnsurePump(){GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.01f,true);}
bool UAgentTestSetSubsystem::HasItems() const{return State&&!State->Items.IsEmpty();}
const TArray<FAgentTestSetItem>& UAgentTestSetSubsystem::Items() const{static const TArray<FAgentTestSetItem> Empty;return State?State->Items:Empty;}
int32 UAgentTestSetSubsystem::Completed() const{int32 N=0;for(const auto& I:Items())N+=I.State==EAgentTestSetItemState::Completed;return N;}
bool UAgentTestSetSubsystem::IsComplete() const{return State&&!State->Reset&&!State->Replace.IsValid()&&!State->Inputs.ContainsByPredicate([](const auto& I){return bool(I.Replacement);})&&Completed()==Config.Target&&!State->ManifestFailed&&State->SavedCompleted==Config.Target;}
double UAgentTestSetSubsystem::ScenariosPerSecond() const{return State&&State->WorkSeconds>.01?State->GeneratedCount/State->WorkSeconds:0;}
double UAgentTestSetSubsystem::RemainingSeconds() const
{
    const double Rate=ScenariosPerSecond();if(!State||Rate<=0)return -1;
    int32 Remaining=Config.Target-Completed();
    for(int32 N=0;N<State->Inputs.Num();++N)if(State->Inputs[N].Replacement&&State->Items[N].State==EAgentTestSetItemState::Completed)++Remaining;
    return Remaining/Rate;
}
bool UAgentTestSetSubsystem::IsPaused() const{return State&&(State->Paused||State->Yielded);}
FString UAgentTestSetSubsystem::Directory() const{return State?State->Root:FString();}
FString UAgentTestSetSubsystem::Message() const{return State?State->Message:TEXT("시나리오 대기");}
int64 UAgentTestSetSubsystem::Revision() const{return State?State->Revision:0;}
void UAgentTestSetSubsystem::YieldToOrdinaryScenario()
{
    if(!State)return;StopPreview();State->Preview=INDEX_NONE;
    if(State->Active!=INDEX_NONE)if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())if(D->OwnsPlannedRun(State->Items[State->Active].RunId))D->YieldPlannedCapture();
    RestoreStoppedWork();State->Yielded=true;State->Message=TEXT("일반 시나리오 우선 · 테스트셋 대기");++State->Revision;EnsurePump();
}
void UAgentTestSetSubsystem::SetPaused(bool Paused)
{if(!State)return;State->Paused=Paused;if(Paused&&State->Active!=INDEX_NONE)if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())if(D->OwnsPlannedRun(State->Items[State->Active].RunId))D->YieldPlannedCapture();State->Message=Paused?TEXT("테스트셋 일시 중지"):TEXT("테스트셋 재개 대기");++State->Revision;EnsurePump();}
bool UAgentTestSetSubsystem::Retry(int32 Index,FString& Error)
{
    if(!State||State->Reset||!State->Items.IsValidIndex(Index)||State->Items[Index].State!=EAgentTestSetItemState::Failed){Error=TEXT("실패한 시나리오를 선택하세요");return false;}
    auto& I=State->Items[Index];I.State=EAgentTestSetItemState::Waiting;I.RunId=NewTestSetId();I.CompletedFrames=0;I.Message=TEXT("재시도 대기");++State->Revision;EnsurePump();return true;
}
bool UAgentTestSetSubsystem::ResetConfirmed(FString& Error)
{
    if(!State)return true;if(State->Reset){Error=TEXT("기존 작업 정리 중입니다");return false;}
    StopPreview();State->Reset=true;State->Preview=INDEX_NONE;State->Message=TEXT("세트 초기화 중 · 기존 파일은 보존합니다");
    if(State->Active!=INDEX_NONE)if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())if(D->OwnsPlannedRun(State->Items[State->Active].RunId))D->Stop();++State->Revision;EnsurePump();return true;
}
bool UAgentTestSetSubsystem::SelectPreview(int32 Index,FString& Error)
{
    if(!IsComplete()||!State->Items.IsValidIndex(Index)){Error=TEXT("목표 시나리오를 모두 생성한 뒤 선택하세요");return false;}
    StopPreview();PcdPreview=MakeShared<FAgentTestSetPcdPreview>();
    PcdPreview->Start(GetWorld(),State->Items[Index],Index);EnsurePump();return true;
}
void UAgentTestSetSubsystem::StopPreview()
{
    if(!PcdPreview)return;PcdPreview->Stop();PcdPreview.Reset();
}
void UAgentTestSetSubsystem::SetPreviewPaused(bool Paused){if(PcdPreview){PcdPreview->SetPaused(Paused);EnsurePump();}}
FAgentTestSetPreviewStatus UAgentTestSetSubsystem::PreviewStatus() const{return PcdPreview?PcdPreview->Status():FAgentTestSetPreviewStatus();}
UTexture2D* UAgentTestSetSubsystem::PreviewTexture() const{return PcdPreview?PcdPreview->Texture():nullptr;}
bool UAgentTestSetSubsystem::HasBusyWork() const{return State&&State->Blocked&&State->Items.ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Waiting;});}
bool UAgentTestSetSubsystem::BusyReason(FString& Reason) const
{
    if(!State)return false;const auto& S=*State;TArray<FString> Reasons;
    if(S.Slab.IsValid()&&S.Slab->IsSimulationActive())Reasons.Add(TEXT("일반 시나리오 실행 중 · 완료 후 자동 시작"));
    if(TestSetWorldBusy(GetWorld()))
    {
        const auto Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus();
        Reasons.Add(Session.State==EVirtualSlabSessionState::Draining?TEXT("시나리오 송신 정리 중"):TEXT("일반 시나리오가 센서를 사용 중"));
    }
    if(auto* R=TestSetReplay(GetWorld());R&&R->IsReplayBusy())Reasons.Add(TEXT("일반 시나리오 재생 중"));
    if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();D&&D->IsActive()&&S.Active==INDEX_NONE)Reasons.Add(TEXT("다른 데이터셋 생성 중"));
    auto Sensor=[&](AVirtualSensorActorBase* A)
    {
        if(!A)return;TArray<FString> Work;
        if(A->IsSensorRunning())Work.Add(TEXT("측정"));
        auto* Files=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>();
        auto* Periodic=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();
        if(Periodic&&Periodic->HasSensorWork(A))Work.Add(TEXT("주기 저장 또는 마지막 파일 저장"));
        else if(Files&&Files->HasPendingSave(A))Work.Add(TEXT("파일 저장 마무리"));
        auto* P=A->OutputComponent?A->OutputComponent->StreamPublisherComponent.Get():nullptr;
        if(P)
        {
            if(!P->IsLocalCaptureSuspended(A->GetSensorId()))for(const auto& Stream:P->GetStreamStatuses())
                if((Stream.SensorId.IsEmpty()||Stream.SensorId==A->GetSensorId())&&(Stream.bEnabled||P->IsStreamActiveOrAwaitingResume(Stream.StreamKind,Stream.SensorId)))
                    if((A->GetSensorKind()==EVirtualSensorKind::Lidar&&(Stream.StreamKind==EVirtualSensorStreamKind::PointCloud||Stream.StreamKind==EVirtualSensorStreamKind::LidarPayload))||(A->GetSensorKind()==EVirtualSensorKind::Camera&&Stream.StreamKind==EVirtualSensorStreamKind::CameraImage))
                        Work.AddUnique(Stream.StreamKind==EVirtualSensorStreamKind::PointCloud?TEXT("독립 PCD 송신"):TEXT("독립 센서 송신"));
            if(P->HasSensorPlacementWork(A->GetSensorId(),A->GetSensorKind())&&Work.IsEmpty())Work.Add(TEXT("송신 프레임 정리"));
        }
        if(const auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();Raw&&Raw->GetOutstandingSensorFrameCount(A->GetSensorId())>0)Work.AddUnique(TEXT("미확인 송신 프레임 정리"));
        if(A->OutputComponent&&A->OutputComponent->RecorderComponent&&A->OutputComponent->RecorderComponent->IsRecordingSensor(A->GetSensorId()))Work.Add(TEXT("센서 녹화"));
        FString Detail;if(!A->CanEditSensorConfiguration(Detail)||!A->CanEditSensorPlacement(Detail))if(Work.IsEmpty())Work.Add(Detail);
        if(!Work.IsEmpty())Reasons.Add(A->GetSensorId()+TEXT("의 ")+FString::Join(Work,TEXT("·"))+TEXT(" 사용 중"));
    };
    Sensor(S.Lidar.Get());if(Config.CaptureCamera)Sensor(S.Camera.Get());
    Reason=Reasons.IsEmpty()?FString():TEXT("대기 · ")+FString::Join(Reasons,TEXT(" / "));return !Reason.IsEmpty();
}
bool UAgentTestSetSubsystem::StopBusyWorkAndStart(FString& Error)
{
    if(!State||State->Reset){Error=TEXT("먼저 테스트셋 시나리오를 등록하세요");return false;}
    State->StopRequested=true;State->Paused=false;State->NextReadiness=0;EnsurePump();return true;
}
void UAgentTestSetSubsystem::SuspendBusyWork()
{
    auto& S=*State;if(!S.Stopped.IsEmpty())return;
    auto Suspend=[&](AVirtualSensorActorBase* A)
    {
        if(!A)return;FTestSetStoppedWork W;W.Sensor=A;W.SensorId=A->GetSensorId();W.Running=A->IsSensorRunning();
        if(W.Running)A->StopSensor();W.RunRevision=A->GetRunControlRevision();
        if(A->OutputComponent)
        {
            W.Publisher=A->OutputComponent->StreamPublisherComponent;W.Recorder=A->OutputComponent->RecorderComponent;
            if(W.Publisher.IsValid()&&!W.Publisher->IsLocalCaptureSuspended(A->GetSensorId())){W.Publisher->SetLocalCaptureSuspended(A->GetSensorId(),true);W.PublisherHeld=true;}
            if(W.Recorder.IsValid()&&W.Recorder->IsRecordingSensor(A->GetSensorId())){W.Recorder->SetLocalCaptureSuspended(A->GetSensorId(),true);W.RecorderHeld=true;}
        }
        if(auto* P=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>())for(const auto& V:P->GetRecentPeriodicSessions())if(V.SensorId==A->GetSensorId()&&V.bActive)
            if(const uint64 Token=P->SuspendForLocalCapture(V.SessionId))W.Periodic.Add({V.SessionId,Token});
        S.Stopped.Add(MoveTemp(W));
    };
    Suspend(S.Lidar.Get());if(Config.CaptureCamera)Suspend(S.Camera.Get());
}
void UAgentTestSetSubsystem::RestoreStoppedWork(bool Resume)
{
    if(!State)return;auto Work=MoveTemp(State->Stopped);State->Stopped.Reset();
    for(auto& W:Work)
    {
        if(W.PublisherHeld&&W.Publisher.IsValid())W.Publisher->SetLocalCaptureSuspended(W.SensorId,false);
        if(W.RecorderHeld&&W.Recorder.IsValid())W.Recorder->SetLocalCaptureSuspended(W.SensorId,false);
        auto* A=W.Sensor.Get();
        if(Resume&&A&&A->GetSensorId()==W.SensorId&&W.Running&&A->GetRunControlRevision()==W.RunRevision&&!A->IsSensorRunning())A->StartSensor();
        if(Resume&&A&&A->GetSensorId()==W.SensorId)if(auto* P=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>())for(const auto& V:W.Periodic)P->ResumeAfterLocalCapture(V.Key,V.Value);
    }
}
TSharedRef<FJsonObject> UAgentTestSetSubsystem::StatusJson() const
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("schema_version"),TEXT("ma0t10.testset.v1"));O->SetStringField(TEXT("trigger_mtl_no"),Config.Trigger);O->SetNumberField(TEXT("target_scenarios"),Config.Target);
    O->SetBoolField(TEXT("enabled"),Config.Enabled);O->SetBoolField(TEXT("pipeline"),Config.Pipeline);O->SetBoolField(TEXT("paused"),IsPaused());O->SetBoolField(TEXT("reset"),State&&State->Reset);
    O->SetNumberField(TEXT("completed_scenarios"),Completed());O->SetBoolField(TEXT("complete"),IsComplete());O->SetStringField(TEXT("directory"),Directory());
    O->SetStringField(TEXT("project_commit"),TEXT(MA0T10_PROJECT_COMMIT));O->SetStringField(TEXT("build"),FApp::GetBuildVersion());O->SetBoolField(TEXT("sending_supported"),false);O->SetStringField(TEXT("message"),Message());
    O->SetBoolField(TEXT("capture_camera"),Config.CaptureCamera);O->SetBoolField(TEXT("camera_truth"),Config.CameraTruth);O->SetBoolField(TEXT("include_imu"),Config.IncludeImu);O->SetNumberField(TEXT("sample_stride"),Config.SampleStride);
    O->SetStringField(TEXT("export_format"),Config.ExportFormat);O->SetBoolField(TEXT("lidar_fidelity"),Config.LidarFidelity);
    O->SetBoolField(TEXT("auto_fill_examples"),Config.AutoFillExamples);O->SetNumberField(TEXT("example_max_frames"),Config.ExampleMaxFrames);O->SetNumberField(TEXT("example_seed"),Config.ExampleSeed);
    O->SetStringField(TEXT("frame_policy"),Config.AutoFillExamples?TEXT("uniform_capped_stride_samples"):TEXT("full_stride_samples"));
    O->SetStringField(TEXT("example_purpose"),TEXT("demonstration representative frames; synthetic examples, not received training data"));
    O->SetNumberField(TEXT("scenarios_per_second"),ScenariosPerSecond());O->SetNumberField(TEXT("remaining_seconds"),RemainingSeconds());
    int32 Received=0,Examples=0,Pending=0;for(const auto& I:Items()){Received+=!I.Example;Examples+=I.Example;}
    if(State){for(const auto& I:State->Inputs)Pending+=bool(I.Replacement);O->SetArrayField(TEXT("replaced_examples"),State->Replaced);}
    O->SetNumberField(TEXT("received_scenarios"),Received);O->SetNumberField(TEXT("example_scenarios"),Examples);O->SetNumberField(TEXT("pending_received_replacements"),Pending);
    O->SetStringField(TEXT("replacement_policy"),TEXT("archive drained example directory under examples_replaced; retain all files; exclude from active count"));
    if(State){O->SetStringField(TEXT("set_id"),State->SetId);O->SetNumberField(TEXT("revision"),State->Revision);O->SetNumberField(TEXT("pending_input_bytes"),double(State->InputBytes));O->SetStringField(TEXT("preview_run_id"),State->PreviewRun);}
    const auto View=PreviewStatus();auto Preview=MakeShared<FJsonObject>();Preview->SetNumberField(TEXT("scenario_index"),View.Scenario);Preview->SetNumberField(TEXT("frame_index"),View.Frame);Preview->SetNumberField(TEXT("frames"),View.Frames);Preview->SetNumberField(TEXT("points"),View.Points);Preview->SetBoolField(TEXT("playing"),View.Playing);O->SetObjectField(TEXT("preview"),Preview);
    TArray<TSharedPtr<FJsonValue>> A;
    for(const auto& I:Items())
    {auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("scenario_id"),I.ScenarioId);V->SetStringField(TEXT("run_uuid"),I.RunId);V->SetStringField(TEXT("directory"),I.Directory);V->SetStringField(TEXT("state"),TestSetStateName(I.State));V->SetStringField(TEXT("message"),I.Message);V->SetNumberField(TEXT("frames"),I.Frames);V->SetNumberField(TEXT("completed_frames"),I.CompletedFrames);V->SetNumberField(TEXT("seconds"),I.Seconds);V->SetNumberField(TEXT("attempts"),I.Attempts);
     V->SetStringField(TEXT("source"),I.Example?TEXT("example"):TEXT("received"));V->SetStringField(TEXT("source_uuid"),I.SourceUUID);
     if(I.Example){V->SetNumberField(TEXT("example_seed"),I.Seed);V->SetBoolField(TEXT("representative_frames"),true);TSharedPtr<FJsonObject> Variation;if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(I.VariationJson),Variation)&&Variation)V->SetObjectField(TEXT("example_variation"),Variation);}A.Add(MakeShared<FJsonValueObject>(V));}
    O->SetArrayField(TEXT("scenarios"),A);return O;
}

void UAgentTestSetSubsystem::Pump()
{
    if(!State)return;auto& S=*State;auto* Dataset=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();auto* R=TestSetReplay(GetWorld());
    const double Now=FPlatformTime::Seconds();
    if(S.LastPump>0&&!S.Paused&&!S.Yielded&&!S.Blocked&&(S.Active!=INDEX_NONE||S.Prepare.IsValid()||S.Verify.IsValid()||!S.Spools.IsEmpty()))S.WorkSeconds+=Now-S.LastPump;S.LastPump=Now;
    if(PcdPreview)PcdPreview->Pump(GetWorld());
    // Drain spool results first; no UObject is captured by a worker.
    for(int32 I=S.Spools.Num()-1;I>=0;--I)if(S.Spools[I].Result.IsReady())
    {
        const int32 N=S.Spools[I].Index;const auto Result=S.Spools[I].Result.Get();auto& Input=S.Inputs[N];Input.Stored=Result.Ok;
        if(Result.Ok){Input.Hash=Result.Hash;S.InputBytes-=Input.Bytes;Input.Bytes=0;Input.Data.Reset();}else {S.Items[N].State=EAgentTestSetItemState::Failed;S.Items[N].Message=TEXT("원본 JSON 저장 실패 · 재시도하세요");++S.Revision;}
        S.Spools.RemoveAt(I);
    }
    if(S.Manifest.IsValid()&&S.Manifest.IsReady())
    {S.ManifestFailed=!S.Manifest.Get();S.Manifest=TFuture<bool>();if(!S.ManifestFailed){S.ManifestRevision=S.PendingManifestRevision;S.SavedCompleted=S.PendingManifestCompleted;}else S.Message=TEXT("testset.json 저장 실패 · 폴더 권한·디스크 공간을 확인하세요");}
    if(S.Prepare.IsValid()&&S.Prepare.IsReady()){S.Prepared=S.Prepare.Get();S.Prepare=TFuture<FAgentTestSetPrepared>();}
    if(S.Active!=INDEX_NONE&&!Dataset->OwnsPlannedRun(S.Items[S.Active].RunId))
    {auto& I=S.Items[S.Active];I.State=EAgentTestSetItemState::Failed;I.Message=TEXT("생성 세션 변경 · 새 작업을 보존합니다");S.Active=INDEX_NONE;++S.Revision;}
    if(S.Verify.IsValid()&&S.Verify.IsReady())
    {
        const FString Error=S.Verify.Get();S.Verify=TFuture<FString>();auto& I=S.Items[S.Verifying];
        I.State=Error.IsEmpty()?EAgentTestSetItemState::Completed:EAgentTestSetItemState::Failed;
        I.Message=Error.IsEmpty()?TEXT("생성 완료 · 파일 검증 완료"):Error;if(Error.IsEmpty())++S.GeneratedCount;S.Verifying=INDEX_NONE;++S.Revision;
        if(Completed()==Config.Target)S.Message=TEXT("생성 완료 · 목록에서 점군 보기를 선택하세요");
    }
    if(S.Replace.IsValid()&&S.Replace.IsReady())
    {
        const bool Ok=S.Replace.Get();S.Replace=TFuture<bool>();const int32 N=S.Replacing;S.Replacing=INDEX_NONE;auto& Input=S.Inputs[N];
        if(Ok)
        {
            auto Archived=MakeShared<FJsonObject>();Archived->SetStringField(TEXT("scenario_id"),S.Items[N].ScenarioId);Archived->SetStringField(TEXT("directory"),S.ArchivePath);Archived->SetStringField(TEXT("replaced_by"),Input.Replacement->ScenarioUUID);Archived->SetStringField(TEXT("reason"),TEXT("received scenario takes priority over temporary example"));S.Replaced.Add(MakeShared<FJsonValueObject>(Archived));
            FAgentTestSetItem Item;Item.ScenarioId=Input.Replacement->ScenarioUUID;Item.SourceUUID=Item.ScenarioId;Item.RunId=NewTestSetId();Item.Frames=FMath::DivideAndRoundUp(Input.Replacement->Rows.Num(),Config.SampleStride);Item.Message=TEXT("원본 생성 대기");Item.Directory=S.Root/FString::Printf(TEXT("scenario_%03d_%s"),N+1,*Item.ScenarioId);
            if(Config.AutoFillExamples)Item.Frames=FMath::Min(Item.Frames,Config.ExampleMaxFrames);
            S.Items[N]=MoveTemp(Item);S.InputBytes-=Input.Bytes;Input.Data=Input.Replacement;Input.Hash=Input.Data->OriginalContentSha1;Input.Bytes=Input.ReplacementBytes;Input.Stored=false;Input.Replacement.Reset();Input.ReplacementBytes=0;
            S.Message=TEXT("예시를 원본으로 대체 · 생성 대기");++S.Revision;
        }
        else {S.Message=TEXT("예시 보관 대기 · 결과 폴더 권한을 확인하세요");S.NextReplace=Now+1;++S.Revision;}
    }
    if(!S.Reset&&!S.Replace.IsValid()&&Now>=S.NextReplace)
    {
        for(int32 N=0;N<S.Items.Num();++N)if(S.Inputs[N].Replacement&&S.Active!=N&&S.Verifying!=N&&S.Preparing!=N&&!S.Spools.ContainsByPredicate([N](const auto& P){return P.Index==N;}))
        {
            // No worker or preview still owns the old directory. Rename only inside this set.
            const FString Old=S.Items[N].Directory;S.ArchivePath=S.Root/TEXT("examples_replaced")/FPaths::GetCleanFilename(Old);const FString Archive=S.ArchivePath;
            const FString Id=S.Inputs[N].Replacement->ScenarioUUID;S.Replacing=N;const auto Cancel=S.Cancelled;
            S.Replace=Async(EAsyncExecution::ThreadPool,[Old,Archive,Id,Cancel]()
            {
                if(Cancel->load())return false;
                auto Note=MakeShared<FJsonObject>();Note->SetStringField(TEXT("replaced_by"),Id);Note->SetStringField(TEXT("reason"),TEXT("received scenario replaces temporary example; all example files retained"));
                if(!IFileManager::Get().DirectoryExists(*Old))return SaveTestSetJson(Archive/TEXT("replacement.json"),EncodeTestSetJson(Note));
                if(!SaveTestSetJson(Old/TEXT("replacement.json"),EncodeTestSetJson(Note)))return false;
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(Archive),true);
                return FPlatformFileManager::Get().GetPlatformFile().MoveFile(*Archive,*Old);
            });break;
        }
    }
    if(S.Active!=INDEX_NONE)
    {
        auto& Item=S.Items[S.Active];const int32 Count=Dataset->CompletedScenes();
        if(Item.CompletedFrames!=Count){Item.CompletedFrames=Count;Item.Seconds=FPlatformTime::Seconds()-S.ActiveStarted;++S.Revision;}
        if(!Dataset->IsActive()&&!S.Verify.IsValid())
        {
            Item.Message=Dataset->Message;Item.Seconds=FPlatformTime::Seconds()-S.ActiveStarted;
            if(!Dataset->HasSucceeded())Item.State=EAgentTestSetItemState::Failed;
            else
            {
                S.Verifying=S.Active;const auto Copy=Item;const FString Root=Dataset->Directory();const auto Cancel=S.Cancelled;
                Item.Message=TEXT("프레임 파일 검증 중");
                S.Verify=Async(EAsyncExecution::ThreadPool,[Copy,Root,Cancel]() -> FString
                {
                    int64 Bytes=0;
                    for(int32 F=0;F<Copy.Frames;++F)
                    {
                        if(Cancel->load())return TEXT("검증 중단");
                        const FString Dir=Root/FString::Printf(TEXT("scene_%05d"),F);FString Text;TSharedPtr<FJsonObject> Label;
                        if(!FFileHelper::LoadFileToString(Text,*(Dir/TEXT("label.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Label)||!Label||Label->GetStringField(TEXT("scenario_id"))!=Copy.ScenarioId||Label->GetStringField(TEXT("run_uuid"))!=Copy.RunId)
                            return TEXT("프레임 라벨 검증 실패");
                        const int64 Size=IFileManager::Get().FileSize(*(Dir/TEXT("lidar.pcd")));if(Size<1)return TEXT("PCD 파일 검증 실패");Bytes+=Size;
                    }
                    auto Summary=MakeShared<FJsonObject>();Summary->SetStringField(TEXT("schema_version"),TEXT("ma0t10.testset.scenario.v1"));Summary->SetStringField(TEXT("scenario_id"),Copy.ScenarioId);Summary->SetStringField(TEXT("run_uuid"),Copy.RunId);Summary->SetStringField(TEXT("frames_directory"),Root);Summary->SetNumberField(TEXT("frames"),Copy.Frames);Summary->SetNumberField(TEXT("seconds"),Copy.Seconds);Summary->SetNumberField(TEXT("pcd_bytes"),double(Bytes));Summary->SetStringField(TEXT("state"),TEXT("completed"));
                    if(Cancel->load()||!SaveTestSetJson(Copy.Directory/TEXT("summary.json"),EncodeTestSetJson(Summary)))return TEXT("시나리오 요약 저장 실패");return FString();
                });
            }
            S.Active=INDEX_NONE;++S.Revision;
        }
    }
    const bool OrdinaryBusy=!S.Slab.IsValid()||S.Slab->IsSimulationActive()||TestSetWorldBusy(GetWorld())||(R&&R->IsReplayBusy());
    if(S.Active==INDEX_NONE&&FPlatformTime::Seconds()>=S.NextReadiness)
    {
        S.NextReadiness=FPlatformTime::Seconds()+.2;
        if(!S.Lidar.IsValid())S.Lidar=FindTestSetSensor<AVirtualLidarSensorActor>(GetWorld(),Config.LidarId);
        if(Config.CaptureCamera&&!S.Camera.IsValid())S.Camera=FindTestSetSensor<AVirtualCameraSensorActor>(GetWorld(),Config.CameraId);
        if(S.StopRequested&&!OrdinaryBusy&&!Dataset->IsActive())SuspendBusyWork();
        FString Reason;S.Blocked=BusyReason(Reason);
        const bool Waiting=S.Items.ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Waiting;});
        if(S.Blocked&&Waiting&&S.Message!=Reason)
        {
            S.Message=Reason;for(auto& I:S.Items)if(I.State==EAgentTestSetItemState::Waiting)I.Message=Reason;++S.Revision;
            UE_LOG(LogMA0T10,Log,TEXT("[TestSet][Waiting] %s"),*Reason);
        }
    }
    const bool Busy=OrdinaryBusy||(S.Active==INDEX_NONE&&S.Blocked);
    if(S.Yielded&&!Busy){S.Yielded=false;++S.Revision;}
    if((Busy||S.Paused)&&S.Active!=INDEX_NONE)Dataset->YieldPlannedCapture();
    if(S.Reset)
    {
        if(S.Active!=INDEX_NONE||S.Prepare.IsValid()||S.Verify.IsValid()||!S.Spools.IsEmpty()||S.Manifest.IsValid()||S.Replace.IsValid())return;
        // Persist the abandoned set and wait for the last write before detaching it.
        if(!S.ResetManifestQueued)
        {
            const auto Snapshot=StatusJson();const FString Path=S.Root/TEXT("testset.json");S.ResetManifestQueued=true;
            S.Manifest=Async(EAsyncExecution::ThreadPool,[Snapshot,Path](){return SaveTestSetJson(Path,EncodeTestSetJson(Snapshot));});return;
        }
        S.Cancelled->store(true);
        RestoreStoppedWork();State.Reset();GetWorld()->GetTimerManager().ClearTimer(Timer);return;
    }
    for(int32 I=0;I<S.Items.Num()&&S.Spools.Num()<2;++I)
        if(!S.Inputs[I].Stored&&(S.Inputs[I].Data||(S.Items[I].Example&&S.Inputs[0].Stored))&&!S.Inputs[I].Replacement&&S.Items[I].State==EAgentTestSetItemState::Waiting&&!S.Spools.ContainsByPredicate([I](const auto& V){return V.Index==I;}))
        {
            const auto Data=S.Inputs[I].Data;const auto Item=S.Items[I];const FString Path=Item.Directory/TEXT("original.json"),TemplatePath=S.Inputs[I].TemplatePath,TemplateHash=S.Inputs[I].TemplateHash;const auto Cancel=S.Cancelled;
            S.Spools.Add({I,Async(EAsyncExecution::ThreadPool,[Data,Item,Path,Cancel,TemplatePath,TemplateHash]()
            {
                FTestSetStored Result;FSlabScenarioDataPtr Generated=Data;FString Error;
                if(!Generated)
                {
                    FString Json;if(!FFileHelper::LoadFileToString(Json,*TemplatePath)||!FSlabScenarioCodec::Parse(Json,Generated,Error,true)||Generated->OriginalContentSha1!=TemplateHash)return Result;
                }
                const auto Source=Generated;
                if(Cancel->load()||(Item.Example&&!AgentTestSetExamples::Generate(*Source,Item.ScenarioId,Item.VariationJson,Generated,Error)))return Result;
                Result.Ok=SaveTestSetJson(Path,Generated->OriginalJson);Result.Hash=Generated->OriginalContentSha1;return Result;
            })});
        }
    if(!Busy&&!S.Paused&&S.Active!=INDEX_NONE&&Dataset->IsPaused()&&!S.Slab->HasPendingScenarioInput()&&FPlatformTime::Seconds()>=S.NextReadiness)
    {
        S.NextReadiness=FPlatformTime::Seconds()+.2;
        if(S.StopRequested)SuspendBusyWork();FString Error;
        if(!BusyReason(Error)){if(!Dataset->Resume(Error)){Dataset->Message=Error;Dataset->Stop();}}
        else if(S.Message!=Error){S.Message=Error;S.Items[S.Active].Message=Error;++S.Revision;}
    }
    if(S.Prepared.IsSet()&&!Busy&&!S.Paused&&S.Active==INDEX_NONE&&!Dataset->IsActive()&&!S.Slab->HasPendingScenarioInput())
    {
        const int32 Index=S.Preparing;auto Prepared=MoveTemp(S.Prepared.GetValue());S.Prepared.Reset();S.Preparing=INDEX_NONE;
        if(S.Items.IsValidIndex(Index)&&!S.Inputs[Index].Replacement&&S.Items[Index].State==EAgentTestSetItemState::Waiting)
        {
            auto& Item=S.Items[Index];FString Error=Prepared.Error;
            auto* Lidar=S.Lidar.Get();auto* Camera=Config.CaptureCamera?S.Camera.Get():nullptr;
            if(Prepared.Plan&&BusyReason(Error))
            {S.Blocked=true;S.Message=Item.Message=Error;S.Prepared=MoveTemp(Prepared);S.Preparing=Index;++S.Revision;}
            else if(Prepared.Plan&&S.Slab.IsValid()&&Prepared.Configuration!=S.Slab->GetMetricsConfigurationHash())
            {S.Message=TEXT("기준 배치 변경 · 프레임 다시 준비 중");}
            else
            {
                FVirtualDatasetSettings Settings;Settings.Enabled=true;Settings.Name=TEXT("agent_testset");Settings.Scenes=Item.Frames;Settings.SettleFrames=Config.SettleFrames;Settings.CameraTruth=Config.CameraTruth;Settings.IncludeImu=Config.IncludeImu;Settings.LidarFidelity=Config.LidarFidelity;Settings.ExportFormat=Config.ExportFormat;Settings.ExportUprightEnvelope=Config.ExportUprightEnvelope;
                ++Item.Attempts;
                if(Prepared.Plan&&Dataset->Start(Settings,S.Slab.Get(),Lidar,Camera,nullptr,Error,Prepared.Plan))
                {S.Active=Index;S.ActiveStarted=FPlatformTime::Seconds();Item.State=EAgentTestSetItemState::Generating;Item.Message=TEXT("생성 중");S.Message=TEXT("테스트셋 생성 중");}
                else {Item.State=EAgentTestSetItemState::Failed;Item.Message=Error.IsEmpty()?TEXT("시나리오 준비 실패"):Error;S.Message=Item.Message;}
                ++S.Revision;
            }
        }
    }
    // One immutable prefetch overlaps the active capture/writer pipeline. Disk JSON is parsed on a worker.
    if(!S.Prepare.IsValid()&&!S.Prepared.IsSet()&&S.Slab.IsValid())
    {
        int32 Index=INDEX_NONE;
        if(Index==INDEX_NONE)for(int32 I=0;I<S.Items.Num();++I)if(!S.Inputs[I].Replacement&&S.Items[I].State==EAgentTestSetItemState::Waiting&&S.Inputs[I].Stored){Index=I;break;}
        if(Index!=INDEX_NONE)
        {
            S.Preparing=Index;const auto Item=S.Items[Index];const FString Path=Item.Directory/TEXT("original.json"),SourceHash=S.Inputs[Index].Hash;const auto Track=S.Slab->GetDatasetTrackTransform();const auto Dimension=S.Slab->DimensionUnit,Position=S.Slab->PositionUnit;const auto ConfigCopy=Config;const uint32 Hash=S.Slab->GetMetricsConfigurationHash();
            S.Prepare=Async(EAsyncExecution::ThreadPool,[Item,Path,SourceHash,Track,Dimension,Position,ConfigCopy,Hash]()
            {
                FAgentTestSetPrepared Out;Out.Configuration=Hash;FString Json;
                if(!FFileHelper::LoadFileToString(Json,*Path)||!FSlabScenarioCodec::Parse(Json,Out.Data,Out.Error,true))return Out;
                if(Out.Data->OriginalContentSha1!=SourceHash){Out.Error=TEXT("보관된 원본 JSON이 변경되었습니다");Out.Data.Reset();return Out;}
                if(Out.Data->ScenarioUUID!=Item.ScenarioId)
                {
                    if(!Out.Data->bGeneratedArchiveId){Out.Error=TEXT("보관된 시나리오 ID가 다릅니다");Out.Data.Reset();return Out;}
                    // Match the original admission's generated archive ID without rewriting raw JSON.
                    auto Stable=MakeShared<FSlabScenarioData,ESPMode::ThreadSafe>(*Out.Data);Stable->ScenarioUUID=Item.ScenarioId;Out.Data=Stable;
                }
                Out.Plan=MakeShared<FVirtualDatasetFramePlan,ESPMode::ThreadSafe>();auto& Plan=*Out.Plan;Plan.Root=Item.Directory/(TEXT("run_")+Item.RunId);Plan.ScenarioId=Item.ScenarioId;Plan.RunId=Item.RunId;Plan.CaptureCamera=ConfigCopy.CaptureCamera;Plan.Pipeline=ConfigCopy.Pipeline;Plan.SlabConfiguration=Hash;
                Plan.Source=Item.Example?TEXT("example"):TEXT("received");Plan.SourceUUID=Item.SourceUUID;Plan.ExampleSeed=Item.Seed;Plan.ExampleVariation=Item.VariationJson;
                Plan.FrameCap=ConfigCopy.AutoFillExamples?ConfigCopy.ExampleMaxFrames:0;Plan.SampleStride=ConfigCopy.SampleStride;
                // Match live playback's first-row dimension policy, with identical BuildPose and units.
                const auto& First=Out.Data->Rows[0];const FVector Size(FSlabScenarioCodec::ToCm(First.Length,Dimension),FSlabScenarioCodec::ToCm(First.Width,Dimension),FSlabScenarioCodec::ToCm(First.Thickness,Dimension));
                if(Size.ContainsNaN()||Size.GetMin()<=0||Size.GetMax()>1000000){Out.Error=TEXT("Slab 치수가 지원 범위를 벗어났습니다");Out.Plan.Reset();return Out;}
                for(int32 I:AgentTestSetExamples::SampleRows(Out.Data->Rows.Num(),ConfigCopy.SampleStride,Plan.FrameCap))
                {const auto& Row=Out.Data->Rows[I];Plan.Frames.Add({USlabMotionComponent::BuildPose(Row,Track,Size,Position),Size,Row.FrameNo,Row.ElapsedSec,Row.MtlNo});}
                return Out;
            });
        }
    }
    if(S.ManifestRevision!=S.Revision&&!S.Manifest.IsValid())
    {
        const auto Snapshot=StatusJson();Snapshot->SetBoolField(TEXT("complete"),Completed()==Config.Target&&!S.Inputs.ContainsByPredicate([](const auto& I){return bool(I.Replacement);}));const FString Path=S.Root/TEXT("testset.json");const auto Cancel=S.Cancelled;S.PendingManifestRevision=S.Revision;S.PendingManifestCompleted=Completed();
        S.Manifest=Async(EAsyncExecution::ThreadPool,[Snapshot,Path,Cancel](){return !Cancel->load()&&SaveTestSetJson(Path,EncodeTestSetJson(Snapshot));});
    }
    // Terminal/idle sets have no service timer. New input, retry or UI commands re-arm it.
    if(S.Active==INDEX_NONE&&!S.Prepare.IsValid()&&!S.Verify.IsValid()&&!S.Prepared.IsSet()&&S.Spools.IsEmpty()&&!S.Manifest.IsValid()&&!S.Replace.IsValid()&&!S.Inputs.ContainsByPredicate([](const auto& I){return bool(I.Replacement);})&&S.Preview==INDEX_NONE)
    {
        const bool Waiting=S.Items.ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Waiting;});
        if(!Waiting)
        {
            RestoreStoppedWork();S.StopRequested=false;
            if(!PcdPreview||!PcdPreview->NeedsPump())GetWorld()->GetTimerManager().ClearTimer(Timer);
        }
    }
}
void UAgentTestSetSubsystem::Deinitialize()
{
    StopPreview();RestoreStoppedWork(false);if(State){State->Cancelled->store(true);if(State->Active!=INDEX_NONE)if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())if(D->OwnsPlannedRun(State->Items[State->Active].RunId))D->Stop();}
    GetWorld()->GetTimerManager().ClearTimer(Timer);State.Reset();Super::Deinitialize();
}
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool UAgentTestSetSubsystem::ConfigureForTests(const FAgentTestSetSettings& In){if(HasItems())return false;Config=In;return true;}
#endif
