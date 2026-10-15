#include "VirtualLidarSemanticScene.h"
#include "VirtualLidarSurfaceResponseComponent.h"
#include "VirtualLidarScanComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorCaptureRendering.h"

FVirtualLidarSemanticScene::FVirtualLidarSemanticScene()
    : Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false).SetCreatePhysicsScene(false).SetTransactional(false).SetForceMipsResident(false))
{
    CaptureComponent = NewObject<USceneCaptureComponent2D>();
    CaptureComponent->bCaptureEveryFrame = false;
    CaptureComponent->bCaptureOnMovement = false;
    CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
    CaptureComponent->ShowFlags.DisableAdvancedFeatures();
    CaptureComponent->ShowFlags.SetLighting(false);
    CaptureComponent->ShowFlags.SetPostProcessing(false);
    CaptureComponent->ShowFlags.SetEyeAdaptation(false);
    CaptureComponent->ShowFlags.SetTonemapper(false);
    CaptureComponent->ShowFlags.SetAtmosphere(false);
    CaptureComponent->ShowFlags.SetFog(false);
    CaptureComponent->ShowFlags.SetAntiAliasing(false);
    CaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>(CaptureComponent);
    CaptureComponent->TextureTarget->ClearColor = FLinearColor::Black;
    Scene.AddComponent(CaptureComponent, FTransform::Identity);
}
UTextureRenderTarget2D* FVirtualLidarSemanticScene::GetTarget() const { return CaptureComponent->TextureTarget; }

bool FVirtualLidarSemanticScene::Capture(UVirtualLidarScanComponent& Scan, const FVirtualLidarDepthAcquisitionRequest& Request, int32 Width, int32 Height)
{
    return Capture(Scan.GetWorld(), Scan.GetOwner(), Request.AcquisitionTransform, Request.HorizontalFovDegrees, Width, Height,
        [&Scan](const UPrimitiveComponent* Component){ return Scan.ResolveSemanticLabelForComponent(Component); }, false, {}, Request.bRequestFidelity);
}

