#include "VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarExportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorSemanticLabels.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "Async/Async.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Json.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include <atomic>

namespace
{
struct FDatasetWriteResult { FString Error,Commit;int64 Bytes=0;double Seconds=0; };
enum class EDatasetPhase { Settle,Capture,Write,Paused,Draining,Done };
TArray<TSharedPtr<FJsonValue>> N(std::initializer_list<double> Values)
{TArray<TSharedPtr<FJsonValue>> A;for(double V:Values)A.Add(MakeShared<FJsonValueNumber>(V));return A;}
TSharedRef<FJsonObject> Pose(const FTransform& T)
{
    auto O=MakeShared<FJsonObject>();const auto P=T.GetLocation()*.01;const auto Q=T.GetRotation();const auto R=T.Rotator();
    O->SetArrayField(TEXT("xyz_m"),N({P.X,P.Y,P.Z}));O->SetArrayField(TEXT("quaternion_xyzw"),N({Q.X,Q.Y,Q.Z,Q.W}));
    O->SetArrayField(TEXT("pitch_yaw_roll_deg"),N({R.Pitch,R.Yaw,R.Roll}));return O;
}
FString Json(const TSharedRef<FJsonObject>& O){FString V;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&V));return V;}
FVirtualSensorVerificationFile TextFile(const FString& Name,const FString& Value)
{FVirtualSensorVerificationFile F;F.Name=Name;FTCHARToUTF8 U(*Value);F.Bytes.Append(reinterpret_cast<const uint8*>(U.Get()),U.Length());return F;}
bool Image(IImageWrapperModule& M,FVirtualSensorVerificationFile& F,EImageFormat Format,const void* Pixels,int64 Bytes,int32 W,int32 H,ERGBFormat Raw,int32 Bits)
{auto Wrapper=M.CreateImageWrapper(Format);if(!Wrapper||!Wrapper->SetRaw(Pixels,Bytes,W,H,Raw,Bits))return false;F.Bytes=Wrapper->GetCompressed(0);return !F.Bytes.IsEmpty();}
}
struct FVirtualDatasetSession
{
    FVirtualDatasetSettings Settings;
    TWeakObjectPtr<ASlabActor> Slab;
    TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    TWeakObjectPtr<ADirectionalLight> Light;
    FVirtualSensorEditableState LidarBefore,CameraBefore;
    FTransform SlabBefore,LightBefore,SlabApplied,LidarApplied,CameraApplied;
    FVector MeshScaleBefore,SizeBefore;
    FTransform LightApplied;float LightAppliedIntensity=0;
    float RoughBefore=0,OxBefore=0,EmissiveBefore=0,LightBeforeIntensity=0;
    bool HotBefore=false;
    ESlabSurfacePreset SurfaceBefore=ESlabSurfacePreset::Default;
    bool LinkBefore=false,PauseRequested=false,Restored=false;
    FString Root,Commit;
    int32 Completed=0,Frames=0,NoiseSeedBefore=0;
    int32 LidarRevision=0,CameraRevision=0;
    uint64 LastFrame=0,CaptureRequestFrame=0;
    double Started=0,CaptureStarted=0,MaxPumpMs=0;
    double MaxFrameMs=0,FrameMsSum=0;int64 FrameSamples=0,TotalBytes=0;
    TArray<double> FrameMs;
    uint64 InitialMemory=0;
    EDatasetPhase Phase=EDatasetPhase::Settle;
    FVirtualDatasetSample Sample;
    TMap<FName,uint32> ActorNames;
    TMap<FString,uint32> ActorPaths;
    TSharedRef<FJsonObject> Conditions=MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> Label=MakeShared<FJsonObject>();
    TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> LidarFrame;
    FVirtualLidarSlabAnalysisResult Analysis;
    TFuture<FDatasetWriteResult> Write;
    TSharedRef<std::atomic<bool>,ESPMode::ThreadSafe> Cancelled=MakeShared<std::atomic<bool>,ESPMode::ThreadSafe>(false);
};

bool UVirtualDatasetSubsystem::IsActive() const{return Session&&Session->Phase!=EDatasetPhase::Done;}
bool UVirtualDatasetSubsystem::IsPaused() const{return Session&&Session->Phase==EDatasetPhase::Paused;}
int32 UVirtualDatasetSubsystem::CompletedScenes() const{return Session?Session->Completed:0;}
int32 UVirtualDatasetSubsystem::RequestedScenes() const{return Session?Session->Settings.Scenes:0;}
FString UVirtualDatasetSubsystem::Directory() const{return Session?Session->Root:FString();}
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool UVirtualDatasetSubsystem::HasTruthResourcesForTests() const{return Truth&&Truth->HasTruthResourcesForTests();}
bool UVirtualDatasetSubsystem::IsCapturingForTests() const{return Session&&Session->Phase==EDatasetPhase::Capture;}
bool UVirtualDatasetSubsystem::IsWritingForTests() const{return Session&&Session->Phase==EDatasetPhase::Write;}
int32 UVirtualDatasetSubsystem::GetReadbackAllocationsForTests() const{return Truth?Truth->GetReadbackAllocationsForTests():0;}
#endif

