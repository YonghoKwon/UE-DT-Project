#include "VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
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
enum class EDatasetPhase { Settle,Capture,Write,Paused,Draining,Export,Done };
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
    TSharedPtr<const FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Plan;
    int32 NextScene=0;
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
    FString Root,Commit,ExportPython,ExportScript;
    bool ExportSucceeded=false;
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
bool UVirtualDatasetSubsystem::OwnsPlannedRun(const FString& RunId) const{return Session&&Session->Plan&&Session->Plan->RunId==RunId;}
int32 UVirtualDatasetSubsystem::CompletedScenes() const{return Session?Session->Completed:0;}
int32 UVirtualDatasetSubsystem::RequestedScenes() const{return Session?Session->Settings.Scenes:0;}
FString UVirtualDatasetSubsystem::Directory() const{return Session?Session->Root:FString();}
FString UVirtualDatasetSubsystem::ExportDirectory() const{return Session&&Session->Settings.ExportFormat!=TEXT("none")?Session->Root+TEXT("_export"):FString();}
bool UVirtualDatasetSubsystem::HasSucceeded() const{return Session&&Session->Phase==EDatasetPhase::Done&&!Session->Cancelled->load()&&Session->Completed==Session->Settings.Scenes&&(Session->Settings.ExportFormat==TEXT("none")||Session->ExportSucceeded);}
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool UVirtualDatasetSubsystem::HasTruthResourcesForTests() const{return Truth&&Truth->HasTruthResourcesForTests();}
bool UVirtualDatasetSubsystem::IsCapturingForTests() const{return Session&&Session->Phase==EDatasetPhase::Capture;}
bool UVirtualDatasetSubsystem::IsWritingForTests() const{return Session&&Session->Phase==EDatasetPhase::Write;}
int32 UVirtualDatasetSubsystem::GetReadbackAllocationsForTests() const{return Truth?Truth->GetReadbackAllocationsForTests():0;}
#endif

