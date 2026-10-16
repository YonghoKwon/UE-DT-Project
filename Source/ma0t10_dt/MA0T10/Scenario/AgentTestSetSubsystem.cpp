#include "AgentTestSetSubsystem.h"
#include "VirtualDatasetSubsystem.h"
#include "SlabScenarioReplaySubsystem.h"
#include "VirtualSensorSlabContextSubsystem.h"
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
#include "TimerManager.h"
#include <atomic>

namespace
{
constexpr int64 InputLimit=64ll*1024*1024;
struct FPrepared
{
    FSlabScenarioDataPtr Data;
    TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Plan;
    FString Error;
    uint32 Configuration=0;
};
struct FInput
{
    FSlabScenarioDataPtr Data;
    FString Hash;
    int64 Bytes=0;
    bool Stored=false;
};
struct FSpool {int32 Index;TFuture<bool> Result;};
FString Id(){return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);}
FString Encode(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
bool Save(const FString& Path,const FString& Text)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    const FString Temp=Path+TEXT(".pending");
    if(!FFileHelper::SaveStringToFile(Text,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return false;
    return IFileManager::Get().Move(*Path,*Temp,true,true,false,true);
}
const TCHAR* StateName(EAgentTestSetItemState S)
{switch(S){case EAgentTestSetItemState::Generating:return TEXT("generating");case EAgentTestSetItemState::Completed:return TEXT("completed");case EAgentTestSetItemState::Failed:return TEXT("failed");default:return TEXT("waiting");}}
USlabScenarioReplaySubsystem* Replay(UWorld* W)
{return W&&W->GetGameInstance()?W->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>():nullptr;}
bool SessionBusy(UWorld* W)
{
    const auto S=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus().State;
    return S==EVirtualSlabSessionState::Preparing||S==EVirtualSlabSessionState::Ready||S==EVirtualSlabSessionState::Running||S==EVirtualSlabSessionState::Paused||S==EVirtualSlabSessionState::Draining;
}
template<class T> T* Sensor(UWorld* W,const FString& Explicit)
{
    T* Found=nullptr;
    for(TActorIterator<T> It(W);It;++It)if(!It->IsActorBeingDestroyed()&&(Explicit.IsEmpty()||It->GetSensorId()==Explicit))
    {if(Found)return nullptr;Found=*It;}
    return Found;
}
}
struct FAgentTestSetState
{
    FString Root,SetId,Message,PreviewRun;
    TArray<FAgentTestSetItem> Items;
    TArray<FInput> Inputs;
    TArray<FSpool> Spools;
    TWeakObjectPtr<ASlabActor> Slab;
    int32 Active=INDEX_NONE,Preparing=INDEX_NONE,Preview=INDEX_NONE;
    int64 Revision=1,ManifestRevision=0,PendingManifestRevision=0,InputBytes=0;
    int32 SavedCompleted=0,PendingManifestCompleted=0;
    bool Paused=false,Yielded=false,Reset=false,ManifestFailed=false;
    double Started=0,ActiveStarted=0;
    TFuture<FPrepared> Prepare;
    TOptional<FPrepared> Prepared;
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
    GConfig->GetString(Section,TEXT("LidarSensorId"),S.LidarId,GGameIni);GConfig->GetString(Section,TEXT("CameraSensorId"),S.CameraId,GGameIni);
    GConfig->GetBool(Section,TEXT("LidarFidelity"),S.LidarFidelity,GGameIni);GConfig->GetString(Section,TEXT("ExportFormat"),S.ExportFormat,GGameIni);GConfig->GetBool(Section,TEXT("ExportUprightEnvelope"),S.ExportUprightEnvelope,GGameIni);
    S.Target=FMath::Clamp(S.Target,1,1000);S.SampleStride=FMath::Clamp(S.SampleStride,1,10000);S.SettleFrames=FMath::Clamp(S.SettleFrames,1,600);
    S.Enabled&=!S.Trigger.IsEmpty();S.CaptureCamera|=S.CameraTruth;return S;
}
void UAgentTestSetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{Super::Initialize(Collection);Config=FAgentTestSetSettings::Load();}
bool UAgentTestSetSubsystem::Matches(const FSlabScenarioData& Data,const FString& Trigger)
{if(Data.Rows.IsEmpty()||Trigger.TrimStartAndEnd().IsEmpty())return false;for(const auto& R:Data.Rows)if(R.MtlNo.TrimStartAndEnd()!=Trigger.TrimStartAndEnd())return false;return true;}
bool UAgentTestSetSubsystem::IsTrigger(const FSlabScenarioData& Data) const {return Config.Enabled&&Matches(Data,Config.Trigger);}
bool UAgentTestSetSubsystem::Enqueue(FSlabScenarioDataPtr Data,ASlabActor* Slab,FString& Error)
{
    if(!Data||!IsTrigger(*Data)||!IsValid(Slab)||Slab->GetWorld()!=GetWorld()){Error=TEXT("테스트셋 입력과 대상 Slab을 확인하세요");return false;}
    if(!State)
    {
        State=MakeShared<FAgentTestSetState>();State->SetId=Id();State->Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TestSets")/State->SetId);State->Slab=Slab;State->Started=FPlatformTime::Seconds();
    }
    auto& S=*State;
    if(S.Reset||S.Slab.Get()!=Slab){Error=TEXT("세트 초기화 또는 다른 Slab의 작업이 진행 중입니다");return false;}
    for(int32 I=0;I<S.Items.Num();++I)if(S.Items[I].ScenarioId==Data->ScenarioUUID)
    {Error=S.Inputs[I].Hash==Data->OriginalContentSha1?TEXT("이미 등록된 테스트셋 시나리오입니다"):TEXT("같은 시나리오 ID의 원본이 다릅니다");return false;}
    const int64 Bytes=int64(Data->OriginalJson.Len())*sizeof(TCHAR)+int64(Data->Rows.Num())*(sizeof(FSlabScenarioRow)+64);
    if(S.Items.Num()>=Config.Target||Bytes>InputLimit-S.InputBytes){Error=TEXT("테스트셋 목표 개수 또는 입력 대기 64MiB 한도에 도달했습니다");return false;}
    FAgentTestSetItem Item;Item.ScenarioId=Data->ScenarioUUID;Item.RunId=Id();Item.Frames=FMath::DivideAndRoundUp(Data->Rows.Num(),Config.SampleStride);
    if(Item.Frames>10000){Error=TEXT("샘플 프레임은 시나리오당 10000개 이하여야 합니다 · SampleStride를 늘리세요");return false;}
    Item.Directory=S.Root/FString::Printf(TEXT("scenario_%03d_%s"),S.Items.Num()+1,*Item.ScenarioId);Item.Message=TEXT("대기");
    S.Items.Add(MoveTemp(Item));S.Inputs.Add({Data,Data->OriginalContentSha1,Bytes,false});S.InputBytes+=Bytes;S.Message=TEXT("테스트셋 시나리오 대기");++S.Revision;EnsurePump();return true;
}
void UAgentTestSetSubsystem::EnsurePump(){GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.02f,true);}
bool UAgentTestSetSubsystem::HasItems() const{return State&&!State->Items.IsEmpty();}
const TArray<FAgentTestSetItem>& UAgentTestSetSubsystem::Items() const{static const TArray<FAgentTestSetItem> Empty;return State?State->Items:Empty;}
int32 UAgentTestSetSubsystem::Completed() const{int32 N=0;for(const auto& I:Items())N+=I.State==EAgentTestSetItemState::Completed;return N;}
bool UAgentTestSetSubsystem::IsComplete() const{return State&&!State->Reset&&Completed()==Config.Target&&!State->ManifestFailed&&State->SavedCompleted==Config.Target;}
bool UAgentTestSetSubsystem::IsPaused() const{return State&&(State->Paused||State->Yielded);}
FString UAgentTestSetSubsystem::Directory() const{return State?State->Root:FString();}
FString UAgentTestSetSubsystem::Message() const{return State?State->Message:TEXT("等待 시나리오 · 트리거 소재 번호만 테스트셋으로 생성");}
int64 UAgentTestSetSubsystem::Revision() const{return State?State->Revision:0;}
void UAgentTestSetSubsystem::YieldToOrdinaryScenario()
{
    if(!State)return;StopPreview();State->Preview=INDEX_NONE;
    if(State->Active!=INDEX_NONE)if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())if(D->OwnsPlannedRun(State->Items[State->Active].RunId))D->YieldPlannedCapture();
    State->Yielded=true;State->Message=TEXT("일반 시나리오 우선 · 테스트셋 대기");++State->Revision;EnsurePump();
}
void UAgentTestSetSubsystem::SetPaused(bool Paused)
{if(!State)return;State->Paused=Paused;if(Paused&&State->Active!=INDEX_NONE)if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())if(D->OwnsPlannedRun(State->Items[State->Active].RunId))D->YieldPlannedCapture();State->Message=Paused?TEXT("테스트셋 일시 중지"):TEXT("테스트셋 재개 대기");++State->Revision;EnsurePump();}
bool UAgentTestSetSubsystem::Retry(int32 Index,FString& Error)
{
    if(!State||State->Reset||!State->Items.IsValidIndex(Index)||State->Items[Index].State!=EAgentTestSetItemState::Failed){Error=TEXT("실패한 시나리오를 선택하세요");return false;}
    auto& I=State->Items[Index];I.State=EAgentTestSetItemState::Waiting;I.RunId=Id();I.CompletedFrames=0;I.Message=TEXT("재시도 대기");++State->Revision;EnsurePump();return true;
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
    StopPreview();State->Preview=Index;State->Message=TEXT("보기 전용 재생 준비 · 전송 꺼짐");++State->Revision;EnsurePump();return true;
}
void UAgentTestSetSubsystem::StopPreview()
{if(!State)return;const bool Changed=!State->PreviewRun.IsEmpty()||State->Preview!=INDEX_NONE;if(auto* R=Replay(GetWorld()))R->StopObservationPreview(State->PreviewRun);State->PreviewRun.Reset();State->Preview=INDEX_NONE;if(Changed){State->Message=TEXT("보기 중지 · 데이터셋 유지");++State->Revision;EnsurePump();}}
TSharedRef<FJsonObject> UAgentTestSetSubsystem::StatusJson() const
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("schema_version"),TEXT("ma0t10.testset.v1"));O->SetStringField(TEXT("trigger_mtl_no"),Config.Trigger);O->SetNumberField(TEXT("target_scenarios"),Config.Target);
    O->SetBoolField(TEXT("enabled"),Config.Enabled);O->SetBoolField(TEXT("pipeline"),Config.Pipeline);O->SetBoolField(TEXT("paused"),IsPaused());O->SetBoolField(TEXT("reset"),State&&State->Reset);
    O->SetNumberField(TEXT("completed_scenarios"),Completed());O->SetBoolField(TEXT("complete"),IsComplete());O->SetStringField(TEXT("directory"),Directory());
    O->SetStringField(TEXT("project_commit"),TEXT(MA0T10_PROJECT_COMMIT));O->SetStringField(TEXT("build"),FApp::GetBuildVersion());O->SetBoolField(TEXT("sending_supported"),false);
    O->SetBoolField(TEXT("capture_camera"),Config.CaptureCamera);O->SetBoolField(TEXT("camera_truth"),Config.CameraTruth);O->SetBoolField(TEXT("include_imu"),Config.IncludeImu);O->SetNumberField(TEXT("sample_stride"),Config.SampleStride);
    O->SetStringField(TEXT("export_format"),Config.ExportFormat);O->SetBoolField(TEXT("lidar_fidelity"),Config.LidarFidelity);
    if(State){O->SetStringField(TEXT("set_id"),State->SetId);O->SetNumberField(TEXT("revision"),State->Revision);O->SetNumberField(TEXT("pending_input_bytes"),double(State->InputBytes));O->SetStringField(TEXT("preview_run_id"),State->PreviewRun);}
    TArray<TSharedPtr<FJsonValue>> A;
    for(const auto& I:Items())
    {auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("scenario_id"),I.ScenarioId);V->SetStringField(TEXT("run_uuid"),I.RunId);V->SetStringField(TEXT("directory"),I.Directory);V->SetStringField(TEXT("state"),StateName(I.State));V->SetStringField(TEXT("message"),I.Message);V->SetNumberField(TEXT("frames"),I.Frames);V->SetNumberField(TEXT("completed_frames"),I.CompletedFrames);V->SetNumberField(TEXT("seconds"),I.Seconds);V->SetNumberField(TEXT("attempts"),I.Attempts);A.Add(MakeShared<FJsonValueObject>(V));}
    O->SetArrayField(TEXT("scenarios"),A);return O;
}

