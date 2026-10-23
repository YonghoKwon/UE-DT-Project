#pragma once
#include "CoreMinimal.h"
#include "PreviewScene.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorAcquisitionBackend.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UVirtualLidarScanComponent;

/** Private render world: no writes to source materials, stencil, visibility or collision. */
class FVirtualLidarSemanticScene
{
public:
    FVirtualLidarSemanticScene();
    bool Capture(UVirtualLidarScanComponent& Scan, const FVirtualLidarDepthAcquisitionRequest& Request, int32 Width, int32 Height);
    bool Capture(UWorld* World, AActor* IgnoreOwner, const FTransform& Pose, float HorizontalFov, int32 Width, int32 Height, TFunctionRef<FName(const UPrimitiveComponent*)> Resolve, bool bRecordActorPaths=true, TFunction<bool(const UPrimitiveComponent*)> VisibilityFilter={}, bool bCaptureFidelity=false);
    UTextureRenderTarget2D* GetTarget() const;
    /** Capture accepts/fixes a snapshot; an async PSO may defer its one render pass. */
    bool IsCapturePending() const { return bCapturePending; }
    bool PollPendingCapture();
    void CancelPendingCapture() { bCapturePending=false; }
    TMap<int32, FString> ActorPaths;
    TMap<int32, TWeakObjectPtr<AActor>> Actors;
    TMap<int32, FVirtualLidarGpuSemanticIdentity> Identities;
    FString Status;
private:
    FPreviewScene Scene;
    USceneCaptureComponent2D* CaptureComponent = nullptr;
    struct FProxy { UStaticMeshComponent* Mesh = nullptr; int32 Id = 0; };
    TMap<TWeakObjectPtr<UStaticMeshComponent>, FProxy> Proxies;
    double NextDiscoveryTime = 0;
    int32 NextId = 1;
    bool bShaderWarmupRequested = false;
    bool bCapturePending = false;
};
