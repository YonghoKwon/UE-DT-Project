#include "VirtualLidarFidelity.h"
#include "VirtualLidarSurfaceResponseComponent.h"
#include "VirtualSensorSemanticLabels.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace VirtualLidarFidelity
{
int64 PointOffset(EVirtualLidarDeviceProfile Profile, int32 RayIndex, int32 Width, int32 Height, double PeriodSeconds)
{
    const int64 Count = int64(Width) * Height;
    if (Count <= 1 || Width <= 0 || Height <= 0 || !FMath::IsFinite(PeriodSeconds) || PeriodSeconds <= 0) return 0;
    const int32 Index = FMath::Clamp<int64>(RayIndex, 0, Count - 1);
    // Generic rotating scanner: azimuth column then ring. ML-X/Livox: assumed
    // row raster, NOT a vendor firing table or the Livox non-repeating pattern.
    const int64 Order = Profile == EVirtualLidarDeviceProfile::Generic
        ? int64(Index % Width) * Height + Index / Width : Index;
    // Half-open scan interval: even the last firing precedes the next frame.
    return int64(FMath::Clamp(PeriodSeconds, 0.0, 60.0) * 1.e9 * double(Order) / double(Count));
}

FVirtualSensorMotionSample CaptureMotion(const AActor* Actor)
{
    FVirtualSensorMotionSample Sample;
    if (!Actor) return Sample;
    if (const auto* Provider = Cast<IVirtualSensorMotionProvider>(Actor)) return Provider->CaptureSensorMotion();
    for (const UActorComponent* Component : Actor->GetComponents())
        if (const auto* Provider = Cast<IVirtualSensorMotionProvider>(Component)) return Provider->CaptureSensorMotion();
    Sample.PivotWorldCm = Actor->GetActorLocation();
    Sample.LinearVelocityCmPerSecond = Actor->GetVelocity();
    if (const auto* Primitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()); Primitive && Primitive->IsSimulatingPhysics())
        Sample.AngularVelocityRadiansPerSecond = Primitive->GetPhysicsAngularVelocityInRadians();
    return Sample;
}

float SemanticReflectivity(FName Label, bool bOxidized)
{
    // Project assumptions at 940nm; these are not measured material constants.
    if (bOxidized) return .08f;
    if (Label == TEXT("Slab")) return .20f;
    if (Label == TEXT("Roller")) return .35f;
    if (Label == TEXT("Floor")) return .25f;
    if (Label == TEXT("Conveyor")) return .12f;
    return .20f;
}

FSurface ResolveSurface(const UPrimitiveComponent* Primitive)
{
    FSurface Surface;
    if (!Primitive || !Primitive->GetOwner()) return Surface;
    const AActor* Actor = Primitive->GetOwner();
    Surface.Reflectivity = SemanticReflectivity(VirtualSensorSemanticLabels::Resolve(Primitive), Actor->ActorHasTag(TEXT("Oxidized")) || Primitive->ComponentHasTag(TEXT("Oxidized")));
    if (const auto* Response = Actor->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>())
    {
        Surface.Reflectivity = FMath::Clamp(Response->Reflectivity940Nm, .001f, 1.f);
        Surface.IntensityGain = FMath::Max(0.f, Response->IntensityGain);
        Surface.DetectionProbabilityScale = FMath::Max(0.f, Response->DetectionProbabilityScale);
        Surface.bTwoSided = Response->bTwoSided;
        Surface.bAllowSaturation = Response->bAllowSaturation;
    }
    return Surface;
}

float Intensity(const FSurface& Surface, float RangeCm, float Cosine, float AmbientLux)
{
    const float Ambient = FMath::Clamp(1.f - .05f * FMath::Clamp(AmbientLux / 100000.f, 0.f, 4.f), 0.f, 1.f);
    return FMath::Clamp(Surface.Reflectivity * FMath::Clamp(Cosine, 0.f, 1.f)
        / (1.f + FMath::Square(RangeCm * .01f / 45.f)) * Ambient * Surface.IntensityGain, 0.f, 1.f);
}
float DetectionProbability(float Signal, const FSurface& Surface)
{
    return FMath::Clamp(Signal / (Signal + .001f) * Surface.DetectionProbabilityScale, 0.f, 1.f);
}
float NoiseScale(float RangeRatio, float Reflectivity, float Cosine)
{
    return (.5f + .5f * FMath::Clamp(RangeRatio, 0.f, 1.f))
        * FMath::Clamp(FMath::Sqrt(.2f / FMath::Max(.001f, Reflectivity)), .5f, 2.f)
        / FMath::Sqrt(FMath::Max(.05f, Cosine));
}

static FQuat Rotation(const FVector& AngularVelocity, double Seconds)
{
    const double Speed = AngularVelocity.Size();
    return Speed > UE_SMALL_NUMBER ? FQuat(AngularVelocity / Speed, Speed * Seconds) : FQuat::Identity;
}
bool RollingRange(const FVector& Hit, const FVector& Normal, const FVector& Ray, const FTransform& Pose,
    const FVirtualSensorMotionSample& Target, const FVirtualSensorMotionSample& Sensor, double Seconds, float& Range, float& Cosine)
{
    const FQuat TargetRotation = Rotation(Target.AngularVelocityRadiansPerSecond, Seconds);
    const FVector MovingNormal = TargetRotation.RotateVector(Normal).GetSafeNormal();
    const FVector MovingHit = Target.PivotWorldCm + Target.LinearVelocityCmPerSecond * Seconds
        + TargetRotation.RotateVector(Hit - Target.PivotWorldCm);
    const FVector MovingOrigin = Pose.GetLocation() + (Sensor.LinearVelocityCmPerSecond
        + FVector::CrossProduct(Sensor.AngularVelocityRadiansPerSecond, Pose.GetLocation() - Sensor.PivotWorldCm)) * Seconds;
    const FVector MovingRay = Rotation(Sensor.AngularVelocityRadiansPerSecond, Seconds).RotateVector(Ray).GetSafeNormal();
    const double Denominator = FVector::DotProduct(MovingNormal, MovingRay);
    if (FMath::Abs(Denominator) < 1.e-5) return false;
    const double Distance = FVector::DotProduct(MovingNormal, MovingHit - MovingOrigin) / Denominator;
    if (!FMath::IsFinite(Distance) || Distance <= 0) return false;
    Range = float(Distance);
    Cosine = float(-Denominator);
    return true;
}
}
