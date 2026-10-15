#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VirtualSensorMotionProvider.generated.h"

/** World-space rigid velocity at acquisition; no dependency on a business Actor. */
struct FVirtualSensorMotionSample
{
    FVector LinearVelocityCmPerSecond = FVector::ZeroVector;
    FVector AngularVelocityRadiansPerSecond = FVector::ZeroVector;
    FVector PivotWorldCm = FVector::ZeroVector;
};

UINTERFACE(MinimalAPI)
class UVirtualSensorMotionProvider : public UInterface
{
    GENERATED_BODY()
};

class MA0T10_DT_API IVirtualSensorMotionProvider
{
    GENERATED_BODY()
public:
    /** Return an immutable velocity sample. Paused/stopped providers return zero. */
    virtual FVirtualSensorMotionSample CaptureSensorMotion() const = 0;
};
