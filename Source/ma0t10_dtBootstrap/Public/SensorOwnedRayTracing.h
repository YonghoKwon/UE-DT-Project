#pragma once
#include "CoreMinimal.h"

/** Immutable triangle list. W stores the acquisition-local surface index bits. */
struct FSensorRtMesh
{
    uint64 Revision = 0;
    TSharedPtr<const TArray<FVector3f>, ESPMode::ThreadSafe> Vertices;
    TSharedPtr<const TArray<FVector4f>, ESPMode::ThreadSafe> NormalsAndSurfaces;
    FTransform LocalToWorld=FTransform::Identity;
};
struct FSensorRtRay
{
    FVector3f Origin;
    float MaxDistance = 0;
    FVector3f Direction;
    float Padding = 0;
};
struct FSensorRtHit
{
    FVector4f NormalAndDistance;
    uint32 Surface = MAX_uint32;
    uint32 Mesh = MAX_uint32;
    uint32 Triangle = MAX_uint32;
    uint32 Reserved = 0;
};
struct FSensorRtRequest
{
    TArray<FSensorRtMesh> Meshes;
    TArray<FSensorRtRay> Rays;
    /** Optional RT notification. Enqueue CPU work only; no UObject access or waits.
     * Called once on terminal readback/failure, omitted on cancellation. */
    TFunction<void(TArray<FSensorRtHit>&&, const FString&)> OnReadback;
};
class MA0T10_DTBOOTSTRAP_API ISensorRtScan
{
public:
    virtual ~ISensorRtScan() = default;
    /** Nonblocking. A terminal result is immutable; caller owns generation checks. */
    virtual bool Poll(TArray<FSensorRtHit>& OutHits, FString& Error) = 0;
    virtual void Cancel() = 0;
};
namespace SensorOwnedRayTracing
{
    MA0T10_DTBOOTSTRAP_API bool IsAvailable();
    MA0T10_DTBOOTSTRAP_API uint64 GetBlasBuildCount();
    /** One request per owning session. Bounded globally; nullptr means not admitted. */
    MA0T10_DTBOOTSTRAP_API TSharedPtr<ISensorRtScan, ESPMode::ThreadSafe> Submit(FSensorRtRequest&& Request);
    void RegisterShaders();
    void Shutdown();
}
