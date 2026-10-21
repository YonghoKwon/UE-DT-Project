#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

struct FVirtualSensorRayHit
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    FVector3f Normal=FVector3f::ZeroVector;
    float Time=1;
};
/** Immutable geometry shared by CPU collision and sensor-owned acceleration structures.
 * Vertices are a local-centimetre triangle list plus immutable instance pose. Identity is acquisition-local.
 * No rendering, Scenario or sensor types cross this contract. */
struct FVirtualSensorCollisionMesh
{
    uint64 Revision = 0;
    FTransform LocalToWorld=FTransform::Identity;
    TSharedPtr<const TArray<FVector3f>,ESPMode::ThreadSafe> Vertices;
    TSharedPtr<const TArray<FVector4f>,ESPMode::ThreadSafe> NormalsAndSurfaces;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> Surfaces;
};
/** Optional project-scene query snapshot. Neutral to sensor and scenario types.
 * Capture and Trace run on the game thread; geometry stays immutable for one
 * acquisition. Unregistered consumers retain the ordinary world trace path. */
class MA0T10_DT_API IVirtualSensorRayQuery
{
public:
    virtual ~IVirtualSensorRayQuery()=default;
    virtual bool Trace(FVector Start,FVector End,FHitResult& Hit) const=0;
    /** Pure geometry only: no UObject access, world queries or mutation. */
    virtual void TraceGeometry(FVector Start,FVector End,FVirtualSensorRayHit& Hit) const=0;
    /** Resolve weak metadata and any unmanaged collision on the game thread. */
    virtual bool Resolve(FVector Start,FVector End,const FVirtualSensorRayHit& Raw,FHitResult& Hit) const=0;
    /** Pure conservative geometry test; true routes this ray through GT Resolve. */
    virtual bool RequiresUnmanagedResolve(FVector Start,FVector End) const { return true; }
    virtual FName BackendName() const=0;
    /** False retains CPU behavior; never silently omit unmanaged collision. */
    virtual bool CollisionMeshes(TArray<FVirtualSensorCollisionMesh>& Out) const { return false; }
    /** Game-thread diagnostics: candidate bounds and actual fallback traces. */
    virtual FVector2D FallbackStatistics() const { return FVector2D::ZeroVector; }
};
using FVirtualSensorRayQueryFactory=TFunction<TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe>(AActor*,ECollisionChannel)>;
