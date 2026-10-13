#pragma once
#include "CoreMinimal.h"

/** Bounded continuous input integration; units are cm/s and degrees/s. */
namespace VirtualSensorPlacementMotion
{
inline float BoundedDelta(float Delta) { return FMath::IsFinite(Delta)?FMath::Clamp(Delta,0.f,.05f):0.f; }
inline FVector Translation(const FVector& Axes,float Speed,float Delta,float Multiplier=1)
{return Axes.GetClampedToMaxSize(1)*FMath::Max(0.f,Speed)*BoundedDelta(Delta)*Multiplier;}
inline FRotator Rotation(const FRotator& Axes,float Speed,float Delta,float Multiplier=1)
{const float Step=FMath::Max(0.f,Speed)*BoundedDelta(Delta)*Multiplier;return FRotator(Axes.Pitch*Step,Axes.Yaw*Step,Axes.Roll*Step);}
}
