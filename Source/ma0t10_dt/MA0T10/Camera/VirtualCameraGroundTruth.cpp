#include "VirtualCameraGroundTruth.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorTextureReadback.h"
#include "VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorCaptureRendering.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSemanticScene.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorSemanticLabels.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHIGPUReadback.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "TimerManager.h"
#include <atomic>

namespace
{
std::atomic<int32> ActiveTruthWork{0};
constexpr float MaxDepthMeters=10000.f;
constexpr float PreviewMaxMeters=50.f;
FColor InstanceColor(uint32 Id)
{
    if(!Id)return FColor::Black;
    const uint32 H=Id*2654435761u;
    return FColor(64+(H&191),64+((H>>8)&191),64+((H>>16)&191));
}
TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for(double V:Values)Result.Add(MakeShared<FJsonValueNumber>(V));
    return Result;
}
bool EncodeImage(IImageWrapperModule& Module, EImageFormat Format, FVirtualSensorVerificationFile& File, const void* Pixels, int64 Bytes, int32 W, int32 H, ERGBFormat Raw, int32 Bits)
{
    auto Wrapper=Module.CreateImageWrapper(Format);
    if(!Wrapper||!Wrapper->CanSetRawFormat(Raw,Bits)||!Wrapper->SetRaw(Pixels,Bytes,W,H,Raw,Bits))return false;
    File.Bytes=Wrapper->GetCompressed(0);
    return !File.Bytes.IsEmpty();
}
}

FVirtualCameraIntrinsics FVirtualCameraIntrinsics::Make(int32 W,int32 H,double Fov)
{
    FVirtualCameraIntrinsics K;
    if(W<=0||H<=0||!FMath::IsFinite(Fov)||Fov<=0||Fov>=180)return K;
    K.Width=W;K.Height=H;K.Fx=K.Fy=W/(2.0*FMath::Tan(FMath::DegreesToRadians(Fov)*0.5));
    // Integer coordinates denote pixel centers; the optical axis lies at (W-1,H-1)/2.
    K.Cx=(W-1)*0.5;K.Cy=(H-1)*0.5;
    return K;
}
TArray<FVirtualCameraTruthBox> FVirtualCameraTruthFrame::FindBoxes(const TArray<uint8>& Labels,const TArray<uint32>& Ids,int32 W,int32 H)
{
    TArray<FVirtualCameraTruthBox> Result;
    if(W<=0||H<=0||int64(W)*H!=Ids.Num()||Labels.Num()!=Ids.Num())return Result;
    TMap<uint64,int32> Index;
    for(int32 I=0;I<Ids.Num();++I)
    {
        if(!Ids[I]||!Labels[I])continue;
        const uint64 Key=(uint64(Ids[I])<<8)|Labels[I];
        int32* Found=Index.Find(Key);
        if(!Found){const int32 N=Result.AddDefaulted();Index.Add(Key,N);Found=Index.Find(Key);Result[N].Instance=Ids[I];Result[N].Class=Labels[I];}
        auto& B=Result[*Found];const int32 X=I%W,Y=I/W;
        B.XMin=FMath::Min(B.XMin,X);B.YMin=FMath::Min(B.YMin,Y);B.XMax=FMath::Max(B.XMax,X);B.YMax=FMath::Max(B.YMax,Y);++B.Pixels;
    }
    Result.Sort([](const auto& A,const auto& B){return A.Instance==B.Instance?A.Class<B.Class:A.Instance<B.Instance;});
    return Result;
}

struct FVirtualCameraTruthWork
{
    ~FVirtualCameraTruthWork(){--ActiveTruthWork;}
    std::atomic<bool> Cancelled{false}, CopiesQueued{false}, InitialCopiesQueued{false};
    TSharedPtr<FRHIGPUTextureReadback,ESPMode::ThreadSafe> Depth,Semantic,Rgb;
    TSharedPtr<FVirtualCameraTruthFrame,ESPMode::ThreadSafe> Frame=MakeShared<FVirtualCameraTruthFrame,ESPMode::ThreadSafe>();
    TMap<int32,TPair<uint8,uint32>> Identities;
    TArray<FColor> Displays[3];
    TWeakObjectPtr<UVirtualCameraCaptureComponent> Source;
    int32 Revision=0;
    double Started=0;
    bool Mapping=false, Save=false,Dataset=false,IncludeTruth=true;
    bool WaitingSemanticCapture=false;
    FString Directory,Error;
    FVirtualCameraTruthSaveCompletion SaveCompleted;
    TArray<int64> SavedSizes;
};

