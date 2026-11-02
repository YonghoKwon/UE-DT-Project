#include "TrainBatchWorkerSubsystem.h"
#include "TrainBatchProcessProtocol.h"
#include "TrainBatchStorage.h"
#include "ma0t10_dt/MA0T10/Contracts/LocalCapturePath.h"
#include "ma0t10_dt/MA0T10/Contracts/ConsumeReadyFuture.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "Core/DxWebSocketSubsystem.h"
#include "SensorOwnedRayTracing.h"
#include "SensorNativeSafetyStatus.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
struct FWorkerOp {FString Error;TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Plan;};
}
struct FTrainBatchWorkerState
{
    TSharedPtr<FJsonObject> Config;
    FString ConfigPath,IpcRoot,JobId,Lease,RunId,Phase=TEXT("booting"),Error;
    uint64 Epoch=0,Sequence=0,PeakGpu=0;uint32 ParentPid=0;uint64 ParentStamp=0;
    int32 Index=INDEX_NONE,Stage=0;double Started=FPlatformTime::Seconds(),NextHeartbeat=0,CanaryAt=0,OrphanAt=0;
    bool Ending=false;
    TWeakObjectPtr<ASlabActor> Slab;
    TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    FTrainBatchJobSpec Spec;
    TSharedRef<FTrainBatchStorage,ESPMode::ThreadSafe> Storage=MakeShared<FTrainBatchStorage,ESPMode::ThreadSafe>();
    TSharedRef<std::atomic<bool>,ESPMode::ThreadSafe> Cancel=MakeShared<std::atomic<bool>,ESPMode::ThreadSafe>(false);
    TFuture<FWorkerOp> Work;
    TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Prepared;
};
bool UTrainBatchWorkerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* W=Cast<UWorld>(Outer);FString Path;
    return W&&W->WorldType==EWorldType::Game&&FParse::Value(FCommandLine::Get(),TEXT("TrainBatchWorker="),Path)&&!Path.IsEmpty();
}
void UTrainBatchWorkerSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);State=MakeShared<FTrainBatchWorkerState>();auto& S=*State;
    FParse::Value(FCommandLine::Get(),TEXT("TrainBatchWorker="),S.ConfigPath);S.IpcRoot=FPaths::GetPath(S.ConfigPath);
    S.Config=TrainBatchProcess::Read(S.ConfigPath,1024*1024);
    if(!S.Config||!LocalCapturePath::SafeDescendant(S.IpcRoot,S.ConfigPath)){Finish(TEXT("failed"),TEXT("Invalid worker configuration"));return;}
    FString Stamp;double Parent=0,Index=0,Epoch=0;
    if(!S.Config->TryGetStringField(TEXT("job_id"),S.JobId)||!TrainBatchCodec::CanonicalGuid(S.JobId)||!S.Config->TryGetStringField(TEXT("lease"),S.Lease)||!TrainBatchCodec::CanonicalGuid(S.Lease)||!S.Config->TryGetNumberField(TEXT("epoch"),Epoch)||Epoch<1||Epoch>100000||Epoch!=FMath::FloorToDouble(Epoch)||!S.Config->TryGetNumberField(TEXT("index"),Index)||Index<0||Index>=TrainBatchContract::MaxScenarios||Index!=FMath::FloorToDouble(Index)||!S.Config->TryGetNumberField(TEXT("parent_pid"),Parent)||!S.Config->TryGetStringField(TEXT("parent_stamp"),Stamp)||!LexTryParseString(S.ParentStamp,*Stamp))
    {Finish(TEXT("failed"),TEXT("Worker identity rejected"));return;}
    S.ParentPid=uint32(Parent);S.Epoch=uint64(Epoch);S.Index=int32(Index);
    if(auto* GI=World.GetGameInstance())if(auto* WS=GI->GetSubsystem<UDxWebSocketSubsystem>())WS->DisconnectStompClient(TMap<FName,FString>());
    // Only this isolated worker World is affected. Stop automatic acquisition before Actor BeginPlay.
    for(TActorIterator<AVirtualLidarSensorActor> It(&World);It;++It)It->ScanComponent->bAutoStartScan=false;
    for(TActorIterator<AVirtualCameraSensorActor> It(&World);It;++It)It->CaptureComponent->bAutoStartCapture=false;
    World.GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.05f,true);
}
void UTrainBatchWorkerSubsystem::Pump()
{
    if(!State||State->Ending)return;auto& S=*State;const double Now=FPlatformTime::Seconds();
    if(Now-S.Started>7200){Finish(TEXT("failed"),TEXT("Worker hard deadline"));return;}
    const auto Control=TrainBatchProcess::Read(S.IpcRoot/TEXT("control.json"));
    bool Paused=false;
    if(Control)
    {
        FString Lease;bool Cancel=false;Control->TryGetStringField(TEXT("lease"),Lease);Control->TryGetBoolField(TEXT("cancel"),Cancel);
        if(Lease==S.Lease&&Cancel){Finish(TEXT("cancelled"));return;}
        if(Lease==S.Lease)Control->TryGetBoolField(TEXT("pause"),Paused);
    }
    const auto Owner=TrainBatchProcess::Read(S.IpcRoot/TEXT("owner.json"));
    if(Owner){FString Lease,Stamp;double Pid=0;if(Owner->TryGetStringField(TEXT("lease"),Lease)&&Lease==S.Lease&&Owner->TryGetNumberField(TEXT("pid"),Pid)&&Owner->TryGetStringField(TEXT("stamp"),Stamp)){uint64 Start=0;if(LexTryParseString(Start,*Stamp)&&TrainBatchProcess::SameProcess(uint32(Pid),Start)){S.ParentPid=uint32(Pid);S.ParentStamp=Start;}}}
    FString PoolLock;S.Config->TryGetStringField(TEXT("pool_lock"),PoolLock);const auto PoolOwner=TrainBatchProcess::Read(PoolLock);double OwnerPid=0;FString OwnerStart;
    const bool HasLease=PoolOwner&&PoolOwner->TryGetNumberField(TEXT("pid"),OwnerPid)&&PoolOwner->TryGetStringField(TEXT("stamp"),OwnerStart)&&uint32(OwnerPid)==S.ParentPid&&OwnerStart==LexToString(S.ParentStamp);
    if(!HasLease||!TrainBatchProcess::SameProcess(S.ParentPid,S.ParentStamp))
    {
        if(!S.OrphanAt)S.OrphanAt=Now;
        if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())D->YieldPlannedCapture();
        S.Phase=TEXT("orphaned");if(Now-S.OrphanAt>60){Finish(TEXT("cancelled"),TEXT("Coordinator lease lost"));return;}
    }
    else S.OrphanAt=0;
    if(Now>=S.NextHeartbeat)
    {
        S.NextHeartbeat=Now+2;TrainBatchProcess::FGpuBudget G;if(TrainBatchProcess::QueryGpuBudget(G))S.PeakGpu=FMath::Max(S.PeakGpu,G.Usage);
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("lease"),S.Lease);O->SetNumberField(TEXT("epoch"),double(S.Epoch));O->SetNumberField(TEXT("sequence"),double(++S.Sequence));O->SetStringField(TEXT("phase"),S.Phase);O->SetNumberField(TEXT("pid"),FPlatformProcess::GetCurrentProcessId());O->SetNumberField(TEXT("peak_gpu_bytes"),double(S.PeakGpu));
        O->SetNumberField(TEXT("native_status"),int32(SensorNativeSafety::GetStatus()));O->SetBoolField(TEXT("native"),SensorOwnedRayTracing::UsesNativeBackend());
        O->SetNumberField(TEXT("completed_frames"),GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>()->CompletedScenes());TrainBatchProcess::Write(S.IpcRoot,S.IpcRoot/TEXT("heartbeat.json"),O);
    }
    if(S.OrphanAt||Paused){GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>()->YieldPlannedCapture();return;}
    if(S.Work.IsValid())
    {
        if(!S.Work.IsReady())return;auto R=MA0T10::ConsumeReadyFuture(S.Work);
        if(!R.Error.IsEmpty()){Finish(TEXT("failed"),R.Error);return;}
        if(S.Stage==2){S.Prepared=MoveTemp(R.Plan);S.Stage=3;}
        else if(S.Stage==5){Finish(TEXT("completed"));return;}
    }
    if(S.Stage==0)
    {
        FString SlabName,LidarId,CameraId,LidarJson,CameraJson,LidarName,CameraName;
        if(!S.Config->TryGetStringField(TEXT("slab_actor"),SlabName)||!S.Config->TryGetStringField(TEXT("lidar_id"),LidarId)||!S.Config->TryGetStringField(TEXT("lidar_state"),LidarJson)){Finish(TEXT("failed"),TEXT("Missing worker targets"));return;}
        S.Config->TryGetStringField(TEXT("camera_id"),CameraId);S.Config->TryGetStringField(TEXT("camera_state"),CameraJson);
        S.Config->TryGetStringField(TEXT("lidar_actor"),LidarName);S.Config->TryGetStringField(TEXT("camera_actor"),CameraName);
        int32 NS=0,NL=0,NC=0;
        for(TActorIterator<AVirtualSensorActorBase> It(GetWorld());It;++It)It->StopSensor();
        for(TActorIterator<ASlabActor> It(GetWorld());It;++It)if(It->GetName()==SlabName){S.Slab=*It;++NS;}
        for(TActorIterator<AVirtualLidarSensorActor> It(GetWorld());It;++It)if(It->GetName()==LidarName){S.Lidar=*It;++NL;}
        if(!CameraId.IsEmpty())for(TActorIterator<AVirtualCameraSensorActor> It(GetWorld());It;++It)if(It->GetName()==CameraName){S.Camera=*It;++NC;}
        if(NS!=1||NL!=1||(!CameraId.IsEmpty()&&NC!=1)){Finish(TEXT("failed"),TEXT("Worker preset actor mismatch"));return;}
        FString Scene;S.Config->TryGetStringField(TEXT("scene_signature"),Scene);
        if(Scene!=TrainBatchProcess::SceneSignature(GetWorld(),S.Slab.Get(),S.Lidar.Get(),S.Camera.Get())){Finish(TEXT("failed"),TEXT("Worker saved-scene geometry differs from coordinator"));return;}
        FVirtualSensorEditableState L,C;FString Error;
        if(!FJsonObjectConverter::JsonObjectStringToUStruct(LidarJson,&L,0,0,true)){Finish(TEXT("failed"),TEXT("Worker LiDAR preset JSON rejected"));return;}
        L.OriginalSensorId=S.Lidar->GetSensorId();
        if(!S.Lidar->ApplyEditableState(L,Error)){Finish(TEXT("failed"),Error);return;}
        double Backend=0,Seed=0;bool Noise=false;
        if(!S.Config->TryGetNumberField(TEXT("lidar_backend"),Backend)||Backend<0||Backend>3||Backend!=FMath::FloorToDouble(Backend)||!S.Config->TryGetBoolField(TEXT("range_noise"),Noise)||!S.Config->TryGetNumberField(TEXT("noise_seed"),Seed)||Seed<MIN_int32||Seed>MAX_int32||Seed!=FMath::FloorToDouble(Seed)||!S.Lidar->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend(int32(Backend)),Error))
        {Finish(TEXT("failed"),TEXT("Worker acquisition backend/noise snapshot rejected"));return;}
        S.Lidar->ScanComponent->bEnableRangeNoise=Noise;S.Lidar->ScanComponent->DeterministicNoiseSeed=int32(Seed);
        if(!CameraId.IsEmpty())
        {if(!FJsonObjectConverter::JsonObjectStringToUStruct(CameraJson,&C,0,0,true)){Finish(TEXT("failed"),TEXT("Worker camera preset JSON rejected"));return;}C.OriginalSensorId=S.Camera->GetSensorId();if(!S.Camera->ApplyEditableState(C,Error)){Finish(TEXT("failed"),Error);return;}}
        double Dim=0,Pos=0,Surface=0,Roughness=0,Oxidation=0,Emissive=0;bool Hot=false,Linked=false;
        if(!S.Config->TryGetNumberField(TEXT("dimension_unit"),Dim)||Dim<0||Dim>2||!S.Config->TryGetNumberField(TEXT("position_unit"),Pos)||Pos<0||Pos>2||!S.Config->TryGetNumberField(TEXT("surface"),Surface)||Surface<0||Surface>4||!S.Config->TryGetNumberField(TEXT("roughness"),Roughness)||!S.Config->TryGetNumberField(TEXT("oxidation"),Oxidation)||!S.Config->TryGetNumberField(TEXT("emissive"),Emissive)||!S.Config->TryGetBoolField(TEXT("hot"),Hot))
        {Finish(TEXT("failed"),TEXT("Worker Slab preset rejected"));return;}
        S.Config->TryGetBoolField(TEXT("surface_linked"),Linked);
        S.Slab->SetDimensionUnit(ESlabInputUnit(int32(Dim)));S.Slab->SetPositionUnit(ESlabInputUnit(int32(Pos)));if(!S.Slab->ApplySurfacePreset(ESlabSurfacePreset(int32(Surface)),Linked,Error)){Finish(TEXT("failed"),Error);return;}
        S.Slab->Roughness=Roughness;S.Slab->Oxidation=Oxidation;S.Slab->bHotAppearance=Hot;S.Slab->EmissiveStrength=Emissive;S.Slab->RefreshDatasetAppearance();
        S.Lidar->ScanComponent->OutputMode=EVirtualLidarOutputMode::None;
        FString ManifestPath,InputRoot;if(!S.Config->TryGetStringField(TEXT("manifest"),ManifestPath)||!S.Config->TryGetStringField(TEXT("input_root"),InputRoot)||!LocalCapturePath::SafeDescendant(InputRoot,ManifestPath)){Finish(TEXT("failed"),TEXT("Worker input boundary"));return;}
        const auto Storage=S.Storage;const FString JobId=S.JobId;const int32 Index=S.Index;const auto Cancel=S.Cancel;
        S.Stage=2;S.Phase=TEXT("preparing");
        const TSharedPtr<FJsonObject>* Transform=nullptr;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if(!S.Config->TryGetObjectField(TEXT("track"),Transform)||!(*Transform)->TryGetArrayField(TEXT("values"),Values)||Values->Num()!=10){Finish(TEXT("failed"),TEXT("Worker track snapshot missing"));return;}
        double V[10];for(int32 I=0;I<10;++I)if(!(*Values)[I]->TryGetNumber(V[I])||!FMath::IsFinite(V[I])){Finish(TEXT("failed"),TEXT("Worker track snapshot invalid"));return;}
        const FTransform Track(FQuat(V[3],V[4],V[5],V[6]),FVector(V[0],V[1],V[2]),FVector(V[7],V[8],V[9]));if(!Track.GetRotation().IsNormalized()){Finish(TEXT("failed"),TEXT("Worker track rotation invalid"));return;}
        const auto Dimension=S.Slab->GetDimensionUnit(),Position=S.Slab->GetPositionUnit();const uint32 Configuration=S.Slab->GetMetricsConfigurationHash();
        S.Work=Async(EAsyncExecution::ThreadPool,[Storage,ManifestPath,InputRoot,JobId,Index,Track,Dimension,Position,Configuration,Cancel]()
        {
            FWorkerOp R;FString Raw;FTrainBatchMessage M;
            if(!FTrainBatchStorage::ReadUtf8(ManifestPath,TrainBatchContract::ManifestBytes,Raw)||!TrainBatchCodec::Parse(Raw,M,R.Error)||M.Kind!=FTrainBatchMessage::EKind::Job||M.Job.Id!=JobId||!M.Job.Scenarios.IsValidIndex(Index)){R.Error=TEXT("Worker manifest rejected");return R;}
            const FString RawPath=InputRoot/JobId/TEXT("input")/M.Job.Scenarios[Index].Id/TEXT("original.json");
            if(!LocalCapturePath::SafeDescendant(InputRoot,RawPath)){R.Error=TEXT("Worker original path rejected");return R;}
            if(!Storage->Receive(M,R.Error)){R.Error.Reset();if(!Storage->Restore(JobId,R.Error))return R;}
            FString Input;FSlabScenarioDataPtr Data;
            if(Cancel->load()||!FTrainBatchStorage::ReadUtf8(RawPath,TrainBatchContract::ScenarioBytes,Input)||!TrainBatchCodec::ValidateScenario(M.Job.Scenarios[Index],Input,Data,R.Error)){if(R.Error.IsEmpty())R.Error=TEXT("Worker original verification failed");return R;}
            FTrainBatchMessage Item;Item.Kind=FTrainBatchMessage::EKind::Scenario;Item.JobId=JobId;Item.ScenarioId=M.Job.Scenarios[Index].Id;Item.InputSha256=M.Job.Scenarios[Index].InputSha256;Item.InputBytes=M.Job.Scenarios[Index].InputBytes;Item.Scenario=Data;
            if(!Storage->Receive(Item,R.Error))return R;
            if(!Storage->ReleaseLease(JobId,Index,R.Error))return R;
            R.Plan=Storage->Prepare(JobId,Index,Track,Dimension,Position,Configuration,16*1024*1024,R.Error);return R;
        });return;
    }
    if(S.Stage==3)
    {
        if(!S.Prepared){Finish(TEXT("failed"),TEXT("Worker plan missing"));return;}
        auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();FString Error;
        if(!S.Lidar->CanEditSensorConfiguration(Error))return;
        const auto& Job=S.Storage->Jobs[0].Spec;FVirtualDatasetSettings C;C.Enabled=true;C.Name=TEXT("train_worker");C.Scenes=S.Prepared->Frames.Num();C.SettleFrames=1;C.CameraTruth=Job.CameraTruth;
        S.RunId=S.Prepared->RunId;
        if(!D->Start(C,S.Slab.Get(),S.Lidar.Get(),Job.CaptureCamera?S.Camera.Get():nullptr,nullptr,Error,S.Prepared)){Finish(TEXT("failed"),Error);return;}
        S.Prepared.Reset();S.Stage=4;S.Phase=TEXT("capturing");return;
    }
    if(S.Stage==4)
    {
        auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();if(D->CompletedScenes()>0&&SensorNativeSafety::GetStatus()==SensorNativeSafety::EStatus::Recovery){Finish(TEXT("recovery"));return;}
        if(D->IsPaused()){FString Error;if(!D->Resume(Error)){Finish(TEXT("failed"),Error);}return;}if(D->IsActive())return;
        if(SensorNativeSafety::GetStatus()==SensorNativeSafety::EStatus::Recovery){Finish(TEXT("recovery"));return;}
        const bool Success=D->HasSucceeded();const FString Why=D->Message;const auto Storage=S.Storage;const auto Cancel=S.Cancel;const FString Id=S.JobId;const int32 Index=S.Index;
        S.Stage=5;S.Phase=TEXT("verifying");S.Work=Async(EAsyncExecution::ThreadPool,[Storage,Cancel,Id,Index,Success,Why](){FWorkerOp R;if(!Storage->Finish(Id,Index,Success,Why,*Cancel,R.Error)&&R.Error.IsEmpty())R.Error=TEXT("Worker capture/verification failed");return R;});
    }
}
void UTrainBatchWorkerSubsystem::Finish(const FString& Phase,const FString& Error)
{
    if(!State||State->Ending)return;auto& S=*State;S.Ending=true;S.Cancel->store(true);
    if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())D->Stop();GetWorld()->GetTimerManager().ClearTimer(Timer);
    TrainBatchProcess::FGpuBudget G;if(TrainBatchProcess::QueryGpuBudget(G))S.PeakGpu=FMath::Max(S.PeakGpu,G.Usage);
    const auto Stats=SensorOwnedRayTracing::GetSubmissionStats();auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("lease"),S.Lease);O->SetNumberField(TEXT("epoch"),double(S.Epoch));O->SetStringField(TEXT("phase"),Phase);O->SetStringField(TEXT("error"),Error);O->SetStringField(TEXT("run_uuid"),S.RunId);O->SetNumberField(TEXT("peak_gpu_bytes"),double(S.PeakGpu));O->SetNumberField(TEXT("native_status"),int32(SensorNativeSafety::GetStatus()));O->SetNumberField(TEXT("native_dispatches"),double(Stats.NativeDispatches));O->SetNumberField(TEXT("native_fallbacks"),double(Stats.NativeFallbacks));O->SetStringField(TEXT("gpu_identity"),G.Identity);O->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-S.Started);
    TrainBatchProcess::Write(S.IpcRoot,S.IpcRoot/TEXT("result.json"),O);FPlatformMisc::RequestExitWithStatus(false,Phase==TEXT("failed")?1:0);
}
void UTrainBatchWorkerSubsystem::Deinitialize()
{if(State)State->Cancel->store(true);if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);State.Reset();Super::Deinitialize();}
