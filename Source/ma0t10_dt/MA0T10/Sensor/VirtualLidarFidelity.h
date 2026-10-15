#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorMotionProvider.h"
#include "VirtualSensorDeviceProfileTypes.h"

class AActor;
class UPrimitiveComponent;

/** Pure, SI-explicit approximations shared by CPU/GPU conversion and regression tests. */
namespace VirtualLidarFidelity
{
struct FSurface
{
    float Reflectivity = .2f;
    float IntensityGain = 1.f;
    float DetectionProbabilityScale = 1.f;
    bool bTwoSided = false;
    bool bAllowSaturation = true;
};
MA0T10_DT_API int64 PointOffset(EVirtualLidarDeviceProfile Profile, int32 RayIndex, int32 Width, int32 Height, double PeriodSeconds);
MA0T10_DT_API FVirtualSensorMotionSample CaptureMotion(const AActor* Actor);
MA0T10_DT_API float SemanticReflectivity(FName Label, bool bOxidized = false);
MA0T10_DT_API FSurface ResolveSurface(const UPrimitiveComponent* Primitive);
MA0T10_DT_API float Intensity(const FSurface& Surface, float RangeCm, float IncidenceCosine, float AmbientLux);
MA0T10_DT_API float DetectionProbability(float Intensity, const FSurface& Surface);
MA0T10_DT_API float NoiseScale(float RangeRatio, float Reflectivity, float IncidenceCosine);
/** Tangent-plane rigid-motion approximation. Output remains in the acquisition reference pose (no deskew). */
MA0T10_DT_API bool RollingRange(const FVector& HitWorldCm, const FVector& NormalWorld, const FVector& RayWorld,
    const FTransform& AcquisitionPose, const FVirtualSensorMotionSample& Target, const FVirtualSensorMotionSample& Sensor,
    double OffsetSeconds, float& OutRangeCm, float& OutIncidenceCosine);
}