bool UVirtualDatasetSubsystem::Start(const FVirtualDatasetSettings& Settings,ASlabActor* Slab,AVirtualLidarSensorActor* Lidar,AVirtualCameraSensorActor* Camera,ADirectionalLight* Light,FString& Error)
{
    if(IsActive()){Error=TEXT("진행 중인 데이터셋을 먼저 마치세요");return false;}
    if(!Settings.Validate(Error))return false;
    if(!IsValid(Slab)||!Slab->SlabMesh||!IsValid(Lidar)||!Lidar->ScanComponent||!IsValid(Camera)||!Camera->CaptureComponent||Slab->GetWorld()!=GetWorld()||Lidar->GetWorld()!=GetWorld()||Camera->GetWorld()!=GetWorld())
    {Error=TEXT("같은 월드의 Slab·LiDAR·Camera를 명시적으로 선택하세요");return false;}
    if(Slab->IsSimulationActive()||Lidar->IsSensorRunning()||Camera->IsSensorRunning()||!Lidar->CanEditSensorConfiguration(Error)||!Camera->CanEditSensorConfiguration(Error)||!Lidar->CanEditSensorPlacement(Error)||!Camera->CanEditSensorPlacement(Error))
    {Error=TEXT("시나리오·선택 센서의 측정/출력/저장을 중지한 뒤 시작하세요 · 기존 작업은 자동 중지하지 않습니다");return false;}
    const bool UsesLight=Settings.LightIntensity.Enabled||Settings.LightPitch.Enabled||Settings.LightYaw.Enabled;
    if(UsesLight&&(!IsValid(Light)||Light->GetWorld()!=GetWorld()||!Light->GetLightComponent()||Light->GetLightComponent()->Mobility!=EComponentMobility::Movable))
    {Error=TEXT("조명 랜덤화에는 이 월드의 Movable DirectionalLight를 선택하세요");return false;}
    if(Settings.Surface.Enabled)for(int32 I=FMath::FloorToInt(Settings.Surface.Min);I<=FMath::FloorToInt(Settings.Surface.Max);++I)
        if(!Slab->CanApplySurfacePreset(ESlabSurfacePreset(I),Error))return false;
    // Bound resident input before any directory/resource allocation: one scene, <=256 MiB.
    const int64 Pixels=int64(Camera->CaptureComponent->CaptureResolution.X)*Camera->CaptureComponent->CaptureResolution.Y;
    const int64 Points=int64(Lidar->ScanComponent->HorizontalSamples)*Lidar->ScanComponent->VerticalChannels*FMath::Max(1,Lidar->ScanComponent->MaxHitsPerRay);
    if(Pixels<=0||Pixels>1920ll*1080||Points<=0||Points>250000||Pixels*64+Points*512>256ll*1024*1024)
    {Error=TEXT("1장면 메모리 한도: Camera 1920×1080 이하·LiDAR 25만 반환점 이하·256MiB 추정 이하");return false;}
    auto S=MakeShared<FVirtualDatasetSession,ESPMode::ThreadSafe>();S->Settings=Settings;S->Slab=Slab;S->Lidar=Lidar;S->Camera=Camera;S->Light=UsesLight?Light:nullptr;
    if(!Lidar->ReadEditableState(S->LidarBefore)||!Camera->ReadEditableState(S->CameraBefore)){Error=TEXT("센서 설정을 읽을 수 없습니다");return false;}
    S->SlabBefore=Slab->GetActorTransform();S->MeshScaleBefore=Slab->SlabMesh->GetRelativeScale3D();S->SizeBefore=S->MeshScaleBefore*(Slab->SlabMesh->GetStaticMesh()?Slab->SlabMesh->GetStaticMesh()->GetBounds().BoxExtent*2.:FVector::ZeroVector);
    S->SurfaceBefore=Slab->GetSurfacePreset();S->LinkBefore=Slab->IsSurfaceLidarLinked();S->RoughBefore=Slab->Roughness;S->OxBefore=Slab->Oxidation;S->HotBefore=Slab->bHotAppearance;S->EmissiveBefore=Slab->EmissiveStrength;
    S->NoiseSeedBefore=Lidar->ScanComponent->DeterministicNoiseSeed;
    if(S->Light.IsValid()){S->LightBefore=Light->GetActorTransform();S->LightBeforeIntensity=Light->GetLightComponent()->Intensity;}
    S->Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Datasets")/(Settings.Name+TEXT("_")+FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
    TArray<AActor*> Actors;for(TActorIterator<AActor> It(GetWorld());It;++It)Actors.Add(*It);
    Actors.Sort([](const AActor& A,const AActor& B){return A.GetName()<B.GetName();});
    uint32 Id=1;for(const auto* A:Actors){S->ActorNames.Add(A->GetFName(),Id);S->ActorPaths.Add(A->GetPathName(),Id++);}
    S->Conditions->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.v1"));S->Conditions->SetNumberField(TEXT("seed"),Settings.Seed);S->Conditions->SetNumberField(TEXT("requested_scenes"),Settings.Scenes);
    S->Conditions->SetNumberField(TEXT("settle_frames"),Settings.SettleFrames);S->Conditions->SetBoolField(TEXT("camera_truth"),Settings.CameraTruth);S->Conditions->SetBoolField(TEXT("lidar_fidelity"),Settings.LidarFidelity);
    int32 Index=0;for(const auto* R:{&Settings.Yaw,&Settings.Pitch,&Settings.Roll,&Settings.X,&Settings.Y,&Settings.Z,&Settings.Size,&Settings.Surface,&Settings.Roughness,&Settings.Oxidation,&Settings.LightIntensity,&Settings.LightPitch,&Settings.LightYaw,&Settings.SensorPosition,&Settings.SensorAngle})
    {auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("enabled"),R->Enabled);O->SetNumberField(TEXT("min"),R->Min);O->SetNumberField(TEXT("max"),R->Max);S->Conditions->SetObjectField(LexToString(Index++),O);}
    S->Conditions->SetStringField(TEXT("range_order"),TEXT("yaw,pitch,roll,x_cm,y_cm,z_cm,size_multiplier,surface_preset,roughness,oxidation,light_intensity,light_pitch,light_yaw,sensor_position_cm,sensor_angle_deg"));
    TArray<TSharedPtr<FJsonValue>> Classes;for(uint8 C=0;C<7;++C){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("id"),C);O->SetStringField(TEXT("name"),VirtualSensorSemanticLabels::Label(C).ToString());Classes.Add(MakeShared<FJsonValueObject>(O));}S->Conditions->SetArrayField(TEXT("classes"),Classes);
    TArray<TSharedPtr<FJsonValue>> Instances;
    for(const auto* A:Actors){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("id"),S->ActorNames.FindRef(A->GetFName()));O->SetStringField(TEXT("actor_name"),A->GetName());O->SetStringField(TEXT("actor_class"),A->GetClass()->GetName());Instances.Add(MakeShared<FJsonValueObject>(O));}
    S->Conditions->SetArrayField(TEXT("instances"),Instances);S->Conditions->SetStringField(TEXT("map"),GetWorld()->GetMapName());
    S->Started=FPlatformTime::Seconds();S->InitialMemory=FPlatformMemory::GetStats().UsedPhysical;
    Session=S;
    if(Truth){Truth->DestroyComponent();Truth=nullptr;}
    Truth=NewObject<UVirtualCameraGroundTruth>(this);Truth->RegisterComponentWithWorld(GetWorld());
    if(!ApplyScene(Error)){Restore();Session.Reset();Truth->DestroyComponent();Truth=nullptr;return false;}
    Message=TEXT("데이터셋 생성 중 · 장면 안정화");++Revision;
    GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.01f,true);return true;
}

