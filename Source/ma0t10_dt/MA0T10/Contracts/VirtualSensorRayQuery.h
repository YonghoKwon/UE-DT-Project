#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

struct FVirtualSensorRayHit
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    FVector3f Normal=FVector3f::ZeroVector;
    float Time=1;
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
    virtual FName BackendName() const=0;
    /** Game-thread diagnostics: candidate bounds and actual fallback traces. */
    virtual FVector2D FallbackStatistics() const { return FVector2D::ZeroVector; }
};
using FVirtualSensorRayQueryFactory=TFunction<TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe>(AActor*,ECollisionChannel)>;