UVirtualCameraGroundTruth::UVirtualCameraGroundTruth(){PrimaryComponentTick.bCanEverTick=false;}
UVirtualCameraGroundTruth::~UVirtualCameraGroundTruth()=default;
bool UVirtualCameraGroundTruth::IsSaving() const { return Work&&Work->Save; }
void UVirtualCameraGroundTruth::ReleaseReadbackCache()
{
    auto D=MoveTemp(CachedDepth),S=MoveTemp(CachedSemantic),R=MoveTemp(CachedRgb);CachedSize=FIntPoint::ZeroValue;
    ENQUEUE_RENDER_COMMAND(ReleaseCameraTruthCache)([D=MoveTemp(D),S=MoveTemp(S),R=MoveTemp(R)](FRHICommandListImmediate&)mutable{D.Reset();S.Reset();R.Reset();});
}
void UVirtualCameraGroundTruth::EndPlay(const EEndPlayReason::Type Reason){Cancel();ReleaseReadbackCache();Super::EndPlay(Reason);}
void UVirtualCameraGroundTruth::OnUnregister(){Cancel();ReleaseReadbackCache();if(DepthCapture)DepthCapture->DestroyComponent();DepthCapture=nullptr;DepthTarget=nullptr;SemanticScene.Reset();Super::OnUnregister();}
void UVirtualCameraGroundTruth::Cancel(const FString& Reason)
{
    if(SemanticScene)SemanticScene->CancelPendingCapture();
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(PollTimer);
    if(Work)
    {
        Work->Cancelled=true;auto Old=MoveTemp(Work);
        ENQUEUE_RENDER_COMMAND(CameraTruthRelease)([Old](FRHICommandListImmediate&)mutable{Old->Depth.Reset();Old->Semantic.Reset();Old->Rgb.Reset();});
        auto Completion=MoveTemp(Old->SaveCompleted);if(Completion)Completion(nullptr,FString(),Reason,{});
    }
    LastFrame.Reset();PreviewTextures.Reset();Status=TEXT("정답 데이터 대기");
}
bool UVirtualCameraGroundTruth::Capture(UVirtualCameraCaptureComponent& Source,const FVirtualCameraPayloadSnapshot& Snapshot,bool Save,bool Dataset,bool IncludeTruth,FVirtualCameraTruthSaveCompletion Completion)
{
    auto Reject=[&](const FString& Reason){Status=Reason;if(Completion)Completion(nullptr,FString(),Reason,{});return false;};
    if(Work)return Reject(TEXT("정답 데이터 처리 중 · 새 요청 생략"));
    if(!GetWorld()||(IncludeTruth&&!Source.bGroundTruthEnabled)||(!IncludeTruth&&!Dataset)||Snapshot.Width<=0||Snapshot.Height<=0||int64(Snapshot.Width)*Snapshot.Height>4096ll*2160||Source.ProjectionType!=ECameraProjectionMode::Perspective||Source.bUseCustomProjectionMatrix)
    {return Reject(TEXT("정답 데이터: 기본 원근 카메라만 지원합니다"));}
    if(ActiveTruthWork.fetch_add(1)>=2){--ActiveTruthWork;return Reject(TEXT("정답 데이터: 전역 처리 한도 2개"));}
    auto State=MakeShared<FVirtualCameraTruthWork,ESPMode::ThreadSafe>();
    State->Source=&Source;State->Revision=Source.GetFileCaptureRevision();State->Started=FPlatformTime::Seconds();State->Save=Save;State->Dataset=Dataset;State->IncludeTruth=IncludeTruth;
    State->Frame->Acquisition=Snapshot;State->Frame->Intrinsics=FVirtualCameraIntrinsics::Make(Snapshot.Width,Snapshot.Height,Snapshot.HorizontalFov);
    if(IncludeTruth)
    {
    if(!DepthCapture)
    {
        DepthCapture=NewObject<USceneCaptureComponent2D>(this);
        DepthCapture->bCaptureEveryFrame=false;DepthCapture->bCaptureOnMovement=false;
        DepthCapture->CaptureSource=ESceneCaptureSource::SCS_SceneDepth;
        DepthCapture->ShowFlags.DisableAdvancedFeatures();DepthCapture->ShowFlags.SetAntiAliasing(false);DepthCapture->ShowFlags.SetPostProcessing(false);
        DepthTarget=NewObject<UTextureRenderTarget2D>(this);DepthTarget->RenderTargetFormat=RTF_R32f;
        DepthTarget->ClearColor=FLinearColor::Black;DepthCapture->TextureTarget=DepthTarget;DepthCapture->RegisterComponentWithWorld(GetWorld());
        SemanticScene=MakeUnique<FVirtualLidarSemanticScene>();
    }
    if(DepthTarget->SizeX!=Snapshot.Width||DepthTarget->SizeY!=Snapshot.Height)
    {DepthTarget->InitAutoFormat(Snapshot.Width,Snapshot.Height);DepthTarget->UpdateResourceImmediate(true);}
    DepthCapture->SetWorldTransform(Source.GetComponentTransform());DepthCapture->FOVAngle=Snapshot.HorizontalFov;
    DepthCapture->HiddenActors=Source.HiddenActors;DepthCapture->HiddenComponents=Source.HiddenComponents;
    DepthCapture->ShowOnlyActors=Source.ShowOnlyActors;DepthCapture->ShowOnlyComponents=Source.ShowOnlyComponents;DepthCapture->PrimitiveRenderMode=Source.PrimitiveRenderMode;
    // Isolated proxies share the existing LiDAR ID material; no stencil, source material or asset writes.
    auto Visible=[&Source](const UPrimitiveComponent* C)
    {
        if(Source.HiddenActors.Contains(C->GetOwner())||Source.HiddenComponents.Contains(C))return false;
        return Source.PrimitiveRenderMode!=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList||Source.ShowOnlyActors.Contains(C->GetOwner())||Source.ShowOnlyComponents.Contains(C);
    };
    if(!SemanticScene->Capture(GetWorld(),Source.GetOwner(),Source.GetComponentTransform(),Snapshot.HorizontalFov,Snapshot.Width,Snapshot.Height,VirtualSensorSemanticLabels::Resolve,true,Visible))
    {return Reject(SemanticScene->Status);}
    State->WaitingSemanticCapture=SemanticScene->IsCapturePending();
    for(const auto& Entry:SemanticScene->Identities)
    {
        const FString Path=SemanticScene->ActorPaths.FindRef(Entry.Key);
        const auto Actor=SemanticScene->Actors.FindRef(Entry.Key);
        uint32* Id=ActorIds.Find(Actor);
        if(!Id){if(NextActorId>0xffffff)return Reject(TEXT("정답 데이터 Actor ID 한도 초과"));ActorIds.Add(Actor,NextActorId++);Id=ActorIds.Find(Actor);}
        State->Identities.Add(Entry.Key,TPair<uint8,uint32>(VirtualSensorSemanticLabels::ClassId(Entry.Value.Label),*Id));State->Frame->ActorPaths.Add(*Id,Path);
    }
    VirtualSensorCaptureRendering::Prepare(*DepthCapture,true);
    DepthCapture->CaptureScene();
    }
    auto* DepthResource=IncludeTruth?DepthTarget->GameThread_GetRenderTargetResource():nullptr;
    auto* SemanticResource=IncludeTruth?SemanticScene->GetTarget()->GameThread_GetRenderTargetResource():nullptr;
    if(CachedSize!=FIntPoint(Snapshot.Width,Snapshot.Height)){ReleaseReadbackCache();CachedSize=FIntPoint(Snapshot.Width,Snapshot.Height);}
    if(IncludeTruth)
    {
        if(!CachedDepth){CachedDepth=MakeShared<FRHIGPUTextureReadback,ESPMode::ThreadSafe>(TEXT("CameraTruthDepth"));++ReadbackAllocations;}
        if(!CachedSemantic){CachedSemantic=MakeShared<FRHIGPUTextureReadback,ESPMode::ThreadSafe>(TEXT("CameraTruthIds"));++ReadbackAllocations;}
        State->Depth=CachedDepth;State->Semantic=CachedSemantic;
    }
    auto* RgbResource=Dataset&&Source.GetCameraRenderTarget()?Source.GetCameraRenderTarget()->GameThread_GetRenderTargetResource():nullptr;
    if(Dataset){if(!CachedRgb){CachedRgb=MakeShared<FRHIGPUTextureReadback,ESPMode::ThreadSafe>(TEXT("DatasetRgb"));++ReadbackAllocations;}State->Rgb=CachedRgb;}
    const bool QueueSemanticNow=!State->WaitingSemanticCapture;
    ENQUEUE_RENDER_COMMAND(CameraTruthCopy)([State,DepthResource,SemanticResource,RgbResource,QueueSemanticNow](FRHICommandListImmediate& Cmd)
    {
        if(State->Cancelled)return;
        // InitResource/UpdateResource and both captures precede this command. Fetching
        // texture refs on the game thread can observe an uninitialized first target.
        const auto D=DepthResource?DepthResource->GetRenderTargetTexture():FTextureRHIRef(), S=SemanticResource?SemanticResource->GetRenderTargetTexture():FTextureRHIRef();
        if(!State->IncludeTruth||(D.IsValid()&&(!QueueSemanticNow||S.IsValid())))
        {
            if(State->IncludeTruth){VirtualSensorTextureReadback::Enqueue(Cmd,*State->Depth,D);if(QueueSemanticNow)VirtualSensorTextureReadback::Enqueue(Cmd,*State->Semantic,S);}
            if(State->Rgb){if(!RgbResource||!RgbResource->GetRenderTargetTexture().IsValid())return;VirtualSensorTextureReadback::Enqueue(Cmd,*State->Rgb,RgbResource->GetRenderTargetTexture());}
            if(QueueSemanticNow)State->CopiesQueued.store(true,std::memory_order_release);
            else State->InitialCopiesQueued.store(true,std::memory_order_release);
        }
    });
    State->Frame->SubmitMs=(FPlatformTime::Seconds()-State->Started)*1000;
    State->SaveCompleted=MoveTemp(Completion);Work=State;Status=TEXT("정답 데이터 읽는 중");
    GetWorld()->GetTimerManager().SetTimer(PollTimer,this,&ThisClass::Poll,0.02f,true);
    return true;
}

