#include "TrainBatchCoordinatorProbe.h"
#include "TrainBatchSubsystem.h"
#include "TrainBatchCodec.h"
#include "TrainBatchProcessProtocol.h"
#include "TrainBatchDelivery.h"
#include "Device/Contracts/LocalCapturePath.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarScanComponent.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

struct FTrainCoordinatorProbeState
{
    TSharedPtr<FJsonObject> Config;
    FString Root,JobId,Manifest,Preset,Error;
    int32 Stage=0,Workers=1,ArchiveIndex=0;bool Restore=false,TestAuto=false,Ending=false,DefaultOff=false,BuildArchives=false;
    TArray<FTrainArchiveEntry> Entries;
    TArray<TSharedPtr<FJsonValue>> Archives;
    TSharedPtr<FTrainBatchDelivery,ESPMode::ThreadSafe> Delivery;
    double Started=FPlatformTime::Seconds(),NextStatus=0,ConfiguredAt=0;
    TWeakObjectPtr<UTrainBatchSubsystem> Service;
};
bool UTrainBatchCoordinatorProbe::ShouldCreateSubsystem(UObject* Outer) const
{const auto* W=Cast<UWorld>(Outer);FString P;return W&&W->WorldType==EWorldType::Game&&(FParse::Value(FCommandLine::Get(),TEXT("TrainBatchCoordinator="),P)||FParse::Value(FCommandLine::Get(),TEXT("TrainBatchDefaultOffSmoke="),P));}
void UTrainBatchCoordinatorProbe::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);State=MakeShared<FTrainCoordinatorProbeState>();auto& S=*State;FString Path;
    if(FParse::Value(FCommandLine::Get(),TEXT("TrainBatchDefaultOffSmoke="),Path))
    {
        S.Root=FPaths::GetPath(Path);S.DefaultOff=true;
        if(!LocalCapturePath::SafeDescendant(LocalCapturePath::Full(FPaths::ProjectDir()/TEXT("Saved/Reports")),Path)){State.Reset();FPlatformMisc::RequestExitWithStatus(false,1);return;}
        S.Service=World.GetSubsystem<UTrainBatchSubsystem>();World.GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.1f,true);return;
    }
    FParse::Value(FCommandLine::Get(),TEXT("TrainBatchCoordinator="),Path);S.Root=FPaths::GetPath(Path);S.Config=TrainBatchProcess::Read(Path,65536);
    const FString Reports=LocalCapturePath::Full(FPaths::ProjectDir()/TEXT("Saved/Reports"));
    if(!S.Config||!LocalCapturePath::SafeDescendant(Reports,Path)){Finish(false,TEXT("Coordinator config must be under project Saved/Reports"));return;}
    double W=0;if(!S.Config->TryGetStringField(TEXT("job_id"),S.JobId)||!TrainBatchCodec::CanonicalGuid(S.JobId)||!S.Config->TryGetNumberField(TEXT("workers"),W)||W<1||W>4||W!=FMath::FloorToDouble(W)||!S.Config->TryGetStringField(TEXT("preset_id"),S.Preset)||!TrainBatchCodec::SafeId(S.Preset))
    {Finish(false,TEXT("Coordinator identity/settings rejected"));return;}
    S.Workers=int32(W);S.Config->TryGetStringField(TEXT("manifest"),S.Manifest);S.Config->TryGetBoolField(TEXT("restore"),S.Restore);S.Config->TryGetBoolField(TEXT("test_auto"),S.TestAuto);S.Config->TryGetBoolField(TEXT("build_archives"),S.BuildArchives);
#if !(WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR)
    if(S.TestAuto){Finish(false,TEXT("Automatic test actor selection is Editor automation only"));return;}
