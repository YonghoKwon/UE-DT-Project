#include "AutomationControlSubsystem.h"
#include "AutomationProtocol.h"
#include "AutomationLoopbackServer.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "RHIGlobals.h"
#include <atomic>

using namespace MA0T10Automation;
namespace
{
struct FScenarioLoad {FSlabScenarioDataPtr Data;FString Error;};
struct FControlErrors final:FOutputDevice
{
    std::atomic<int64> Count{0};
    void Serialize(const TCHAR*,ELogVerbosity::Type V,const FName&) override {if(V<=ELogVerbosity::Error)++Count;}
    bool CanBeUsedOnAnyThread() const override{return true;}
};
template<class T>T* NamedActor(UWorld* W,const FString& Name)
{T* Found=nullptr;for(TActorIterator<T> It(W);It;++It)if(It->GetName()==Name){if(Found)return nullptr;Found=*It;}return Found;}
AVirtualSensorCoordinator* Coordinator(UWorld* W)
{AVirtualSensorCoordinator* Found=nullptr;for(TActorIterator<AVirtualSensorCoordinator> It(W);It;++It){if(Found)return nullptr;Found=*It;}return Found;}
AVirtualSensorActorBase* Sensor(UWorld* W,const FString& Id)
{auto* C=Coordinator(W);if(!C)return nullptr;AVirtualSensorActorBase* Found=nullptr;for(auto* S:C->GetSensorActors())if(IsValid(S)&&S->GetSensorId()==Id){if(Found)return nullptr;Found=S;}return Found;}
USlabScenarioReplaySubsystem* Replay(UWorld* W)
{return W&&W->GetGameInstance()?W->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>():nullptr;}
double Num(const FJsonObject& C,const TCHAR* K,double Default)
{double V=Default;C.TryGetNumberField(K,V);return V;}
bool Flag(const FJsonObject& C,const TCHAR* K)
{bool V=false;C.TryGetBoolField(K,V);return V;}
FString Str(const FJsonObject& C,const TCHAR* K)
{FString V;C.TryGetStringField(K,V);return V;}
template<class T>bool EnumValue(const FString& Text,T& Out)
{const auto* E=StaticEnum<T>();const int64 V=E->GetValueByNameString(Text);if(V==INDEX_NONE||V==E->GetMaxEnumValue())return false;Out=T(V);return true;}
TSharedRef<FJsonObject> Answer(bool Ok,const FString& Error=FString())
{auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),Ok);if(!Error.IsEmpty())R->SetStringField(TEXT("error"),Error);return R;}
TSharedRef<FJsonObject> SensorJson(AVirtualSensorActorBase* A)
{
    auto R=MakeShared<FJsonObject>();FVirtualSensorEditableState S;A->ReadEditableState(S);
    R->SetStringField(TEXT("sensor_id"),A->GetSensorId());R->SetStringField(TEXT("actor"),A->GetName());
    R->SetStringField(TEXT("kind"),A->GetSensorKind()==EVirtualSensorKind::Camera?TEXT("camera"):TEXT("lidar"));
    R->SetBoolField(TEXT("running"),A->IsSensorRunning());R->SetNumberField(TEXT("revision"),A->GetConfigurationRevision());
    const bool Camera=A->GetSensorKind()==EVirtualSensorKind::Camera;
    R->SetNumberField(TEXT("interval_sec"),Camera?S.CameraCaptureInterval:S.LidarScanInterval);
    R->SetStringField(TEXT("quality"),StaticEnum<EVirtualSensorSimulationQuality>()->GetNameStringByValue(int64(S.SimulationQuality)));
    R->SetStringField(TEXT("profile"),Camera?StaticEnum<EVirtualCameraDeviceProfile>()->GetNameStringByValue(int64(S.CameraProfile)):StaticEnum<EVirtualLidarDeviceProfile>()->GetNameStringByValue(int64(S.LidarProfile)));
    R->SetNumberField(TEXT("frame_id"),UVirtualSensorFileSaveSubsystem::GetAvailableFrameId(A));
    return R;
}
FVirtualDatasetSettings DatasetSettings(const FJsonObject& C)
{
    FVirtualDatasetSettings S;S.Enabled=true;S.Name=Str(C,TEXT("name"));S.Scenes=int32(Num(C,TEXT("scenes"),1));S.Seed=int32(Num(C,TEXT("seed"),1337));S.SettleFrames=int32(Num(C,TEXT("settle_frames"),3));S.CameraTruth=Flag(C,TEXT("camera_truth"));S.LidarFidelity=Flag(C,TEXT("lidar_fidelity"));
    const TSharedPtr<FJsonObject>* R=nullptr;
    C.TryGetStringField(TEXT("export_format"),S.ExportFormat);C.TryGetBoolField(TEXT("export_upright_envelope"),S.ExportUprightEnvelope);
    if(C.TryGetObjectField(TEXT("ranges"),R))
    {
        const TMap<FString,FVirtualDatasetRange*> Fields={{TEXT("yaw"),&S.Yaw},{TEXT("pitch"),&S.Pitch},{TEXT("roll"),&S.Roll},{TEXT("x"),&S.X},{TEXT("y"),&S.Y},{TEXT("z"),&S.Z},{TEXT("size"),&S.Size},{TEXT("surface"),&S.Surface},{TEXT("roughness"),&S.Roughness},{TEXT("oxidation"),&S.Oxidation},{TEXT("light_intensity"),&S.LightIntensity},{TEXT("light_pitch"),&S.LightPitch},{TEXT("light_yaw"),&S.LightYaw},{TEXT("sensor_position"),&S.SensorPosition},{TEXT("sensor_angle"),&S.SensorAngle}};
        for(const auto& F:Fields)if((*R)->HasField(F.Key)){const auto& V=(*R)->GetArrayField(F.Key);F.Value->Enabled=true;F.Value->Min=V[0]->AsNumber();F.Value->Max=V[1]->AsNumber();}
    }
    return S;
}
}
struct FAutomationControlState
{
    TUniquePtr<FAutomationLoopbackServer> Server;
    TUniquePtr<FControlErrors> Errors;
    FString PcdSensorId;int32 PcdSensorRevision=0,PcdStreamRevision=0;
    FString ApiMessage=TEXT("외부 제어 API · 꺼짐"),Operation,OperationId,Phase,Error,FileRequest,ScenarioId,RunId;
    TWeakObjectPtr<AVirtualSensorActorBase> Target;
    TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> Publisher;
    FVirtualSensorStreamConfig BeforeStream;
    FVirtualSensorStreamStatus StartStream;
    bool StartedSensor=false,OwnedStream=false,OwnedDataset=false,OwnedReplay=false,Terminal=true,Success=true;
    double Began=0,Deadline=0,Until=0,DrainBegan=0;
    TFuture<FScenarioLoad> Load;
    TSharedRef<FJsonObject> Detail=MakeShared<FJsonObject>();
    FJob Job;bool JobActive=false,ExitWhenDone=true;int32 Step=0;double JobBegan=0;
    FString JobCancelFile;double NextCancelCheck=0;
    TArray<TSharedPtr<FJsonValue>> Results;
    double PreviousFrame=0,MaxFrameMs=0,FrameSumMs=0;int64 Frames=0,Hitches=0;
    uint16 Port=8099;
    double ExitAt=0;
};