bool UVirtualDatasetSubsystem::Start(const FVirtualDatasetSettings& Settings,ASlabActor* Slab,AVirtualLidarSensorActor* Lidar,AVirtualCameraSensorActor* Camera,ADirectionalLight* Light,FString& Error,TSharedPtr<const FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Plan)
{
    const bool HasCamera=!Plan||Plan->CaptureCamera;
    if(Plan&&(Plan->Frames.Num()!=Settings.Scenes||!FPaths::IsUnderDirectory(Plan->Root,FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TestSets"))))){Error=TEXT("Invalid test set frame plan");return false;}
    if(IsActive()){Error=TEXT("진행 중인 데이터셋을 먼저 마치세요");return false;}
    if(!Settings.Validate(Error))return false;
    FString ExportPython,ExportScript;
    if(Settings.ExportFormat!=TEXT("none"))
    {
        ExportPython=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_DATASET_PYTHON"));
        ExportScript=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Tools/DatasetTools/export_dataset.py"));
        if(FPaths::IsRelative(ExportPython)||!FPaths::FileExists(ExportPython)||!FPaths::FileExists(ExportScript))
        {Error=TEXT("내보내기 Python 환경이 필요합니다 · Tools/DatasetTools/README.md의 설치 및 MA0T10_DATASET_PYTHON 경로를 확인하세요");return false;}
    }
    if(!IsValid(Slab)||!Slab->SlabMesh||!IsValid(Lidar)||!Lidar->ScanComponent||(HasCamera&&(!IsValid(Camera)||!Camera->CaptureComponent||Camera->GetWorld()!=GetWorld()))||Slab->GetWorld()!=GetWorld()||Lidar->GetWorld()!=GetWorld())
    {Error=TEXT("같은 월드의 Slab·LiDAR·Camera를 명시적으로 선택하세요");return false;}
    if(Slab->IsSimulationActive()||Lidar->IsSensorRunning()||(HasCamera&&Camera->IsSensorRunning())||!Lidar->CanEditSensorConfiguration(Error)||(HasCamera&&!Camera->CanEditSensorConfiguration(Error))||!Lidar->CanEditSensorPlacement(Error)||(HasCamera&&!Camera->CanEditSensorPlacement(Error)))
    {Error=TEXT("시나리오·선택 센서의 측정/출력/저장을 중지한 뒤 시작하세요 · 기존 작업은 자동 중지하지 않습니다");return false;}
    const bool UsesLight=Settings.LightIntensity.Enabled||Settings.LightPitch.Enabled||Settings.LightYaw.Enabled;
    if(UsesLight&&(!IsValid(Light)||Light->GetWorld()!=GetWorld()||!Light->GetLightComponent()||Light->GetLightComponent()->Mobility!=EComponentMobility::Movable))
    {Error=TEXT("조명 랜덤화에는 이 월드의 Movable DirectionalLight를 선택하세요");return false;}
    if(Settings.Surface.Enabled)for(int32 I=FMath::FloorToInt(Settings.Surface.Min);I<=FMath::FloorToInt(Settings.Surface.Max);++I)
        if(!Slab->CanApplySurfacePreset(ESlabSurfacePreset(I),Error))return false;
    // Bound resident input before any directory/resource allocation: one scene, <=256 MiB.
    const int64 Pixels=HasCamera?int64(Camera->CaptureComponent->CaptureResolution.X)*Camera->CaptureComponent->CaptureResolution.Y:0;
    const int64 Points=int64(Lidar->ScanComponent->HorizontalSamples)*Lidar->ScanComponent->VerticalChannels*FMath::Max(1,Lidar->ScanComponent->MaxHitsPerRay);
    if((HasCamera&&Pixels<=0)||Pixels>1920ll*1080||Points<=0||Points>250000||(Pixels*64+Points*512)*(Plan&&Plan->Pipeline?2:1)>256ll*1024*1024)
    {Error=TEXT("1장면 메모리 한도: Camera 1920×1080 이하·LiDAR 25만 반환점 이하·256MiB 추정 이하");return false;}
    auto S=MakeShared<FVirtualDatasetSession,ESPMode::ThreadSafe>();S->Settings=Settings;S->Plan=Plan;S->Slab=Slab;S->Lidar=Lidar;S->Camera=HasCamera?Camera:nullptr;S->Light=UsesLight?Light:nullptr;
    S->ExportPython=ExportPython;S->ExportScript=ExportScript;
    if(!Lidar->ReadEditableState(S->LidarBefore)||(HasCamera&&!Camera->ReadEditableState(S->CameraBefore))){Error=TEXT("센서 설정을 읽을 수 없습니다");return false;}
    S->SlabBefore=Slab->GetActorTransform();S->MeshScaleBefore=Slab->SlabMesh->GetRelativeScale3D();S->SizeBefore=S->MeshScaleBefore*(Slab->SlabMesh->GetStaticMesh()?Slab->SlabMesh->GetStaticMesh()->GetBounds().BoxExtent*2.:FVector::ZeroVector);
    S->SurfaceBefore=Slab->GetSurfacePreset();S->LinkBefore=Slab->IsSurfaceLidarLinked();S->RoughBefore=Slab->Roughness;S->OxBefore=Slab->Oxidation;S->HotBefore=Slab->bHotAppearance;S->EmissiveBefore=Slab->EmissiveStrength;
    S->NoiseSeedBefore=Lidar->ScanComponent->DeterministicNoiseSeed;
    if(S->Light.IsValid()){S->LightBefore=Light->GetActorTransform();S->LightBeforeIntensity=Light->GetLightComponent()->Intensity;}
    S->Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Datasets")/(Settings.Name+TEXT("_")+FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
    if(Plan)S->Root=Plan->Root;
    TArray<AActor*> Actors;for(TActorIterator<AActor> It(GetWorld());It;++It)Actors.Add(*It);
    Actors.Sort([](const AActor& A,const AActor& B){return A.GetName()<B.GetName();});
    uint32 Id=1;for(const auto* A:Actors){S->ActorNames.Add(A->GetFName(),Id);S->ActorPaths.Add(A->GetPathName(),Id++);}
    S->Conditions->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.v1"));S->Conditions->SetNumberField(TEXT("seed"),Settings.Seed);S->Conditions->SetNumberField(TEXT("requested_scenes"),Settings.Scenes);
    S->Conditions->SetNumberField(TEXT("schema_minor"),1);
    S->Conditions->SetStringField(TEXT("export_format"),Settings.ExportFormat);S->Conditions->SetBoolField(TEXT("export_upright_envelope"),Settings.ExportUprightEnvelope);
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
    if(HasCamera){Truth=NewObject<UVirtualCameraGroundTruth>(this);Truth->RegisterComponentWithWorld(GetWorld());}
    if(!ApplyScene(Error)){Restore();Session.Reset();if(Truth){Truth->DestroyComponent();Truth=nullptr;}return false;}
    Message=TEXT("데이터셋 생성 중 · 장면 안정화");++Revision;
    GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.01f,true);return true;
}

bool UVirtualDatasetSubsystem::ApplyScene(FString& Error)
{
    auto S=Session;if(!S||!S->Slab.IsValid()||!S->Lidar.IsValid()||((!S->Plan||S->Plan->CaptureCamera)&&!S->Camera.IsValid()))return false;
    auto* Slab=S->Slab.Get();auto* Lidar=S->Lidar.Get();auto* Camera=S->Camera.Get();
    if(Slab->IsSimulationActive()){Error=TEXT("시나리오가 시작되어 데이터셋을 중단합니다");return false;}
    S->Sample=FVirtualDatasetSample::Make(S->Settings,S->Plan?S->NextScene:S->Completed);const auto& V=S->Sample;S->Restored=false;
    if(!Slab->ApplyDatasetGeometry((S->Plan?S->Plan->Frames[S->NextScene].SizeCm:S->SizeBefore*V.Size))){Error=TEXT("현재 Slab 치수를 변경할 수 없습니다");return false;}
    if(S->Settings.Surface.Enabled&&!Slab->ApplySurfacePreset(ESlabSurfacePreset(V.Surface),false,Error))return false;
    if(S->Settings.Surface.Enabled&&V.Surface==0)
    {Slab->bHotAppearance=S->HotBefore;Slab->EmissiveStrength=S->EmissiveBefore;Slab->Roughness=S->RoughBefore;Slab->Oxidation=S->OxBefore;Slab->RefreshDatasetAppearance();}
    if(S->Settings.Roughness.Enabled)Slab->Roughness=V.Roughness;
    if(S->Settings.Oxidation.Enabled)Slab->Oxidation=V.Oxidation;
    if(S->Settings.Roughness.Enabled||S->Settings.Oxidation.Enabled)Slab->RefreshDatasetAppearance();
    S->SlabApplied=FTransform(S->SlabBefore.GetRotation()*V.SlabRotation.Quaternion(),S->SlabBefore.GetLocation()+V.SlabOffset,S->SlabBefore.GetScale3D());
    if(S->Plan)S->SlabApplied=S->Plan->Frames[S->NextScene].SlabPose;
    Slab->SetActorTransform(S->SlabApplied,false,nullptr,ETeleportType::TeleportPhysics);
    auto L=S->LidarBefore,C=S->CameraBefore;
    L.ActorTransform=FTransform(L.ActorTransform.GetRotation()*V.LidarRotation.Quaternion(),L.ActorTransform.GetLocation()+V.LidarOffset,L.ActorTransform.GetScale3D());
    C.ActorTransform=FTransform(C.ActorTransform.GetRotation()*V.CameraRotation.Quaternion(),C.ActorTransform.GetLocation()+V.CameraOffset,C.ActorTransform.GetScale3D());
    L.bLidarPointTime=S->Settings.LidarFidelity;L.bLidarMotionDistortion=S->Settings.LidarFidelity;L.bLidarMaterialResponse=S->Settings.LidarFidelity;
    L.bExportCsvOnScan=L.bExportJsonLinesOnScan=L.bExportPcdOnScan=false;L.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;C.bCameraGroundTruth=S->Settings.CameraTruth;
    if(!Lidar->ApplyDatasetState(L,Error)||(Camera&&!Camera->ApplyDatasetState(C,Error)))return false;
    Lidar->ScanComponent->DatasetNoiseSequence=TPair<int32,int64>(S->Settings.Seed,S->Plan?S->NextScene:S->Completed);
    Lidar->ScanComponent->bDatasetSemanticDemand=true;
    S->LidarApplied=L.ActorTransform;S->CameraApplied=C.ActorTransform;
    S->LidarRevision=Lidar->GetConfigurationRevision();S->CameraRevision=Camera?Camera->GetConfigurationRevision():0;
    if(S->Light.IsValid())
    {
        auto* Light=S->Light.Get();auto Rot=S->LightBefore.Rotator();
        if(S->Settings.LightPitch.Enabled)Rot.Pitch=V.LightRotation.Pitch;if(S->Settings.LightYaw.Enabled)Rot.Yaw=V.LightRotation.Yaw;
        Light->SetActorRotation(Rot);if(S->Settings.LightIntensity.Enabled)Light->GetLightComponent()->SetIntensity(V.Intensity);
        S->LightApplied=Light->GetActorTransform();S->LightAppliedIntensity=Light->GetLightComponent()->Intensity;
    }
    S->Label=MakeShared<FJsonObject>();S->Label->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.scene.v1"));S->Label->SetNumberField(TEXT("scene_index"),S->Plan?S->NextScene:S->Completed);S->Label->SetNumberField(TEXT("seed"),S->Settings.Seed);
    S->Label->SetNumberField(TEXT("schema_minor"),1);
    // Selected Slab's actual mesh OBB, including pivot/relative transform and full rotation.
    // No hidden-world scan, diagnostic helper bounds or PCA estimate is used as truth.
    const auto MeshTransform=Slab->SlabMesh->GetComponentTransform();const auto MeshBounds=Slab->SlabMesh->GetStaticMesh()->GetBounds();
    const FVector Center=MeshTransform.TransformPosition(MeshBounds.Origin)*.01;
    const FVector BoxSize=MeshTransform.GetScale3D().GetAbs()*MeshBounds.BoxExtent*2.*.01;const auto BoxRotation=MeshTransform.GetRotation();
    auto Box=MakeShared<FJsonObject>();Box->SetNumberField(TEXT("instance_id"),S->ActorNames.FindRef(Slab->GetFName()));Box->SetNumberField(TEXT("semantic_id"),2);
    Box->SetStringField(TEXT("coordinates"),TEXT("UE world meters; local XYZ dimensions; quaternion_xyzw"));
    Box->SetArrayField(TEXT("center_m"),N({Center.X,Center.Y,Center.Z}));Box->SetArrayField(TEXT("size_m"),N({BoxSize.X,BoxSize.Y,BoxSize.Z}));
    Box->SetArrayField(TEXT("quaternion_xyzw"),N({BoxRotation.X,BoxRotation.Y,BoxRotation.Z,BoxRotation.W}));Box->SetNumberField(TEXT("yaw_deg"),BoxRotation.Rotator().Yaw);
    S->Label->SetArrayField(TEXT("boxes_3d"),{MakeShared<FJsonValueObject>(Box)});
    S->Label->SetObjectField(TEXT("slab_pose"),Pose(Slab->GetActorTransform()));const FVector Size=Slab->SlabMesh->GetComponentScale()*(Slab->SlabMesh->GetStaticMesh()->GetBounds().BoxExtent*2.)*.01;
    S->Label->SetArrayField(TEXT("slab_size_m"),N({Size.X,Size.Y,Size.Z}));S->Label->SetStringField(TEXT("surface_preset"),ASlabActor::SurfacePresetId(Slab->GetSurfacePreset()));
    S->Label->SetNumberField(TEXT("roughness"),Slab->Roughness);S->Label->SetNumberField(TEXT("oxidation"),Slab->Oxidation);TSharedPtr<FJsonObject> Random;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(V.ToJson()),Random);S->Label->SetObjectField(TEXT("random_values"),Random);
    S->Label->SetObjectField(TEXT("lidar_pose"),Pose(Lidar->ScanComponent->GetComponentTransform()));if(Camera)S->Label->SetObjectField(TEXT("camera_pose"),Pose(Camera->CaptureComponent->GetComponentTransform()));
    S->Label->SetStringField(TEXT("pairing"),TEXT("same stable scene; independent sensor acquisition UTC; CPU chunk spans frames; no hardware simultaneous timestamp claim"));
    S->Label->SetBoolField(TEXT("hot_surface"),Slab->bHotAppearance);S->Label->SetNumberField(TEXT("emissive_strength"),Slab->EmissiveStrength);
    const auto* Scan=Lidar->ScanComponent.Get();auto Intrinsics=MakeShared<FJsonObject>();
    Intrinsics->SetNumberField(TEXT("fidelity_mode"),int32(Scan->FidelityMode));Intrinsics->SetNumberField(TEXT("backend"),int32(Scan->GetActiveAcquisitionBackend()));Intrinsics->SetBoolField(TEXT("range_noise"),Scan->bEnableRangeNoise);Intrinsics->SetNumberField(TEXT("noise_seed"),S->Settings.Seed);
    Intrinsics->SetStringField(TEXT("sensor_id"),Scan->SensorId);Intrinsics->SetNumberField(TEXT("horizontal_samples"),Scan->HorizontalSamples);Intrinsics->SetNumberField(TEXT("vertical_channels"),Scan->VerticalChannels);
    Intrinsics->SetNumberField(TEXT("horizontal_fov_deg"),Scan->HorizontalFov);Intrinsics->SetNumberField(TEXT("min_vertical_deg"),Scan->MinVerticalAngle);Intrinsics->SetNumberField(TEXT("max_vertical_deg"),Scan->MaxVerticalAngle);Intrinsics->SetNumberField(TEXT("max_range_m"),Scan->MaxDistance*.01);Intrinsics->SetNumberField(TEXT("scan_period_s"),Scan->ScanInterval);
    S->Label->SetObjectField(TEXT("lidar_intrinsics"),Intrinsics);
    if(S->Light.IsValid()){S->Label->SetObjectField(TEXT("light_pose"),Pose(S->Light->GetActorTransform()));S->Label->SetNumberField(TEXT("light_intensity_lux"),S->Light->GetLightComponent()->Intensity);}
    if(S->Plan)
    {
        const auto& P=S->Plan->Frames[S->NextScene];
        S->Label->SetStringField(TEXT("scenario_id"),S->Plan->ScenarioId);S->Label->SetStringField(TEXT("run_uuid"),S->Plan->RunId);
        S->Label->SetStringField(TEXT("mtl_no"),P.MtlNo);S->Label->SetStringField(TEXT("scenario_frame_no"),LexToString(P.FrameNo));
        S->Label->SetNumberField(TEXT("scenario_elapsed_s"),P.ElapsedSec);
    }
    S->Frames=S->Settings.SettleFrames;S->LastFrame=GFrameCounter;S->LidarFrame.Reset();S->Phase=EDatasetPhase::Settle;return true;
}

void UVirtualDatasetSubsystem::Pump()
{
    auto S=Session;if(!S)return;if(S->Plan&&S->Phase!=EDatasetPhase::Export){PumpPlanned();return;}const double Begin=FPlatformTime::Seconds();
    if(S->Phase==EDatasetPhase::Export)
    {
        if(!S->Write.IsReady())return;
        const auto Result=S->Write.Get();S->Write=TFuture<FDatasetWriteResult>();S->ExportSucceeded=Result.Error.IsEmpty()&&!S->Cancelled->load();
        Message=S->ExportSucceeded?TEXT("데이터셋·표준 형식 내보내기 완료 · ")+ExportDirectory():TEXT("원본 데이터셋 보존 · ")+Result.Error;
        S->Phase=EDatasetPhase::Done;GetWorld()->GetTimerManager().ClearTimer(Timer);++Revision;return;
    }
    if(S->Phase==EDatasetPhase::Write||S->Phase==EDatasetPhase::Draining)
    {
        if(!S->Write.IsValid()||!S->Write.IsReady())return;
        const auto Result=S->Write.Get();S->Write=TFuture<FDatasetWriteResult>();S->Commit=Result.Commit;
        if(S->Phase==EDatasetPhase::Draining){if(Result.Error.IsEmpty())++S->Completed;S->Phase=EDatasetPhase::Done;GetWorld()->GetTimerManager().ClearTimer(Timer);++Revision;return;}
        if(!Result.Error.IsEmpty()){Message=Result.Error;Stop();return;}
        ++S->Completed;S->TotalBytes+=Result.Bytes;S->LidarFrame.Reset();Truth->LastFrame.Reset();++Revision;
        Message=FString::Printf(TEXT("%d / %d 장면 · %.2f초/장면 · %.2f MiB/장면 · %s"),S->Completed,S->Settings.Scenes,Result.Seconds,Result.Bytes/1048576.,*S->Root);
        if(S->Completed>=S->Settings.Scenes)
        {Restore();if(S->Settings.ExportFormat!=TEXT("none")){StartExport();return;}S->Phase=EDatasetPhase::Done;GetWorld()->GetTimerManager().ClearTimer(Timer);return;}
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

void UVirtualDatasetSubsystem::StartExport()
{
    auto S=Session;S->Phase=EDatasetPhase::Export;Message=TEXT("원본 생성 완료 · 표준 형식 내보내기 중");++Revision;
    const FString Root=S->Root,Out=ExportDirectory(),Python=S->ExportPython,Script=S->ExportScript,Format=S->Settings.ExportFormat;
    const bool Envelope=S->Settings.ExportUprightEnvelope;const auto Cancelled=S->Cancelled;
    // The child owns conversion/validation. This worker only waits and forwards cancellation;
    // UObject/scene/transport work never moves into it and the game thread never waits on Python.
    S->Write=Async(EAsyncExecution::ThreadPool,[Root,Out,Python,Script,Format,Envelope,Cancelled]()
    {
        FDatasetWriteResult Result;const FString CancelFile=Out+TEXT(".cancel");
        const FString Args=TEXT("\"")+Script+TEXT("\" \"")+Root+TEXT("\" \"")+Out+TEXT("\" --formats ")+Format+TEXT(" --cancel-file \"")+CancelFile+TEXT("\"")+(Envelope?TEXT(" --upright-envelope"):TEXT(""));
        if(Cancelled->load()){Result.Error=TEXT("내보내기 중단");return Result;}
        auto Process=FPlatformProcess::CreateProc(*Python,*Args,false,true,true,nullptr,0,nullptr,nullptr);
        if(!Process.IsValid()){Result.Error=TEXT("내보내기 Python 시작 실패 · 설치 경로를 확인하세요");return Result;}
        while(FPlatformProcess::IsProcRunning(Process))
        {if(Cancelled->load())FFileHelper::SaveStringToFile(TEXT("cancel"),*CancelFile);FPlatformProcess::SleepNoStats(.05f);}
        int32 Exit=-1;FPlatformProcess::GetProcReturnCode(Process,&Exit);FPlatformProcess::CloseProc(Process);IFileManager::Get().Delete(*CancelFile,false,true);
        if(Cancelled->load())Result.Error=TEXT("내보내기 중단");
        else if(Exit!=0||!FPaths::FileExists(Out/TEXT("export-report.json")))Result.Error=TEXT("내보내기 실패 · Python 의존성·3D 박스 기울기 정책 확인 후 CLI로 재시도하세요");
        return Result;
    });
}

void UVirtualDatasetSubsystem::YieldPlannedCapture()
{
    auto S=Session;if(!S||!S->Plan||!IsActive()||IsPaused()||S->Phase==EDatasetPhase::Export)return;
    // Cancel only the unadmitted capture. The worker owns an immutable previous frame.
    Restore();S->LidarFrame.Reset();if(Truth)Truth->LastFrame.Reset();
    S->Phase=EDatasetPhase::Paused;S->PauseRequested=true;++Revision;
    if(!S->Write.IsValid())GetWorld()->GetTimerManager().ClearTimer(Timer);
}

void UVirtualDatasetSubsystem::PumpPlanned()
{
    auto S=Session;const double Begin=FPlatformTime::Seconds();
    if(S->Write.IsValid()&&S->Write.IsReady())
    {
        const auto R=S->Write.Get();S->Write=TFuture<FDatasetWriteResult>();
        if(!R.Error.IsEmpty()){Message=R.Error;Stop();}
        else {++S->Completed;S->TotalBytes+=R.Bytes;S->Commit=R.Commit;++Revision;}
    }
    if(S->Cancelled->load())
    {if(!S->Write.IsValid()){S->Phase=EDatasetPhase::Done;GetWorld()->GetTimerManager().ClearTimer(Timer);}return;}
    if(S->Completed==S->Settings.Scenes)
    {Restore();if(S->Settings.ExportFormat!=TEXT("none")){StartExport();return;}S->Phase=EDatasetPhase::Done;Message=TEXT("시나리오 데이터셋 생성 완료");GetWorld()->GetTimerManager().ClearTimer(Timer);++Revision;return;}
    if(S->Phase==EDatasetPhase::Paused){if(!S->Write.IsValid())GetWorld()->GetTimerManager().ClearTimer(Timer);return;}
    if(S->Phase==EDatasetPhase::Write)
    {
        if(S->Write.IsValid()||(S->Slab.IsValid()&&S->Slab->HasPendingScenarioInput()))return;
        FString Error;if(!ApplyScene(Error)){Message=Error;Stop();}return;
    }
    if(!TargetsCurrent()){Message=TEXT("대상 변경으로 테스트셋 생성을 중단합니다");Stop();return;}
    if(S->Phase==EDatasetPhase::Settle&&GFrameCounter!=S->LastFrame)
    {
        S->LastFrame=GFrameCounter;if(--S->Frames>0)return;
        S->CaptureStarted=FPlatformTime::Seconds();S->CaptureRequestFrame=GFrameCounter;
        if(S->Camera.IsValid()&&!S->Camera->CaptureComponent->CaptureDatasetFrame(*Truth,S->Settings.CameraTruth))
        {Message=TEXT("카메라 캡처 요청 실패");Stop();return;}
        S->Lidar->ScanComponent->RequestDatasetScanOnce();S->Phase=EDatasetPhase::Capture;
    }
    if(S->Phase==EDatasetPhase::Capture)
    {
        if(FPlatformTime::Seconds()-S->CaptureStarted>30){Message=TEXT("장면 캡처 대기 초과");Stop();return;}
        if(!S->LidarFrame)
        {
            const auto F=S->Lidar->ScanComponent->GetLastFrameSnapshot();
            if(F&&F->AcquisitionStartedMonotonic>=S->CaptureStarted)
            {S->LidarFrame=F;S->Analysis=S->Lidar->ScanComponent->GetLastSlabAnalysis();S->Lidar->ScanComponent->StopDatasetScan();}
        }
        if(S->LidarFrame&&(!Truth||(Truth->LastFrame&&!Truth->IsBusy()))&&!S->Write.IsValid())
        {
            WriteScene();S->LidarFrame.Reset();if(Truth)Truth->LastFrame.Reset();
            if(S->Plan->Pipeline&&S->NextScene<S->Settings.Scenes&&!S->Slab->HasPendingScenarioInput())
            {FString Error;if(!ApplyScene(Error)){Message=Error;Stop();}}
        }
    }
    S->MaxPumpMs=FMath::Max(S->MaxPumpMs,(FPlatformTime::Seconds()-Begin)*1000.);
}
bool UVirtualDatasetSubsystem::TargetsCurrent() const
{
    const auto S=Session;
    return S&&S->Slab.IsValid()&&S->Lidar.IsValid()&&(!S->Plan||!S->Plan->CaptureCamera||S->Camera.IsValid())&&!S->Slab->IsSimulationActive()&&
        S->Slab->GetActorTransform().Equals(S->SlabApplied)&&S->Lidar->GetActorTransform().Equals(S->LidarApplied)&&(!S->Camera.IsValid()||S->Camera->GetActorTransform().Equals(S->CameraApplied))&&
        S->Lidar->GetConfigurationRevision()==S->LidarRevision&&(!S->Camera.IsValid()||(S->Camera->GetConfigurationRevision()==S->CameraRevision&&!S->Camera->IsSensorRunning()))&&
        ((S->Phase==EDatasetPhase::Capture&&!S->LidarFrame)?S->Lidar->IsSensorRunning():!S->Lidar->IsSensorRunning())&&
        (!S->Light.IsValid()||(S->Light->GetActorTransform().Equals(S->LightApplied)&&FMath::IsNearlyEqual(S->Light->GetLightComponent()->Intensity,S->LightAppliedIntensity)));
}

void UVirtualDatasetSubsystem::WriteScene()
{
    auto S=Session;auto Frame=Truth?Truth->LastFrame:nullptr;auto* Module=&FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    S->Phase=EDatasetPhase::Write;
    S->Label->SetNumberField(TEXT("analysis_yaw_deg"),S->Analysis.EstimatedYawDegrees);S->Label->SetBoolField(TEXT("analysis_valid"),S->Analysis.bValid);S->Label->SetNumberField(TEXT("analysis_points"),S->Analysis.SlabHitPointCount);
    S->Label->SetStringField(TEXT("lidar_frame_id"),LexToString(S->LidarFrame->FrameId));S->Label->SetStringField(TEXT("lidar_utc"),S->LidarFrame->GetAcquisitionStartUtc().ToIso8601());
    if(Frame){S->Label->SetStringField(TEXT("camera_frame_id"),LexToString(Frame->Acquisition.FrameId));S->Label->SetStringField(TEXT("camera_utc"),Frame->Acquisition.TimestampUtc.ToIso8601());
    const auto& K=Frame->Intrinsics;S->Label->SetArrayField(TEXT("camera_K"),N({K.Fx,0,K.Cx,0,K.Fy,K.Cy,0,0,1}));S->Label->SetArrayField(TEXT("camera_resolution"),N({double(K.Width),double(K.Height)}));}
    S->Label->SetNumberField(TEXT("max_pump_ms"),S->MaxPumpMs);S->Label->SetNumberField(TEXT("resident_memory_bytes"),double(FPlatformMemory::GetStats().UsedPhysical));S->Label->SetNumberField(TEXT("initial_memory_bytes"),double(S->InitialMemory));
    S->Label->SetObjectField(TEXT("lidar_pose"),Pose(S->LidarFrame->AcquisitionTransform));if(Frame)S->Label->SetObjectField(TEXT("camera_pose"),Pose(FTransform(Frame->Acquisition.Rotation,Frame->Acquisition.Location)));
    S->Label->SetStringField(TEXT("request_engine_frame"),LexToString(S->CaptureRequestFrame));
    S->Label->SetNumberField(TEXT("lidar_backend"),int32(S->LidarFrame->AcquisitionBackend));S->Label->SetStringField(TEXT("lidar_profile"),S->LidarFrame->ProfileKey);S->Label->SetStringField(TEXT("lidar_calibration"),S->LidarFrame->CalibrationId);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if(OnSceneSnapshotForTests)OnSceneSnapshotForTests(*S->Label);
#endif
    if(S->Settings.IncludeImu)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for(AActor* A:{static_cast<AActor*>(S->Lidar.Get()),static_cast<AActor*>(S->Camera.Get())})
            if(A)if(const auto* C=A->FindComponentByClass<UVirtualImuComponent>())if(C->HasSample())Values.Add(MakeShared<FJsonValueObject>(C->StatusJson()));
        S->Label->SetArrayField(TEXT("imu"),Values);S->Label->SetStringField(TEXT("imu_pairing"),TEXT("latest fixed-step sample at scene write admission; independent timestamps; not simultaneous capture"));
    }
    const auto LabelObject=S->Label;const auto ConditionsObject=S->Conditions;const auto LidarFrame=S->LidarFrame;const auto Paths=S->ActorPaths;const auto Names=S->ActorNames;
    const FString Root=S->Root,SensorId=S->LidarBefore.SensorId,Commit=S->Commit;const int32 Scene=S->Plan?S->NextScene++:S->Completed;const bool IncludeTruth=S->Settings.CameraTruth;const auto Cancelled=S->Cancelled;const double Started=S->CaptureStarted;
    float Delay=0;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    Delay=WorkerDelayForTests;
#endif
    const bool TestSet=bool(S->Plan);
    S->Write=Async(EAsyncExecution::ThreadPool,[Frame,LidarFrame,Module,LabelObject,ConditionsObject,Paths,Names,Root,SensorId,Commit,Scene,IncludeTruth,Cancelled,Started,Delay,TestSet]()
    {
        const FString Label=Json(LabelObject),Conditions=Json(ConditionsObject);
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
        if(Frame){const int32 W=Frame->Intrinsics.Width,H=Frame->Intrinsics.Height;
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
        }
        Files.Add(TextFile(TEXT("label.json"),Label));for(const auto& F:Files)Result.Bytes+=F.Bytes.Num();
        const FString Directory=Root/FString::Printf(TEXT("scene_%05d"),Scene);
        const bool Written=TestSet?UVirtualSensorFileSaveSubsystem::WriteTestSetSceneBundle(Directory,Files,[Cancelled](){return Cancelled->load();}):UVirtualSensorFileSaveSubsystem::WriteDatasetSceneBundle(Directory,Files,[Cancelled](){return Cancelled->load();});
        if(!Written){Result.Error=TEXT("데이터셋 저장 실패 또는 중단");return Result;}
        TSharedPtr<FJsonObject> Manifest;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Conditions),Manifest);
        Manifest->SetNumberField(TEXT("completed_scenes"),Scene+1);Manifest->SetStringField(TEXT("coordinates"),TEXT("PCD: RH sensor-local X forward,Y left,Z up meters; pose: UE world X forward,Y right,Z up meters; depth: optical Z meters; instance: R+256G+65536B"));
        Manifest->SetStringField(TEXT("lidar_labels"),TEXT("5 byte little-endian records in PCD point order: uint8 semantic_id,uint32 instance_id; 0 unknown; no padding"));
        FString Sha=Commit.IsEmpty()?TEXT(MA0T10_PROJECT_COMMIT):Commit;
        int32 Exit=0;
#if WITH_EDITOR
        // Editor-only fallback for source builds made without Git at build time.
        if(Sha.IsEmpty())
        {
            FString GitError;const FString Args=TEXT("-C \"")+FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())+TEXT("\" rev-parse HEAD");
            if(!FPlatformProcess::ExecProcess(TEXT("git.exe"),*Args,&Exit,&Sha,&GitError))Exit=-1;
        }
#endif
        Result.Commit=Sha.TrimStartAndEnd();Manifest->SetStringField(TEXT("project_commit"),Result.Commit);
        const FString Temp=Root/TEXT("dataset.json.pending"),Final=Root/TEXT("dataset.json");
        if(Exit!=0||Result.Commit.IsEmpty()||Cancelled->load()||!FFileHelper::SaveStringToFile(Json(Manifest.ToSharedRef()),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)||!IFileManager::Get().Move(*Final,*Temp,true,true,false,true))
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
    if(S->Camera.IsValid()&&S->Camera->GetConfigurationRevision()==S->CameraRevision&&(!S->Camera.IsValid()||S->Camera->GetActorTransform().Equals(S->CameraApplied)))S->Camera->ApplyDatasetState(S->CameraBefore,Error);
    if(S->Slab.IsValid()&&!S->Slab->IsSimulationActive()&&S->Slab->GetActorTransform().Equals(S->SlabApplied))
    {auto* Slab=S->Slab.Get();Slab->ApplySurfacePreset(S->SurfaceBefore,S->LinkBefore,Error);Slab->Roughness=S->RoughBefore;Slab->Oxidation=S->OxBefore;Slab->bHotAppearance=S->HotBefore;Slab->EmissiveStrength=S->EmissiveBefore;Slab->RefreshDatasetAppearance();Slab->ApplyDatasetGeometry(S->SizeBefore);Slab->SetActorTransform(S->SlabBefore,false,nullptr,ETeleportType::TeleportPhysics);}
    if(S->Light.IsValid()&&S->Light->GetActorTransform().Equals(S->LightApplied)&&FMath::IsNearlyEqual(S->Light->GetLightComponent()->Intensity,S->LightAppliedIntensity)){S->Light->SetActorTransform(S->LightBefore);S->Light->GetLightComponent()->SetIntensity(S->LightBeforeIntensity);}
}
void UVirtualDatasetSubsystem::Pause(){if(Session&&Session->Plan){YieldPlannedCapture();return;}if(IsActive()){Session->PauseRequested=true;++Revision;}}
bool UVirtualDatasetSubsystem::Resume(FString& Error)
{
    if(!IsPaused()){Error=TEXT("일시 중지 상태에서 재개하세요");return false;}
    auto S=Session;
    if(S->Plan)
    {
        if(!S->Slab.IsValid()||!S->Lidar.IsValid()||S->Slab->IsSimulationActive()||S->Lidar->IsSensorRunning()||!S->Lidar->CanEditSensorConfiguration(Error))return false;
        // A normal scenario may have changed the Slab while yielded. Rebase restoration ownership.
        S->SlabBefore=S->Slab->GetActorTransform();S->SizeBefore=S->Slab->SlabMesh->GetRelativeScale3D()*S->Slab->SlabMesh->GetStaticMesh()->GetBounds().BoxExtent*2.;
        S->SurfaceBefore=S->Slab->GetSurfacePreset();S->LinkBefore=S->Slab->IsSurfaceLidarLinked();S->RoughBefore=S->Slab->Roughness;S->OxBefore=S->Slab->Oxidation;S->HotBefore=S->Slab->bHotAppearance;S->EmissiveBefore=S->Slab->EmissiveStrength;
        if(!S->Lidar->ReadEditableState(S->LidarBefore)||(S->Camera.IsValid()&&!S->Camera->ReadEditableState(S->CameraBefore)))return false;
    }
    S->PauseRequested=false;
    if(S->Plan&&S->NextScene>=S->Settings.Scenes)S->Phase=EDatasetPhase::Write;
    else if(!ApplyScene(Error))return false;
    GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.01f,true);++Revision;return true;
}
void UVirtualDatasetSubsystem::Stop()
{if(!Session||Session->Phase==EDatasetPhase::Done||Session->Phase==EDatasetPhase::Draining)return;Session->Cancelled->store(true);Restore();if(Session->Phase!=EDatasetPhase::Export)Session->Phase=Session->Write.IsValid()?EDatasetPhase::Draining:EDatasetPhase::Done;if(Session->Phase==EDatasetPhase::Done&&GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);++Revision;}
void UVirtualDatasetSubsystem::Deinitialize()
{Stop();if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);if(Truth){Truth->DestroyComponent();Truth=nullptr;}Session.Reset();Super::Deinitialize();}
