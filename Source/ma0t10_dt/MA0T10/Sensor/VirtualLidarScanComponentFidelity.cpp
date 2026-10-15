#include "VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorAcquisitionBackend.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

void UVirtualLidarScanComponent::BeginFidelityAcquisition(bool bCpu)
{
    AcquisitionTargetMotion.Reset();
    AcquisitionSensorMotion = FVirtualSensorMotionSample();
    if (!bEnableMotionDistortion || !bUseRollingPointTimestamps || !GetWorld()) return;
    AcquisitionSensorMotion = VirtualLidarFidelity::CaptureMotion(GetOwner());
    const FTransform Pose = GetComponentTransform();
    const double Now = GetWorld()->GetTimeSeconds();
    const double Dt = Now - PreviousFidelityWorldTime;
    // Kinematically moved sensors may not publish Actor velocity. First sample
    // remains zero; subsequent samples use the previous acquisition pose.
    if (PreviousFidelityWorldTime >= 0 && Dt > 1.e-5 && Dt < 1.0)
    {
        if (AcquisitionSensorMotion.LinearVelocityCmPerSecond.IsNearlyZero())
            AcquisitionSensorMotion.LinearVelocityCmPerSecond = (Pose.GetLocation() - PreviousFidelityPose.GetLocation()) / Dt;
        if (AcquisitionSensorMotion.AngularVelocityRadiansPerSecond.IsNearlyZero())
        {
            FQuat Delta = Pose.GetRotation() * PreviousFidelityPose.GetRotation().Inverse();
            if (Delta.W < 0) Delta = Delta * -1.0;
            FVector Axis; double Angle;
            Delta.ToAxisAndAngle(Axis, Angle);
            AcquisitionSensorMotion.AngularVelocityRadiansPerSecond = Axis * (Angle / Dt);
        }
        AcquisitionSensorMotion.PivotWorldCm = Pose.GetLocation();
    }
    PreviousFidelityPose = Pose;
    PreviousFidelityWorldTime = Now;
    if (bCpu)
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            FMotionSnapshot Snapshot;
            Snapshot.Pose = It->GetActorTransform();
            Snapshot.Motion = VirtualLidarFidelity::CaptureMotion(*It);
            AcquisitionTargetMotion.Add(*It, Snapshot);
        }
}

bool UVirtualLidarScanComponent::ResolveRollingHit(const FHitResult& Hit, const FVector& Direction,
    const FTransform& Pose, double Seconds, float& Range, float& Incidence) const
{
    FVirtualSensorMotionSample Motion;
    FVector Position = Hit.ImpactPoint;
    FVector Normal = Hit.ImpactNormal;
    if (const AActor* Actor = Hit.GetActor())
        if (const auto* Snapshot = AcquisitionTargetMotion.Find(const_cast<AActor*>(Actor)))
        {
            Motion = Snapshot->Motion;
            // CPU chunks run over multiple engine frames. Remove the current
            // rigid Actor pose before applying the acquisition-time velocity.
            const FTransform Current = Actor->GetActorTransform();
            Position = Snapshot->Pose.TransformPosition(Current.InverseTransformPosition(Position));
            Normal = Snapshot->Pose.TransformVectorNoScale(Current.InverseTransformVectorNoScale(Normal));
        }
    return VirtualLidarFidelity::RollingRange(Position, Normal, Direction, Pose, Motion, AcquisitionSensorMotion, Seconds, Range, Incidence);
}