#endif
    if(auto* GI=World.GetGameInstance())if(auto* WS=GI->GetSubsystem<UDxWebSocketSubsystem>())WS->DisconnectStompClient(TMap<FName,FString>());
    for(TActorIterator<AVirtualLidarSensorActor> It(&World);It;++It)It->ScanComponent->bAutoStartScan=false;
    FString Map,Error;
    if(S.Config->TryGetStringField(TEXT("map"),Map))
    {
        const auto Entry=TrainBatchProcess::EnterAssignedMap(&World,Map,Error);
        if(Entry==TrainBatchProcess::EMapEntry::Rejected){Finish(false,Error);return;}
        if(Entry==TrainBatchProcess::EMapEntry::Travelling)return;
    }
    S.Service=World.GetSubsystem<UTrainBatchSubsystem>();World.GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.1f,true);
}
void UTrainBatchCoordinatorProbe::Pump()
{
    if(!State||State->Ending)return;auto& S=*State;const double Now=FPlatformTime::Seconds();
    if(!S.Service.IsValid()||Now-S.Started>1800){Finish(false,TEXT("Coordinator unavailable/timeout"));return;}
    if(S.DefaultOff)
    {
        if(!S.Service->IsDormantForDiagnostics()){Finish(false,TEXT("Default-off training allocated state/handler/timer"));return;}
        if(Now-S.Started>=10)Finish(true,FString());return;
    }
    if(S.Stage==0)
    {
        FString SlabName,LidarName;S.Config->TryGetStringField(TEXT("slab_actor"),SlabName);S.Config->TryGetStringField(TEXT("lidar_actor"),LidarName);
        ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;
        for(TActorIterator<ASlabActor> It(GetWorld());It;++It)if(It->GetName()==SlabName||(S.TestAuto&&!Slab)){Slab=*It;}
        for(TActorIterator<AVirtualLidarSensorActor> It(GetWorld());It;++It)if(It->GetName()==LidarName||(S.TestAuto&&!Lidar)){Lidar=*It;}
        if(!Slab||!Lidar){Finish(false,TEXT("Coordinator requires explicit saved-map Slab/LiDAR actors"));return;}
        Slab->StopSimulation();Lidar->StopSensor();
        if(S.TestAuto)
        {
            Slab->SetDimensionUnit(ESlabInputUnit::Centimeters);Slab->SetPositionUnit(ESlabInputUnit::Centimeters);Slab->SetDiagnosticHelpersVisible(false);
            auto* Scan=Lidar->ScanComponent.Get();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::HardwareRayTracing;Scan->bEnableRangeNoise=false;Scan->OutputMode=EVirtualLidarOutputMode::None;
        }
        FString E;if(!S.Service->SetEnabled(true,E)||!S.Service->ConfigureTargets(S.Preset,Slab,Lidar,nullptr,E)||!S.Service->SetProcessWorkers(S.Workers,E)){Finish(false,E);return;}
        S.Stage=1;S.ConfiguredAt=Now;return;
    }
    if(S.Stage==1)
    {
        if(Now-S.ConfiguredAt<2)return;FString E;const bool Accepted=S.Restore?S.Service->RestoreJob(S.JobId,E):S.Service->ImportManifest(S.Manifest,E);
        if(!Accepted){Finish(false,E);return;}S.Stage=2;S.ConfiguredAt=Now;
    }
    if(S.Stage==2)
    {
        const auto* J=S.Service->Jobs().FindByPredicate([&](const auto& V){return V.Spec.Id==S.JobId;});
        if(J&&J->IsReady())
        {
            FString E;if(!S.Service->StartJob(S.JobId,E)){Finish(false,E);return;}S.Stage=3;
        }
        else if(Now-S.ConfiguredAt>60){Finish(false,S.Service->GetStatus());return;}
    }
    if(Now>=S.NextStatus)
    {
        S.NextStatus=Now+1;auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("phase"),S.Stage==3?TEXT("running"):TEXT("preparing"));O->SetStringField(TEXT("job_id"),S.JobId);O->SetStringField(TEXT("status"),S.Service->GetStatus());
        for(const auto& J:S.Service->Jobs())if(J.Spec.Id==S.JobId){O->SetNumberField(TEXT("completed"),J.CompleteCount());O->SetNumberField(TEXT("requested"),J.Items.Num());}
        TrainBatchProcess::Write(S.Root,S.Root/TEXT("coordinator-status.json"),O);
    }
    if(S.Stage==3)
    {
        const auto* J=S.Service->Jobs().FindByPredicate([&](const auto& V){return V.Spec.Id==S.JobId;});
        if(J&&J->CompleteCount()==J->Items.Num())
        {
            FString E;TArray<FString> Excluded;
            if(S.BuildArchives&&!TrainBatchDelivery::BuildJobEntries(*J,false,S.Entries,Excluded,E)){Finish(false,E);return;}
            if(!S.Service->SetEnabled(false,E)){Finish(false,E);return;}
            if(S.BuildArchives)S.Stage=4;else Finish(true,FString());
        }
        else if(S.Service->GetStatus().StartsWith(TEXT("워커 Job 종료"))||S.Service->GetStatus().StartsWith(TEXT("GPU/driver")))Finish(false,S.Service->GetStatus());
    }
    if(S.Stage==4)
    {
        if(S.Delivery)
        {
            if(S.Delivery->IsRunning())return;
            const auto D=S.Delivery->Status();
            if(D.Phase!=ETrainDeliveryPhase::Done){Finish(false,D.Error);return;}
            auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("archive"),D.ArchivePath);A->SetStringField(TEXT("sha256"),D.ArchiveSha256);A->SetNumberField(TEXT("bytes"),double(D.ArchiveBytes));S.Archives.Add(MakeShared<FJsonValueObject>(A));
            S.Delivery.Reset();++S.ArchiveIndex;
        }
        if(S.ArchiveIndex==2){Finish(true,FString());return;}
        FTrainDeliveryRequest R;R.JobId=S.JobId;R.Entries=S.Entries;R.bUpload=false;
        R.Settings.Format=S.ArchiveIndex==0?ETrainArchiveFormat::Zip:ETrainArchiveFormat::TarGz;
        R.OutputDir=S.Root/(S.ArchiveIndex==0?TEXT("zip"):TEXT("tar-gz"));
        S.Delivery=MakeShared<FTrainBatchDelivery,ESPMode::ThreadSafe>();FString E;if(!S.Delivery->Start(R,E))Finish(false,E);
    }
}
void UTrainBatchCoordinatorProbe::Finish(bool Success,const FString& Error)
{
    if(!State||State->Ending)return;State->Ending=true;State->Error=Error;
    if(State->Service.IsValid())State->Service->CancelJob();if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("phase"),Success?TEXT("completed"):TEXT("failed"));O->SetBoolField(TEXT("success"),Success);O->SetStringField(TEXT("job_id"),State->JobId);O->SetStringField(TEXT("error"),Error);O->SetStringField(TEXT("jobs_root"),TrainBatchCodec::JobsRoot());
    O->SetBoolField(TEXT("default_off_verified"),State->DefaultOff&&Success);O->SetArrayField(TEXT("archives"),State->Archives);
    TrainBatchProcess::Write(State->Root,State->Root/TEXT("coordinator-status.json"),O);FPlatformMisc::RequestExitWithStatus(false,Success?0:1);
}
void UTrainBatchCoordinatorProbe::Deinitialize()
{if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);if(State&&State->Delivery){State->Delivery->Cancel();State->Delivery->Wait();}State.Reset();Super::Deinitialize();}
