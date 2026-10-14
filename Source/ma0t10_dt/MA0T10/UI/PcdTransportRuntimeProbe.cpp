#include "PcdTransportRuntimeProbe.h"
#include "Tickable.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHitchDiagnostics.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"

namespace
{
class FPcdRuntimeProbe final:public FTickableGameObject
{
    FString Directory;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AVirtualSensorCoordinator> Manager;
    TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
    TWeakObjectPtr<ASlabActor> Slab;
    double Created=FPlatformTime::Seconds(),ReadyAt=0,MeasureAt=0,EndAt=0,NextSample=0,NextChange=0;
    double Seconds=90,Interval=.05,MaxReceiptGap=0,LastReceipt=0;
    int32 Switches=0,RejectedChanges=0,Suspensions=0;
    bool StartedScenario=false,WasSuspended=false,Finishing=false,Switching=false,Required=false,Measured=false,Preview=false;
    int64 Receipts=0,Bytes=0,ReceiptStart=0,ByteStart=0;
    FString Csv=TEXT("elapsed,input,serialized,submitted,receipt,bytes,raw_frames,raw_bytes,input_q,prepared_q,overloads,resumes,discarded,receipt_ms,serialization_ms,slab_sec,slab_state,frame_id\n");
    FDelegateHandle Observation;
    TFuture<bool> Writer;
    void Finish(const FString& Reason)
    {
        if(Finishing)return;Finishing=true;
        if(World.IsValid())
        {
            Receipts=Bytes=0;for(const auto& R:World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->GetStreamTelemetry())if(R.StreamKind==EVirtualSensorStreamKind::PointCloud){Receipts+=R.ReceiptCount;Bytes+=R.SubmittedBytes;}
        }
        const auto H=FVirtualSensorHitchDiagnostics::Snapshot();FVirtualSensorHitchDiagnostics::Stop();
        TSharedPtr<FJsonObject> Json=MakeShared<FJsonObject>();Json->SetStringField(TEXT("reason"),Reason);Json->SetBoolField(TEXT("valid"),Reason==TEXT("finished"));
        Json->SetNumberField(TEXT("frames"),H.Frames);Json->SetNumberField(TEXT("hitchesOver150ms"),H.Hitches);Json->SetNumberField(TEXT("maxFrameMs"),H.MaxFrameMs);
        Json->SetNumberField(TEXT("socketIoInsideMutex"),H.SocketIoInsideMutex);Json->SetNumberField(TEXT("maxOutstandingHoldMs"),H.MaxOutstandingHoldMs);Json->SetNumberField(TEXT("maxRunPendingHoldMs"),H.MaxRunPendingHoldMs);
        Json->SetNumberField(TEXT("switchesApplied"),Switches);Json->SetNumberField(TEXT("switchesRejected"),RejectedChanges);Json->SetNumberField(TEXT("suspensions"),Suspensions);Json->SetBoolField(TEXT("requiredData"),Required);
        Json->SetNumberField(TEXT("maxReceiptGapSeconds"),MaxReceiptGap);Json->SetNumberField(TEXT("durationSeconds"),Seconds);
        Json->SetNumberField(TEXT("receiptHz"),FMath::Max<int64>(0,Receipts-ReceiptStart)/Seconds);Json->SetNumberField(TEXT("MBps"),FMath::Max<int64>(0,Bytes-ByteStart)/Seconds/1e6);
        Json->SetBoolField(TEXT("worldPointPreview"),Preview);
        Json->SetStringField(TEXT("configuration"),
#if UE_BUILD_SHIPPING
            TEXT("Shipping")
#else
            TEXT("Development")
#endif
        );
        if(World.IsValid())if(auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>())
        {Json->SetNumberField(TEXT("socketWouldBlock"),Raw->GetSocketWouldBlockCount());Json->SetNumberField(TEXT("socketSendBufferBytes"),Raw->GetActualSocketSendBufferBytes());Raw->OnTransportObservation.Remove(Observation);}
        // Diagnostics disk IO belongs to a background task and never to the
        // frame being measured. Final orderly process exit is outside the window.
        FString Text;FJsonSerializer::Serialize(Json.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));
        const FString Dir=Directory,Samples=Csv,Hitches=H.HitchCsv;
        Writer=Async(EAsyncExecution::ThreadPool,[Dir,Text,Samples,Hitches](){
            IFileManager::Get().MakeDirectory(*Dir,true);
            return FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("result.json")))&&FFileHelper::SaveStringToFile(Samples,*(Dir/TEXT("samples.csv")))&&FFileHelper::SaveStringToFile(Hitches,*(Dir/TEXT("hitches.csv")));});
    }