void UVirtualCameraGroundTruth::Poll()
{
    auto State=Work;if(!State||State->Cancelled||State->Mapping)return;
    if(!State->Source.IsValid()||State->Source->GetFileCaptureRevision()!=State->Revision||(State->IncludeTruth&&!State->Source->bGroundTruthEnabled)){Cancel();return;}
    if(State->WaitingSemanticCapture&&State->InitialCopiesQueued.load(std::memory_order_acquire)&&SemanticScene->PollPendingCapture())
    {
        State->WaitingSemanticCapture=false;
        auto* Resource=SemanticScene->GetTarget()->GameThread_GetRenderTargetResource();
        ENQUEUE_RENDER_COMMAND(CameraTruthDeferredSemanticCopy)([State,Resource](FRHICommandListImmediate& Cmd)
        {
            if(State->Cancelled)return;
            const auto Texture=Resource?Resource->GetRenderTargetTexture():FTextureRHIRef();
            if(Texture.IsValid())
            {VirtualSensorTextureReadback::Enqueue(Cmd,*State->Semantic,Texture);State->CopiesQueued.store(true,std::memory_order_release);}
        });
    }
    if(!State->CopiesQueued.load(std::memory_order_acquire)||(State->Depth&&!State->Depth->IsReady())||(State->Semantic&&!State->Semantic->IsReady())||(State->Rgb&&!State->Rgb->IsReady()))
    {if(FPlatformTime::Seconds()-State->Started>10){const FString Reason=TEXT("정답 데이터 GPU 대기 시간 초과");Cancel(Reason);Status=Reason;}return;}
    State->Mapping=true;GetWorld()->GetTimerManager().ClearTimer(PollTimer);
    auto* Module=&FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TWeakObjectPtr<UVirtualCameraGroundTruth> Weak(this);
    ENQUEUE_RENDER_COMMAND(CameraTruthMap)([State,Weak,Module](FRHICommandListImmediate&)mutable
    {
        TArray<FLinearColor> IdPixels;int32 DP=0,SP=0;
        const float* D=State->Depth?static_cast<const float*>(State->Depth->Lock(DP)):nullptr;
        const FLinearColor* S=State->Semantic?static_cast<const FLinearColor*>(State->Semantic->Lock(SP)):nullptr;
        const int32 W=State->Frame->Intrinsics.Width,H=State->Frame->Intrinsics.Height;
        if(State->Rgb)
        {
            int32 RP=0;const auto* R=static_cast<const FColor*>(State->Rgb->Lock(RP));
            if(R&&RP>=W&&!State->Cancelled){State->Frame->DatasetRgb.SetNumUninitialized(W*H);for(int32 Y=0;Y<H;++Y)FMemory::Memcpy(State->Frame->DatasetRgb.GetData()+Y*W,R+Y*RP,W*sizeof(FColor));}
            else State->Error=TEXT("데이터셋 RGB 읽기 실패");
            if(R)State->Rgb->Unlock();State->Rgb.Reset();
        }
        if(D&&S&&DP>=W&&SP>=W&&!State->Cancelled)
        {
            State->Frame->DepthMeters.SetNumUninitialized(W*H);IdPixels.SetNumUninitialized(W*H);
            for(int32 Y=0;Y<H;++Y){FMemory::Memcpy(State->Frame->DepthMeters.GetData()+Y*W,D+Y*DP,W*sizeof(float));FMemory::Memcpy(IdPixels.GetData()+Y*W,S+Y*SP,W*sizeof(FLinearColor));}
        }
        else if(State->IncludeTruth)State->Error=TEXT("정답 데이터 GPU 읽기 실패");
        if(D)State->Depth->Unlock();if(S)State->Semantic->Unlock();State->Depth.Reset();State->Semantic.Reset();
        State->Frame->ReadbackLatencyMs=(FPlatformTime::Seconds()-State->Started)*1000;
        Async(EAsyncExecution::ThreadPool,[State,Weak,Module,IdPixels=MoveTemp(IdPixels),W,H]()mutable
        {
            const double Start=FPlatformTime::Seconds();auto& F=*State->Frame;
            if(!State->Cancelled&&(!State->IncludeTruth||IdPixels.Num()==W*H))
            {
                if(State->IncludeTruth)
                {
                F.Classes.SetNumZeroed(W*H);F.Instances.SetNumZeroed(W*H);
                if(!State->Dataset)for(auto& Display:State->Displays)Display.SetNumZeroed(W*H);
                for(int32 I=0;I<W*H;++I)
                {
                    float& Z=F.DepthMeters[I];Z*=0.01f;
                    if(!FMath::IsFinite(Z)||Z<=0||Z>MaxDepthMeters){Z=0;continue;}
                    const auto& P=IdPixels[I];
                    const auto* Identity=FMath::IsFinite(P.R)&&P.R>0&&P.R<16777216?State->Identities.Find(FMath::RoundToInt(P.R)):nullptr;
                    if(Identity&&FMath::Abs(P.A*.01f-Z)<=FMath::Max(.01f,Z*.001f))
                    {F.Classes[I]=Identity->Key;F.Instances[I]=Identity->Value;}
                    else ++F.UnclassifiedPixels;
                    if(!State->Dataset)
                    {
                    const float T=FMath::Clamp(Z/PreviewMaxMeters,0.f,1.f);
                    State->Displays[0][I]=FLinearColor::LerpUsingHSV(FLinearColor::Blue,FLinearColor::Red,T).ToFColor(true);
                    State->Displays[1][I]=VirtualSensorSemanticLabels::Color(F.Classes[I]);State->Displays[2][I]=InstanceColor(F.Instances[I]);
                    }
                }
                F.Boxes=FVirtualCameraTruthFrame::FindBoxes(F.Classes,F.Instances,W,H);
                }
                F.DecodeMs=(FPlatformTime::Seconds()-Start)*1000;
                if(State->Save&&!State->Cancelled)
                {
                    // Numeric IDs in RGB bytes; no palette/gamma conversion is applied to saved masks.
                    TArray<FColor> InstancePng;InstancePng.SetNumUninitialized(W*H);
                    for(int32 I=0;I<W*H;++I){const uint32 Id=F.Instances[I];InstancePng[I]=FColor(Id&255,(Id>>8)&255,(Id>>16)&255,255);}
                    State->Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("CameraGroundTruth"),FGuid::NewGuid().ToString(EGuidFormats::Digits));
                    TArray<FVirtualSensorVerificationFile> Files;Files.SetNum(4);
                    Files[0].Name=TEXT("depth.exr");Files[1].Name=TEXT("segmentation.png");Files[2].Name=TEXT("instance.png");Files[3].Name=TEXT("camera_info.json");
                    bool Ok=EncodeImage(*Module,EImageFormat::EXR,Files[0],F.DepthMeters.GetData(),int64(W)*H*sizeof(float),W,H,ERGBFormat::GrayF,32);
                    Ok=EncodeImage(*Module,EImageFormat::PNG,Files[1],F.Classes.GetData(),int64(W)*H,W,H,ERGBFormat::Gray,8)&&Ok;
                    Ok=EncodeImage(*Module,EImageFormat::PNG,Files[2],InstancePng.GetData(),int64(W)*H*sizeof(FColor),W,H,ERGBFormat::BGRA,8)&&Ok;
                    auto Info=MakeShared<FJsonObject>();Info->SetStringField(TEXT("schemaVersion"),TEXT("camera-ground-truth.local.v1"));Info->SetStringField(TEXT("sensor_id"),F.Acquisition.SensorId);
                    Info->SetStringField(TEXT("frame_id"),LexToString(F.Acquisition.FrameId));Info->SetStringField(TEXT("timestamp_utc"),F.Acquisition.TimestampUtc.ToIso8601());Info->SetNumberField(TEXT("revision"),F.Acquisition.AcquisitionRevision);
                    Info->SetArrayField(TEXT("resolution"),Numbers({double(W),double(H)}));Info->SetArrayField(TEXT("K"),Numbers({F.Intrinsics.Fx,0,F.Intrinsics.Cx,0,F.Intrinsics.Fy,F.Intrinsics.Cy,0,0,1}));
                    Info->SetNumberField(TEXT("horizontal_fov_degrees"),F.Acquisition.HorizontalFov);Info->SetNumberField(TEXT("vertical_fov_degrees"),FMath::RadiansToDegrees(2*FMath::Atan(H/(2*F.Intrinsics.Fy))));
                    Info->SetStringField(TEXT("depth"),TEXT("depth.exr: float32 optical-axis Z meters; 0 invalid/background; max 10000 m; radial range = Z*sqrt(1+((u-cx)/fx)^2+((v-cy)/fy)^2)"));
                    Info->SetStringField(TEXT("coordinates"),TEXT("UE world: X forward Y right Z up, pose translation meters. Optical: x right y down z forward. Integer pixel centers, bbox inclusive, ideal pinhole, zero distortion."));
                    Info->SetArrayField(TEXT("ue_camera_to_optical"),Numbers({0,1,0,0,0,-1,1,0,0}));
                    const FTransform Pose(F.Acquisition.Rotation,F.Acquisition.Location);
                    TArray<TSharedPtr<FJsonValue>> Matrix;
                    const FVector X=Pose.GetUnitAxis(EAxis::Y),Y=-Pose.GetUnitAxis(EAxis::Z),Z=Pose.GetUnitAxis(EAxis::X),P=Pose.GetLocation()*.01;
                    for(double V:{X.X,Y.X,Z.X,P.X,X.Y,Y.Y,Z.Y,P.Y,X.Z,Y.Z,Z.Z,P.Z,0.,0.,0.,1.})Matrix.Add(MakeShared<FJsonValueNumber>(V));
                    Info->SetArrayField(TEXT("optical_to_ue_world_m_row_major"),Matrix);
                    Info->SetArrayField(TEXT("ue_world_pose_xyz_m"),Numbers({P.X,P.Y,P.Z}));
                    const FQuat Q=Pose.GetRotation();Info->SetArrayField(TEXT("ue_world_pose_quaternion_xyzw"),Numbers({Q.X,Q.Y,Q.Z,Q.W}));
                    Info->SetStringField(TEXT("segmentation_encoding"),TEXT("uint8 grayscale class IDs. 0 invalid/background/unsupported. Palette colors are display only."));Info->SetStringField(TEXT("instance_encoding"),TEXT("Actor session ID = R + 256*G + 65536*B; 0 invalid. ISM instances owned by one Actor share its ID."));
                    Info->SetStringField(TEXT("semantic_support"),TEXT("opaque StaticMesh/ISM/HISM without WPO, source depth agreement required; masked/translucent/skeletal/procedural/unsupported pixels unclassified"));
                    Info->SetNumberField(TEXT("unclassified_pixels"),F.UnclassifiedPixels);
                    TArray<TSharedPtr<FJsonValue>> Palette,Actors,Boxes;
                    for(uint8 Id=0;Id<7;++Id){auto O=MakeShared<FJsonObject>();const auto C=VirtualSensorSemanticLabels::Color(Id);O->SetNumberField(TEXT("id"),Id);O->SetStringField(TEXT("name"),VirtualSensorSemanticLabels::Label(Id).ToString());O->SetArrayField(TEXT("rgb"),Numbers({double(C.R),double(C.G),double(C.B)}));Palette.Add(MakeShared<FJsonValueObject>(O));}
                    for(const auto& Pair:F.ActorPaths){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("id"),Pair.Key);O->SetStringField(TEXT("actor_path"),Pair.Value);Actors.Add(MakeShared<FJsonValueObject>(O));}
                    for(const auto& B:F.Boxes){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("instance_id"),B.Instance);O->SetNumberField(TEXT("class_id"),B.Class);O->SetNumberField(TEXT("visible_pixels"),B.Pixels);O->SetArrayField(TEXT("xyxy_inclusive"),Numbers({double(B.XMin),double(B.YMin),double(B.XMax),double(B.YMax)}));Boxes.Add(MakeShared<FJsonValueObject>(O));}
                    Info->SetArrayField(TEXT("palette"),Palette);Info->SetArrayField(TEXT("actors"),Actors);Info->SetArrayField(TEXT("boxes"),Boxes);
                    FString Json;FJsonSerializer::Serialize(Info,TJsonWriterFactory<>::Create(&Json));
                    // camera_info is the completion marker. Cancelled/partial exports never get this file.
                    FTCHARToUTF8 Utf8(*Json);Files[3].Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
                    if(Ok)Ok=UVirtualSensorFileSaveSubsystem::WriteCameraGroundTruthBundle(State->Directory,Files,[&State](){return State->Cancelled.load();});
                    if(Ok)for(const auto& File:Files)State->SavedSizes.Add(File.Bytes.Num());
                    if(!Ok)State->Error=TEXT("정답 데이터 저장 실패 또는 취소 · 부분 파일 확인");
                }
            }
            AsyncTask(ENamedThreads::GameThread,[Weak,State](){if(Weak.IsValid())Weak->Complete(State);});
        });
    });
}
void UVirtualCameraGroundTruth::Complete(const TSharedPtr<FVirtualCameraTruthWork,ESPMode::ThreadSafe>& State)
{
    if(Work!=State||State->Cancelled)return;
    if(!State->Source.IsValid()||State->Source->GetFileCaptureRevision()!=State->Revision||(State->IncludeTruth&&!State->Source->bGroundTruthEnabled)){Cancel();return;}
    Work.Reset();
    auto Completion=MoveTemp(State->SaveCompleted);
    if(!State->Error.IsEmpty()){Status=State->Error;if(Completion)Completion(nullptr,FString(),Status,{});return;}
    LastFrame=State->Frame;
    if(State->Dataset){Status=TEXT("데이터셋 카메라 준비 완료");return;}
    PreviewTextures.SetNum(3);
    const int32 W=LastFrame->Intrinsics.Width,H=LastFrame->Intrinsics.Height;
    for(int32 I=0;I<3;++I)
    {
        auto* Texture=PreviewTextures[I].Get();
        if(!Texture||Texture->GetSizeX()!=W||Texture->GetSizeY()!=H)Texture=UTexture2D::CreateTransient(W,H,PF_B8G8R8A8);
        Texture->SRGB=true;Texture->Filter=TF_Nearest;
        auto& Bulk=Texture->GetPlatformData()->Mips[0].BulkData;Bulk.Lock(LOCK_READ_WRITE);
        void* Data=Bulk.Realloc(int64(W)*H*sizeof(FColor));
        FMemory::Memcpy(Data,State->Displays[I].GetData(),int64(W)*H*sizeof(FColor));Texture->GetPlatformData()->Mips[0].BulkData.Unlock();Texture->UpdateResource();PreviewTextures[I]=Texture;
    }
    if(State->Save)LastSavedDirectory=State->Directory;
    Status=FString::Printf(TEXT("정답 #%lld · Z 0–50 m · 미분류 %d · 제출 %.2f ms · 읽기 %.2f ms · CPU %.2f ms%s"),LastFrame->Acquisition.FrameId,LastFrame->UnclassifiedPixels,LastFrame->SubmitMs,LastFrame->ReadbackLatencyMs,LastFrame->DecodeMs,State->Save?TEXT(" · 저장 완료"):TEXT(""));
    if(Completion)Completion(State->Frame.Get(),State->Directory,FString(),State->SavedSizes);
}
UTexture2D* UVirtualCameraGroundTruth::Preview(uint8 Mode) const{return Mode>=1&&Mode<=3&&PreviewTextures.IsValidIndex(Mode-1)?PreviewTextures[Mode-1].Get():nullptr;}
