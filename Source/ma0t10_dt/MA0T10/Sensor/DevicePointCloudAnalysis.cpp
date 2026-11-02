#include "DevicePointCloudAnalysis.h"
#include "Misc/ScopeRWLock.h"
namespace
{
    FRWLock ProviderLock;
    TSharedPtr<const IDevicePointCloudAnalysisProvider,ESPMode::ThreadSafe> Provider;
float DeviceNormalizeSignedAngleDegrees(float Angle) { while (Angle > 180.0f) Angle -= 360.0f; while (Angle < -180.0f) Angle += 360.0f; return Angle; }
float DeviceNormalizeAxisAngleDegrees(float Angle) { Angle = DeviceNormalizeSignedAngleDegrees(Angle); if (Angle > 90.0f) Angle -= 180.0f; if (Angle < -90.0f) Angle += 180.0f; return Angle; }
}
void DevicePointCloudAnalysis::Register(TSharedRef<const IDevicePointCloudAnalysisProvider,ESPMode::ThreadSafe> Value)
{ FWriteScopeLock Lock(ProviderLock); Provider=MoveTemp(Value); }
void DevicePointCloudAnalysis::Unregister()
{ FWriteScopeLock Lock(ProviderLock); Provider.Reset(); }
TSharedPtr<const IDevicePointCloudAnalysisProvider,ESPMode::ThreadSafe> DevicePointCloudAnalysis::Snapshot()
{ FReadScopeLock Lock(ProviderLock); return Provider; }
FVirtualLidarSlabAnalysisResult DevicePointCloudAnalysis::Geometry(TConstArrayView<FVector> SelectedPoints,float ReferenceYawDegrees)
{
    FVirtualLidarSlabAnalysisResult Result;
    Result.ReferenceYawDegrees=ReferenceYawDegrees;
    Result.SlabHitPointCount=SelectedPoints.Num();
    if(SelectedPoints.Num()<3)return Result;
    FVector Sum = FVector::ZeroVector;
    Result.BoundsMin = FVector(FLT_MAX);
    Result.BoundsMax = FVector(-FLT_MAX);
    for (const FVector& Point : SelectedPoints)
    {
        Sum += Point;
        Result.BoundsMin.X = FMath::Min(Result.BoundsMin.X, Point.X);
        Result.BoundsMin.Y = FMath::Min(Result.BoundsMin.Y, Point.Y);
        Result.BoundsMin.Z = FMath::Min(Result.BoundsMin.Z, Point.Z);
        Result.BoundsMax.X = FMath::Max(Result.BoundsMax.X, Point.X);
        Result.BoundsMax.Y = FMath::Max(Result.BoundsMax.Y, Point.Y);
        Result.BoundsMax.Z = FMath::Max(Result.BoundsMax.Z, Point.Z);
    }

    Result.Center = Sum / static_cast<float>(SelectedPoints.Num());

    double XX = 0.0;
    double XY = 0.0;
    double YY = 0.0;
    for (const FVector& Point : SelectedPoints)
    {
        const double DX = static_cast<double>(Point.X - Result.Center.X);
        const double DY = static_cast<double>(Point.Y - Result.Center.Y);
        XX += DX * DX;
        XY += DX * DY;
        YY += DY * DY;
    }

    const double Count = static_cast<double>(SelectedPoints.Num());
    XX /= Count;
    XY /= Count;
    YY /= Count;

    const double Trace = XX + YY;
    const double Delta = FMath::Sqrt(FMath::Max(0.0, ((XX - YY) * (XX - YY) * 0.25) + (XY * XY)));
    const double LambdaMajor = Trace * 0.5 + Delta;
    const double LambdaMinor = Trace * 0.5 - Delta;

    FVector2D Axis(1.0f, 0.0f);
    if (FMath::Abs(XY) > KINDA_SMALL_NUMBER || FMath::Abs(LambdaMajor - XX) > KINDA_SMALL_NUMBER)
    {
        Axis = FVector2D(static_cast<float>(XY), static_cast<float>(LambdaMajor - XX)).GetSafeNormal();
    }

    Result.EstimatedYawDegrees = DeviceNormalizeAxisAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X)));
    Result.AngleDeviationDegrees = DeviceNormalizeAxisAngleDegrees(Result.EstimatedYawDegrees - ReferenceYawDegrees);
    Result.Confidence = static_cast<float>(FMath::Clamp((LambdaMajor - LambdaMinor) / FMath::Max(1.0, LambdaMajor), 0.0, 1.0));
    Result.bValid=true;
    return Result;
}
