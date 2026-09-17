#include "VirtualLidarSemanticScene.h"
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
    auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId"));
    if (!Material) { Status = TEXT("GPU 분류 재질 누락: 미분류 (센서 측정 유지)"); return false; }
    UWorld* World = Scan.GetWorld();
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
            if (*It == Scan.GetOwner()) continue;
            TInlineComponentArray<UStaticMeshComponent*> Components(*It);
            for (auto* Source : Components)
                if (Source && Source->GetStaticMesh() && !Proxies.Contains(Source)) Proxies.Add(Source, FProxy());
        }
    }
    Identities.Reset();
    int32 Unsupported = 0;
    for (auto It = Proxies.CreateIterator(); It; ++It)
    {
        auto* Source = It.Key().Get();
        FProxy& Entry = It.Value();
        if (!Source || !Source->IsRegistered())
        {
            if (Entry.Mesh) { Scene.RemoveComponent(Entry.Mesh); Entry.Mesh->DestroyComponent(); }
            It.RemoveCurrent(); continue;
        }
        bool bSupported = Source->GetStaticMesh() && Source->IsVisible() && !Source->bHiddenInGame && !Source->GetOwner()->IsHidden();
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
        FVirtualLidarGpuSemanticIdentity Identity;
        Identity.Label = Scan.ResolveSemanticLabelForComponent(Source);
        Identity.ActorName = Source->GetOwner()->GetFName();
        Identity.ActorClass = Source->GetOwner()->GetClass()->GetFName();
        Identity.ActorTags = Source->GetOwner()->Tags;
        Identities.Add(Entry.Id, MoveTemp(Identity));
    }
    auto* Target = GetTarget();
    if (Target->SizeX != Width || Target->SizeY != Height)
    {
        Target->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
        Target->InitAutoFormat(Width, Height);
        Target->UpdateResourceImmediate(true);
    }
    CaptureComponent->SetWorldTransform(Request.AcquisitionTransform);
    CaptureComponent->FOVAngle = Request.HorizontalFovDegrees;
    VirtualSensorCaptureRendering::Prepare(*CaptureComponent, true);
    Scene.GetWorld()->SendAllEndOfFrameUpdates();
    CaptureComponent->CaptureScene();
    Status = FString::Printf(TEXT("GPU 의미 분류: %d개 proxy · 미지원/숨김 %d개 (깊이 불일치=미분류)"), Identities.Num(), Unsupported);
    return true;
}