bool UVirtualDatasetSubsystem::ApplyScene(FString& Error)
{
    auto S=Session;if(!S||!S->Slab.IsValid()||!S->Lidar.IsValid()||!S->Camera.IsValid())return false;
    auto* Slab=S->Slab.Get();auto* Lidar=S->Lidar.Get();auto* Camera=S->Camera.Get();
    if(Slab->IsSimulationActive()){Error=TEXT("시나리오가 시작되어 데이터셋을 중단합니다");return false;}
    S->Sample=FVirtualDatasetSample::Make(S->Settings,S->Completed);const auto& V=S->Sample;S->Restored=false;
    if(!Slab->ApplyDatasetGeometry(S->SizeBefore*V.Size)){Error=TEXT("현재 Slab 치수를 변경할 수 없습니다");return false;}
    if(S->Settings.Surface.Enabled&&!Slab->ApplySurfacePreset(ESlabSurfacePreset(V.Surface),false,Error))return false;
    if(S->Settings.Surface.Enabled&&V.Surface==0)
    {Slab->bHotAppearance=S->HotBefore;Slab->EmissiveStrength=S->EmissiveBefore;Slab->Roughness=S->RoughBefore;Slab->Oxidation=S->OxBefore;Slab->RefreshDatasetAppearance();}
    if(S->Settings.Roughness.Enabled)Slab->Roughness=V.Roughness;
    if(S->Settings.Oxidation.Enabled)Slab->Oxidation=V.Oxidation;
    if(S->Settings.Roughness.Enabled||S->Settings.Oxidation.Enabled)Slab->RefreshDatasetAppearance();
    S->SlabApplied=FTransform(S->SlabBefore.GetRotation()*V.SlabRotation.Quaternion(),S->SlabBefore.GetLocation()+V.SlabOffset,S->SlabBefore.GetScale3D());
    Slab->SetActorTransform(S->SlabApplied,false,nullptr,ETeleportType::TeleportPhysics);
    auto L=S->LidarBefore,C=S->CameraBefore;
    L.ActorTransform=FTransform(L.ActorTransform.GetRotation()*V.LidarRotation.Quaternion(),L.ActorTransform.GetLocation()+V.LidarOffset,L.ActorTransform.GetScale3D());
    C.ActorTransform=FTransform(C.ActorTransform.GetRotation()*V.CameraRotation.Quaternion(),C.ActorTransform.GetLocation()+V.CameraOffset,C.ActorTransform.GetScale3D());
    L.bLidarPointTime=S->Settings.LidarFidelity;L.bLidarMotionDistortion=S->Settings.LidarFidelity;L.bLidarMaterialResponse=S->Settings.LidarFidelity;
    L.bExportCsvOnScan=L.bExportJsonLinesOnScan=L.bExportPcdOnScan=false;L.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;C.bCameraGroundTruth=S->Settings.CameraTruth;
    if(!Lidar->ApplyDatasetState(L,Error)||!Camera->ApplyDatasetState(C,Error))return false;
    Lidar->ScanComponent->DatasetNoiseSequence=TPair<int32,int64>(S->Settings.Seed,S->Completed);
    Lidar->ScanComponent->bDatasetSemanticDemand=true;
    S->LidarApplied=L.ActorTransform;S->CameraApplied=C.ActorTransform;
    S->LidarRevision=Lidar->GetConfigurationRevision();S->CameraRevision=Camera->GetConfigurationRevision();
    if(S->Light.IsValid())
    {
        auto* Light=S->Light.Get();auto Rot=S->LightBefore.Rotator();
        if(S->Settings.LightPitch.Enabled)Rot.Pitch=V.LightRotation.Pitch;if(S->Settings.LightYaw.Enabled)Rot.Yaw=V.LightRotation.Yaw;
        Light->SetActorRotation(Rot);if(S->Settings.LightIntensity.Enabled)Light->GetLightComponent()->SetIntensity(V.Intensity);
        S->LightApplied=Light->GetActorTransform();S->LightAppliedIntensity=Light->GetLightComponent()->Intensity;
    }
    S->Label=MakeShared<FJsonObject>();S->Label->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.scene.v1"));S->Label->SetNumberField(TEXT("scene_index"),S->Completed);S->Label->SetNumberField(TEXT("seed"),S->Settings.Seed);
    S->Label->SetObjectField(TEXT("slab_pose"),Pose(Slab->GetActorTransform()));const FVector Size=Slab->SlabMesh->GetComponentScale()*(Slab->SlabMesh->GetStaticMesh()->GetBounds().BoxExtent*2.)*.01;
    S->Label->SetArrayField(TEXT("slab_size_m"),N({Size.X,Size.Y,Size.Z}));S->Label->SetStringField(TEXT("surface_preset"),ASlabActor::SurfacePresetId(Slab->GetSurfacePreset()));
    S->Label->SetNumberField(TEXT("roughness"),Slab->Roughness);S->Label->SetNumberField(TEXT("oxidation"),Slab->Oxidation);TSharedPtr<FJsonObject> Random;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(V.ToJson()),Random);S->Label->SetObjectField(TEXT("random_values"),Random);
    S->Label->SetObjectField(TEXT("lidar_pose"),Pose(Lidar->ScanComponent->GetComponentTransform()));S->Label->SetObjectField(TEXT("camera_pose"),Pose(Camera->CaptureComponent->GetComponentTransform()));
    S->Label->SetStringField(TEXT("pairing"),TEXT("same stable scene; independent sensor acquisition UTC; CPU chunk spans frames; no hardware simultaneous timestamp claim"));
    S->Label->SetBoolField(TEXT("hot_surface"),Slab->bHotAppearance);S->Label->SetNumberField(TEXT("emissive_strength"),Slab->EmissiveStrength);
    const auto* Scan=Lidar->ScanComponent.Get();auto Intrinsics=MakeShared<FJsonObject>();
    Intrinsics->SetNumberField(TEXT("fidelity_mode"),int32(Scan->FidelityMode));Intrinsics->SetNumberField(TEXT("backend"),int32(Scan->GetActiveAcquisitionBackend()));Intrinsics->SetBoolField(TEXT("range_noise"),Scan->bEnableRangeNoise);Intrinsics->SetNumberField(TEXT("noise_seed"),S->Settings.Seed);
    Intrinsics->SetStringField(TEXT("sensor_id"),Scan->SensorId);Intrinsics->SetNumberField(TEXT("horizontal_samples"),Scan->HorizontalSamples);Intrinsics->SetNumberField(TEXT("vertical_channels"),Scan->VerticalChannels);
    Intrinsics->SetNumberField(TEXT("horizontal_fov_deg"),Scan->HorizontalFov);Intrinsics->SetNumberField(TEXT("min_vertical_deg"),Scan->MinVerticalAngle);Intrinsics->SetNumberField(TEXT("max_vertical_deg"),Scan->MaxVerticalAngle);Intrinsics->SetNumberField(TEXT("max_range_m"),Scan->MaxDistance*.01);Intrinsics->SetNumberField(TEXT("scan_period_s"),Scan->ScanInterval);
    S->Label->SetObjectField(TEXT("lidar_intrinsics"),Intrinsics);
    if(S->Light.IsValid()){S->Label->SetObjectField(TEXT("light_pose"),Pose(S->Light->GetActorTransform()));S->Label->SetNumberField(TEXT("light_intensity_lux"),S->Light->GetLightComponent()->Intensity);}
    S->Frames=S->Settings.SettleFrames;S->LastFrame=GFrameCounter;S->LidarFrame.Reset();S->Phase=EDatasetPhase::Settle;return true;
}