void UVirtualLidarScanComponent::ApplyGpuFidelityPoint(FVirtualLidarPoint& Point,
    const FVirtualLidarDepthAcquisitionFrame& Frame, int32 Pixel, int32 RayIndex, float Range)
{
    const FVector Direction = ScheduledScanTransform.TransformVectorNoScale(Point.LocalDirection).GetSafeNormal();
    const FVirtualLidarGpuSemanticIdentity* Identity = nullptr;
    int32 Id = 0;
    if (Frame.SemanticIdDepth.IsValidIndex(Pixel))
    {
        const auto& Value = Frame.SemanticIdDepth[Pixel];
        if (FMath::IsFinite(Value.R) && FMath::IsFinite(Value.A)
            && FMath::Abs(Value.A - Frame.ForwardDepthCentimeters[Pixel]) <= 1.f)
        {
            Id = FMath::RoundToInt(Value.R);
            Identity = Frame.SemanticIdentities.Find(Id);
        }
    }
    VirtualLidarFidelity::FSurface Surface;
    Surface.Reflectivity = DefaultSurfaceReflectivity;
    FVirtualSensorMotionSample Target;
    if (Identity)
    {
        Surface = Identity->Surface;
        Target = Identity->Motion;
        Point.SemanticLabel = Identity->Label;
        Point.HitActorName = Identity->ActorName;
        Point.HitActorClassName = Identity->ActorClass;
        Point.HitActorTags = Identity->ActorTags;
    }
    // Reconstruct a tangent normal from the existing depth buffer. Do not mix
    // identities across silhouettes. Unresolved edge normals use front-facing
    // approximation; no extra SceneCapture, readback or CPU trace is introduced.
    FVector Normal = -Direction;
    const int32 W = Frame.CaptureWidth, H = Frame.CaptureHeight;
    const int32 X = Pixel % W, Y = Pixel / W;
    const double TanH = FidelityDepthTangents.X;
    const double TanV = FidelityDepthTangents.Y;
    const auto Position = [&](int32 P) {
        const double Z = Frame.ForwardDepthCentimeters[P];
        return FVector(Z, (2.0 * (P % W) / FMath::Max(1,W-1) - 1.0) * TanH * Z,
            (1.0 - 2.0 * (P / W) / FMath::Max(1,H-1)) * TanV * Z);
    };
    const auto Compatible = [&](int32 P) {
        if (!Frame.ForwardDepthCentimeters.IsValidIndex(P) || Frame.ForwardDepthCentimeters[P] <= 0) return false;
        if (FMath::Abs(Frame.ForwardDepthCentimeters[P]-Frame.ForwardDepthCentimeters[Pixel]) > FMath::Max(5.f,Range*.03f)) return false;
        return !Identity || (Frame.SemanticIdDepth.IsValidIndex(P) && FMath::RoundToInt(Frame.SemanticIdDepth[P].R)==Id);
    };
    const int32 PX = X+1<W ? Pixel+1 : Pixel-1;
    const int32 PY = Y+1<H ? Pixel+W : Pixel-W;
    if (W>1 && H>1 && Compatible(PX) && Compatible(PY))
    {
        FVector LocalNormal = FVector::CrossProduct(Position(PX)-Position(Pixel),Position(PY)-Position(Pixel)).GetSafeNormal();
        if (!LocalNormal.IsNearlyZero())
        {
            if (FVector::DotProduct(LocalNormal,Point.LocalDirection)>0) LocalNormal *= -1;
            Normal = ScheduledScanTransform.TransformVectorNoScale(LocalNormal).GetSafeNormal();
        }
    }
    float Cosine = FVector::DotProduct(-Direction,Normal);
    if (bEnableMotionDistortion && bUseRollingPointTimestamps)
    {
        const FVector Hit = ScheduledScanTransform.GetLocation() + Direction * Range;
        if (!VirtualLidarFidelity::RollingRange(Hit,Normal,Direction,ScheduledScanTransform,Target,AcquisitionSensorMotion,
            Point.PointTimeOffsetNanoseconds*1.e-9,Range,Cosine)) return;
    }
    if (Range<DeviceSpec.MinRangeCm || Range>MaxDistance) {Point.Validity=EVirtualLidarPointValidity::OutOfRange;return;}
    Cosine = Surface.bTwoSided ? FMath::Abs(Cosine) : FMath::Max(0.f,Cosine);
    const float Ratio=FMath::Clamp(Range/FMath::Max(1.f,MaxDistance),0.f,1.f);
    const float Ambient=1.f-.05f*FMath::Clamp(AmbientLightLux/100000.f,0.f,4.f);
    float Signal = bEnableMaterialResponse ? VirtualLidarFidelity::Intensity(Surface,Range,Cosine,AmbientLightLux)
        : FMath::Clamp(DefaultSurfaceReflectivity/(1.f+FMath::Square(Range*.01f/45.f))*5.f,0.f,1.f);
    const float Probability = bEnableMaterialResponse ? VirtualLidarFidelity::DetectionProbability(Signal,Surface)
        : FMath::Clamp((1.f-.05f*FMath::Square(Ratio))*Ambient,0.f,.9995f);
    Point.Confidence=Probability;
    if ((bEnableMaterialResponse || FidelityMode!=EVirtualSensorFidelityMode::IdealTruth)
        && DeterministicUnitRandom(RayIndex,0,0x47505531u)>Probability)
    { Point.Validity=EVirtualLidarPointValidity::BelowSignalThreshold;return; }
    if (bEnableRangeNoise && (bEnableMaterialResponse || FidelityMode!=EVirtualSensorFidelityMode::IdealTruth))
    {
        const float Scale=bEnableMaterialResponse?VirtualLidarFidelity::NoiseScale(Ratio,Surface.Reflectivity,Cosine):(.5f+.5f*Ratio);
        Range=FMath::Clamp(Range+.1f*FMath::Clamp(DeterministicGaussian(RayIndex,0)*RangeNoiseStandardDeviationMillimeters*Scale,
            -MaximumRangeErrorMillimeters,MaximumRangeErrorMillimeters),0.f,MaxDistance);
    }
    Point.bHit=true; Point.Distance=Range; Point.RangeMillimeters=FMath::RoundToInt(Range*10.f);
    Point.WorldLocation=ScheduledScanTransform.TransformPosition(Point.LocalDirection*Range);
    Point.SensorLocalPositionMeters=Point.LocalDirection*Range*.01;
    Point.SensorLocalPositionMeters.Y *= -1;
    Point.SurfaceReflectivity=Surface.Reflectivity; Point.IncidenceCosine=Cosine;
    Point.NormalizedIntensity=Signal; Point.RawIntensity=FMath::RoundToInt(Signal*65535.f);
    Point.Validity=Surface.bAllowSaturation&&Signal>=1.f?EVirtualLidarPointValidity::Saturated:EVirtualLidarPointValidity::Valid;
    Point.EchoCount=1; Point.EchoType=EVirtualLidarEchoType::Single;
}