void UAgentTestSetSubsystem::Pump()
{
    if(!State)return;auto& S=*State;auto* Dataset=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();auto* R=Replay(GetWorld());
    // Drain spool results first; no UObject is captured by a worker.
    for(int32 I=S.Spools.Num()-1;I>=0;--I)if(S.Spools[I].Result.IsReady())
    {
        const int32 N=S.Spools[I].Index;const bool Ok=S.Spools[I].Result.Get();auto& Input=S.Inputs[N];Input.Stored=Ok;
        if(Ok){S.InputBytes-=Input.Bytes;Input.Bytes=0;Input.Data.Reset();}else {S.Items[N].State=EAgentTestSetItemState::Failed;S.Items[N].Message=TEXT("原본 JSON 저장 실패 · 재시도하세요");++S.Revision;}
        S.Spools.RemoveAt(I);
    }
    if(S.Manifest.IsValid()&&S.Manifest.IsReady())
    {S.ManifestFailed=!S.Manifest.Get();S.Manifest=TFuture<bool>();if(!S.ManifestFailed){S.ManifestRevision=S.PendingManifestRevision;S.SavedCompleted=S.PendingManifestCompleted;}else S.Message=TEXT("testset.json 저장 실패 · 폴더 권한·디스크 공간을 확인하세요");}
    if(S.Prepare.IsValid()&&S.Prepare.IsReady()){S.Prepared=S.Prepare.Get();S.Prepare=TFuture<FPrepared>();}
    if(S.Active!=INDEX_NONE&&!Dataset->OwnsPlannedRun(S.Items[S.Active].RunId))
    {auto& I=S.Items[S.Active];I.State=EAgentTestSetItemState::Failed;I.Message=TEXT("생성 세션 변경 · 새 작업을 보존합니다");S.Active=INDEX_NONE;++S.Revision;}
    if(S.Verify.IsValid()&&S.Verify.IsReady())
    {
        const FString Error=S.Verify.Get();S.Verify=TFuture<FString>();auto& I=S.Items[S.Verifying];
        I.State=Error.IsEmpty()?EAgentTestSetItemState::Completed:EAgentTestSetItemState::Failed;
        I.Message=Error.IsEmpty()?TEXT("생성 완료 · 파일 검증 완료"):Error;S.Verifying=INDEX_NONE;++S.Revision;
        if(Completed()==Config.Target)S.Message=TEXT("테스트셋 생성 완료 · 목록에서 보기 전용 재생을 선택하세요");
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
                    if(Cancel->load()||!Save(Copy.Directory/TEXT("summary.json"),Encode(Summary)))return TEXT("시나리오 요약 저장 실패");return FString();
                });
            }
            S.Active=INDEX_NONE;++S.Revision;
        }
    }
    const bool Busy=!S.Slab.IsValid()||S.Slab->IsSimulationActive()||SessionBusy(GetWorld())||(R&&R->IsReplayBusy());
    if(S.Yielded&&!Busy){S.Yielded=false;++S.Revision;}
    if((Busy||S.Paused)&&S.Active!=INDEX_NONE)Dataset->YieldPlannedCapture();
    if(S.Reset)
    {
        if(S.Active!=INDEX_NONE||S.Prepare.IsValid()||S.Verify.IsValid()||!S.Spools.IsEmpty()||S.Manifest.IsValid())return;
        // Persist the abandoned set once, then detach. Reset never deletes existing datasets.
        const auto Snapshot=StatusJson();const FString Path=S.Root/TEXT("testset.json");
        S.Cancelled->store(true);Async(EAsyncExecution::ThreadPool,[Snapshot,Path](){Save(Path,Encode(Snapshot));});
        State.Reset();GetWorld()->GetTimerManager().ClearTimer(Timer);return;
    }
    for(int32 I=0;I<S.Items.Num()&&S.Spools.Num()<2;++I)
        if(S.Inputs[I].Data&&S.Items[I].State==EAgentTestSetItemState::Waiting&&!S.Spools.ContainsByPredicate([I](const auto& V){return V.Index==I;}))
        {
            const auto Data=S.Inputs[I].Data;const FString Path=S.Items[I].Directory/TEXT("original.json");const auto Cancel=S.Cancelled;
            S.Spools.Add({I,Async(EAsyncExecution::ThreadPool,[Data,Path,Cancel](){return !Cancel->load()&&Save(Path,Data->OriginalJson);})});
        }
    if(!Busy&&!S.Paused&&S.Active!=INDEX_NONE&&Dataset->IsPaused()&&!S.Slab->HasPendingScenarioInput())
    {FString Error;if(!Dataset->Resume(Error)){Dataset->Message=Error;Dataset->Stop();}}
    if(S.Prepared.IsSet()&&!Busy&&!S.Paused&&S.Active==INDEX_NONE&&!Dataset->IsActive()&&!S.Slab->HasPendingScenarioInput())
    {
        const int32 Index=S.Preparing;auto Prepared=MoveTemp(S.Prepared.GetValue());S.Prepared.Reset();S.Preparing=INDEX_NONE;
        if(Index==S.Preview)
        {
            S.Preview=INDEX_NONE;
            if(Prepared.Data&&R&&R->RequestObservationPreview(Prepared.Data)){S.PreviewRun=R->GetReplayStatus().RunUUID;S.Message=TEXT("보기 전용 시뮬레이션 · 전송 꺼짐");}
            else S.Message=TEXT("보기 전용 재생 실패 · ")+Prepared.Error;++S.Revision;
        }
        else if(S.Items.IsValidIndex(Index)&&S.Items[Index].State==EAgentTestSetItemState::Waiting)
        {
            auto& Item=S.Items[Index];FString Error=Prepared.Error;
            auto* Lidar=Sensor<AVirtualLidarSensorActor>(GetWorld(),Config.LidarId);auto* Camera=Config.CaptureCamera?Sensor<AVirtualCameraSensorActor>(GetWorld(),Config.CameraId):nullptr;
            if(Prepared.Plan&&S.Slab.IsValid()&&Prepared.Configuration!=S.Slab->GetMetricsConfigurationHash())
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
        int32 Index=S.Preview;
        if(Index==INDEX_NONE)for(int32 I=0;I<S.Items.Num();++I)if(S.Items[I].State==EAgentTestSetItemState::Waiting&&S.Inputs[I].Stored){Index=I;break;}
        if(Index!=INDEX_NONE)
        {
            S.Preparing=Index;const auto Item=S.Items[Index];const FString Path=Item.Directory/TEXT("original.json");const auto Track=S.Slab->GetDatasetTrackTransform();const auto Dimension=S.Slab->DimensionUnit,Position=S.Slab->PositionUnit;const auto ConfigCopy=Config;const uint32 Hash=S.Slab->GetMetricsConfigurationHash();
            S.Prepare=Async(EAsyncExecution::ThreadPool,[Item,Path,Track,Dimension,Position,ConfigCopy,Hash]()
            {
                FPrepared Out;Out.Configuration=Hash;FString Json;
                if(!FFileHelper::LoadFileToString(Json,*Path)||!FSlabScenarioCodec::Parse(Json,Out.Data,Out.Error,false))return Out;
                Out.Plan=MakeShared<FVirtualDatasetFramePlan,ESPMode::ThreadSafe>();auto& Plan=*Out.Plan;Plan.Root=Item.Directory/(TEXT("run_")+Item.RunId);Plan.ScenarioId=Item.ScenarioId;Plan.RunId=Item.RunId;Plan.CaptureCamera=ConfigCopy.CaptureCamera;Plan.Pipeline=ConfigCopy.Pipeline;
                // Match live playback's first-row dimension policy, with identical BuildPose and units.
                const auto& First=Out.Data->Rows[0];const FVector Size(FSlabScenarioCodec::ToCm(First.Length,Dimension),FSlabScenarioCodec::ToCm(First.Width,Dimension),FSlabScenarioCodec::ToCm(First.Thickness,Dimension));
                if(Size.ContainsNaN()||Size.GetMin()<=0||Size.GetMax()>1000000){Out.Error=TEXT("Slab 치수가 지원 범위를 벗어났습니다");Out.Plan.Reset();return Out;}
                for(int32 I=0;I<Out.Data->Rows.Num();I+=ConfigCopy.SampleStride)
                {const auto& Row=Out.Data->Rows[I];Plan.Frames.Add({USlabMotionComponent::BuildPose(Row,Track,Size,Position),Size,Row.FrameNo,Row.ElapsedSec,Row.MtlNo});}
                return Out;
            });
        }
    }
    if(S.ManifestRevision!=S.Revision&&!S.Manifest.IsValid())
    {
        const auto Snapshot=StatusJson();Snapshot->SetBoolField(TEXT("complete"),Completed()==Config.Target);const FString Path=S.Root/TEXT("testset.json");const auto Cancel=S.Cancelled;S.PendingManifestRevision=S.Revision;S.PendingManifestCompleted=Completed();
        S.Manifest=Async(EAsyncExecution::ThreadPool,[Snapshot,Path,Cancel](){return !Cancel->load()&&Save(Path,Encode(Snapshot));});
    }
    // Terminal/idle sets have no service timer. New input, retry or UI commands re-arm it.
    if(S.Active==INDEX_NONE&&!S.Prepare.IsValid()&&!S.Verify.IsValid()&&!S.Prepared.IsSet()&&S.Spools.IsEmpty()&&!S.Manifest.IsValid()&&S.Preview==INDEX_NONE)
    {
        const bool Waiting=S.Items.ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Waiting;});
        if(!Waiting)GetWorld()->GetTimerManager().ClearTimer(Timer);
    }
}
void UAgentTestSetSubsystem::Deinitialize()
{
    StopPreview();if(State){State->Cancelled->store(true);if(State->Active!=INDEX_NONE)if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())if(D->OwnsPlannedRun(State->Items[State->Active].RunId))D->Stop();}
    GetWorld()->GetTimerManager().ClearTimer(Timer);State.Reset();Super::Deinitialize();
}
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool UAgentTestSetSubsystem::ConfigureForTests(const FAgentTestSetSettings& In){if(HasItems())return false;Config=In;return true;}
#endif