bool UAutomationControlSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{auto* W=Cast<UWorld>(Outer);return W&&(W->WorldType==EWorldType::Game||W->WorldType==EWorldType::PIE);}
void UAutomationControlSubsystem::OnWorldBeginPlay(UWorld& W)
{
    Super::OnWorldBeginPlay(W);State=MakeShared<FAutomationControlState>();
    // Process command line belongs only to the standalone game world, never every PIE instance.
    if(W.WorldType!=EWorldType::Game)return;
    if(FParse::Param(FCommandLine::Get(),TEXT("MA0T10Control")))SetApiEnabled(true);
    FString Path;if(FParse::Value(FCommandLine::Get(),TEXT("MA0T10Job="),Path))StartJob(Path);
}
void UAutomationControlSubsystem::EnsurePolling()
{
    if(!Ticker.IsValid())Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::Poll),.05f);
    if(!FrameHook.IsValid())FrameHook=FWorldDelegates::OnWorldPostActorTick.AddUObject(this,&ThisClass::ObserveFrame);
    if(!State->Errors){State->Errors=MakeUnique<FControlErrors>();if(GLog)GLog->AddOutputDevice(State->Errors.Get());}
}
void UAutomationControlSubsystem::ObserveFrame(UWorld* W,ELevelTick,float)
{
    if(W!=GetWorld()||!State)return;auto& S=*State;const double Now=FPlatformTime::Seconds();
    if(S.PreviousFrame>0){const double Ms=(Now-S.PreviousFrame)*1000;S.MaxFrameMs=FMath::Max(S.MaxFrameMs,Ms);S.FrameSumMs+=Ms;++S.Frames;if(Ms>150)++S.Hitches;}S.PreviousFrame=Now;
}
bool UAutomationControlSubsystem::IsApiEnabled() const{return State&&State->Server.IsValid();}
bool UAutomationControlSubsystem::SetApiEnabled(bool Enabled)
{
    check(IsInGameThread());if(!State)State=MakeShared<FAutomationControlState>();auto& S=*State;
    if(!Enabled){S.Server.Reset();S.ApiMessage=TEXT("외부 제어 API · 꺼짐");return true;}
    if(S.Server)return true;
    FString Token=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_AUTOMATION_TOKEN"));
    FString Config;if(FParse::Value(FCommandLine::Get(),TEXT("MA0T10ControlConfig="),Config))
    {
        FString Text;TSharedPtr<FJsonObject> O;
        if(!ReadDocument(Config,Text,8192)||!ParseObject(Text,O)){S.ApiMessage=TEXT("외부 제어 API · 설정 파일 오류");return false;}
        for(const auto& F:O->Values)if(F.Key!=TEXT("port")&&F.Key!=TEXT("token")){S.ApiMessage=TEXT("외부 제어 API · 허용하지 않는 설정");return false;}
        double Port=8099;if(O->HasField(TEXT("port"))&&(O->TryGetField(TEXT("port"))->Type!=EJson::Number||!O->TryGetNumberField(TEXT("port"),Port))){S.ApiMessage=TEXT("외부 제어 API · 포트 설정 오류");return false;}
        if(!FMath::IsFinite(Port)||Port<1024||Port>65535||FMath::FloorToDouble(Port)!=Port||(O->HasField(TEXT("token"))&&(O->TryGetField(TEXT("token"))->Type!=EJson::String||!O->TryGetStringField(TEXT("token"),Token)))){S.ApiMessage=TEXT("외부 제어 API · 설정 값 오류");return false;}S.Port=uint16(Port);
    }
    bool TokenValid=Token.Len()<=256;for(TCHAR C:Token)TokenValid&=C>=33&&C<=126;
    if(!TokenValid){S.ApiMessage=TEXT("외부 제어 API · 인증 설정 오류");return false;}
    auto Server=MakeUnique<FAutomationLoopbackServer>();
    if(!Server->Start(S.Port,MoveTemp(Token))){S.ApiMessage=TEXT("외부 제어 API · 포트 열기 실패");return false;}
    S.Server=MoveTemp(Server);S.ApiMessage=FString::Printf(TEXT("외부 제어 API · 켜짐 · http://127.0.0.1:%u"),S.Port);EnsurePolling();return true;
}
FString UAutomationControlSubsystem::GetPresentationStatus() const
{
    if(!State)return TEXT("외부 제어 API · 꺼짐");
    return State->ApiMessage+(State->JobActive?FString::Printf(TEXT("\n배치 실행 중 · %d/%d · %s"),State->Step+1,State->Job.Steps.Num(),*State->Operation):FString());
}
TSharedRef<FJsonObject> UAutomationControlSubsystem::GetStatus() const
{
    auto O=Answer(true);O->SetStringField(TEXT("schema"),TEXT("ma0t10.control.v1"));O->SetBoolField(TEXT("api_enabled"),IsApiEnabled());
    if(!State)return O;const auto& S=*State;
    O->SetBoolField(TEXT("job_active"),S.JobActive);O->SetNumberField(TEXT("job_step"),S.Step);O->SetNumberField(TEXT("job_steps"),S.Job.Steps.Num());
    TArray<TSharedPtr<FJsonValue>> Slabs,Lights;
    for(TActorIterator<ASlabActor> It(GetWorld());It;++It)Slabs.Add(MakeShared<FJsonValueString>(It->GetName()));
    for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It)Lights.Add(MakeShared<FJsonValueString>(It->GetName()));
    O->SetArrayField(TEXT("slabs"),Slabs);O->SetArrayField(TEXT("lights"),Lights);
    O->SetStringField(TEXT("operation_id"),S.OperationId);O->SetStringField(TEXT("operation"),S.Operation);O->SetStringField(TEXT("phase"),S.Phase);O->SetBoolField(TEXT("terminal"),S.Terminal);O->SetBoolField(TEXT("success"),S.Success);O->SetStringField(TEXT("error"),S.Error);O->SetObjectField(TEXT("detail"),S.Detail);
    auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();O->SetBoolField(TEXT("dataset_active"),D->IsActive());O->SetNumberField(TEXT("dataset_completed"),D->CompletedScenes());O->SetNumberField(TEXT("dataset_requested"),D->RequestedScenes());O->SetStringField(TEXT("dataset_directory"),D->Directory());
    if(auto* R=Replay(GetWorld())){const auto Status=R->GetReplayStatus();O->SetStringField(TEXT("scenario_run_id"),Status.RunUUID);O->SetStringField(TEXT("scenario_state"),StaticEnum<ESlabScenarioReplayState>()->GetNameStringByValue(int64(Status.State)));}
    O->SetNumberField(TEXT("hitches_over_150ms"),S.Hitches);O->SetNumberField(TEXT("max_tick_wall_ms"),S.MaxFrameMs);O->SetNumberField(TEXT("log_error_count"),S.Errors?S.Errors->Count.load():0);return O;
}
TSharedRef<FJsonObject> UAutomationControlSubsystem::Dispatch(const FString& Method,const FString& Path,const TSharedPtr<FJsonObject>& Body,int32& Code)
{
    check(IsInGameThread());if(!State)State=MakeShared<FAutomationControlState>();Code=200;
    if(Method==TEXT("GET")&&Path==TEXT("/v1/status"))return GetStatus();
    if(Method==TEXT("GET")&&Path==TEXT("/v1/sensors"))
    {
        auto R=Answer(true);auto* C=Coordinator(GetWorld());if(!C){Code=409;return Answer(false,TEXT("Exactly one coordinator is required"));}
        TArray<TSharedPtr<FJsonValue>> Items;for(auto* A:C->GetSensorActors())if(IsValid(A))Items.Add(MakeShared<FJsonValueObject>(SensorJson(A)));R->SetArrayField(TEXT("sensors"),Items);return R;
    }
    if(Method!=TEXT("POST")){Code=404;return Answer(false,TEXT("Unknown endpoint"));}
    if(Path==TEXT("/v1/shutdown"))
    {
        if(!Body||!Body->Values.IsEmpty()){Code=400;return Answer(false,TEXT("Expected empty object"));}
        if(GetWorld()->WorldType!=EWorldType::Game||State->JobActive||!State->Terminal){Code=409;return Answer(false,TEXT("Shutdown requires an idle standalone game"));}
        State->ExitAt=FPlatformTime::Seconds()+.5;EnsurePolling();return Answer(true);
    }
    const TMap<FString,FString> Routes={{TEXT("/v1/sensor"),TEXT("sensor.set")},{TEXT("/v1/sensor/start"),TEXT("sensor.start")},{TEXT("/v1/sensor/stop"),TEXT("sensor.stop")},{TEXT("/v1/capture"),TEXT("capture")},{TEXT("/v1/pcd/start"),TEXT("pcd.start")},{TEXT("/v1/pcd/stop"),TEXT("pcd.stop")},{TEXT("/v1/scenario/start"),TEXT("scenario.start")},{TEXT("/v1/scenario/stop"),TEXT("scenario.stop")},{TEXT("/v1/dataset/start"),TEXT("dataset.start")},{TEXT("/v1/dataset/stop"),TEXT("dataset.stop")}};
    const auto* Op=Routes.Find(Path);if(!Op){Code=404;return Answer(false,TEXT("Unknown endpoint"));}
    if(!Body||Body->HasField(TEXT("op"))){Code=400;return Answer(false,TEXT("Invalid JSON object"));}
    auto C=MakeShared<FJsonObject>(*Body);C->SetStringField(TEXT("op"),*Op);FString Error;
    if(!ValidateCommand(*C,Error)){Code=400;return Answer(false,Error);}
    if(State->JobActive){Code=409;return Answer(false,TEXT("Batch owns mutations; status remains available"));}
    if(!StartCommand(C,Error)){Code=409;return Answer(false,Error);}
    Code=State->Terminal?200:202;return GetStatus();
}
bool UAutomationControlSubsystem::StartCommand(const TSharedRef<FJsonObject>& C,FString& Error)
{
    if(!ValidateCommand(*C,Error))return false;auto& S=*State;const FString Op=Str(*C,TEXT("op"));
    if(Op.EndsWith(TEXT(".stop"))&&Op!=TEXT("sensor.stop"))
    {
        const FString Expected=Op.LeftChop(5);
        if(S.Terminal||!S.Operation.StartsWith(Expected)){Error=TEXT("No matching owned operation");return false;}
        if(Expected==TEXT("pcd")){S.Until=FPlatformTime::Seconds();return true;}
        StopOwnedOperation();CompleteOperation(false,TEXT("Cancelled by caller"));return true;
    }
    if(!S.Terminal){Error=TEXT("An operation is already active");return false;}
    auto* W=GetWorld();AVirtualSensorActorBase* A=nullptr;
    if(C->HasField(TEXT("sensor_id")))
    {A=Sensor(W,Str(*C,TEXT("sensor_id")));if(!A){Error=TEXT("Sensor ID is missing/ambiguous, or coordinator is not unique");return false;}}
    if(Op==TEXT("sensor.set"))
    {
        FVirtualSensorEditableState V;if(!A->ReadEditableState(V)||!A->CanEditSensorConfiguration(Error))return false;
        const bool Camera=A->GetSensorKind()==EVirtualSensorKind::Camera;const FString Profile=Str(*C,TEXT("profile")),Quality=Str(*C,TEXT("quality"));
        if((!Profile.IsEmpty()&&!(Camera?EnumValue(Profile,V.CameraProfile):EnumValue(Profile,V.LidarProfile)))||(!Quality.IsEmpty()&&!EnumValue(Quality,V.SimulationQuality))){Error=TEXT("Unknown profile or quality enum name");return false;}
        if(!Profile.IsEmpty()||!Quality.IsEmpty())
        {FVirtualSensorEditableState Applied;if(!A->ApplyProfileAndSimulationQuality(V,Applied,Error))return false;}
        else
        {
            const float Interval=float(Num(*C,TEXT("interval_sec"),.1));if(Camera)V.CameraCaptureInterval=Interval;else V.LidarScanInterval=Interval;
            // Preserve the existing quality/backend and all other editable values.
            if(!A->ValidateEditableState(V,Error)||!A->ApplyEditableState(V,Error))return false;
        }
    }
    else if(Op==TEXT("sensor.start"))
    {if(!A->PrepareForDataCollection(Error))return false;A->StartSensor();if(!A->IsSensorRunning()){Error=TEXT("Sensor did not start");return false;}}
    else if(Op==TEXT("sensor.stop"))
    {if(!A->CanEditSensorConfiguration(Error))return false;A->StopSensor();}
    else if(Op==TEXT("dataset.start"))
    {
        if(GUsingNullRHI){Error=TEXT("Dataset requires a real rendering RHI");return false;}
        auto Settings=DatasetSettings(*C);if(!Settings.Validate(Error))return false;
        auto* Slab=NamedActor<ASlabActor>(W,Str(*C,TEXT("slab")));auto* Lidar=Cast<AVirtualLidarSensorActor>(Sensor(W,Str(*C,TEXT("lidar_id"))));auto* Camera=Cast<AVirtualCameraSensorActor>(Sensor(W,Str(*C,TEXT("camera_id"))));
        auto* Light=C->HasField(TEXT("light"))?NamedActor<ADirectionalLight>(W,Str(*C,TEXT("light"))):nullptr;
        if(C->HasField(TEXT("light"))&&!Light){Error=TEXT("Selected light is missing");return false;}
        if(!W->GetSubsystem<UVirtualDatasetSubsystem>()->Start(Settings,Slab,Lidar,Camera,Light,Error))return false;
        S.OwnedDataset=true;
    }
    else if(Op==TEXT("capture"))
    {
        if(GUsingNullRHI){Error=TEXT("Capture requires a real rendering RHI");return false;}
        FVirtualSensorCaptureSelection Selection;Selection.bCameraImage=A->GetSensorKind()==EVirtualSensorKind::Camera;Selection.bCameraPayload=Selection.bLidarPayload=false;Selection.bPointCloud=!Selection.bCameraImage;Selection.PointCloudFormat=EVirtualSensorExportKind::PointCloudPcd;
        S.FileRequest=W->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->RequestSave(EVirtualSensorFileSaveMode::NewFrame,A,Selection);
        const auto Status=W->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(S.FileRequest);
        if(Status.State==EVirtualSensorFileSaveState::Failed){Error=Status.Message;S.FileRequest.Reset();return false;}
    }
    else if(Op==TEXT("scenario.start"))
    {
        auto* R=Replay(W);if(!R||!R->CanReplay()){Error=TEXT("Scenario adapter is not ready or busy");return false;}
        S.ScenarioId=Str(*C,TEXT("uuid"));
        if(C->HasField(TEXT("json_path")))
        {
            const FString Path=Str(*C,TEXT("json_path"));
            S.Load=Async(EAsyncExecution::ThreadPool,[Path](){FScenarioLoad R;FString Text;if(!ReadDocument(Path,Text,8*1024*1024)||!FSlabScenarioCodec::Parse(Text,R.Data,R.Error,false))R.Error=TEXT("Scenario file could not be loaded or validated");return R;});
        }
        else if(!R->RequestScenarioReplay(S.ScenarioId,Flag(*C,TEXT("send_pcd")),A?TArray<FString>{A->GetSensorId()}:TArray<FString>())){Error=R->GetReplayStatus().Message;return false;}
        S.OwnedReplay=true;
    }
    else if(Op==TEXT("pcd.start")||Op==TEXT("pcd.run"))
    {
        auto* CoordinatorActor=Coordinator(W);auto* P=CoordinatorActor?CoordinatorActor->StreamPublisherComponent.Get():nullptr;
        if(!P||A->GetSensorKind()!=EVirtualSensorKind::Lidar){Error=TEXT("PCD requires a LiDAR and publisher");return false;}
        if(P->IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind::PointCloud,A->GetSensorId())||P->IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind::PointCloud,TEXT(""))){Error=TEXT("Existing PCD stream is active");return false;}
        if(!A->PrepareForDataCollection(Error))return false;
        S.BeforeStream=P->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,A->GetSensorId());
        auto Config=S.BeforeStream;Config.SensorId=A->GetSensorId();Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Config.bEnabled=false;
        P->ConfigureStream(Config);P->StartStream(Config.StreamKind,Config.SensorId);
        if(!P->IsStreamEnabled(Config.StreamKind,Config.SensorId)){P->ConfigureStream(S.BeforeStream);Error=TEXT("PCD preflight rejected stream");return false;}
        S.StartedSensor=!A->IsSensorRunning();A->StartSensor();S.Publisher=P;S.OwnedStream=true;S.PcdSensorId=A->GetSensorId();S.PcdSensorRevision=A->GetConfigurationRevision();
        S.Until=Op==TEXT("pcd.run")?FPlatformTime::Seconds()+Num(*C,TEXT("seconds"),30):DBL_MAX;
        S.StartStream=FVirtualSensorStreamStatus();for(const auto& T:P->GetStreamStatuses())if(T.StreamKind==Config.StreamKind&&T.SensorId==Config.SensorId)S.StartStream=T;
        S.PcdStreamRevision=S.StartStream.ConfigRevision;
    }
    else if(Op!=TEXT("wait")){Error=TEXT("Unsupported operation");return false;}
    S.Operation=Op;S.OperationId=FGuid::NewGuid().ToString(EGuidFormats::Digits);S.Error.Reset();S.Detail=MakeShared<FJsonObject>();
    S.Target=A;S.Terminal=false;S.Success=false;S.Began=FPlatformTime::Seconds();S.Deadline=S.Began+Num(*C,TEXT("timeout_sec"),Op==TEXT("pcd.start")?86400:300);S.DrainBegan=0;
    S.Phase=TEXT("running");
    if(Op==TEXT("wait"))S.Until=S.Began+Num(*C,TEXT("seconds"),1);
    if(Op==TEXT("scenario.start"))
    {S.Phase=C->HasField(TEXT("json_path"))?TEXT("loading"):TEXT("playing");S.Detail->SetBoolField(TEXT("send_pcd"),Flag(*C,TEXT("send_pcd")));S.RunId=Replay(W)->GetReplayStatus().RunUUID;}
    if(Op.StartsWith(TEXT("sensor."))){S.Detail=SensorJson(A);CompleteOperation(true,TEXT(""));}
    EnsurePolling();return true;
}
void UAutomationControlSubsystem::StopOwnedOperation()
{
    if(!State)return;auto& S=*State;
    if(S.OwnedDataset){GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>()->Stop();S.OwnedDataset=false;}
    if(!S.FileRequest.IsEmpty()){GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->CancelRequest(S.FileRequest);S.FileRequest.Reset();}
    if(S.OwnedReplay)
    {
        auto* R=Replay(GetWorld());if(R&&R->IsReplayBusy()&&R->GetReplayStatus().RunUUID==S.RunId)
        {if(auto* Adapter=R->GetPlaybackAdapter())ISlabScenarioPlaybackAdapter::Execute_StopScenarioPlayback(Adapter,S.RunId);R->NotifyPlaybackFinished(S.RunId,true);}
        S.OwnedReplay=false;
    }
    if(S.OwnedStream)
    {
        bool StillOwned=false;
        if(S.Publisher.IsValid())for(const auto& V:S.Publisher->GetStreamStatuses())if(V.StreamKind==EVirtualSensorStreamKind::PointCloud&&V.SensorId==S.PcdSensorId)StillOwned=V.ConfigRevision==S.PcdStreamRevision;
        if(StillOwned){S.Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,S.PcdSensorId);S.Publisher->ConfigureStream(S.BeforeStream);}
        if(StillOwned&&S.StartedSensor&&S.Target.IsValid()&&S.Target->GetConfigurationRevision()==S.PcdSensorRevision&&S.Target->GetSensorId()==S.PcdSensorId)S.Target->StopSensor();
        S.OwnedStream=false;S.StartedSensor=false;
    }
}
void UAutomationControlSubsystem::CompleteOperation(bool Success,const FString& Error)
{
    auto& S=*State;S.Terminal=true;S.Success=Success;S.Error=Error;S.Phase=Success?TEXT("completed"):TEXT("failed");
    S.Detail->SetNumberField(TEXT("elapsed_sec"),FPlatformTime::Seconds()-S.Began);
    // Cleanup only resources acquired by this operation.
    StopOwnedOperation();
}
bool UAutomationControlSubsystem::StartJob(const FString& Path,bool ExitWhenDone)
{
    check(IsInGameThread());if(!State)State=MakeShared<FAutomationControlState>();auto& S=*State;
    if(S.JobActive||!S.Terminal)return false;
    S.ExitWhenDone=ExitWhenDone;S.JobBegan=FPlatformTime::Seconds();S.Step=0;S.Results.Reset();S.JobActive=true;S.OperationId.Reset();S.Success=true;
    S.JobCancelFile.Reset();S.NextCancelCheck=0;
    FString Text,Error;const FString Full=FPaths::ConvertRelativePathToFull(Path);
    S.Job.ResultPath=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/job-result.json"));
    FJob Parsed;
    if(!ReadDocument(Full,Text)||!ParseJob(Text,FPaths::GetPath(Full),Parsed,Error))
    {FinishJob(false,Error.IsEmpty()?TEXT("Job could not be read"):Error);return false;}
    S.Job=MoveTemp(Parsed);EnsurePolling();return true;
}
void UAutomationControlSubsystem::FinishJob(bool Success,const FString& Error)
{
    auto& S=*State;StopOwnedOperation();S.JobActive=false;S.Success=Success;S.Error=Error;S.Terminal=true;auto R=Answer(Success,Error);R->SetStringField(TEXT("schema"),TEXT("ma0t10.job.result.v1"));R->SetStringField(TEXT("project_commit"),UTF8_TO_TCHAR(MA0T10_PROJECT_COMMIT));R->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName());R->SetNumberField(TEXT("elapsed_sec"),FPlatformTime::Seconds()-S.JobBegan);R->SetArrayField(TEXT("steps"),S.Results);R->SetObjectField(TEXT("status"),GetStatus());
    R->SetNumberField(TEXT("hitches_over_150ms"),S.Hitches);R->SetNumberField(TEXT("max_tick_wall_ms"),S.MaxFrameMs);R->SetNumberField(TEXT("mean_tick_wall_ms"),S.Frames?S.FrameSumMs/S.Frames:0);R->SetNumberField(TEXT("log_error_count"),S.Errors?S.Errors->Count.load():0);
    R->SetStringField(TEXT("rendering"),GUsingNullRHI?TEXT("NullRHI"):TEXT("real-rhi"));R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.Job.ResultPath),true);const FString Temp=S.Job.ResultPath+TEXT(".tmp");
    const bool Written=FFileHelper::SaveStringToFile(Json(R),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)&&IFileManager::Get().Move(*S.Job.ResultPath,*Temp,true,true);
    S.JobActive=false;S.Error=Written?Error:TEXT("Result JSON could not be committed");S.Success=Success&&Written;S.Terminal=true;
    if(S.ExitWhenDone)FPlatformMisc::RequestExitWithStatus(false,S.Success?0:1);
}
bool UAutomationControlSubsystem::Poll(float)
{
    if(!State)return false;auto& S=*State;const double Now=FPlatformTime::Seconds();auto* W=GetWorld();
    if(S.ExitAt>0&&Now>=S.ExitAt){FPlatformMisc::RequestExitWithStatus(false,0);S.ExitAt=0;}
    if(S.Server)
    {
        TSharedPtr<FAutomationLoopbackServer::FRequest,ESPMode::ThreadSafe> Request;
        if(S.Server->Dequeue(Request))
        {
            TSharedPtr<FJsonObject> Body;int32 Code=400;TSharedRef<FJsonObject> Result=Answer(false,TEXT("Invalid JSON object"));
            if(Request->Method==TEXT("GET")||ParseObject(Request->Body,Body))Result=Dispatch(Request->Method,Request->Path,Body,Code);
            Request->Response=Json(Result);Request->Code=Code;Request->Complete.store(true,std::memory_order_release);
        }
    }
    if(!S.Terminal)
    {
        if(Now>S.Deadline)CompleteOperation(false,TEXT("Operation timeout"));
        else if(S.Operation==TEXT("wait")){if(Now>=S.Until)CompleteOperation(true,TEXT(""));}
        else if(S.Operation==TEXT("dataset.start"))
        {
            auto* D=W->GetSubsystem<UVirtualDatasetSubsystem>();
            if(!D->IsActive()){S.Detail->SetStringField(TEXT("directory"),D->Directory());S.Detail->SetStringField(TEXT("export_directory"),D->ExportDirectory());S.Detail->SetNumberField(TEXT("completed_scenes"),D->CompletedScenes());const bool Ok=D->HasSucceeded();S.OwnedDataset=false;CompleteOperation(Ok,Ok?FString():D->Message);}
        }
        else if(S.Operation==TEXT("capture"))
        {
            const auto F=W->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(S.FileRequest);
            if(F.IsTerminal())
            {
                TArray<TSharedPtr<FJsonValue>> Paths;for(const auto& File:F.FileResults)if(File.bSucceeded)Paths.Add(MakeShared<FJsonValueString>(File.AbsolutePath));
                S.Detail->SetArrayField(TEXT("artifacts"),Paths);S.Detail->SetNumberField(TEXT("frame_id"),F.FrameId);S.FileRequest.Reset();
                const bool Ok=F.State==EVirtualSensorFileSaveState::Succeeded;CompleteOperation(Ok,Ok?FString():F.Message);
            }
        }
        else if(S.Operation==TEXT("scenario.start"))
        {
            auto* R=Replay(W);
            if(!R)CompleteOperation(false,TEXT("Replay subsystem ended"));
            else if(S.Phase==TEXT("loading")&&S.Load.IsReady())
            {
                auto Loaded=S.Load.Get();
                if(!Loaded.Data||!Loaded.Error.IsEmpty())CompleteOperation(false,Loaded.Error);
                else
                {
                    S.ScenarioId=Loaded.Data->ScenarioUUID;
                    TArray<FString> Ids;if(S.Target.IsValid())Ids.Add(S.Target->GetSensorId());
                    // Import once. Reusing the same immutable file is an explicit
                    // replay, not a second archive admission or duplicate fallback.
                    const auto Existing=R->GetValidatedScenario(S.ScenarioId);
                    const bool Same=Existing&&Existing->OriginalContentSha1==Loaded.Data->OriginalContentSha1;
                    if(Existing&&!Same)CompleteOperation(false,TEXT("Scenario UUID already belongs to different content"));
                    else if((!Existing&&!R->RegisterValidatedScenario(Loaded.Data))||!R->RequestScenarioReplay(S.ScenarioId,Flag(*S.Detail,TEXT("send_pcd")),Ids))CompleteOperation(false,TEXT("Scenario admission rejected"));
                    else {S.RunId=R->GetReplayStatus().RunUUID;S.Phase=TEXT("playing");}
                }
            }
            else if(S.Phase==TEXT("playing"))
            {
                const auto Status=R->GetReplayStatus();
                if(Status.RunUUID!=S.RunId)CompleteOperation(false,TEXT("Scenario ownership changed"));
                else if(Status.State==ESlabScenarioReplayState::Completed||Status.State==ESlabScenarioReplayState::Failed)
                {S.Detail->SetStringField(TEXT("run_id"),S.RunId);S.Detail->SetStringField(TEXT("scenario_id"),S.ScenarioId);S.OwnedReplay=false;const bool Ok=Status.State==ESlabScenarioReplayState::Completed;CompleteOperation(Ok,Ok?FString():Status.Message);}
            }
        }
        else if(S.Operation.StartsWith(TEXT("pcd.")))
        {
            if(!S.Target.IsValid()||!S.Publisher.IsValid())CompleteOperation(false,TEXT("PCD target ended"));
            else
            {
                FVirtualSensorStreamStatus Status;bool Found=false;for(const auto& V:S.Publisher->GetStreamStatuses())if(V.StreamKind==EVirtualSensorStreamKind::PointCloud&&V.SensorId==S.PcdSensorId){Status=V;Found=true;}
                if(!Found||Status.ConfigRevision!=S.PcdStreamRevision){S.OwnedStream=false;S.StartedSensor=false;CompleteOperation(false,TEXT("PCD route changed outside this operation"));return true;}
                if(S.Target->GetConfigurationRevision()!=S.PcdSensorRevision||S.Target->GetSensorId()!=S.PcdSensorId){CompleteOperation(false,TEXT("PCD sensor configuration changed outside this operation"));return true;}
                const int64 Receipts=Status.ReceiptReceivedCount-S.StartStream.ReceiptReceivedCount;
                const int64 Failures=(Status.EncodeFailureCount-S.StartStream.EncodeFailureCount)+(Status.DeliveryFailureCount-S.StartStream.DeliveryFailureCount)+(Status.RawDeliveryFailureCount-S.StartStream.RawDeliveryFailureCount);
                S.Detail->SetNumberField(TEXT("receipt_count"),Receipts);S.Detail->SetNumberField(TEXT("submitted_count"),Status.SubmittedFrameCount-S.StartStream.SubmittedFrameCount);S.Detail->SetNumberField(TEXT("input_count"),Status.InputFrameCount-S.StartStream.InputFrameCount);S.Detail->SetNumberField(TEXT("error_count"),Failures);S.Detail->SetNumberField(TEXT("outstanding_bytes"),Status.RawOutstandingBytes);S.Detail->SetNumberField(TEXT("overload_count"),Status.OverloadCount-S.StartStream.OverloadCount);
                if(Now>=S.Until)
                {
                    if(S.DrainBegan==0){S.DrainBegan=Now;S.Phase=TEXT("draining");S.Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,S.PcdSensorId);++S.PcdStreamRevision;if(S.StartedSensor)S.Target->StopSensor();}
                    if(Status.RawOutstandingFrameCount==0&&Status.ReceiptQueueDepth==0&&!Status.bProcessing)
                    {const bool Ok=Receipts>0&&Failures==0;CompleteOperation(Ok,Ok?FString():TEXT("PCD completed without receipts or with delivery errors"));}
                }
            }
        }
    }
    if(S.JobActive)
    {
        if(Now>=S.NextCancelCheck)
        {
            S.NextCancelCheck=Now+.2;
            if(S.JobCancelFile.IsEmpty())FParse::Value(FCommandLine::Get(),TEXT("MA0T10JobCancelFile="),S.JobCancelFile);
            if(!S.JobCancelFile.IsEmpty()&&IFileManager::Get().FileExists(*S.JobCancelFile)){FinishJob(false,TEXT("Cancelled by process runner"));return true;}
        }
        if(Now-S.JobBegan>S.Job.TimeoutSeconds){FinishJob(false,TEXT("Job timeout"));return true;}
        if(S.Terminal)
        {
            if(!S.OperationId.IsEmpty())
            {
                auto Result=Answer(S.Success,S.Error);Result->SetStringField(TEXT("op"),S.Operation);Result->SetObjectField(TEXT("detail"),S.Detail);S.Results.Add(MakeShared<FJsonValueObject>(Result));S.OperationId.Reset();
                if(!S.Success){FinishJob(false,S.Error);return true;}++S.Step;
            }
            if(S.Step>=S.Job.Steps.Num())FinishJob(true,TEXT(""));
            else {FString Error;if(!StartCommand(S.Job.Steps[S.Step].ToSharedRef(),Error)){auto Result=Answer(false,Error);Result->SetStringField(TEXT("op"),Str(*S.Job.Steps[S.Step],TEXT("op")));S.Results.Add(MakeShared<FJsonValueObject>(Result));FinishJob(false,Error);}}
        }
    }
    if(!S.Server&&!S.JobActive&&S.Terminal)
    {
        if(FrameHook.IsValid()){FWorldDelegates::OnWorldPostActorTick.Remove(FrameHook);FrameHook.Reset();}
        if(S.Errors&&GLog)GLog->RemoveOutputDevice(S.Errors.Get());S.Errors.Reset();Ticker.Reset();return false;
    }
    return true;
}
void UAutomationControlSubsystem::Deinitialize()
{
    if(State)
    {
        State->Server.Reset();if(State->JobActive){State->ExitWhenDone=false;FinishJob(false,TEXT("World ended before job completion"));}StopOwnedOperation();
        if(State->Errors&&GLog)GLog->RemoveOutputDevice(State->Errors.Get());State->Errors.Reset();
    }
    if(Ticker.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();
    if(FrameHook.IsValid())FWorldDelegates::OnWorldPostActorTick.Remove(FrameHook);FrameHook.Reset();State.Reset();Super::Deinitialize();
}
