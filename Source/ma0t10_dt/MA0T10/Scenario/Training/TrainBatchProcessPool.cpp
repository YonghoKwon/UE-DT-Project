#include "TrainBatchProcessPool.h"
#include "TrainBatchCodec.h"
#include "TrainBatchWorkerSafety.h"
#include "Device/Contracts/LocalCapturePath.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

bool FTrainBatchProcessPool::Acquire(FString& Error)
{
    const FString Path=WorkerBase/TEXT("owner.lock");bool Alive=false;OwnerStamp=TrainBatchProcess::ProcessStamp(FPlatformProcess::GetCurrentProcessId(),Alive);if(!OwnerStamp)return false;
    if(IFileManager::Get().FileExists(*Path))
    {
        const auto Old=TrainBatchProcess::Read(Path);double Pid=0;FString Stamp;uint64 Start=0;
        if(!Old||!Old->TryGetNumberField(TEXT("pid"),Pid)||!Old->TryGetStringField(TEXT("stamp"),Stamp)||!LexTryParseString(Start,*Stamp)){Error=TEXT("Pool owner record unreadable");return false;}
        const uint64 Actual=TrainBatchProcess::ProcessStamp(uint32(Pid),Alive);
        if(Alive&&(!Actual||!Start||Actual==Start)){Error=TEXT("Another coordinator owns this Job");return false;}
        FString PreviousJob;if(Old->TryGetStringField(TEXT("job_id"),PreviousJob)&&PreviousJob!=Job.Id&&TrainBatchCodec::CanonicalGuid(PreviousJob))
        {
            const auto Previous=TrainBatchProcess::Read(TrainBatchCodec::JobDirectory(PreviousJob)/TEXT("processes/pool.json"),2*1024*1024);const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
            if(!Previous||!Previous->TryGetArrayField(TEXT("slots"),A)){Error=TEXT("Resume the prior Job before taking worker slots");return false;}
            for(const auto& V:*A){const auto O=V->AsObject();double Task=-1;if(!O||!O->TryGetNumberField(TEXT("task"),Task)||Task>=0){Error=TEXT("Prior Job has unsettled worker leases; resume/cancel it first");return false;}}
        }
        if(!IFileManager::Get().Delete(*Path,false,true)){Error=TEXT("Stale pool owner still locked");return false;}
    }
    IFileManager::Get().MakeDirectory(*Root,true);IFileManager::Get().MakeDirectory(*WorkerBase,true);OwnerLease.Reset(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_NoReplaceExisting|FILEWRITE_AllowRead));if(!OwnerLease)return false;
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("pid"),FPlatformProcess::GetCurrentProcessId());O->SetStringField(TEXT("stamp"),LexToString(OwnerStamp));O->SetStringField(TEXT("job_id"),Job.Id);const FString Text=TrainBatchProcess::Encode(O);FTCHARToUTF8 U(*Text);OwnerLease->Serialize((void*)U.Get(),U.Length());OwnerLease->Flush();return !OwnerLease->IsError();
}
bool FTrainBatchProcessPool::Start(const FTrainBatchJobView& InView,const TSharedRef<FJsonObject>& InPreset,int32 Limit,FString& Error)
{
    const auto& InJob=InView.Spec;
    if(!TrainBatchCodec::CanonicalGuid(InJob.Id)||Limit<1||Limit>4||OwnerLease){Error=TEXT("Invalid process pool request");return false;}
    Job=InJob;Preset=InPreset;Requested=FMath::Min(Limit,Job.WorkerLimit);Root=TrainBatchCodec::JobDirectory(Job.Id)/TEXT("processes");
    WorkerBase=LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("TrainBatch/Workers"));
    if(!LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),Root)||!LocalCapturePath::SafeDescendant(LocalCapturePath::Full(FPaths::ProjectSavedDir()),WorkerBase)||!Acquire(Error))return false;
    TrainBatchProcess::FGpuBudget G;if(!TrainBatchProcess::QueryGpuBudget(G)){Error=TEXT("DXGI local GPU budget unavailable");return false;}Identity=G.Identity;AdapterIndex=G.AdapterIndex;LastBudget=G.Budget;LastUsage=G.Usage;
    // Guard state is authoritative. Legacy pool counters/ban flags may include timeouts.
    BanNative=false;NativeFailureLeases.Reset();NativeCrashes=0;
    for(int32 Slot=0;Slot<4;++Slot)BanNative|=TrainBatchWorkerSafety::GuardBanned(WorkerBase,Slot);
    Queue.Initialize(Job.Scenarios.Num());Slots.SetNum(4);
    for(int32 I=0;I<InView.Items.Num();++I)if(InView.Items[I].State==ETrainBatchItemState::Completed)Queue.Tasks[I].State=TrainBatchProcess::ETask::Completed;
    const auto Old=TrainBatchProcess::Read(Root/TEXT("pool.json"),2*1024*1024);
    if(Old)
    {
        FString Hash,Version,Config;
        if(!Old->TryGetStringField(TEXT("manifest_sha256"),Hash)||Hash!=Job.ManifestSha256||!Old->TryGetStringField(TEXT("version"),Version)||Version!=FString(TEXT(MA0T10_PROJECT_COMMIT))||!Old->TryGetStringField(TEXT("preset"),Config)||Config!=TrainBatchProcess::Encode(InPreset)||!Queue.Restore(Old,Job.Scenarios.Num(),Error))return false;
        FString OldIdentity;if(!Old->TryGetStringField(TEXT("gpu_identity"),OldIdentity)||OldIdentity!=Identity){Error=TEXT("GPU/driver changed; use a new Job ID");return false;}
        double N=0;Old->TryGetNumberField(TEXT("peak_gpu_bytes"),N);Peak=uint64(FMath::Max(0.,N));Old->TryGetBoolField(TEXT("calibrated"),Calibrated);
        const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(Old->TryGetArrayField(TEXT("slots"),A))for(int32 I=0;I<FMath::Min(4,A->Num());++I)
        {
            const auto V=(*A)[I]->AsObject();double Task=-1,Pid=0,Epoch=0;FString Stamp;if(!V||!V->TryGetNumberField(TEXT("task"),Task)||Task<0)continue;
            auto& S=Slots[I];S.Task=int32(Task);V->TryGetNumberField(TEXT("pid"),Pid);S.Pid=uint32(Pid);V->TryGetStringField(TEXT("stamp"),Stamp);LexTryParseString(S.Stamp,*Stamp);V->TryGetStringField(TEXT("lease"),S.Lease);V->TryGetNumberField(TEXT("epoch"),Epoch);S.Epoch=uint64(Epoch);
            S.Ipc=Root/TEXT("leases")/S.Lease;S.Launched=FPlatformTime::Seconds();
            V->TryGetBoolField(TEXT("coordinator_terminated"),S.CoordinatorTerminated);V->TryGetBoolField(TEXT("cancel_requested"),S.CancelRequested);V->TryGetBoolField(TEXT("native_fault"),S.NativeFault);
            if(!Queue.Matches(S.Task,S.Lease,S.Epoch)){Error=TEXT("Restored slot/lease mismatch");return false;}
            bool Alive=false;const uint64 Actual=TrainBatchProcess::ProcessStamp(S.Pid,Alive);
            if(Alive&&(!Actual||!S.Stamp)){Error=TEXT("Cannot prove restored worker ownership");return false;}
            if(Alive&&Actual==S.Stamp){S.Process=FPlatformProcess::OpenProcess(S.Pid);if(!S.Process.IsValid()){Error=TEXT("Cannot adopt live worker");return false;}}
            auto Owner=MakeShared<FJsonObject>();Owner->SetStringField(TEXT("lease"),S.Lease);Owner->SetNumberField(TEXT("pid"),FPlatformProcess::GetCurrentProcessId());Owner->SetStringField(TEXT("stamp"),LexToString(OwnerStamp));TrainBatchProcess::Write(S.Ipc,S.Ipc/TEXT("owner.json"),Owner);
        }
    }
    Allowed=Calibrated?TrainBatchProcess::Capacity(Requested,Job.Scenarios.Num(),G.Budget,G.Usage,Peak):TrainBatchProcess::Capacity(1,Job.Scenarios.Num(),G.Budget,G.Usage,3ull*1024*1024*1024);
    if(!Allowed&&!Slots.ContainsByPredicate([](const auto& S){return S.Process.IsValid();})){Error=TEXT("GPU reserve does not admit one worker");return false;}return SaveSafety()&&Save();
}
bool FTrainBatchProcessPool::SaveSafety()
{
    auto Safety=MakeShared<FJsonObject>();Safety->SetNumberField(TEXT("schema_version"),2);Safety->SetStringField(TEXT("ban_policy"),TEXT("SensorNativeSafety_two_unclean_exits"));Safety->SetStringField(TEXT("guard_state_name"),SensorNativeSafety::StateFileName());
    TArray<TSharedPtr<FJsonValue>> Leases;for(const auto& L:NativeFailureLeases)Leases.Add(MakeShared<FJsonValueString>(L));Safety->SetArrayField(TEXT("leases"),Leases);Safety->SetBoolField(TEXT("banned"),BanNative);
    return TrainBatchProcess::Write(WorkerBase,WorkerBase/(TEXT("safety-")+Identity+TEXT(".json")),Safety);
}
bool FTrainBatchProcessPool::Save()
{
    auto O=Queue.Json();O->SetStringField(TEXT("manifest_sha256"),Job.ManifestSha256);O->SetStringField(TEXT("version"),TEXT(MA0T10_PROJECT_COMMIT));O->SetStringField(TEXT("preset"),TrainBatchProcess::Encode(Preset.ToSharedRef()));O->SetNumberField(TEXT("peak_gpu_bytes"),double(Peak));O->SetNumberField(TEXT("native_crashes"),NativeCrashes);O->SetStringField(TEXT("gpu_identity"),Identity);O->SetNumberField(TEXT("requested"),Requested);O->SetNumberField(TEXT("actual_max"),MaxObserved);
    O->SetBoolField(TEXT("calibrated"),Calibrated);O->SetBoolField(TEXT("ban_native"),BanNative);
    O->SetNumberField(TEXT("budget_bytes"),double(LastBudget));O->SetNumberField(TEXT("coordinator_usage_bytes"),double(LastUsage));O->SetNumberField(TEXT("allowed"),Allowed);
    TArray<TSharedPtr<FJsonValue>> A;for(const auto& S:Slots){auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("task"),S.Task);V->SetNumberField(TEXT("pid"),S.Pid);V->SetStringField(TEXT("stamp"),LexToString(S.Stamp));V->SetStringField(TEXT("lease"),S.Lease);V->SetNumberField(TEXT("epoch"),double(S.Epoch));V->SetBoolField(TEXT("coordinator_terminated"),S.CoordinatorTerminated);V->SetBoolField(TEXT("cancel_requested"),S.CancelRequested);V->SetBoolField(TEXT("native_fault"),S.NativeFault);A.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("slots"),A);
    return TrainBatchProcess::Write(Root,Root/TEXT("pool.json"),O);
}
bool FTrainBatchProcessPool::Launch(int32 Index)
{
    auto* Task=Queue.LeaseNext(Index,FPlatformTime::Seconds());if(!Task)return false;auto& S=Slots[Index];S=FSlot();S.Task=Task->Index;S.Lease=Task->Lease;S.Epoch=Task->Epoch;S.Ipc=Root/TEXT("leases")/S.Lease;S.Launched=FPlatformTime::Seconds();
    const FString User=WorkerBase/FString::Printf(TEXT("slot-%02d/UserDir"),Index);IFileManager::Get().MakeDirectory(*User,true);
    auto C=MakeShared<FJsonObject>(*Preset);C->SetStringField(TEXT("job_id"),Job.Id);C->SetStringField(TEXT("lease"),S.Lease);C->SetNumberField(TEXT("epoch"),double(S.Epoch));C->SetNumberField(TEXT("index"),S.Task);C->SetNumberField(TEXT("parent_pid"),FPlatformProcess::GetCurrentProcessId());C->SetStringField(TEXT("parent_stamp"),LexToString(OwnerStamp));C->SetStringField(TEXT("input_root"),TrainBatchCodec::JobsRoot());C->SetStringField(TEXT("manifest"),TrainBatchCodec::JobDirectory(Job.Id)/TEXT("input/job.json"));
    C->SetStringField(TEXT("pool_lock"),WorkerBase/TEXT("owner.lock"));
    FString Map;C->TryGetStringField(TEXT("map"),Map);const FString Config=S.Ipc/TEXT("config.json");
    if(Map.IsEmpty()||!TrainBatchProcess::Write(S.Ipc,Config,C)||!Save()){Failure=TEXT("Worker assignment persistence failed");return false;}
    const FString Executable=FPlatformProcess::ExecutablePath();FString Args=TrainBatchProcess::WorkerMapArguments(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),Map,FPaths::GetCleanFilename(Executable).Contains(TEXT("UnrealEditor")));
    if(Args.IsEmpty()){Failure=TEXT("Worker map/project argument rejected");return false;}
    Args+=TEXT(" -d3d12 -RenderOffscreen -unattended -nosplash -nosound -nop4 -ResX=1920 -ResY=1080 -UserDir=")+TrainBatchProcess::Quote(User)+TEXT(" -TrainBatchWorker=")+TrainBatchProcess::Quote(Config)+TEXT(" -abslog=")+TrainBatchProcess::Quote(S.Ipc/TEXT("worker.log"))+FString::Printf(TEXT(" -graphicsadapter=%d"),AdapterIndex);if(BanNative)Args+=TEXT(" -SensorRtRhi");
    S.Process=FPlatformProcess::CreateProc(*Executable,*Args,true,true,true,&S.Pid,0,*FPaths::ProjectDir(),nullptr);bool Alive=false;S.Stamp=TrainBatchProcess::ProcessStamp(S.Pid,Alive);Save();
    if(S.Process.IsValid()&&S.Stamp){FTrainBatchPoolEvent E;E.Index=S.Task;E.Slot=Index;E.Lease=S.Lease;E.Epoch=S.Epoch;E.Phase=TEXT("leased");Events.Add(MoveTemp(E));return true;}return false;
}
void FTrainBatchProcessPool::Control(FSlot& S,bool Pause,bool CancelRequested)
{auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("lease"),S.Lease);O->SetBoolField(TEXT("pause"),Pause);O->SetBoolField(TEXT("cancel"),CancelRequested);TrainBatchProcess::Write(S.Ipc,S.Ipc/TEXT("control.json"),O);}
void FTrainBatchProcessPool::Tick(bool Pause)
{
    const double Now=FPlatformTime::Seconds();if(Now<NextPoll)return;NextPoll=Now+1;
    int32 Live=0;
    for(int32 I=0;I<Slots.Num();++I)
    {
        auto& S=Slots[I];if(S.Task==INDEX_NONE)continue;if(Pause!=LastPause||Stopping)Control(S,Pause,Stopping);
        const auto H=TrainBatchProcess::Read(S.Ipc/TEXT("heartbeat.json"));
        if(H)
        {
            FString Lease;double Epoch=0,Sequence=0,Bytes=0;bool Fault=false;double Status=0;
            if(H->TryGetStringField(TEXT("lease"),Lease)&&H->TryGetNumberField(TEXT("epoch"),Epoch)&&Lease==S.Lease&&Epoch==S.Epoch&&H->TryGetNumberField(TEXT("sequence"),Sequence)&&Sequence>S.Sequence)
            {S.Sequence=uint64(Sequence);Queue.Heartbeat(S.Task,S.Lease,S.Epoch,Now);H->TryGetNumberField(TEXT("peak_gpu_bytes"),Bytes);Peak=FMath::Max(Peak,uint64(FMath::Max(0.,Bytes)));H->TryGetBoolField(TEXT("native_fault"),Fault);S.NativeFault|=Fault;H->TryGetNumberField(TEXT("native_status"),Status);BanNative|=Status==int32(SensorNativeSafety::EStatus::Banned);}
        }
        const bool Alive=S.Process.IsValid()&&FPlatformProcess::IsProcRunning(S.Process);
        if(Alive)
        {
            ++Live;const double Timeout=S.Sequence?45:180;
            if(Now-Queue.Tasks[S.Task].LastSeen>Timeout||(Stopping&&Now-CancelAt>10))
            {if(TrainBatchProcess::SameProcess(S.Pid,S.Stamp)){S.CoordinatorTerminated=true;S.CancelRequested|=Stopping;Save();FPlatformProcess::TerminateProc(S.Process,true);}}
            continue;
        }
        if(S.Reported)continue;S.Reported=true;if(S.Process.IsValid()){FPlatformProcess::CloseProc(S.Process);S.Process.Reset();}
        FTrainBatchPoolEvent E;E.Index=S.Task;E.Slot=I;E.Lease=S.Lease;E.Epoch=S.Epoch;E.WorkerRoot=WorkerBase/FString::Printf(TEXT("slot-%02d/UserDir/Saved/TrainBatch/Jobs"),I);
        const auto Result=TrainBatchProcess::Read(S.Ipc/TEXT("result.json"));FString Lease;double Epoch=0,Bytes=0,NativeStatus=0;
        if(Result&&Result->TryGetStringField(TEXT("lease"),Lease)&&Lease==S.Lease&&Result->TryGetNumberField(TEXT("epoch"),Epoch)&&Epoch==S.Epoch)
        {Result->TryGetStringField(TEXT("phase"),E.Phase);Result->TryGetStringField(TEXT("error"),E.Error);Result->TryGetStringField(TEXT("run_uuid"),E.RunId);Result->TryGetNumberField(TEXT("peak_gpu_bytes"),Bytes);Peak=FMath::Max(Peak,uint64(FMath::Max(0.,Bytes)));Calibrated|=E.Phase==TEXT("completed")&&Peak>0;Result->TryGetNumberField(TEXT("native_status"),NativeStatus);if(NativeStatus==int32(SensorNativeSafety::EStatus::Banned))BanNative=true;bool Fault=false;Result->TryGetBoolField(TEXT("native_fault"),Fault);S.NativeFault|=Fault;}
        else
        {
            E.Phase=Stopping?TEXT("cancelled"):TEXT("failed");E.Error=TEXT("Worker exited without a current lease result");
        }
        S.NativeFault|=TrainBatchWorkerSafety::GuardFaulted(WorkerBase,I);
        TrainBatchWorkerSafety::FExitEvidence Evidence;Evidence.CoordinatorTerminated=S.CoordinatorTerminated;Evidence.CancelRequested=Stopping||S.CancelRequested;Evidence.NativeFault=S.NativeFault;
        if(TrainBatchWorkerSafety::CountsNativeFailure(Evidence)&&NativeFailureLeases.Num()<2)NativeFailureLeases.AddUnique(S.Lease);
        NativeCrashes=NativeFailureLeases.Num();
        TrainBatchWorkerSafety::CleanupControlledMarker(WorkerBase,I,S.Pid,S.Stamp,Evidence);
        for(int32 Slot=0;Slot<4;++Slot)BanNative|=TrainBatchWorkerSafety::GuardBanned(WorkerBase,Slot);
        SaveSafety(); // A diagnostic fault count is never itself a ban decision.
        Events.Add(MoveTemp(E));Save();
    }
    LastPause=Pause;
    TrainBatchProcess::FGpuBudget G;if(TrainBatchProcess::QueryGpuBudget(G))
    {LastBudget=G.Budget;LastUsage=G.Usage;if(G.Identity!=Identity){Failure=TEXT("GPU/driver changed during batch");Cancel();}Allowed=Calibrated?TrainBatchProcess::Capacity(Requested,Job.Scenarios.Num(),G.Budget,G.Usage,Peak):1;}
    if(!Stopping&&!Pause&&Failure.IsEmpty())for(int32 I=0;I<Slots.Num()&&Live<Allowed;++I)if(Slots[I].Task==INDEX_NONE){if(!Launch(I))break;++Live;}
    MaxObserved=FMath::Max(MaxObserved,Live);
}
bool FTrainBatchProcessPool::Pop(FTrainBatchPoolEvent& E)
{if(Events.IsEmpty())return false;E=MoveTemp(Events[0]);Events.RemoveAt(0);return true;}
void FTrainBatchProcessPool::Acknowledge(const FTrainBatchPoolEvent& E,bool Valid)
{
    if(!Slots.IsValidIndex(E.Slot)||!Queue.Matches(E.Index,E.Lease,E.Epoch))return;
    if(E.Phase==TEXT("completed")&&Valid)Queue.Complete(E.Index,E.Lease,E.Epoch);
    else Queue.Retry(E.Index,E.Lease,E.Epoch,E.Error,E.Phase!=TEXT("recovery")&&(E.Phase!=TEXT("cancelled")||!Stopping));
    Slots[E.Slot]=FSlot();Save();
}
void FTrainBatchProcessPool::Cancel(){if(!Stopping)CancelAt=FPlatformTime::Seconds();Stopping=true;for(auto& S:Slots)if(S.Task!=INDEX_NONE){S.CancelRequested=true;Control(S,false,true);}Save();}
bool FTrainBatchProcessPool::IsDone() const
{return Events.IsEmpty()&&!Slots.ContainsByPredicate([](const auto& S){return S.Task!=INDEX_NONE;})&&(Stopping||Queue.Terminal()||!Failure.IsEmpty());}
FString FTrainBatchProcessPool::Status() const
{int32 Done=0,Failed=0;for(const auto& T:Queue.Tasks){Done+=T.State==TrainBatchProcess::ETask::Completed;Failed+=T.State==TrainBatchProcess::ETask::Failed;}return FString::Printf(TEXT("워커 요청 %d · 허용 %d · 최대 동시 %d · 완료 %d/%d · 실패 %d%s"),Requested,Allowed,MaxObserved,Done,Queue.Tasks.Num(),Failed,BanNative?TEXT(" · native 안전 비활성/RHI"):TEXT(""));}
FTrainBatchProcessPool::~FTrainBatchProcessPool()
{
    // An abrupt coordinator death leaves workers/lease records available for adoption.
    for(auto& S:Slots)if(S.Process.IsValid())FPlatformProcess::CloseProc(S.Process);
    if(OwnerLease){OwnerLease.Reset();IFileManager::Get().Delete(*(WorkerBase/TEXT("owner.lock")),false,true);}
}