void UVirtualDatasetSubsystem::Pump()
{
    auto S=Session;if(!S)return;const double Begin=FPlatformTime::Seconds();
    if(S->Phase==EDatasetPhase::Write||S->Phase==EDatasetPhase::Draining)
    {
        if(!S->Write.IsValid()||!S->Write.IsReady())return;
        const auto Result=S->Write.Get();S->Write=TFuture<FDatasetWriteResult>();S->Commit=Result.Commit;
        if(S->Phase==EDatasetPhase::Draining){if(Result.Error.IsEmpty())++S->Completed;S->Phase=EDatasetPhase::Done;GetWorld()->GetTimerManager().ClearTimer(Timer);++Revision;return;}
        if(!Result.Error.IsEmpty()){Message=Result.Error;Stop();return;}
        ++S->Completed;S->TotalBytes+=Result.Bytes;S->LidarFrame.Reset();Truth->LastFrame.Reset();++Revision;
        Message=FString::Printf(TEXT("%d / %d 장면 · %.2f초/장면 · %.2f MiB/장면 · %s"),S->Completed,S->Settings.Scenes,Result.Seconds,Result.Bytes/1048576.,*S->Root);
        if(S->Completed>=S->Settings.Scenes){Restore();S->Phase=EDatasetPhase::Done;GetWorld()->GetTimerManager().ClearTimer(Timer);return;}
        if(S->PauseRequested){Restore();S->Phase=EDatasetPhase::Paused;GetWorld()->GetTimerManager().ClearTimer(Timer);return;}
        if(!TargetsCurrent()){Message=TEXT("대상 설정 변경으로 데이터셋을 중단합니다");Stop();return;}
        FString Error;if(!ApplyScene(Error)){Message=Error;Stop();}return;
    }
    if(S->Phase==EDatasetPhase::Paused||S->Phase==EDatasetPhase::Done)return;
    if(!TargetsCurrent())
    {Message=TEXT("대상 삭제·이동·시나리오 시작으로 데이터셋을 중단합니다");Stop();return;}
    if(S->Phase==EDatasetPhase::Settle&&GFrameCounter!=S->LastFrame)
    {
        S->LastFrame=GFrameCounter;if(--S->Frames>0)return;
        S->CaptureStarted=FPlatformTime::Seconds();S->CaptureRequestFrame=GFrameCounter;
        if(!S->Camera->CaptureComponent->CaptureDatasetFrame(*Truth,S->Settings.CameraTruth)){Message=TEXT("데이터셋 카메라 요청 실패");Stop();return;}
        S->Lidar->ScanComponent->RequestDatasetScanOnce();S->Phase=EDatasetPhase::Capture;
    }
    if(S->Phase==EDatasetPhase::Capture)
    {
        if(FPlatformTime::Seconds()-S->CaptureStarted>30){Message=TEXT("장면 캡처 30초 대기 초과");Stop();return;}
        if(!S->LidarFrame)
        {
            auto F=S->Lidar->ScanComponent->GetLastFrameSnapshot();
            if(F&&F->AcquisitionStartedMonotonic>=S->CaptureStarted)
            {S->LidarFrame=F;S->Analysis=S->Lidar->ScanComponent->GetLastSlabAnalysis();S->Lidar->ScanComponent->StopDatasetScan();}
        }
        if(S->LidarFrame&&Truth->LastFrame&&!Truth->IsBusy())WriteScene();
    }
    S->MaxPumpMs=FMath::Max(S->MaxPumpMs,(FPlatformTime::Seconds()-Begin)*1000.);
}
bool UVirtualDatasetSubsystem::TargetsCurrent() const
{
    const auto S=Session;
    return S&&S->Slab.IsValid()&&S->Lidar.IsValid()&&S->Camera.IsValid()&&!S->Slab->IsSimulationActive()&&
        S->Slab->GetActorTransform().Equals(S->SlabApplied)&&S->Lidar->GetActorTransform().Equals(S->LidarApplied)&&S->Camera->GetActorTransform().Equals(S->CameraApplied)&&
        S->Lidar->GetConfigurationRevision()==S->LidarRevision&&S->Camera->GetConfigurationRevision()==S->CameraRevision&&!S->Camera->IsSensorRunning()&&
        ((S->Phase==EDatasetPhase::Capture&&!S->LidarFrame)?S->Lidar->IsSensorRunning():!S->Lidar->IsSensorRunning())&&
        (!S->Light.IsValid()||(S->Light->GetActorTransform().Equals(S->LightApplied)&&FMath::IsNearlyEqual(S->Light->GetLightComponent()->Intensity,S->LightAppliedIntensity)));
}