public:
    explicit FPcdRuntimeProbe(FString In):Directory(MoveTemp(In))
    {
        FParse::Value(FCommandLine::Get(),TEXT("PcdOptSeconds="),Seconds);Seconds=FMath::Clamp(Seconds,20.0,600.0);
        FParse::Value(FCommandLine::Get(),TEXT("PcdOptInterval="),Interval);Interval=FMath::Clamp(Interval,.05,.2);
        Switching=FParse::Param(FCommandLine::Get(),TEXT("PcdOptSwitch"));Required=FParse::Param(FCommandLine::Get(),TEXT("PcdOptRequired"));
        Preview=FParse::Param(FCommandLine::Get(),TEXT("PcdOptPreview"));
    }
    ~FPcdRuntimeProbe(){FVirtualSensorHitchDiagnostics::Stop();}
    TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(FPcdRuntimeProbe,STATGROUP_Tickables);}
    ETickableTickType GetTickableTickType() const override{return ETickableTickType::Always;}
    void Tick(float) override
    {
        const double Now=FPlatformTime::Seconds();
        if(Finishing){if(Writer.IsValid()&&Writer.IsReady()){const bool Saved=Writer.Get();FPlatformMisc::RequestExitWithStatus(false,Saved?0:2);}return;}
        if(!World.IsValid())
        {
            if(GEngine)for(const auto& Context:GEngine->GetWorldContexts())if(Context.World()&&Context.World()->IsGameWorld()){World=Context.World();break;}
            if(!World.IsValid()||!World->GetFirstPlayerController()){if(Now-Created>60)Finish(TEXT("world-timeout"));return;}
            for(TActorIterator<AVirtualSensorCoordinator> It(World.Get());It;++It){Manager=*It;break;}
            if(!Manager.IsValid()){Finish(TEXT("map-needs-coordinator"));return;}
            FTransform Pose;bool Found=false;for(auto* A:Manager->GetSensorActors())if(A&&A->GetSensorKind()==EVirtualSensorKind::Lidar){Pose=A->GetActorTransform();Found=true;break;}
            if(!Found){Finish(TEXT("map-needs-lidar-pose"));return;}
            for(auto* A:Manager->GetSensorActors())if(A){A->StopSensor();Manager->UnregisterSensorActor(A);}
            auto* T=Manager->SharedTransportComponent.Get();auto Profile=T->GetTransportProfile();
            Profile.BrokerUrl=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_URL"));Profile.UserName=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_USER"));
            if(Profile.BrokerUrl.IsEmpty()){Finish(TEXT("explicit-local-probe-broker-required"));return;}
            // Session-only credentials, same existing API as the rendered test.
            T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;T->ConfigureTransportProfile(Profile);
            T->SetSessionCredentials(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD")),FString());
            Manager->StreamPublisherComponent->SetTransportComponent(T);
            FString Error;Lidar=Cast<AVirtualLidarSensorActor>(World->GetSubsystem<UVirtualSensorLayoutSubsystem>()->CreateSensorAtPose(EVirtualSensorKind::Lidar,Pose,Error,false));
            if(!Lidar.IsValid()){Finish(TEXT("device-create-failed"));return;}
            FVirtualSensorEditableState State;Lidar->ReadEditableState(State);State.LidarScanInterval=Interval;State.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;
            if(!Lidar->ApplyEditableState(State,Error)){Finish(TEXT("device-config-failed"));return;}
            Lidar->ScanComponent->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Lidar->VisualizationComponent->SetWorldPointCloudEnabled(Preview);Lidar->StartSensor();
            auto* P=Manager->StreamPublisherComponent.Get();P->StopAllStreams(FString());FVirtualSensorStreamConfig C;C.StreamKind=EVirtualSensorStreamKind::PointCloud;C.SensorId=Lidar->GetSensorId();C.bEnabled=true;P->ConfigureStream(C);
            auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
            FVirtualSensorReceiveSelection Selection;Selection.bEnabled=true;Raw->ConfigureReceiver(Selection);
            Observation=Raw->OnTransportObservation.AddLambda([this](const FVirtualSensorTransportObservation& O){
                if(O.Phase==EVirtualSensorTransportObservationPhase::Receipt&&Measured&&O.Kind==EVirtualSensorStreamKind::PointCloud)
                {if(LastReceipt>0)MaxReceiptGap=FMath::Max(MaxReceiptGap,O.MonotonicSeconds-LastReceipt);LastReceipt=O.MonotonicSeconds;}});
            ReadyAt=Now;MeasureAt=Now+10;EndAt=MeasureAt+Seconds;NextChange=MeasureAt+2;return;
        }
        if(Now<MeasureAt)return;
        if(!Measured)
        {
            Measured=true;FVirtualSensorHitchDiagnostics::Start();ReceiptStart=Receipts;ByteStart=Bytes;
            auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();for(const auto& S:Raw->GetStreamTelemetry()){ReceiptStart+=S.ReceiptCount;ByteStart+=S.SubmittedBytes;}
        }
        if(Switching&&!StartedScenario)
        {
            for(TActorIterator<ASlabActor> It(World.Get());It;++It){Slab=*It;break;}
            if(!Slab.IsValid())Slab=World->SpawnActor<ASlabActor>();
            if(!Slab.IsValid()){Finish(TEXT("slab-fixture-create-failed"));return;}
            Slab->TargetSensorIds={Lidar->GetSensorId()};FSlabExecutionOptions Options;Options.Policy=Required?ESlabExecutionPolicy::RequireData:ESlabExecutionPolicy::ObservationAllowed;Slab->SetExecutionOptions(Options);
            FVirtualSlabSensorOutputSelection Outputs;Outputs.bPointCloud=true;Outputs.bLidarTelemetry=Outputs.bCameraImage=false;Slab->SetSensorOutputs(Outputs);
            TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower))),Json);
            const double Factor=(Seconds+30)/30;for(const auto& R:Json->GetArrayField(TEXT("DATA_MAP")))R->AsObject()->SetNumberField(TEXT("elapsed_sec"),R->AsObject()->GetNumberField(TEXT("elapsed_sec"))*Factor);
            Json->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("duration_sec"),Seconds+30);Json->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("sample_period_sec"),.05*Factor);
            FString Text;FJsonSerializer::Serialize(Json.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));if(!Slab->SubmitScenarioJson(Text)){Finish(TEXT("scenario-admission-failed"));return;}StartedScenario=true;
        }
        if(Switching&&Switches+RejectedChanges<12&&Now>=NextChange)
        {
            NextChange=Now+2;FVirtualSensorEditableState State;FString Error;Lidar->ReadEditableState(State);State.LidarScanInterval=(Switches+RejectedChanges)%2==0?.1:.05;
            if(Lidar->ApplyEditableState(State,Error))++Switches;else ++RejectedChanges;
        }
        if(Now>=NextSample)
        {
            NextSample=Now+1;auto* P=Manager->StreamPublisherComponent.Get();
            for(const auto& S:P->GetStreamStatuses())if(S.StreamKind==EVirtualSensorStreamKind::PointCloud&&S.SensorId==Lidar->GetSensorId())
            {
                if(S.bOverloadSuspended&&!WasSuspended)++Suspensions;WasSuspended=S.bOverloadSuspended;
                Receipts=S.ReceiptReceivedCount;Bytes=S.TotalSubmittedBytes;
                // Raw telemetry owns socket submission bytes for this backend.
                for(const auto& R:World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->GetStreamTelemetry())if(R.SensorId==S.SensorId&&R.StreamKind==S.StreamKind)Bytes=R.SubmittedBytes;
                const auto SS=Slab.IsValid()?Slab->GetSimulationStatus():FSlabSimulationStatus();
                Csv+=FString::Printf(TEXT("%.3f,%lld,%lld,%lld,%lld,%lld,%d,%lld,%d,%d,%lld,%lld,%lld,%.3f,%.3f,%.3f,%d,%lld\n"),Now-MeasureAt,S.InputFrameCount,S.SerializedFrameCount,S.SubmittedFrameCount,S.ReceiptReceivedCount,Bytes,S.RawOutstandingFrameCount,S.RawOutstandingBytes,S.InputQueueDepth,S.PreparedQueueDepth,S.OverloadCount,S.ResumeCount,S.OverloadDiscardedFrameCount,S.LastReceiptLatencyMs,S.LastSerializationLatencyMs,SS.ElapsedSec,int32(SS.State),Lidar->GetSensorRuntimeStatus().FrameId);
            }
        }
        if(Now>=EndAt)Finish(TEXT("finished"));
    }
};
TUniquePtr<FPcdRuntimeProbe> Probe;
}
void StartPcdTransportRuntimeProbe()
{
    FString Directory;if(!FParse::Value(FCommandLine::Get(),TEXT("PcdOptReport="),Directory))return;
    Directory=FPaths::ConvertRelativePathToFull(Directory);
    Probe=MakeUnique<FPcdRuntimeProbe>(Directory);
}
void StopPcdTransportRuntimeProbe(){Probe.Reset();}