bool FVirtualLidarSemanticScene::Capture(UWorld* World, AActor* IgnoreOwner, const FTransform& Pose, float HorizontalFov, int32 Width, int32 Height, TFunctionRef<FName(const UPrimitiveComponent*)> Resolve, bool bRecordActorPaths, TFunction<bool(const UPrimitiveComponent*)> VisibilityFilter, bool bCaptureFidelity)
{
    auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId"));
    if (!Material) { Status = TEXT("GPU 분류 재질 누락: 미분류 (센서 측정 유지)"); return false; }
    if (!World) return false;
    if (!bShaderWarmupRequested)
    {
        Material->CacheShaders(EMaterialShaderPrecompileMode::Default);
        bShaderWarmupRequested = true;
    }
    const FMaterialResource* IdResource = Material->GetMaterialResource(World->GetFeatureLevel());
    if (!IdResource || !IdResource->IsGameThreadShaderMapComplete())
    {
        Status = TEXT("GPU 분류 shader 준비 중: 미분류 (측정 유지)");
        return false;
    }
    const double Now = FPlatformTime::Seconds();
    if (Now >= NextDiscoveryTime)
    {
        NextDiscoveryTime = Now + 0.5;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (*It == IgnoreOwner) continue;
            TInlineComponentArray<UStaticMeshComponent*> Components(*It);
            for (auto* Source : Components)
                if (Source && !Source->bHiddenInSceneCapture && Source->GetStaticMesh() && !Proxies.Contains(Source)) Proxies.Add(Source, FProxy());
        }
    }
    Identities.Reset();
    ActorPaths.Reset();
    Actors.Reset();
    int32 Unsupported = 0;
    for (auto It = Proxies.CreateIterator(); It; ++It)
    {
        auto* Source = It.Key().Get();
        FProxy& Entry = It.Value();
        // Diagnostic meshes must not enter the isolated semantic capture either.
        // Remove an existing proxy immediately when capture visibility changes.
        if (!Source || !Source->IsRegistered() || Source->bHiddenInSceneCapture)
        {
            if (Entry.Mesh) { Scene.RemoveComponent(Entry.Mesh); Entry.Mesh->DestroyComponent(); }
            It.RemoveCurrent(); continue;
        }
        bool bSupported = Source->GetStaticMesh() && Source->IsVisible() && !Source->bHiddenInGame && !Source->GetOwner()->IsHidden();
        if(VisibilityFilter&&!VisibilityFilter(Source))bSupported=false;
        for (auto* M : Source->GetMaterials())
        {
            const auto* Resource = M ? M->GetMaterialResource(World->GetFeatureLevel()) : nullptr;
            if (!M || M->GetBlendMode() != BLEND_Opaque || !Resource || static_cast<const FMaterial*>(Resource)->HasVertexPositionOffsetConnected()) bSupported = false;
        }
        if (!bSupported)
        {
            ++Unsupported;
            if (Entry.Mesh) Entry.Mesh->SetVisibility(false);
            continue;
        }
        if (!Entry.Mesh)
        {
            Entry.Id = NextId++;
            Entry.Mesh = Cast<UInstancedStaticMeshComponent>(Source) ? NewObject<UInstancedStaticMeshComponent>() : NewObject<UStaticMeshComponent>();
            Entry.Mesh->SetMobility(EComponentMobility::Movable);
            Entry.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Entry.Mesh->SetCastShadow(false);
            Entry.Mesh->SetStaticMesh(Source->GetStaticMesh());
            auto* MID = UMaterialInstanceDynamic::Create(Material, Entry.Mesh);
            MID->SetVectorParameterValue(TEXT("SemanticId"), FLinearColor(float(Entry.Id), 0, 0, 1));
            for (int32 Slot = 0; Slot < FMath::Max(1, Source->GetNumMaterials()); ++Slot) Entry.Mesh->SetMaterial(Slot, MID);
            Scene.AddComponent(Entry.Mesh, Source->GetComponentTransform());
        }
        Entry.Mesh->SetVisibility(true);
        Entry.Mesh->SetStaticMesh(Source->GetStaticMesh());
        if (!Entry.Mesh->GetComponentTransform().Equals(Source->GetComponentTransform())) Entry.Mesh->SetWorldTransform(Source->GetComponentTransform());
        if (auto* Instances = Cast<UInstancedStaticMeshComponent>(Source))
        {
            auto* Proxy = CastChecked<UInstancedStaticMeshComponent>(Entry.Mesh);
            const int32 Count = Instances->GetInstanceCount();
            bool bInstancesChanged = Proxy->GetInstanceCount() != Count;
            while (Proxy->GetInstanceCount() > Count) Proxy->RemoveInstance(Proxy->GetInstanceCount() - 1);
            for (int32 I = 0; I < Count; ++I)
            {
                FTransform T, Previous;
                Instances->GetInstanceTransform(I, T, false);
                if (I >= Proxy->GetInstanceCount()) Proxy->AddInstance(T);
                else if (Proxy->GetInstanceTransform(I, Previous, false) && !Previous.Equals(T)) { Proxy->UpdateInstanceTransform(I, T, false, false, true); bInstancesChanged = true; }
            }
            if (bInstancesChanged) Proxy->MarkRenderStateDirty();
        }
        // UE 5.3 caches mesh draw bindings in these isolated preview scenes.
        // With two semantic scenes captured concurrently the cached bindings can
        // retain a frame-scoped uniform buffer (RHI validation reports a previous
        // frame / null contents, then D3D12 dereferences it). Rebuild only our
        // proxy render state before capture; source components and global cached
        // draw-command settings remain untouched. UObject/MID/geometry are reused.
        Entry.Mesh->MarkRenderStateDirty();
        FVirtualLidarGpuSemanticIdentity Identity;
        Identity.Label = Resolve(Source);
        if (bCaptureFidelity)
        {
            Identity.Surface = VirtualLidarFidelity::ResolveSurface(Source);
            Identity.Motion = VirtualLidarFidelity::CaptureMotion(Source->GetOwner());
        }
        Identity.ActorName = Source->GetOwner()->GetFName();
        Identity.ActorClass = Source->GetOwner()->GetClass()->GetFName();
        Identity.ActorTags = Source->GetOwner()->Tags;
        if(const auto* Response=Source->GetOwner()->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>();Response&&Response->bUseGpuReflectivityOverride)
        {
            Identity.bHasOpticalOverride=true;Identity.Reflectivity940Nm=FMath::Clamp(Response->Reflectivity940Nm,.001f,1.f);Identity.IntensityGain=FMath::Max(0.f,Response->IntensityGain);
        }
        if(bRecordActorPaths){ActorPaths.Add(Entry.Id, Source->GetOwner()->GetPathName());Actors.Add(Entry.Id,Source->GetOwner());}
        Identities.Add(Entry.Id, MoveTemp(Identity));
    }
    auto* Target = GetTarget();
    if (Target->SizeX != Width || Target->SizeY != Height)
    {
        Target->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
        Target->InitAutoFormat(Width, Height);
        Target->UpdateResourceImmediate(true);
    }
    CaptureComponent->SetWorldTransform(Pose);
    CaptureComponent->FOVAngle = HorizontalFov;
    VirtualSensorCaptureRendering::Prepare(*CaptureComponent, true);
    Scene.GetWorld()->SendAllEndOfFrameUpdates();
    CaptureComponent->CaptureScene();
    Status = FString::Printf(TEXT("GPU 의미 분류: %d개 proxy · 미지원/숨김 %d개 (깊이 불일치=미분류)"), Identities.Num(), Unsupported);
    return true;
}