void UVirtualDatasetSubsystem::WriteScene()
{
    auto S=Session;auto Frame=Truth->LastFrame;auto* Module=&FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    S->Phase=EDatasetPhase::Write;
    S->Label->SetNumberField(TEXT("analysis_yaw_deg"),S->Analysis.EstimatedYawDegrees);S->Label->SetBoolField(TEXT("analysis_valid"),S->Analysis.bValid);S->Label->SetNumberField(TEXT("analysis_points"),S->Analysis.SlabHitPointCount);
    S->Label->SetStringField(TEXT("lidar_frame_id"),LexToString(S->LidarFrame->FrameId));S->Label->SetStringField(TEXT("lidar_utc"),S->LidarFrame->GetAcquisitionStartUtc().ToIso8601());
    S->Label->SetStringField(TEXT("camera_frame_id"),LexToString(Frame->Acquisition.FrameId));S->Label->SetStringField(TEXT("camera_utc"),Frame->Acquisition.TimestampUtc.ToIso8601());
    const auto& K=Frame->Intrinsics;S->Label->SetArrayField(TEXT("camera_K"),N({K.Fx,0,K.Cx,0,K.Fy,K.Cy,0,0,1}));S->Label->SetArrayField(TEXT("camera_resolution"),N({double(K.Width),double(K.Height)}));
    S->Label->SetNumberField(TEXT("max_pump_ms"),S->MaxPumpMs);S->Label->SetNumberField(TEXT("resident_memory_bytes"),double(FPlatformMemory::GetStats().UsedPhysical));S->Label->SetNumberField(TEXT("initial_memory_bytes"),double(S->InitialMemory));
    S->Label->SetObjectField(TEXT("lidar_pose"),Pose(S->LidarFrame->AcquisitionTransform));S->Label->SetObjectField(TEXT("camera_pose"),Pose(FTransform(Frame->Acquisition.Rotation,Frame->Acquisition.Location)));
    S->Label->SetStringField(TEXT("request_engine_frame"),LexToString(S->CaptureRequestFrame));
    S->Label->SetNumberField(TEXT("lidar_backend"),int32(S->LidarFrame->AcquisitionBackend));S->Label->SetStringField(TEXT("lidar_profile"),S->LidarFrame->ProfileKey);S->Label->SetStringField(TEXT("lidar_calibration"),S->LidarFrame->CalibrationId);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if(OnSceneSnapshotForTests)OnSceneSnapshotForTests(*S->Label);
#endif
    const FString Label=Json(S->Label);const FString Conditions=Json(S->Conditions);const auto LidarFrame=S->LidarFrame;const auto Paths=S->ActorPaths;const auto Names=S->ActorNames;
    const FString Root=S->Root,SensorId=S->LidarBefore.SensorId,Commit=S->Commit;const int32 Scene=S->Completed;const bool IncludeTruth=S->Settings.CameraTruth;const auto Cancelled=S->Cancelled;const double Started=S->CaptureStarted;
    float Delay=0;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    Delay=WorkerDelayForTests;
#endif
    S->Write=Async(EAsyncExecution::ThreadPool,[Frame,LidarFrame,Module,Label,Conditions,Paths,Names,Root,SensorId,Commit,Scene,IncludeTruth,Cancelled,Started,Delay]()
    {
        FDatasetWriteResult Result;TArray<FVirtualSensorVerificationFile> Files;
        if(Delay>0)FPlatformProcess::Sleep(Delay);
        if(Cancelled->load()){Result.Error=TEXT("데이터셋 저장 취소");return Result;}
        FVirtualSensorFrameEnvelope Envelope;Envelope.SensorId=SensorId;Envelope.SensorKind=EVirtualSensorKind::Lidar;Envelope.FrameId=LidarFrame->FrameId;Envelope.TimestampUtc=LidarFrame->GetAcquisitionStartUtc();Envelope.LidarFrameSnapshot=LidarFrame;Envelope.PointSnapshot=LidarFrame->Points;
        FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Config.PcdDataMode=EVirtualPcdDataMode::Binary;
        TArray<uint8> Bytes;FString Ext,Error;int32 Count=0;
        if(!UVirtualLidarExportComponent::SerializeImmutableFrame(Envelope,Config,Ext,Bytes,Count,Error)){Result.Error=Error;return Result;}
        FVirtualSensorVerificationFile Pcd;Pcd.Name=TEXT("lidar.pcd");Pcd.Bytes.Append(Bytes);Files.Add(MoveTemp(Pcd));
        FVirtualSensorVerificationFile Labels;Labels.Name=TEXT("lidar_labels.bin");int32 LabelCount=0;
        for(const auto& P:*LidarFrame->Points)if(P.bHit)
        {
            const uint32 Id=Names.FindRef(P.HitActorName);Labels.Bytes.Add(VirtualSensorSemanticLabels::ClassId(P.SemanticLabel));
            for(int32 B=0;B<4;++B)Labels.Bytes.Add(uint8(Id>>(B*8)));++LabelCount;
        }
        if(LabelCount!=Count){Result.Error=TEXT("PCD와 점별 라벨 수가 일치하지 않습니다");return Result;}Files.Add(MoveTemp(Labels));
        const int32 W=Frame->Intrinsics.Width,H=Frame->Intrinsics.Height;
        FVirtualSensorVerificationFile Rgb;Rgb.Name=TEXT("rgb.png");
        if(Frame->DatasetRgb.Num()!=W*H||!Image(*Module,Rgb,EImageFormat::PNG,Frame->DatasetRgb.GetData(),int64(W)*H*4,W,H,ERGBFormat::BGRA,8)){Result.Error=TEXT("RGB PNG 생성 실패");return Result;}Files.Add(MoveTemp(Rgb));
        if(IncludeTruth)
        {
            FVirtualSensorVerificationFile Depth,Seg,Instance;Depth.Name=TEXT("depth.exr");Seg.Name=TEXT("seg.png");Instance.Name=TEXT("instance.png");
            TArray<FColor> Ids;Ids.SetNumUninitialized(W*H);
            for(int32 I=0;I<W*H;++I){const uint32 Id=Paths.FindRef(Frame->ActorPaths.FindRef(Frame->Instances[I]));Ids[I]=FColor(Id&255,(Id>>8)&255,(Id>>16)&255,255);}
            if(!Image(*Module,Depth,EImageFormat::EXR,Frame->DepthMeters.GetData(),int64(W)*H*4,W,H,ERGBFormat::GrayF,32)||!Image(*Module,Seg,EImageFormat::PNG,Frame->Classes.GetData(),int64(W)*H,W,H,ERGBFormat::Gray,8)||!Image(*Module,Instance,EImageFormat::PNG,Ids.GetData(),int64(W)*H*4,W,H,ERGBFormat::BGRA,8))
            {Result.Error=TEXT("Camera GT 파일 생성 실패");return Result;}
            Files.Add(MoveTemp(Depth));Files.Add(MoveTemp(Seg));Files.Add(MoveTemp(Instance));
        }
        Files.Add(TextFile(TEXT("label.json"),Label));for(const auto& F:Files)Result.Bytes+=F.Bytes.Num();
        const FString Directory=Root/FString::Printf(TEXT("scene_%05d"),Scene);
        if(!UVirtualSensorFileSaveSubsystem::WriteDatasetSceneBundle(Directory,Files,[Cancelled](){return Cancelled->load();})){Result.Error=TEXT("데이터셋 저장 실패 또는 중단");return Result;}
        TSharedPtr<FJsonObject> Manifest;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Conditions),Manifest);
        Manifest->SetNumberField(TEXT("completed_scenes"),Scene+1);Manifest->SetStringField(TEXT("coordinates"),TEXT("PCD: RH sensor-local X forward,Y left,Z up meters; pose: UE world X forward,Y right,Z up meters; depth: optical Z meters; instance: R+256G+65536B"));
        Manifest->SetStringField(TEXT("lidar_labels"),TEXT("5 byte little-endian records in PCD point order: uint8 semantic_id,uint32 instance_id; 0 unknown; no padding"));
        FString Sha,GitError;int32 Exit=0;const FString Args=TEXT("-C \"")+FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())+TEXT("\" rev-parse HEAD");
        if(Commit.IsEmpty())FPlatformProcess::ExecProcess(TEXT("git.exe"),*Args,&Exit,&Sha,&GitError);else Sha=Commit;Result.Commit=Sha.TrimStartAndEnd();Manifest->SetStringField(TEXT("project_commit"),Result.Commit);
        const FString Temp=Root/TEXT("dataset.json.pending"),Final=Root/TEXT("dataset.json");
        if(Exit!=0||Cancelled->load()||!FFileHelper::SaveStringToFile(Json(Manifest.ToSharedRef()),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)||!IFileManager::Get().Move(*Final,*Temp,true,true,false,true))
        {
            Result.Error=TEXT("dataset.json 확정 실패 또는 생성 취소");IFileManager::Get().Delete(*Temp,false,true);
            for(const auto& F:Files)IFileManager::Get().Delete(*(Directory/F.Name),false,true);
            IFileManager::Get().DeleteDirectory(*Directory,false,false);
        }
        Result.Seconds=FPlatformTime::Seconds()-Started;return Result;
    });
}

