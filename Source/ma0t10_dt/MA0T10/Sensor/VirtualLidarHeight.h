#pragma once
#include "VirtualLidarSensorTypes.h"

/** One colour-height contract for projections, Niagara and CPU preview. */
namespace VirtualLidarHeight
{
inline bool IsWorld(const FVirtualLidarVisualizationSettings& S)
{
    return S.HeightReference == ELidarHeightReference::WorldZ ||
        (S.HeightReference == ELidarHeightReference::ProjectionDefault && S.ProjectionMode == ELidarMonitorProjectionMode::WorldTopDown);
}
inline float Centimeters(const FVirtualLidarVisualizationSettings& S, const FTransform& Pose, const FVirtualLidarPoint& P)
{
    return IsWorld(S) ? P.WorldLocation.Z : Pose.InverseTransformPosition(P.WorldLocation).Z;
}
inline FVector2D Range(const FVirtualLidarVisualizationSettings& S, const FTransform& Pose, const TArray<FVirtualLidarPoint>& Points)
{
    if (!S.bAutoHeightRange && FMath::IsFinite(S.HeightMinMeters) && FMath::IsFinite(S.HeightMaxMeters) && S.HeightMaxMeters > S.HeightMinMeters)
        return FVector2D(S.HeightMinMeters * 100.0, S.HeightMaxMeters * 100.0);
    double Min = TNumericLimits<double>::Max(), Max = -TNumericLimits<double>::Max();
    for (const auto& P : Points)
    {
        if (!P.bHit) continue;
        const double H = Centimeters(S, Pose, P);
        if (!FMath::IsFinite(H)) continue;
        Min = FMath::Min(Min, H); Max = FMath::Max(Max, H);
    }
    if (Min == TNumericLimits<double>::Max()) return FVector2D(0, 1);
    return FVector2D(Min, FMath::Max(Min + 0.001, Max));
}
inline float Normalize(float HeightCm, FVector2D RangeCm)
{
    return FMath::Clamp((HeightCm - RangeCm.X) / FMath::Max(0.001, RangeCm.Y - RangeCm.X), 0.0, 1.0);
}
}
