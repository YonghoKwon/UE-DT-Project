#include "AgentTestSetSubsystem.h"
#include "VirtualDatasetSubsystem.h"
#include "AgentTestSetPcdPreview.h"
#include "AgentTestSetExamples.h"
#include "AgentTestSetTrigger.h"
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
struct FTestSetStored {bool Ok=false;FString Hash,Error;};
struct FTestSetTemplate {FSlabScenarioDataPtr Data;FString Error;ESlabInputUnit Dimension,Position;};
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
template<class T> T* FindTestSetSensor(UWorld* W,const FString& Explicit,bool Demo,const TArray<FString>& Preferred,FString& Rule,FString& Error)
{
    TArray<T*> Candidates;TArray<FString> Ids;
    for(TActorIterator<T> It(W);It;++It)if(!It->IsActorBeingDestroyed()){Candidates.Add(*It);Ids.Add(It->GetSensorId());}
    Ids.Sort();const FString Available=FString::Join(Ids,TEXT(", "));
    auto Unique=[&](const FString& Id)->T*
    {T* Found=nullptr;for(auto* A:Candidates)if(A->GetSensorId().Equals(Id,ESearchCase::CaseSensitive)){if(Found){Error=TEXT("중복 센서 ID · ")+Id+TEXT(" · 각 센서에 고유 ID를 지정하세요");return nullptr;}Found=A;}return Found;};
    if(!Explicit.IsEmpty())
    {
        if(auto* A=Unique(Explicit)){Rule=TEXT("설정 ID");return A;}
        if(Error.IsEmpty())Error=TEXT("설정 센서 ID를 찾을 수 없습니다 · ")+Explicit+TEXT(" · 사용 가능: ")+Available;return nullptr;
    }
    if(Candidates.Num()==1&&!Ids[0].IsEmpty()){Rule=TEXT("단일 센서");return Candidates[0];}
    if(Demo)
    {
        for(const auto& Id:Preferred)if(Ids.Contains(Id))
        {auto* A=Unique(Id);if(A)Rule=TEXT("시연 기본 선택 · ")+Id;return A;}
        for(const auto& Id:Ids)if(!Id.IsEmpty())
        {auto* A=Unique(Id);if(A)Rule=TEXT("시연 ID 오름차순");return A;}
    }
    Error=TEXT("대상 센서를 결정할 수 없습니다 · 대상 센서 ID를 지정하세요 · 사용 가능: ")+Available;return nullptr;
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
    double RetainedNoticeUntil=0;
    double CompletedAt=0;
    int32 DuplicateTriggers=0;
    int32 GeneratedCount=0;
    TFuture<FAgentTestSetPrepared> Prepare;
    TOptional<FAgentTestSetPrepared> Prepared;
    TFuture<bool> Manifest;
    TFuture<FString> Verify;
    TFuture<FTestSetTemplate> Template;
    FString TemplateHash,TemplateError;
    FString LidarRule,CameraRule;
    double NextManifestRetry=0;
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
    GConfig->GetString(Section,TEXT("TemplatePath"),S.TemplatePath,GGameIni);
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
    const FString Key=Trigger.TrimStartAndEnd();return !Data.Rows.IsEmpty()&&!Key.IsEmpty()&&Data.Rows[0].MtlNo.TrimStartAndEnd().Equals(Key,ESearchCase::CaseSensitive);
}
bool UAgentTestSetSubsystem::IsTrigger(const FSlabScenarioData& Data) const {return Config.Enabled&&Matches(Data,Config.Trigger);}
bool UAgentTestSetSubsystem::IsDemoTrigger(const FString& Material) const
{return Config.Enabled&&Config.AutoFillExamples&&!Config.Trigger.IsEmpty()&&Material.TrimStartAndEnd().Equals(Config.Trigger,ESearchCase::CaseSensitive);}
bool UAgentTestSetSubsystem::EnqueueDemoTrigger(const FString& Material,ASlabActor* Slab,FString& Error)
{
    if(!IsDemoTrigger(Material)||!IsValid(Slab)||Slab->GetWorld()!=GetWorld()){Error=TEXT("테스트셋 트리거와 대상 Slab을 확인하세요");return false;}
    if(State)
    {
        if(State->Reset){Error=TEXT("세트 초기화 정리 중 · 완료 후 다시 요청하세요");return false;}
        State->RetainedNoticeUntil=FPlatformTime::Seconds()+2;++State->DuplicateTriggers;
        UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][DuplicateTrigger] 기존 테스트셋 유지 · set=%s count=%d"),*State->SetId,State->DuplicateTriggers);
        ++State->Revision;EnsurePump();return true;
    }
    State=MakeShared<FAgentTestSetState>();auto& S=*State;S.SetId=NewTestSetId();S.Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TestSets")/S.SetId);S.Slab=Slab;S.Started=FPlatformTime::Seconds();
    S.Message=TEXT("테스트셋 생성 요청 수신 · 생성 중");
    S.StopRequested=true;
    UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][Trigger] set=%s mode=demo target=%d root=%s"),*S.SetId,Config.Target,*S.Root);
    // Reserve every slot immediately; no received source or received replacement exists in demo mode.
    for(int32 N=0;N<Config.Target;++N)
    {
        FAgentTestSetItem Item;Item.Example=true;Item.ScenarioId=NewTestSetId();Item.RunId=NewTestSetId();Item.Message=TEXT("생성 준비 중");
        Item.Directory=S.Root/FString::Printf(TEXT("scenario_%03d_%s"),N+1,*Item.ScenarioId);S.Items.Add(MoveTemp(Item));S.Inputs.AddDefaulted();
    }
    LoadTemplate();EnsurePump();return true;
}
void UAgentTestSetSubsystem::LoadTemplate()
{
    auto& S=*State;auto* Slab=S.Slab.Get();if(!Slab)return;
    S.TemplateError.Reset();
    const FString Path=FPaths::IsRelative(Config.TemplatePath)?FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Config.TemplatePath):Config.TemplatePath;
    const FString Key=Config.Trigger,Snapshot=S.Root/TEXT("template.json");const auto D=Slab->DimensionUnit,P=Slab->PositionUnit;const auto Cancel=S.Cancelled;
    S.Template=Async(EAsyncExecution::ThreadPool,[Path,Key,Snapshot,D,P,Cancel]()
    {
        FTestSetTemplate Out;Out.Dimension=D;Out.Position=P;if(!Cancel->load()&&AgentTestSetTrigger::LoadTemplate(Path,Key,D,P,Out.Data,Out.Error))
            if(Cancel->load()||!SaveTestSetJson(Snapshot,Out.Data->OriginalJson)){Out.Data.Reset();Out.Error=TEXT("기본 시나리오 템플릿 보관 실패");}
        if(!Out.Error.IsEmpty())Out.Error+=TEXT(" · ")+Path;
        return Out;
    });
}
bool UAgentTestSetSubsystem::Enqueue(FSlabScenarioDataPtr Data,ASlabActor* Slab,FString& Error)
{
    if(!Data||!IsTrigger(*Data)||!IsValid(Slab)||Slab->GetWorld()!=GetWorld()){Error=TEXT("테스트셋 입력과 대상 Slab을 확인하세요");return false;}
    if(Config.AutoFillExamples)return EnqueueDemoTrigger(Data->Rows[0].MtlNo,Slab,Error);
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
        S.SavedCompleted=0;S.CompletedAt=0;S.Message=TEXT("새 시나리오 수신 · 목록 갱신 중");++S.Revision;EnsurePump();return true;
    }
    Item.SourceUUID=Data->ScenarioUUID;
    Item.Directory=S.Root/FString::Printf(TEXT("scenario_%03d_%s"),S.Items.Num()+1,*Item.ScenarioId);Item.Message=TEXT("대기");
    S.Items.Add(MoveTemp(Item));S.Inputs.Add({Data,Data->OriginalContentSha1,Bytes,false});S.InputBytes+=Bytes;
    S.Message=TEXT("테스트셋 시나리오 대기");++S.Revision;EnsurePump();return true;
}
void UAgentTestSetSubsystem::EnsurePump(){GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.01f,true);}
bool UAgentTestSetSubsystem::HasItems() const{return State&&!State->Items.IsEmpty();}
const TArray<FAgentTestSetItem>& UAgentTestSetSubsystem::Items() const{static const TArray<FAgentTestSetItem> Empty;return State?State->Items:Empty;}
int32 UAgentTestSetSubsystem::Completed() const{int32 N=0;for(const auto& I:Items())N+=I.State==EAgentTestSetItemState::Completed;return N;}
bool UAgentTestSetSubsystem::IsComplete() const{return State&&!State->Reset&&!State->Replace.IsValid()&&!State->Inputs.ContainsByPredicate([](const auto& I){return bool(I.Replacement);})&&Completed()==Config.Target&&!State->ManifestFailed&&State->SavedCompleted==Config.Target;}
double UAgentTestSetSubsystem::ScenariosPerSecond() const{return State&&State->WorkSeconds>.01?State->GeneratedCount/State->WorkSeconds:0;}
double UAgentTestSetSubsystem::TotalSeconds() const
{return State?FMath::Max(0.,(State->CompletedAt>0?State->CompletedAt:FPlatformTime::Seconds())-State->Started):0;}
double UAgentTestSetSubsystem::RemainingSeconds() const
{
    const double Rate=ScenariosPerSecond();if(!State||Rate<=0)return -1;
    int32 Remaining=Config.Target-Completed();
    for(int32 N=0;N<State->Inputs.Num();++N)if(State->Inputs[N].Replacement&&State->Items[N].State==EAgentTestSetItemState::Completed)++Remaining;
    return Remaining/Rate;
}
bool UAgentTestSetSubsystem::IsPaused() const{return State&&(State->Paused||State->Yielded);}
FString UAgentTestSetSubsystem::Directory() const{return State?State->Root:FString();}
FString UAgentTestSetSubsystem::Message() const
{return State&&FPlatformTime::Seconds()<State->RetainedNoticeUntil?TEXT("기존 테스트셋 유지 · 새로 생성하려면 세트를 초기화하세요"):PresentationMessage();}
FString UAgentTestSetSubsystem::PresentationMessage() const
{return State?(State->ManifestFailed?TEXT("testset.json 저장 실패 · 파일은 보존됨 · 폴더 권한·디스크 공간 확인 후 목록 저장 재시도 · ")+State->Root/TEXT("testset.json"):State->Message):TEXT("시나리오 대기");}
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
    if(State&&!State->Reset&&State->ManifestFailed&&Index==INDEX_NONE)
    {State->NextManifestRetry=0;State->Message=TEXT("testset.json 저장 재시도 중");++State->Revision;EnsurePump();return true;}
    if(!State||State->Reset||!State->Items.IsValidIndex(Index)||State->Items[Index].State!=EAgentTestSetItemState::Failed){Error=TEXT("실패한 시나리오를 선택하세요");return false;}
    auto& S=*State;auto& I=S.Items[Index];
    if(!ResolveSensors(Error))
    {S.Message=I.Message=Error;++S.Revision;UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][RetryRejected] item=%d reason=%s"),Index+1,*Error);return false;}
    if(!S.TemplateError.IsEmpty()&&!FPaths::FileExists(FPaths::IsRelative(Config.TemplatePath)?FPaths::ProjectDir()/Config.TemplatePath:Config.TemplatePath))
    {Error=S.TemplateError;S.Message=I.Message=Error;++S.Revision;UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][RetryRejected] item=%d reason=%s"),Index+1,*Error);return false;}
    FString WaitingReason;BusyReason(WaitingReason);
    if(!Config.AutoFillExamples&&!WaitingReason.IsEmpty())
    {Error=WaitingReason;S.Message=I.Message=Error;++S.Revision;return false;}
    // Never reuse a prefetch made for the previous RunUUID.
    if(S.Preparing==Index){Error=TEXT("이전 항목 준비 정리 중 · 잠시 후 재시도하세요");S.Message=I.Message=Error;++S.Revision;return false;}
    I.State=EAgentTestSetItemState::Waiting;I.RunId=NewTestSetId();I.CompletedFrames=0;I.Seconds=0;I.Message=S.Message=TEXT("재시도 대기");S.CompletedAt=0;S.NextReadiness=0;
    if(!WaitingReason.IsEmpty())I.Message=S.Message=WaitingReason;
    if(Config.AutoFillExamples)S.StopRequested=true;
    if(!S.TemplateError.IsEmpty())LoadTemplate();
    UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][Retry] item=%d run=%s"),Index+1,*I.RunId);
    ++S.Revision;EnsurePump();return true;
}
bool UAgentTestSetSubsystem::HasManifestFailure() const{return State&&State->ManifestFailed;}
FString UAgentTestSetSubsystem::SensorSelectionDescription() const
{
    if(!State)return FString();const auto& S=*State;
    return TEXT("LiDAR: ")+(S.Lidar.IsValid()?S.Lidar->GetSensorId():TEXT("미결정"))+TEXT(" · ")+S.LidarRule+
        (Config.CaptureCamera?TEXT("\nCamera: ")+(S.Camera.IsValid()?S.Camera->GetSensorId():TEXT("미결정"))+TEXT(" · ")+S.CameraRule:FString());
}
bool UAgentTestSetSubsystem::ResolveSensors(FString& Error)
{
    auto& S=*State;const FString Before=SensorSelectionDescription();
    if(S.Lidar.IsValid()&&!Config.LidarId.IsEmpty()&&S.Lidar->GetSensorId()!=Config.LidarId)S.Lidar.Reset();
    TArray<FString> Preferred=S.Slab.IsValid()?S.Slab->TargetSensorIds:TArray<FString>();Preferred.Sort();
    Preferred.AddUnique(TEXT("Hatch.MLX80"));Preferred.AddUnique(TEXT("SLAB-LIDAR-001"));
    if(!S.Lidar.IsValid())S.Lidar=FindTestSetSensor<AVirtualLidarSensorActor>(GetWorld(),Config.LidarId,Config.AutoFillExamples,Preferred,S.LidarRule,Error);
    if(!S.Lidar.IsValid())return false;
    if(Config.CaptureCamera&&!S.Camera.IsValid())S.Camera=FindTestSetSensor<AVirtualCameraSensorActor>(GetWorld(),Config.CameraId,Config.AutoFillExamples,{},S.CameraRule,Error);
    if(Config.CaptureCamera&&!S.Camera.IsValid())return false;
    if(Before!=SensorSelectionDescription())UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][Sensors] %s"),*SensorSelectionDescription());
    return true;
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
    if(!ResolveSensors(Error)){State->Message=Error;++State->Revision;return false;}
    UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][StopAndStart] set=%s"),*State->SetId);
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
        const auto& Held=S.Stopped.Last();
        UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][Suspend] sensor=%s measurement=%d publisher=%d recorder=%d periodic=%d"),*Held.SensorId,Held.Running,Held.PublisherHeld,Held.RecorderHeld,Held.Periodic.Num());
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
        UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][Restore] sensor=%s resume=%d measurement=%d periodic=%d"),*W.SensorId,Resume,A&&A->IsSensorRunning(),W.Periodic.Num());
    }
}
TSharedRef<FJsonObject> UAgentTestSetSubsystem::StatusJson() const
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("schema_version"),TEXT("ma0t10.testset.v1"));O->SetStringField(TEXT("trigger_mtl_no"),Config.Trigger);O->SetNumberField(TEXT("target_scenarios"),Config.Target);
    O->SetBoolField(TEXT("enabled"),Config.Enabled);O->SetBoolField(TEXT("pipeline"),Config.Pipeline);O->SetBoolField(TEXT("paused"),IsPaused());O->SetBoolField(TEXT("reset"),State&&State->Reset);
    O->SetNumberField(TEXT("completed_scenarios"),Completed());O->SetBoolField(TEXT("complete"),IsComplete());O->SetStringField(TEXT("directory"),Directory());
    const FString BuildCommit=TEXT(MA0T10_PROJECT_COMMIT);
    O->SetStringField(TEXT("project_commit"),BuildCommit.IsEmpty()?TEXT("unknown"):BuildCommit);O->SetStringField(TEXT("project_commit_source"),BuildCommit.IsEmpty()?TEXT("unavailable"):TEXT("build"));O->SetStringField(TEXT("build"),FApp::GetBuildVersion());O->SetBoolField(TEXT("sending_supported"),false);O->SetStringField(TEXT("message"),Message());
    O->SetNumberField(TEXT("duplicate_triggers"),State?State->DuplicateTriggers:0);
    if(State&&State->DuplicateTriggers>0)O->SetStringField(TEXT("duplicate_message"),TEXT("기존 테스트셋 유지 · 새로 생성하려면 세트를 초기화하세요"));
    O->SetNumberField(TEXT("total_seconds"),TotalSeconds());
    O->SetBoolField(TEXT("capture_camera"),Config.CaptureCamera);O->SetBoolField(TEXT("camera_truth"),Config.CameraTruth);O->SetBoolField(TEXT("include_imu"),Config.IncludeImu);O->SetNumberField(TEXT("sample_stride"),Config.SampleStride);
    O->SetStringField(TEXT("export_format"),Config.ExportFormat);O->SetBoolField(TEXT("lidar_fidelity"),Config.LidarFidelity);
    O->SetBoolField(TEXT("auto_fill_examples"),Config.AutoFillExamples);O->SetNumberField(TEXT("example_max_frames"),Config.ExampleMaxFrames);O->SetNumberField(TEXT("example_seed"),Config.ExampleSeed);
    O->SetStringField(TEXT("template_path"),Config.TemplatePath);O->SetStringField(TEXT("template_sha1"),State?State->TemplateHash:FString());
    O->SetStringField(TEXT("sensor_selection"),SensorSelectionDescription());
    O->SetStringField(TEXT("lidar_sensor_id"),State&&State->Lidar.IsValid()?State->Lidar->GetSensorId():FString());
    O->SetStringField(TEXT("camera_sensor_id"),State&&State->Camera.IsValid()?State->Camera->GetSensorId():FString());
    O->SetBoolField(TEXT("manifest_failed"),HasManifestFailure());
    O->SetStringField(TEXT("frame_policy"),Config.AutoFillExamples?TEXT("uniform_capped_stride_samples"):TEXT("full_stride_samples"));
    O->SetStringField(TEXT("example_purpose"),TEXT("demonstration representative frames; synthetic examples, not received training data"));
    O->SetNumberField(TEXT("scenarios_per_second"),ScenariosPerSecond());O->SetNumberField(TEXT("remaining_seconds"),RemainingSeconds());
    int32 Received=0,Examples=0,Pending=0;for(const auto& I:Items()){Received+=!I.Example;Examples+=I.Example;}
    if(State){for(const auto& I:State->Inputs)Pending+=bool(I.Replacement);O->SetArrayField(TEXT("replaced_examples"),State->Replaced);}
    O->SetNumberField(TEXT("received_scenarios"),Received);O->SetNumberField(TEXT("example_scenarios"),Examples);O->SetNumberField(TEXT("pending_received_replacements"),Pending);
    O->SetStringField(TEXT("replacement_policy"),Config.AutoFillExamples?TEXT("ignore repeated triggers; retain existing set until explicit reset; incoming scenario fields unused"):TEXT("validated received scenarios only"));
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
    if(S.Template.IsValid()&&S.Template.IsReady())
    {
        const auto Result=S.Template.Get();S.Template=TFuture<FTestSetTemplate>();S.TemplateError=Result.Error;
        if(Result.Data)
        {
            S.TemplateHash=Result.Data->OriginalContentSha1;const int32 Seed=AgentTestSetExamples::SeedFor(S.TemplateHash,Config.ExampleSeed);
            for(int32 N=0;N<S.Items.Num();++N)
            {
                auto& I=S.Items[N];I.SourceUUID=Result.Data->ScenarioUUID;I.Seed=Seed;I.VariationJson=AgentTestSetExamples::Variation(Seed,N+1,Result.Dimension,Result.Position);
                I.Frames=FMath::Min(FMath::DivideAndRoundUp(Result.Data->Rows.Num(),Config.SampleStride),Config.ExampleMaxFrames);
                S.Inputs[N].TemplatePath=S.Root/TEXT("template.json");S.Inputs[N].TemplateHash=S.TemplateHash;
            }
        }
        else {S.Message=S.TemplateError;for(auto& I:S.Items){I.State=EAgentTestSetItemState::Failed;I.Message=S.Message;}}
        ++S.Revision;
    }
    if(S.LastPump>0&&!S.Paused&&!S.Yielded&&!S.Blocked&&(S.Active!=INDEX_NONE||S.Prepare.IsValid()||S.Verify.IsValid()||!S.Spools.IsEmpty()))S.WorkSeconds+=Now-S.LastPump;S.LastPump=Now;
    if(PcdPreview)PcdPreview->Pump(GetWorld());
    // Drain spool results first; no UObject is captured by a worker.
    for(int32 I=S.Spools.Num()-1;I>=0;--I)if(S.Spools[I].Result.IsReady())
    {
        const int32 N=S.Spools[I].Index;const auto Result=S.Spools[I].Result.Get();auto& Input=S.Inputs[N];Input.Stored=Result.Ok;
        if(Result.Ok){Input.Hash=Result.Hash;S.InputBytes-=Input.Bytes;Input.Bytes=0;Input.Data.Reset();}else {S.Items[N].State=EAgentTestSetItemState::Failed;S.Message=S.Items[N].Message=Result.Error;UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][SourceFailed] item=%d reason=%s"),N+1,*Result.Error);++S.Revision;}
        S.Spools.RemoveAt(I);
    }
    if(S.Manifest.IsValid()&&S.Manifest.IsReady())
    {
        const bool WasFailed=S.ManifestFailed;S.ManifestFailed=!S.Manifest.Get();S.Manifest=TFuture<bool>();
        if(!S.ManifestFailed){S.ManifestRevision=S.PendingManifestRevision;S.SavedCompleted=S.PendingManifestCompleted;if(WasFailed){S.Message=TEXT("testset.json 저장 복구 완료");++S.Revision;}}
        else
        {
            S.Message=TEXT("testset.json 저장 실패 · 파일은 보존됨 · 폴더 권한·디스크 공간 확인 후 목록 저장 재시도");S.NextManifestRetry=Now+2;
            if(!WasFailed){++S.Revision;UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][ManifestFailed] path=%s"),*(S.Root/TEXT("testset.json")));}
        }
    }
    // Presentation timing only: no change to completion/admission or capture scheduling.
    if(S.CompletedAt==0&&IsComplete()){S.CompletedAt=Now;++S.Revision;}
    if(S.Prepare.IsValid()&&S.Prepare.IsReady()){S.Prepared=S.Prepare.Get();S.Prepare=TFuture<FAgentTestSetPrepared>();}
    if(S.Active!=INDEX_NONE&&!Dataset->OwnsPlannedRun(S.Items[S.Active].RunId))
    {auto& I=S.Items[S.Active];I.State=EAgentTestSetItemState::Failed;I.Message=TEXT("생성 세션 변경 · 새 작업을 보존합니다");S.Active=INDEX_NONE;++S.Revision;}
    if(S.Verify.IsValid()&&S.Verify.IsReady())
    {
        const FString Error=S.Verify.Get();S.Verify=TFuture<FString>();auto& I=S.Items[S.Verifying];
        I.State=Error.IsEmpty()?EAgentTestSetItemState::Completed:EAgentTestSetItemState::Failed;
        I.Message=Error.IsEmpty()?TEXT("생성 완료 · 파일 검증 완료"):Error;if(Error.IsEmpty())++S.GeneratedCount;S.Verifying=INDEX_NONE;++S.Revision;
        if(!Error.IsEmpty()){S.Message=I.Message;UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][VerificationFailed] run=%s reason=%s"),*I.RunId,*Error);}
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
            S.Message=TEXT("시나리오 교체 · 생성 대기");++S.Revision;
        }
        else {S.Message=TEXT("이전 결과 보관 대기 · 결과 폴더 권한을 확인하세요");S.NextReplace=Now+1;++S.Revision;}
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
            if(!Dataset->HasSucceeded())
            {Item.State=EAgentTestSetItemState::Failed;S.Message=Item.Message;UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][CaptureFailed] run=%s root=%s reason=%s"),*Item.RunId,*Dataset->Directory(),*Item.Message);}
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
                        const FString LabelPath=Dir/TEXT("label.json"),PcdPath=Dir/TEXT("lidar.pcd");FString Scenario,Run;
                        if(!FFileHelper::LoadFileToString(Text,*LabelPath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Label)||!Label)
                            return TEXT("프레임 라벨 읽기/JSON 검증 실패 · ")+LabelPath;
                        if(!Label->TryGetStringField(TEXT("scenario_id"),Scenario)||!Label->TryGetStringField(TEXT("run_uuid"),Run)||Scenario!=Copy.ScenarioId||Run!=Copy.RunId)
                            return TEXT("프레임 라벨 ID 불일치 · ")+LabelPath+TEXT(" · expected=")+Copy.ScenarioId+TEXT("/")+Copy.RunId+TEXT(" actual=")+Scenario+TEXT("/")+Run;
                        const int64 Size=IFileManager::Get().FileSize(*PcdPath);TArray<uint8> Pcd;
                        if(Size<1||Size>16ll*1024*1024||!FFileHelper::LoadFileToArray(Pcd,*PcdPath))return TEXT("PCD 읽기/크기 검증 실패 · ")+PcdPath+FString::Printf(TEXT(" · bytes=%lld"),Size);
                        static constexpr ANSICHAR Marker[]="DATA binary\n";int32 Offset=INDEX_NONE;
                        for(int32 B=0;B+sizeof(Marker)-1<=FMath::Min(Pcd.Num(),65536);++B)if(!FMemory::Memcmp(Pcd.GetData()+B,Marker,sizeof(Marker)-1)){Offset=B+sizeof(Marker)-1;break;}
                        if(Offset==INDEX_NONE)return TEXT("PCD binary 헤더 없음 · ")+PcdPath;
                        FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Pcd.GetData()),Offset);const FString Header(Utf8.Length(),Utf8.Get());TArray<FString> Lines;Header.ParseIntoArrayLines(Lines);int32 Points=-1;
                        for(const auto& Line:Lines)if(Line.StartsWith(TEXT("POINTS ")))LexTryParseString(Points,*Line.Mid(7));
                        if(Points<0||Points>250000||int64(Offset)+int64(Points)*33!=Size)return TEXT("PCD POINTS×33 크기 불일치 · ")+PcdPath+FString::Printf(TEXT(" · points=%d header=%d bytes=%lld"),Points,Offset,Size);
                        const FString PointLabels=Dir/TEXT("lidar_labels.bin");const int64 LabelBytes=IFileManager::Get().FileSize(*PointLabels);
                        if(LabelBytes!=int64(Points)*5)return TEXT("점별 라벨 POINTS×5 크기 불일치 · ")+PointLabels+FString::Printf(TEXT(" · points=%d bytes=%lld"),Points,LabelBytes);
                        Bytes+=Size;
                    }
                    auto Summary=MakeShared<FJsonObject>();Summary->SetStringField(TEXT("schema_version"),TEXT("ma0t10.testset.scenario.v1"));Summary->SetStringField(TEXT("scenario_id"),Copy.ScenarioId);Summary->SetStringField(TEXT("run_uuid"),Copy.RunId);Summary->SetStringField(TEXT("frames_directory"),Root);Summary->SetNumberField(TEXT("frames"),Copy.Frames);Summary->SetNumberField(TEXT("seconds"),Copy.Seconds);Summary->SetNumberField(TEXT("pcd_bytes"),double(Bytes));Summary->SetStringField(TEXT("state"),TEXT("completed"));
                    if(Cancel->load()||!SaveTestSetJson(Copy.Directory/TEXT("summary.json"),EncodeTestSetJson(Summary)))return TEXT("시나리오 요약 저장 실패 · ")+Copy.Directory/TEXT("summary.json");return FString();
                });
            }
            S.Active=INDEX_NONE;++S.Revision;
        }
    }
    const bool OrdinaryBusy=!S.Slab.IsValid()||S.Slab->IsSimulationActive()||TestSetWorldBusy(GetWorld())||(R&&R->IsReplayBusy());
    if(S.Active==INDEX_NONE&&FPlatformTime::Seconds()>=S.NextReadiness)
    {
        S.NextReadiness=FPlatformTime::Seconds()+.2;
        FString SelectionError;
        if(!ResolveSensors(SelectionError))
        {
            bool Changed=false;for(auto& I:S.Items)if(I.State==EAgentTestSetItemState::Waiting){I.State=EAgentTestSetItemState::Failed;I.Message=SelectionError;Changed=true;}
            if(Changed){S.Message=SelectionError;++S.Revision;UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][SelectionFailed] %s"),*SelectionError);}
        }
        if(S.StopRequested&&!S.Paused&&!OrdinaryBusy&&!Dataset->IsActive()&&SelectionError.IsEmpty())SuspendBusyWork();
        FString Reason;S.Blocked=BusyReason(Reason);
        const bool Waiting=S.Items.ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Waiting;});
        if(S.Blocked&&Waiting&&S.Message!=Reason)
        {
            S.Message=Reason;for(auto& I:S.Items)if(I.State==EAgentTestSetItemState::Waiting)I.Message=Reason;++S.Revision;
            UE_LOG(LogMA0T10,Display,TEXT("[TESTSET][Waiting] %s"),*Reason);
        }
    }
    const bool Busy=OrdinaryBusy||(S.Active==INDEX_NONE&&S.Blocked);
    if(S.Yielded&&!Busy){S.Yielded=false;++S.Revision;}
    if((Busy||S.Paused)&&S.Active!=INDEX_NONE)Dataset->YieldPlannedCapture();
    if(S.Reset)
    {
        if(S.Active!=INDEX_NONE||S.Prepare.IsValid()||S.Verify.IsValid()||!S.Spools.IsEmpty()||S.Manifest.IsValid()||S.Replace.IsValid()||S.Template.IsValid())return;
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
        if(!S.Inputs[I].Stored&&(S.Inputs[I].Data||(S.Items[I].Example&&!S.Inputs[I].TemplatePath.IsEmpty()))&&!S.Inputs[I].Replacement&&S.Items[I].State==EAgentTestSetItemState::Waiting&&!S.Spools.ContainsByPredicate([I](const auto& V){return V.Index==I;}))
        {
            const auto Data=S.Inputs[I].Data;const auto Item=S.Items[I];const FString Path=Item.Directory/TEXT("original.json"),TemplatePath=S.Inputs[I].TemplatePath,TemplateHash=S.Inputs[I].TemplateHash;const auto Cancel=S.Cancelled;
            S.Spools.Add({I,Async(EAsyncExecution::ThreadPool,[Data,Item,Path,Cancel,TemplatePath,TemplateHash]()
            {
                FTestSetStored Result;FSlabScenarioDataPtr Generated=Data;FString Error;
                Result.Error=TEXT("원본 JSON 준비/저장 실패 · ")+Path;
                if(!Generated)
                {
                    FString Json;if(!FFileHelper::LoadFileToString(Json,*TemplatePath)||!FSlabScenarioCodec::Parse(Json,Generated,Error,true)||Generated->OriginalContentSha1!=TemplateHash){Result.Error+=TEXT(" · 템플릿 읽기/해시 · ")+TemplatePath+TEXT(" · ")+Error;return Result;}
                }
                const auto Source=Generated;
                if(Cancel->load()||(Item.Example&&!AgentTestSetExamples::Generate(*Source,Item.ScenarioId,Item.VariationJson,Generated,Error))){Result.Error+=TEXT(" · ")+Error;return Result;}
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
    if(S.ManifestRevision!=S.Revision&&!S.Manifest.IsValid()&&Now>=S.NextManifestRetry)
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
            const auto* Failed=S.Items.FindByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;});
            if(Failed&&!S.ManifestFailed&&S.Message!=Failed->Message){S.Message=Failed->Message;++S.Revision;}
            if(!S.ManifestFailed&&S.ManifestRevision==S.Revision&&(!PcdPreview||!PcdPreview->NeedsPump()))GetWorld()->GetTimerManager().ClearTimer(Timer);
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
