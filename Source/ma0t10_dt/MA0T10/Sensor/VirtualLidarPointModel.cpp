#include "VirtualLidarPointModel.h"
#include "VirtualLidarFidelity.h"

float VirtualLidarPointModel::FConfig::DeterministicUnitRandom(int32 RayIndex,int32 ReturnIndex,uint32 Salt) const
{
    uint32 Value=HashCombineFast(RandomSequence,HashCombineFast(GetTypeHash(RayIndex),HashCombineFast(GetTypeHash(ReturnIndex),Salt)));
    Value^=Value<<13;Value^=Value>>17;Value^=Value<<5;
    return static_cast<float>(Value&0x00ffffffu)/static_cast<float>(0x01000000u);
}
float VirtualLidarPointModel::FConfig::DeterministicGaussian(int32 RayIndex,int32 ReturnIndex) const
{
    const float U1=FMath::Max(1.e-6f,DeterministicUnitRandom(RayIndex,ReturnIndex,0x4d4c5831u));
    const float U2=DeterministicUnitRandom(RayIndex,ReturnIndex,0x4d4c5832u);
    return FMath::Sqrt(-2.f*FMath::Loge(U1))*FMath::Cos(2.f*PI*U2);
}
bool VirtualLidarPointModel::FConfig::Apply(FVirtualLidarPoint& Point,const FSurface& Surface,const FVector& SafeDirection,
    const FTransform& AcquisitionTransform,float ModelRange,float ModelIncidence,int32 RayIndex,int32 ReturnIndex) const
{
    const float IncidenceCosine = Surface.bTwoSided ? FMath::Abs(ModelIncidence) : FMath::Max(0.0f, ModelIncidence);
    const float RangeRatio = FMath::Clamp(ModelRange / FMath::Max(1.0f, MaxDistance), 0.0f, 1.0f);
    const float ReflectivityRatio = Surface.Reflectivity / 0.2f;
    const float RangeFactor = 1.0f - 0.05f * FMath::Square(RangeRatio);
    const float AmbientFactor = 1.0f - 0.05f * FMath::Clamp(AmbientLightLux / 100000.0f, 0.0f, 4.0f);
    float DetectionProbability = FMath::Clamp(
        RangeFactor
        * AmbientFactor
        * FMath::Sqrt(FMath::Max(0.01f, ReflectivityRatio))
        * FMath::Pow(FMath::Max(0.01f, IncidenceCosine), 0.30f)
        * Surface.DetectionProbabilityScale,
        0.0f,
        0.9995f);

    const float Attenuation = 1.0f / (1.0f + FMath::Square(ModelRange * 0.01f / 45.0f));
    float Intensity = FMath::Max(0.0f,
        Surface.Reflectivity
        * FMath::Sqrt(FMath::Max(0.0f, IncidenceCosine))
        * Attenuation
        * FMath::Max(0.0f, AmbientFactor)
        * Surface.IntensityGain
        * 5.0f);
    VirtualLidarFidelity::FSurface Optical;
    Optical.Reflectivity=Surface.Reflectivity; Optical.IntensityGain=Surface.IntensityGain;
    Optical.DetectionProbabilityScale=Surface.DetectionProbabilityScale;
    if (bEnableMaterialResponse)
    {
        Intensity=VirtualLidarFidelity::Intensity(Optical,ModelRange,IncidenceCosine,AmbientLightLux);
        DetectionProbability=VirtualLidarFidelity::DetectionProbability(Intensity,Optical);
    }
    const bool bSaturated = Surface.bAllowSaturation && Intensity >= 1.0f;

    Point.SurfaceReflectivity = Surface.Reflectivity;
    Point.IncidenceCosine = IncidenceCosine;
    Point.AmbientLux = AmbientLightLux;
    Point.NormalizedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    Point.RawIntensity = FMath::Clamp(FMath::RoundToInt(Point.NormalizedIntensity * 65535.0f), 0, 65535);

    if ((bEnableMaterialResponse || FidelityMode != EVirtualSensorFidelityMode::IdealTruth)
        && DeterministicUnitRandom(RayIndex, ReturnIndex, 0x4d4c5833u) > DetectionProbability)
    {
        Point.bHit = false;
        Point.Validity = EVirtualLidarPointValidity::BelowSignalThreshold;
        Point.RangeMillimeters = 0;
        Point.SensorLocalPositionMeters = FVector::ZeroVector;
        Point.Confidence = DetectionProbability;
        Point.RawIntensity = 0;
        Point.NormalizedIntensity = 0.0f;
        return false;
    }

    float ErrorMillimeters = 0.0f;
    if ((bEnableMaterialResponse || FidelityMode != EVirtualSensorFidelityMode::IdealTruth) && bEnableRangeNoise)
    {
        float DistanceDependentStdDev = RangeNoiseStandardDeviationMillimeters
            * (0.5f + 0.5f * RangeRatio)
            * FMath::Clamp(FMath::Sqrt(0.2f / Surface.Reflectivity), 0.5f, 2.0f);
        if (bEnableMaterialResponse) DistanceDependentStdDev = RangeNoiseStandardDeviationMillimeters
            * VirtualLidarFidelity::NoiseScale(RangeRatio,Surface.Reflectivity,IncidenceCosine);
        ErrorMillimeters = FMath::Clamp(
            DeterministicGaussian(RayIndex, ReturnIndex) * DistanceDependentStdDev,
            -MaximumRangeErrorMillimeters,
            MaximumRangeErrorMillimeters);
    }

    const float MeasuredDistanceCm = FMath::Clamp(ModelRange + ErrorMillimeters * 0.1f, 0.0f, MaxDistance);
    const FVector MeasuredWorldLocation = AcquisitionTransform.GetLocation() + SafeDirection * MeasuredDistanceCm;
    const FVector SensorLocalCentimeters = AcquisitionTransform.InverseTransformPosition(MeasuredWorldLocation);

    Point.bHit = true;
    Point.Distance = MeasuredDistanceCm;
    Point.WorldLocation = MeasuredWorldLocation;
    Point.RangeMillimeters = FMath::Max(0, FMath::RoundToInt(MeasuredDistanceCm * 10.0f));
    Point.SensorLocalPositionMeters = FVector(
        SensorLocalCentimeters.X * 0.01,
        -SensorLocalCentimeters.Y * 0.01,
        SensorLocalCentimeters.Z * 0.01);
    Point.Validity = bSaturated ? EVirtualLidarPointValidity::Saturated : EVirtualLidarPointValidity::Valid;
    Point.Confidence = DetectionProbability;
    Point.ReturnIndex = ReturnIndex;
    Point.EchoIndex = ReturnIndex;
    return true;
}