void UVirtualDatasetSubsystem::Restore()
{
    auto S=Session;if(!S||S->Restored)return;S->Restored=true;FString Error;
    if(Truth)Truth->Cancel();
    if(S->Lidar.IsValid()){if(S->Phase==EDatasetPhase::Capture&&!S->LidarFrame)S->Lidar->ScanComponent->StopScan();S->Lidar->ScanComponent->DatasetNoiseSequence.Reset();if(S->Lidar->GetConfigurationRevision()==S->LidarRevision&&S->Lidar->GetActorTransform().Equals(S->LidarApplied))S->Lidar->ApplyDatasetState(S->LidarBefore,Error);}
    if(S->Camera.IsValid()&&S->Camera->GetConfigurationRevision()==S->CameraRevision&&S->Camera->GetActorTransform().Equals(S->CameraApplied))S->Camera->ApplyDatasetState(S->CameraBefore,Error);
    if(S->Slab.IsValid()&&!S->Slab->IsSimulationActive()&&S->Slab->GetActorTransform().Equals(S->SlabApplied))
    {auto* Slab=S->Slab.Get();Slab->ApplySurfacePreset(S->SurfaceBefore,S->LinkBefore,Error);Slab->Roughness=S->RoughBefore;Slab->Oxidation=S->OxBefore;Slab->bHotAppearance=S->HotBefore;Slab->EmissiveStrength=S->EmissiveBefore;Slab->RefreshDatasetAppearance();Slab->ApplyDatasetGeometry(S->SizeBefore);Slab->SetActorTransform(S->SlabBefore,false,nullptr,ETeleportType::TeleportPhysics);}
    if(S->Light.IsValid()&&S->Light->GetActorTransform().Equals(S->LightApplied)&&FMath::IsNearlyEqual(S->Light->GetLightComponent()->Intensity,S->LightAppliedIntensity)){S->Light->SetActorTransform(S->LightBefore);S->Light->GetLightComponent()->SetIntensity(S->LightBeforeIntensity);}
}
void UVirtualDatasetSubsystem::Pause(){if(IsActive()){Session->PauseRequested=true;++Revision;}}
bool UVirtualDatasetSubsystem::Resume(FString& Error)
{if(!IsPaused()){Error=TEXT("일시 중지 상태에서 재개하세요");return false;}Session->PauseRequested=false;if(!ApplyScene(Error))return false;GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.01f,true);++Revision;return true;}
void UVirtualDatasetSubsystem::Stop()
{if(!Session||Session->Phase==EDatasetPhase::Done||Session->Phase==EDatasetPhase::Draining)return;Session->Cancelled->store(true);Restore();Session->Phase=Session->Write.IsValid()?EDatasetPhase::Draining:EDatasetPhase::Done;if(Session->Phase==EDatasetPhase::Done&&GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);++Revision;}
void UVirtualDatasetSubsystem::Deinitialize()
{Stop();if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);if(Truth){Truth->DestroyComponent();Truth=nullptr;}Session.Reset();Super::Deinitialize();}
