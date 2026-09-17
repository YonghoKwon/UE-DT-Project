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
    UTextureRenderTarget2D* GetTarget() const;
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
};
