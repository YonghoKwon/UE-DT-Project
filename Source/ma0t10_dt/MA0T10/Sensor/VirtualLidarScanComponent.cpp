#include "VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"

#include "Async/Async.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Json.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarPayloadCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include <atomic>

namespace
{
std::atomic<int32> GVirtualLidarJsonJobs{0};
// Four supported FullSpec LiDARs may complete in the same scheduling window.
// One immutable latest-frame JSON job per sensor avoids queue rejection while
// preserving the per-sensor in-flight bound.
constexpr int32 GVirtualLidarJsonJobLimit = 4;
template <typename T> void WriteLasValue(FBufferArchive& A, const T& V) { A.Serialize(const_cast<T*>(&V), sizeof(T)); }
void WriteLasFixedString(FBufferArchive& A, const ANSICHAR* Text, int32 Len) { TArray<ANSICHAR> B; B.SetNumZeroed(Len); if (Text) { FCStringAnsi::Strncpy(B.GetData(), Text, Len); } A.Serialize(B.GetData(), Len); }
void WriteLasBytes(FBufferArchive& A, const uint8* Bytes, int32 Len) { A.Serialize(const_cast<uint8*>(Bytes), Len); }
int64 UtcNowUnixNanoseconds()
{
    static const FDateTime UnixEpoch(1970, 1, 1);
    return (FDateTime::UtcNow() - UnixEpoch).GetTicks() * 100;
}
FString JoinNames(const TArray<FName>& Names) { FString R; for (int32 I = 0; I < Names.Num(); ++I) { if (I > 0) R += TEXT("|"); R += Names[I].ToString(); } return R; }
float NormalizeSignedAngleDegrees(float Angle) { while (Angle > 180.0f) Angle -= 360.0f; while (Angle < -180.0f) Angle += 360.0f; return Angle; }
float NormalizeAxisAngleDegrees(float Angle) { Angle = NormalizeSignedAngleDegrees(Angle); if (Angle > 90.0f) Angle -= 180.0f; if (Angle < -90.0f) Angle += 180.0f; return Angle; }
void AddVectorArray(TSharedRef<FJsonObject> Object, const TCHAR* FieldName, const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    Array.Add(MakeShared<FJsonValueNumber>(Value.X));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
    Object->SetArrayField(FieldName, Array);
}

struct FLidarScheduledPayloadSnapshot
{
    TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> Points;
    FString SensorId;
    FString Manufacturer;
    FString Model;
    FString PreviewBackend;
    int64 FrameId = 0;
    int32 HorizontalSamples = 0;
    int32 VerticalChannels = 0;
    float MaxDistance = 0.0f;
    int32 ServerPayloadStride = 1;
    int32 MaxServerPayloadPoints = 0;
    bool bIncludeMissPoints = false;
    int32 PreviewPointStride = 1;
    int32 MaxPreviewPoints = 0;
    bool bPreviewHitOnly = true;
    bool bSemanticClassification = true;
    bool bGpuRequested = false;
    bool bGpuActive = false;
    bool bExperimentalGpuOptIn = false;
    bool bIncludeSlabAnalysis = true;
    FTransform SensorTransform = FTransform::Identity;
    FVirtualLidarSlabAnalysisResult SlabAnalysis;
};

FString BuildScheduledLidarJson(const FLidarScheduledPayloadSnapshot& S)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    static const TArray<FVirtualLidarPoint> EmptyPoints;
    const TArray<FVirtualLidarPoint>& Points = S.Points.IsValid() ? *S.Points : EmptyPoints;
    int32 HitPointCount = 0;
    for (const FVirtualLidarPoint& Point : Points) if (Point.bHit) ++HitPointCount;

    Root->SetStringField(TEXT("schemaVersion"), TEXT("virtual-lidar.v1"));
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_lidar"));
    Root->SetStringField(TEXT("sensorId"), S.SensorId);
    Root->SetStringField(TEXT("manufacturer"), S.Manufacturer);
    Root->SetStringField(TEXT("model"), S.Model);
    Root->SetNumberField(TEXT("frameId"), static_cast<double>(S.FrameId));
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("horizontalSamples"), S.HorizontalSamples);
    Root->SetNumberField(TEXT("verticalChannels"), S.VerticalChannels);
    Root->SetNumberField(TEXT("rayCount"), S.HorizontalSamples * S.VerticalChannels);
    Root->SetNumberField(TEXT("totalPointCount"), Points.Num());
    Root->SetNumberField(TEXT("hitPointCount"), HitPointCount);
    Root->SetNumberField(TEXT("maxDistance"), S.MaxDistance);
    Root->SetBoolField(TEXT("semanticClassification"), S.bSemanticClassification);
    Root->SetNumberField(TEXT("serverPayloadStride"), S.ServerPayloadStride);
    Root->SetNumberField(TEXT("maxServerPayloadPoints"), S.MaxServerPayloadPoints);
    Root->SetBoolField(TEXT("includeMissPointsInServerPayload"), S.bIncludeMissPoints);
    Root->SetNumberField(TEXT("previewPointStride"), S.PreviewPointStride);
    Root->SetNumberField(TEXT("maxPreviewPoints"), S.MaxPreviewPoints);
    Root->SetStringField(TEXT("previewBackend"), S.PreviewBackend);
    Root->SetBoolField(TEXT("gpuPreviewBackendRequested"), S.bGpuRequested);
    Root->SetBoolField(TEXT("gpuPreviewBackendActive"), S.bGpuActive);

    TSharedRef<FJsonObject> PayloadPolicy = MakeShared<FJsonObject>();
    PayloadPolicy->SetNumberField(TEXT("stride"), S.ServerPayloadStride);
    PayloadPolicy->SetNumberField(TEXT("maxPoints"), S.MaxServerPayloadPoints);
    PayloadPolicy->SetBoolField(TEXT("includeMissPoints"), S.bIncludeMissPoints);
    PayloadPolicy->SetStringField(TEXT("pointSelection"), S.bIncludeMissPoints ? TEXT("hit_and_miss") : TEXT("hit_only"));
    Root->SetObjectField(TEXT("payloadPolicy"), PayloadPolicy);

    TSharedRef<FJsonObject> PreviewPolicy = MakeShared<FJsonObject>();
    PreviewPolicy->SetNumberField(TEXT("stride"), S.PreviewPointStride);
    PreviewPolicy->SetNumberField(TEXT("maxPoints"), S.MaxPreviewPoints);
    PreviewPolicy->SetBoolField(TEXT("hitOnly"), S.bPreviewHitOnly);
    PreviewPolicy->SetStringField(TEXT("backend"), S.PreviewBackend);
    PreviewPolicy->SetBoolField(TEXT("gpuRequested"), S.bGpuRequested);
    PreviewPolicy->SetBoolField(TEXT("gpuActive"), S.bGpuActive);
    PreviewPolicy->SetBoolField(TEXT("experimentalGpuOptIn"), S.bExperimentalGpuOptIn);
    PreviewPolicy->SetStringField(TEXT("activePath"), S.bGpuActive ? TEXT("gpu") : TEXT("cpu_instanced_mesh_fallback"));
    Root->SetObjectField(TEXT("previewPolicy"), PreviewPolicy);

    TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    AddVectorArray(TransformObject, TEXT("location"), S.SensorTransform.GetLocation());
    const FRotator Rotation = S.SensorTransform.Rotator();
    TArray<TSharedPtr<FJsonValue>> RotationJson;
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    TransformObject->SetArrayField(TEXT("rotation"), RotationJson);
    AddVectorArray(TransformObject, TEXT("forward"), S.SensorTransform.GetUnitAxis(EAxis::X));
    AddVectorArray(TransformObject, TEXT("up"), S.SensorTransform.GetUnitAxis(EAxis::Z));
    Root->SetObjectField(TEXT("sensorTransform"), TransformObject);

    if (S.bIncludeSlabAnalysis)
    {
        TSharedRef<FJsonObject> SlabObject = MakeShared<FJsonObject>();
        SlabObject->SetBoolField(TEXT("valid"), S.SlabAnalysis.bValid);
        SlabObject->SetNumberField(TEXT("slabHitPointCount"), S.SlabAnalysis.SlabHitPointCount);
        AddVectorArray(SlabObject, TEXT("boundsMin"), S.SlabAnalysis.BoundsMin);
        AddVectorArray(SlabObject, TEXT("boundsMax"), S.SlabAnalysis.BoundsMax);
        AddVectorArray(SlabObject, TEXT("center"), S.SlabAnalysis.Center);
        SlabObject->SetNumberField(TEXT("estimatedYawDegrees"), S.SlabAnalysis.EstimatedYawDegrees);
        SlabObject->SetNumberField(TEXT("referenceYawDegrees"), S.SlabAnalysis.ReferenceYawDegrees);
        SlabObject->SetNumberField(TEXT("angleDeviationDegrees"), S.SlabAnalysis.AngleDeviationDegrees);
        SlabObject->SetNumberField(TEXT("confidence"), S.SlabAnalysis.Confidence);
        SlabObject->SetStringField(TEXT("status"), S.SlabAnalysis.StatusMessage);
        Root->SetObjectField(TEXT("slabAnalysis"), SlabObject);
    }

    TArray<TSharedPtr<FJsonValue>> JsonPoints;
    const int32 SafeStride = FMath::Max(1, S.ServerPayloadStride);
    const int32 SafeMax = FMath::Max(0, S.MaxServerPayloadPoints);
    int32 Added = 0;
    for (int32 I = 0; I < Points.Num(); I += SafeStride)
    {
        if (SafeMax > 0 && Added >= SafeMax) break;
        const FVirtualLidarPoint& P = Points[I];
        if (!S.bIncludeMissPoints && !P.bHit) continue;
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("pointIndex"), I);
        O->SetNumberField(TEXT("row"), P.bHasGridCoord ? P.Row : (S.HorizontalSamples > 0 ? I / S.HorizontalSamples : 0));
        O->SetNumberField(TEXT("col"), P.bHasGridCoord ? P.Col : (S.HorizontalSamples > 0 ? I % S.HorizontalSamples : I));
        O->SetNumberField(TEXT("returnIndex"), P.ReturnIndex);
        O->SetBoolField(TEXT("gridCoordValid"), P.bHasGridCoord);
        O->SetStringField(TEXT("gridCoordSource"), P.bHasGridCoord ? TEXT("point_metadata") : TEXT("derived_from_point_index"));
        O->SetBoolField(TEXT("hit"), P.bHit);
        O->SetNumberField(TEXT("distance"), P.Distance);
        O->SetStringField(TEXT("hitActor"), P.HitActorName.ToString());
        O->SetStringField(TEXT("hitActorClass"), P.HitActorClassName.ToString());
        O->SetStringField(TEXT("semanticLabel"), P.SemanticLabel.ToString());
        TArray<TSharedPtr<FJsonValue>> Tags;
        for (const FName& Tag : P.HitActorTags) Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        O->SetArrayField(TEXT("hitActorTags"), Tags);
        AddVectorArray(O, TEXT("worldLocation"), P.WorldLocation);
        AddVectorArray(O, TEXT("localDirection"), P.LocalDirection);
        JsonPoints.Add(MakeShared<FJsonValueObject>(O));
        ++Added;
    }
    Root->SetNumberField(TEXT("payloadPointCount"), Added);
    Root->SetArrayField(TEXT("points"), JsonPoints);
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}
}

UVirtualLidarScanComponent::UVirtualLidarScanComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ResetDefaultSemanticClassRules();
}

FString UVirtualLidarScanComponent::BuildLastPhysicalJsonPayload(
    int32 PointStride,
    int32 MaxPoints,
    bool bIncludeInvalidPoints,
    bool bIncludeDigitalTwinExtensions) const
{
    if (!LastFrameSnapshot.IsValid())
    {
        return FString();
    }
    FVirtualLidarPayloadDescriptor Descriptor;
    Descriptor.SensorId = SensorId;
    Descriptor.Manufacturer = DeviceSpec.Manufacturer;
    Descriptor.Model = DeviceSpec.Model;
    Descriptor.HorizontalFovDegrees = HorizontalFov;
    Descriptor.VerticalFovDegrees = MaxVerticalAngle - MinVerticalAngle;
    Descriptor.MaxRangeMeters = MaxDistance * 0.01f;

    FVirtualLidarV2EncodeOptions Options;
    Options.PointStride = FMath::Max(1, PointStride);
    Options.MaxPoints = FMath::Max(0, MaxPoints);
    Options.bIncludeInvalidPoints = bIncludeInvalidPoints;
    Options.bIncludeDigitalTwinExtensions = bIncludeDigitalTwinExtensions;
    return FVirtualLidarPayloadCodec::EncodeV2Json(*LastFrameSnapshot, Descriptor, Options);
}

bool UVirtualLidarScanComponent::BuildLastPhysicalCompactBinary(
    const FVirtualLidarV2EncodeOptions& Options,
    TArray64<uint8>& OutBytes,
    int32& OutEncodedPointCount) const
{
    return LastFrameSnapshot.IsValid()
        && FVirtualLidarPayloadCodec::EncodeCompactBinary(*LastFrameSnapshot, Options, OutBytes, OutEncodedPointCount);
}

void UVirtualLidarScanComponent::InvalidateSurfaceResponseCache()
{
    SurfaceResponseCache.Reset();
}

void UVirtualLidarScanComponent::BuildBeamAngleTables(
    int32 InHorizontalSamples,
    int32 InVerticalChannels,
    TArray<float>& OutHorizontalAngles,
    TArray<float>& OutVerticalAngles) const
{
    const int32 SafeHorizontalSamples = FMath::Max(1, InHorizontalSamples);
    const int32 SafeVerticalChannels = FMath::Max(1, InVerticalChannels);
    OutHorizontalAngles.SetNumUninitialized(SafeHorizontalSamples);
    OutVerticalAngles.SetNumUninitialized(SafeVerticalChannels);

    const bool bUseHorizontalCalibration = HorizontalCalibrationAnglesDegrees.Num() == SafeHorizontalSamples;
    const bool bUseVerticalCalibration = VerticalCalibrationAnglesDegrees.Num() == SafeVerticalChannels;
    for (int32 Column = 0; Column < SafeHorizontalSamples; ++Column)
    {
        OutHorizontalAngles[Column] = bUseHorizontalCalibration
            ? HorizontalCalibrationAnglesDegrees[Column]
            : FMath::Lerp(
                -HorizontalFov * 0.5f,
                HorizontalFov * 0.5f,
                SafeHorizontalSamples == 1 ? 0.5f : static_cast<float>(Column) / static_cast<float>(SafeHorizontalSamples - 1));
    }
    for (int32 Ring = 0; Ring < SafeVerticalChannels; ++Ring)
    {
        OutVerticalAngles[Ring] = bUseVerticalCalibration
            ? VerticalCalibrationAnglesDegrees[Ring]
            : FMath::Lerp(
                MinVerticalAngle,
                MaxVerticalAngle,
                SafeVerticalChannels == 1 ? 0.5f : static_cast<float>(Ring) / static_cast<float>(SafeVerticalChannels - 1));
    }
}

float UVirtualLidarScanComponent::DeterministicUnitRandom(int32 RayIndex, int32 ReturnIndex, uint32 Salt) const
{
    uint32 Value = HashCombineFast(
        HashCombineFast(GetTypeHash(DeterministicNoiseSeed), GetTypeHash(FrameId + 1)),
        HashCombineFast(GetTypeHash(RayIndex), HashCombineFast(GetTypeHash(ReturnIndex), Salt)));
    Value ^= Value << 13;
    Value ^= Value >> 17;
    Value ^= Value << 5;
    return static_cast<float>(Value & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float UVirtualLidarScanComponent::DeterministicGaussian(int32 RayIndex, int32 ReturnIndex) const
{
    const float U1 = FMath::Max(1.0e-6f, DeterministicUnitRandom(RayIndex, ReturnIndex, 0x4d4c5831u));
    const float U2 = DeterministicUnitRandom(RayIndex, ReturnIndex, 0x4d4c5832u);
    return FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(2.0f * PI * U2);
}

int64 UVirtualLidarScanComponent::CalculatePointTimeOffsetNanoseconds(int32 RayIndex, int32 RayCount) const
{
    if (!bUseRollingPointTimestamps || RayCount <= 1)
    {
        return 0;
    }
    const double NormalizedIndex = static_cast<double>(FMath::Clamp(RayIndex, 0, RayCount - 1))
        / static_cast<double>(RayCount - 1);
    return static_cast<int64>(NormalizedIndex * static_cast<double>(ScanInterval) * 1000000000.0);
}

void UVirtualLidarScanComponent::InitializePhysicalPoint(
    FVirtualLidarPoint& Point,
    int32 Row,
    int32 Col,
    int32 RayIndex,
    int32 RayCount,
    const FVector& LocalDirection) const
{
    Point.LocalDirection = LocalDirection;
    Point.Row = Row;
    Point.Col = Col;
    Point.Ring = Row;
    Point.HorizontalIndex = Col;
    Point.ReturnIndex = 0;
    Point.EchoIndex = 0;
    Point.EchoCount = 0;
    Point.EchoType = EVirtualLidarEchoType::None;
    Point.PointTimeOffsetNanoseconds = CalculatePointTimeOffsetNanoseconds(RayIndex, RayCount);
    Point.Validity = EVirtualLidarPointValidity::NoReturn;
    Point.RangeMillimeters = 0;
    Point.RawIntensity = 0;
    Point.NormalizedIntensity = 0.0f;
    Point.Confidence = 0.0f;
    Point.SurfaceReflectivity = 0.0f;
    Point.IncidenceCosine = 0.0f;
    Point.AmbientLux = AmbientLightLux;
    Point.bHasGridCoord = true;
}

UVirtualLidarScanComponent::FSurfaceResponseCacheEntry UVirtualLidarScanComponent::ResolveSurfaceResponse(
    const UPrimitiveComponent* Primitive) const
{
    FSurfaceResponseCacheEntry Default;
    Default.Reflectivity = FMath::Clamp(DefaultSurfaceReflectivity, 0.001f, 1.0f);
    if (!Primitive)
    {
        return Default;
    }

    TWeakObjectPtr<UPrimitiveComponent> Key(const_cast<UPrimitiveComponent*>(Primitive));
    if (const FSurfaceResponseCacheEntry* Existing = SurfaceResponseCache.Find(Key))
    {
        return *Existing;
    }

    FSurfaceResponseCacheEntry Resolved = Default;
    if (const AActor* OwnerActor = Primitive->GetOwner())
    {
        if (const UVirtualLidarSurfaceResponseComponent* Response = OwnerActor->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>())
        {
            Resolved.Reflectivity = FMath::Clamp(Response->Reflectivity940Nm, 0.001f, 1.0f);
            Resolved.IntensityGain = FMath::Max(0.0f, Response->IntensityGain);
            Resolved.DetectionProbabilityScale = FMath::Max(0.0f, Response->DetectionProbabilityScale);
            Resolved.bTwoSided = Response->bTwoSided;
            Resolved.bAllowSaturation = Response->bAllowSaturation;
        }
    }
    SurfaceResponseCache.Add(Key, Resolved);
    return Resolved;
}

bool UVirtualLidarScanComponent::ApplyPhysicalHitModel(
    FVirtualLidarPoint& Point,
    const FHitResult& Hit,
    const FVector& WorldDirection,
    const FTransform& AcquisitionTransform,
    int32 RayIndex,
    int32 ReturnIndex) const
{
    const FSurfaceResponseCacheEntry Surface = ResolveSurfaceResponse(Hit.GetComponent());
    const FVector SafeDirection = WorldDirection.GetSafeNormal();
    const float SignedIncidence = FVector::DotProduct(-SafeDirection, Hit.ImpactNormal.GetSafeNormal());
    const float IncidenceCosine = Surface.bTwoSided ? FMath::Abs(SignedIncidence) : FMath::Max(0.0f, SignedIncidence);
    const float RangeRatio = FMath::Clamp(Hit.Distance / FMath::Max(1.0f, MaxDistance), 0.0f, 1.0f);
    const float ReflectivityRatio = Surface.Reflectivity / 0.2f;
    const float RangeFactor = 1.0f - 0.05f * FMath::Square(RangeRatio);
    const float AmbientFactor = 1.0f - 0.05f * FMath::Clamp(AmbientLightLux / 100000.0f, 0.0f, 4.0f);
    const float DetectionProbability = FMath::Clamp(
        RangeFactor
        * AmbientFactor
        * FMath::Sqrt(FMath::Max(0.01f, ReflectivityRatio))
        * FMath::Pow(FMath::Max(0.01f, IncidenceCosine), 0.30f)
        * Surface.DetectionProbabilityScale,
        0.0f,
        0.9995f);

    const float Attenuation = 1.0f / (1.0f + FMath::Square(Hit.Distance * 0.01f / 45.0f));
    const float Intensity = FMath::Max(0.0f,
        Surface.Reflectivity
        * FMath::Sqrt(FMath::Max(0.0f, IncidenceCosine))
        * Attenuation
        * FMath::Max(0.0f, AmbientFactor)
        * Surface.IntensityGain
        * 5.0f);
    const bool bSaturated = Surface.bAllowSaturation && Intensity >= 1.0f;

    Point.SurfaceReflectivity = Surface.Reflectivity;
    Point.IncidenceCosine = IncidenceCosine;
    Point.AmbientLux = AmbientLightLux;
    Point.NormalizedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    Point.RawIntensity = FMath::Clamp(FMath::RoundToInt(Point.NormalizedIntensity * 65535.0f), 0, 65535);

    if (FidelityMode != EVirtualSensorFidelityMode::IdealTruth
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
    if (FidelityMode != EVirtualSensorFidelityMode::IdealTruth && bEnableRangeNoise)
    {
        const float DistanceDependentStdDev = RangeNoiseStandardDeviationMillimeters
            * (0.5f + 0.5f * RangeRatio)
            * FMath::Clamp(FMath::Sqrt(0.2f / Surface.Reflectivity), 0.5f, 2.0f);
        ErrorMillimeters = FMath::Clamp(
            DeterministicGaussian(RayIndex, ReturnIndex) * DistanceDependentStdDev,
            -MaximumRangeErrorMillimeters,
            MaximumRangeErrorMillimeters);
    }

    const float MeasuredDistanceCm = FMath::Clamp(Hit.Distance + ErrorMillimeters * 0.1f, 0.0f, MaxDistance);
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

void UVirtualLidarScanComponent::FinalizeEchoMetadata(TArray<FVirtualLidarPoint>& Points, int32 StartIndex, int32 EchoCount) const
{
    for (int32 Echo = 0; Echo < EchoCount; ++Echo)
    {
        FVirtualLidarPoint& Point = Points[StartIndex + Echo];
        Point.EchoCount = EchoCount;
        Point.EchoIndex = Echo;
        Point.ReturnIndex = Echo;
        if (EchoCount <= 1)
        {
            Point.EchoType = EVirtualLidarEchoType::Single;
        }
        else if (Echo == 0)
        {
            Point.EchoType = EVirtualLidarEchoType::First;
        }
        else if (Echo == EchoCount - 1)
        {
            Point.EchoType = EVirtualLidarEchoType::Last;
        }
        else
        {
            Point.EchoType = EVirtualLidarEchoType::Strongest;
        }
    }
}

void UVirtualLidarScanComponent::RebuildPhysicalFrameStatistics(FVirtualLidarFrameSnapshot& Snapshot) const
{
    if (!Snapshot.Points.IsValid())
    {
        return;
    }
	double RangeSumMeters = 0.0;
	double IntensitySum = 0.0;
	Snapshot.MinRangeMeters = TNumericLimits<float>::Max();
	Snapshot.MinIntensity = TNumericLimits<float>::Max();
    for (const FVirtualLidarPoint& Point : *Snapshot.Points)
    {
        if (Point.bHit)
        {
            ++Snapshot.ValidPointCount;
            if (Point.EchoIndex == 0) ++Snapshot.FirstEchoCount;
            else if (Point.EchoIndex == 1) ++Snapshot.SecondEchoCount;
			const float RangeMeters = Point.RangeMillimeters > 0
				? Point.RangeMillimeters / 1000.0f
				: Point.Distance / 100.0f;
			Snapshot.MinRangeMeters = FMath::Min(Snapshot.MinRangeMeters, RangeMeters);
			Snapshot.MaxRangeMeters = FMath::Max(Snapshot.MaxRangeMeters, RangeMeters);
			Snapshot.MinIntensity = FMath::Min(Snapshot.MinIntensity, Point.NormalizedIntensity);
			Snapshot.MaxIntensity = FMath::Max(Snapshot.MaxIntensity, Point.NormalizedIntensity);
			RangeSumMeters += RangeMeters;
			IntensitySum += Point.NormalizedIntensity;
        }
        else
        {
            ++Snapshot.InvalidPointCount;
        }
    }
	if (Snapshot.ValidPointCount > 0)
	{
		Snapshot.MeanRangeMeters = static_cast<float>(RangeSumMeters / Snapshot.ValidPointCount);
		Snapshot.MeanIntensity = static_cast<float>(IntensitySum / Snapshot.ValidPointCount);
	}
	else
	{
		Snapshot.MinRangeMeters = 0.0f;
		Snapshot.MinIntensity = 0.0f;
	}
}

void UVirtualLidarScanComponent::BeginPlay()
{
    Super::BeginPlay();
    if (SemanticClassRules.Num() <= 0) { ResetDefaultSemanticClassRules(); }
    if (bApplyDeviceProfileOnBeginPlay) { ApplyDeviceProfile(DeviceProfile); }
    if (Preset != EVirtualLidarPreset::Custom) { ApplyPreset(Preset); }
    TryAutoRegisterToManager();
    if (bPointCloudPreviewEnabled) { EnsurePointCloudPreviewComponent(); ApplyPointCloudPreviewStyle(); }
    if (bAutoStartScan) { StartScan(); }
}

void UVirtualLidarScanComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopScan();
    ++ScheduledGeneration;
    ScheduledPoints.Reset();
    ScheduledHitPointCount = 0;
    ScheduledSemanticCounts.Reset();
    ScheduledHeatmapPixels.Reset();
    ClearPointCloudPreview();
    if (PointCloudPreviewComponent) { PointCloudPreviewComponent->DestroyComponent(); PointCloudPreviewComponent = nullptr; }
    Super::EndPlay(EndPlayReason);
}

void UVirtualLidarScanComponent::StartScan()
{
    if (!GetWorld() || ScanInterval <= 0.0f) return;
    GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    NextScheduledScanTime = GetWorld()->GetTimeSeconds() + (GetTypeHash(SensorId) % 1000) / 1000.0 * FMath::Max(0.001f, ScanInterval);
    bDeadlineMissRecordedForActiveAcquisition = false;
    RegisterWithPerformanceSubsystem();
}
void UVirtualLidarScanComponent::StopScan()
{
	if (GetWorld()) if (auto* Slab=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()) Slab->CompleteAcquisition(SensorId,FrameId+1,false);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    UnregisterFromPerformanceSubsystem();
    NextScheduledScanTime = -1.0;
    bScheduledScanInProgress = false;
    bGpuDepthScanInProgress = false;
    bDeadlineMissRecordedForActiveAcquisition = false;
    if (GpuDepthProjectionComponent.IsValid()) GpuDepthProjectionComponent->CancelAcquisition();
    bScheduledPayloadBuildInFlight = false;
    bScheduledAutoExportInFlight = false;
    bScheduledPayloadRefreshPending = false;
    RuntimeStatus.bAcquisitionInFlight = false;
    ScheduledPoints.Reset();
    ScheduledHeatmapPixels.Reset();
    ++ScheduledGeneration;
}

void UVirtualLidarScanComponent::RequestImmediateScheduledScan()
{
	if (NextScheduledScanTime >= 0.0 && GetWorld())
	{
		NextScheduledScanTime = GetWorld()->GetTimeSeconds();
	}
}

void UVirtualLidarScanComponent::SetInteractivePreviewMode(bool bEnabled, bool bSuppressDerivedOutput)
{
	bInteractivePreviewMode = bEnabled;
	bSuppressInteractiveDerivedOutput = bSuppressDerivedOutput;
}

void UVirtualLidarScanComponent::BindGpuDepthProjectionComponent(
    UVirtualLidarGpuDepthProjectionComponent* InComponent)
{
    GpuDepthProjectionComponent = InComponent;
}

EVirtualLidarAcquisitionBackend UVirtualLidarScanComponent::ResolveAcquisitionBackend(FString& OutReason) const
{
    OutReason.Reset();
    const bool bGpuAvailable = GpuDepthProjectionComponent.IsValid()
        && GpuDepthProjectionComponent->IsAvailable();

    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::AccurateCpuTrace)
    {
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }
    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::GpuDepthProjection)
    {
        if (bGpuAvailable) return EVirtualLidarAcquisitionBackend::GpuDepthProjection;
        OutReason = TEXT("GPU depth backend unavailable; accurate CPU trace fallback is active");
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }
    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::HardwareRayTracing)
    {
        OutReason = TEXT("hardware ray tracing backend is not implemented; accurate CPU trace fallback is active");
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }
    if (AcquisitionBackend == EVirtualLidarAcquisitionBackend::ReplayOrExternal)
    {
        OutReason = TEXT("replay/external backend has no automatic acquisition; CPU fallback is active");
        return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    }

    const bool bDenseSolidStateProfile =
        DeviceProfile == EVirtualLidarDeviceProfile::IYOBOT_MLX80
        || DeviceProfile == EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;
    if (bDenseSolidStateProfile && bGpuAvailable && !bInteractivePreviewMode)
    {
        return EVirtualLidarAcquisitionBackend::GpuDepthProjection;
    }
    if (bDenseSolidStateProfile && !bGpuAvailable)
    {
        OutReason = TEXT("Auto requested GPU depth for ML-X but this RHI/world cannot render; accurate CPU trace fallback is active");
    }
    return EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
}

bool UVirtualLidarScanComponent::BeginGpuDepthScan(double NowSeconds)
{
    if (!GpuDepthProjectionComponent.IsValid()) return false;
    FVirtualLidarDepthAcquisitionRequest Request;
	if (auto* Slab = GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()) Request.SlabContext=Slab->CaptureContext(SensorId,FrameId+1);
	ScheduledSlabContext=Request.SlabContext;
    Request.AcquisitionTransform = GetComponentTransform();
    Request.FrameId = FrameId + 1;
    Request.HorizontalSamples = FMath::Max(1, HorizontalSamples);
    Request.VerticalChannels = FMath::Max(1, VerticalChannels);
    Request.HorizontalFovDegrees = HorizontalFov;
    Request.MinVerticalAngleDegrees = MinVerticalAngle;
    Request.MaxVerticalAngleDegrees = MaxVerticalAngle;
    Request.MaxDistanceCm = MaxDistance;
    Request.AcquisitionStartUnixNanoseconds = UtcNowUnixNanoseconds();

    if (!GpuDepthProjectionComponent->BeginAcquisition(Request))
    {
        AcquisitionBackendFallbackReason = GpuDepthProjectionComponent->GetBackendStatusMessage();
        return false;
    }

    ScheduledScanStartTime = FPlatformTime::Seconds();
    ScheduledAcquisitionStartUnixNanoseconds = Request.AcquisitionStartUnixNanoseconds;
    bGpuDepthScanInProgress = true;
    RuntimeStatus.bAcquisitionInFlight = true;
    RuntimeStatus.ActiveAcquisitionBackend = TEXT("gpu_depth_projection");
    RuntimeStatus.AcquisitionBackendMessage = TEXT("GPU first-surface depth active; multi-echo and semantic extensions require CPU/HWRT");
    return true;
}

int32 UVirtualLidarScanComponent::ProcessGpuDepthScan()
{
    if (!bGpuDepthScanInProgress || !GpuDepthProjectionComponent.IsValid()) return 0;
    FVirtualLidarDepthAcquisitionFrame Frame;
    const EVirtualSensorBackendPollResult Result = GpuDepthProjectionComponent->PollAcquisition(Frame);
    if (Result == EVirtualSensorBackendPollResult::Pending)
    {
        return 0;
    }
    if (Result == EVirtualSensorBackendPollResult::Failed)
    {
		if (GetWorld()) if (auto* Slab=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()) Slab->CompleteAcquisition(SensorId,FrameId+1,false);
        ++RuntimeStatus.FailedAcquisitionFrameCount;
        bGpuDepthScanInProgress = false;
        RuntimeStatus.bAcquisitionInFlight = false;
        AcquisitionBackendFallbackReason = GpuDepthProjectionComponent->GetBackendStatusMessage();
        ActiveAcquisitionBackend = EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
        RuntimeStatus.ActiveAcquisitionBackend = TEXT("accurate_cpu_trace");
        RuntimeStatus.AcquisitionBackendMessage = AcquisitionBackendFallbackReason;
        if (GetWorld()) NextScheduledScanTime = GetWorld()->GetTimeSeconds();
        return 0;
    }
    if (Result != EVirtualSensorBackendPollResult::Completed)
    {
        return 0;
    }

    ConvertGpuDepthFrame(Frame);
    bGpuDepthScanInProgress = false;
    bScheduledScanInProgress = true;
    CompleteScheduledScan(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
    return FMath::Max(1, Frame.Request.HorizontalSamples * Frame.Request.VerticalChannels);
}

void UVirtualLidarScanComponent::ConvertGpuDepthFrame(const FVirtualLidarDepthAcquisitionFrame& Frame)
{
	ScheduledSlabContext=Frame.Request.SlabContext;
    ScheduledScanWidth = FMath::Max(1, Frame.Request.HorizontalSamples);
    ScheduledScanHeight = FMath::Max(1, Frame.Request.VerticalChannels);
    ScheduledScanTransform = Frame.Request.AcquisitionTransform;
    ScheduledAcquisitionStartUnixNanoseconds = Frame.Request.AcquisitionStartUnixNanoseconds;
    ScheduledPoints.Reset();
    ScheduledPoints.Reserve(ScheduledScanWidth * ScheduledScanHeight);
    bScheduledGenerateHeatmap = true;
    if (const UWorld* World = GetWorld())
    {
        if (const UVirtualSensorSchedulerSubsystem* Subsystem = World->GetSubsystem<UVirtualSensorSchedulerSubsystem>())
        {
            bScheduledGenerateHeatmap = Subsystem->ShouldRefreshLidarPreview(this);
        }
    }
    if (bScheduledGenerateHeatmap)
    {
        ScheduledHeatmapPixels.SetNumZeroed(ScheduledScanWidth * ScheduledScanHeight * 4);
    }
    else
    {
        ScheduledHeatmapPixels.Reset();
    }
    ScheduledHitPointCount = 0;
    ScheduledSemanticCounts.Reset();

    EnsureGpuDepthBeamLookup(Frame);
    const int32 TotalRays = ScheduledScanWidth * ScheduledScanHeight;

    for (int32 Ring = 0; Ring < ScheduledScanHeight; ++Ring)
    {
        for (int32 Column = 0; Column < ScheduledScanWidth; ++Column)
        {
            const int32 RayIndex = Ring * ScheduledScanWidth + Column;
            const int32 DepthIndex = GpuDepthPixelIndices[RayIndex];
            const float ForwardDepthCm = Frame.ForwardDepthCentimeters.IsValidIndex(DepthIndex)
                ? Frame.ForwardDepthCentimeters[DepthIndex]
                : 0.0f;
            const FVector& LocalDirection = GpuDepthLocalDirections[RayIndex];
            const FVector WorldDirection = ScheduledScanTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
            const float DirectionForward = FMath::Max(0.001f, LocalDirection.X);
            const float RadialDistanceCm = ForwardDepthCm / DirectionForward;

            FVirtualLidarPoint Point;
            InitializePhysicalPoint(Point, Ring, Column, RayIndex, TotalRays, LocalDirection);
            Point.Distance = MaxDistance;
            Point.WorldLocation = ScheduledScanTransform.GetLocation() + WorldDirection * MaxDistance;

            const bool bDepthValid = FMath::IsFinite(RadialDistanceCm)
                && ForwardDepthCm > FMath::Max(0.1f, DeviceSpec.MinRangeCm)
                && RadialDistanceCm <= MaxDistance;
            if (bDepthValid)
            {
                const float RangeRatio = FMath::Clamp(RadialDistanceCm / FMath::Max(1.0f, MaxDistance), 0.0f, 1.0f);
                const float AmbientFactor = 1.0f - 0.05f * FMath::Clamp(AmbientLightLux / 100000.0f, 0.0f, 4.0f);
                const float DetectionProbability = FMath::Clamp(
                    (1.0f - 0.05f * FMath::Square(RangeRatio)) * AmbientFactor,
                    0.0f,
                    0.9995f);
                const bool bDetected = FidelityMode == EVirtualSensorFidelityMode::IdealTruth
                    || DeterministicUnitRandom(RayIndex, 0, 0x47505531u) <= DetectionProbability;
                if (bDetected)
                {
                    float ErrorMillimeters = 0.0f;
                    if (FidelityMode != EVirtualSensorFidelityMode::IdealTruth && bEnableRangeNoise)
                    {
                        ErrorMillimeters = FMath::Clamp(
                            DeterministicGaussian(RayIndex, 0)
                            * RangeNoiseStandardDeviationMillimeters
                            * (0.5f + 0.5f * RangeRatio),
                            -MaximumRangeErrorMillimeters,
                            MaximumRangeErrorMillimeters);
                    }
                    const float MeasuredDistanceCm = FMath::Clamp(
                        RadialDistanceCm + ErrorMillimeters * 0.1f,
                        0.0f,
                        MaxDistance);
                    const FVector LocalCentimeters = LocalDirection * MeasuredDistanceCm;
                    Point.bHit = true;
                    Point.Distance = MeasuredDistanceCm;
                    Point.WorldLocation = ScheduledScanTransform.TransformPosition(LocalCentimeters);
                    Point.SensorLocalPositionMeters = FVector(
                        LocalCentimeters.X * 0.01,
                        -LocalCentimeters.Y * 0.01,
                        LocalCentimeters.Z * 0.01);
                    Point.RangeMillimeters = FMath::RoundToInt(MeasuredDistanceCm * 10.0f);
                    Point.SurfaceReflectivity = DefaultSurfaceReflectivity;
                    Point.IncidenceCosine = 1.0f;
                    Point.Confidence = DetectionProbability;
                    const float Attenuation = 1.0f / (1.0f + FMath::Square(MeasuredDistanceCm * 0.01f / 45.0f));
                    Point.NormalizedIntensity = FMath::Clamp(DefaultSurfaceReflectivity * Attenuation * 5.0f, 0.0f, 1.0f);
                    Point.RawIntensity = FMath::RoundToInt(Point.NormalizedIntensity * 65535.0f);
                    Point.Validity = Point.NormalizedIntensity >= 1.0f
                        ? EVirtualLidarPointValidity::Saturated
                        : EVirtualLidarPointValidity::Valid;
                    Point.EchoCount = 1;
                    Point.EchoType = EVirtualLidarEchoType::Single;
                    ++ScheduledHitPointCount;
                }
                else
                {
                    Point.Validity = EVirtualLidarPointValidity::BelowSignalThreshold;
                    Point.Confidence = DetectionProbability;
                }
            }
            ScheduledPoints.Add(Point);
            if (bScheduledGenerateHeatmap)
            {
                WriteHeatmapPixel(
                    ScheduledHeatmapPixels,
                    GetHeatmapPixelIndex(Column, Ring, ScheduledScanWidth, ScheduledScanHeight),
                    Point);
            }
        }
    }
}

void UVirtualLidarScanComponent::EnsureGpuDepthBeamLookup(const FVirtualLidarDepthAcquisitionFrame& Frame)
{
    const bool bLookupMatches =
        GpuDepthLookupHorizontalSamples == ScheduledScanWidth
        && GpuDepthLookupVerticalChannels == ScheduledScanHeight
        && GpuDepthLookupCaptureWidth == Frame.CaptureWidth
        && GpuDepthLookupCaptureHeight == Frame.CaptureHeight
        && FMath::IsNearlyEqual(GpuDepthLookupHorizontalFov, Frame.Request.HorizontalFovDegrees)
        && FMath::IsNearlyEqual(GpuDepthLookupMinVerticalAngle, Frame.Request.MinVerticalAngleDegrees)
        && FMath::IsNearlyEqual(GpuDepthLookupMaxVerticalAngle, Frame.Request.MaxVerticalAngleDegrees)
        && GpuDepthLookupHorizontalCalibrationCount == HorizontalCalibrationAnglesDegrees.Num()
        && GpuDepthLookupVerticalCalibrationCount == VerticalCalibrationAnglesDegrees.Num()
        && GpuDepthLocalDirections.Num() == ScheduledScanWidth * ScheduledScanHeight
        && GpuDepthPixelIndices.Num() == ScheduledScanWidth * ScheduledScanHeight;
    if (bLookupMatches)
    {
        return;
    }

    TArray<float> HorizontalAngles;
    TArray<float> VerticalAngles;
    BuildBeamAngleTables(ScheduledScanWidth, ScheduledScanHeight, HorizontalAngles, VerticalAngles);
    const float HalfHorizontalTangent = FMath::Tan(FMath::DegreesToRadians(Frame.Request.HorizontalFovDegrees * 0.5f));
    const float CaptureAspect = static_cast<float>(FMath::Max(1, Frame.CaptureWidth))
        / static_cast<float>(FMath::Max(1, Frame.CaptureHeight));
    const float HalfVerticalTangent = HalfHorizontalTangent / FMath::Max(0.001f, CaptureAspect);
    const int32 RayCount = ScheduledScanWidth * ScheduledScanHeight;
    GpuDepthLocalDirections.SetNumUninitialized(RayCount);
    GpuDepthPixelIndices.SetNumUninitialized(RayCount);

    for (int32 Ring = 0; Ring < ScheduledScanHeight; ++Ring)
    {
        const float PitchDegrees = VerticalAngles[Ring];
        const float NormalizedVertical = FMath::Tan(FMath::DegreesToRadians(PitchDegrees))
            / FMath::Max(0.001f, HalfVerticalTangent);
        const int32 PixelY = FMath::Clamp(
            FMath::RoundToInt((0.5f - 0.5f * NormalizedVertical) * static_cast<float>(FMath::Max(0, Frame.CaptureHeight - 1))),
            0,
            FMath::Max(0, Frame.CaptureHeight - 1));
        for (int32 Column = 0; Column < ScheduledScanWidth; ++Column)
        {
            const int32 RayIndex = Ring * ScheduledScanWidth + Column;
            const float YawDegrees = HorizontalAngles[Column];
            const float NormalizedHorizontal = FMath::Tan(FMath::DegreesToRadians(YawDegrees))
                / FMath::Max(0.001f, HalfHorizontalTangent);
            const int32 PixelX = FMath::Clamp(
                FMath::RoundToInt((0.5f + 0.5f * NormalizedHorizontal) * static_cast<float>(FMath::Max(0, Frame.CaptureWidth - 1))),
                0,
                FMath::Max(0, Frame.CaptureWidth - 1));
            GpuDepthLocalDirections[RayIndex] = FRotator(PitchDegrees, YawDegrees, 0.0f).Vector();
            GpuDepthPixelIndices[RayIndex] = PixelY * Frame.CaptureWidth + PixelX;
        }
    }

    GpuDepthLookupHorizontalSamples = ScheduledScanWidth;
    GpuDepthLookupVerticalChannels = ScheduledScanHeight;
    GpuDepthLookupCaptureWidth = Frame.CaptureWidth;
    GpuDepthLookupCaptureHeight = Frame.CaptureHeight;
    GpuDepthLookupHorizontalFov = Frame.Request.HorizontalFovDegrees;
    GpuDepthLookupMinVerticalAngle = Frame.Request.MinVerticalAngleDegrees;
    GpuDepthLookupMaxVerticalAngle = Frame.Request.MaxVerticalAngleDegrees;
    GpuDepthLookupHorizontalCalibrationCount = HorizontalCalibrationAnglesDegrees.Num();
    GpuDepthLookupVerticalCalibrationCount = VerticalCalibrationAnglesDegrees.Num();
}

void UVirtualLidarScanComponent::RegisterWithPerformanceSubsystem()
{
    if (!GetWorld() || bRegisteredWithPerformanceSubsystem) return;
    if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>())
    {
        Subsystem->RegisterTask(this);
        bRegisteredWithPerformanceSubsystem = true;
    }
}

void UVirtualLidarScanComponent::UnregisterFromPerformanceSubsystem()
{
    if (!GetWorld() || !bRegisteredWithPerformanceSubsystem) return;
    if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()) Subsystem->UnregisterTask(this);
    bRegisteredWithPerformanceSubsystem = false;
}

void UVirtualLidarScanComponent::PrepareScheduledScan(double NowSeconds)
{
    if (NextScheduledScanTime < 0.0) return;
    const double SafeInterval = FMath::Max(0.001, static_cast<double>(ScanInterval));
    RuntimeStatus.RequestedAcquisitionRateHz = static_cast<float>(1.0 / SafeInterval);
    RuntimeStatus.RequestedAcquisitionBackend = AcquisitionBackend == EVirtualLidarAcquisitionBackend::Auto
        ? TEXT("auto")
        : (AcquisitionBackend == EVirtualLidarAcquisitionBackend::GpuDepthProjection
            ? TEXT("gpu_depth_projection")
            : (AcquisitionBackend == EVirtualLidarAcquisitionBackend::HardwareRayTracing
                ? TEXT("hardware_ray_tracing")
                : TEXT("accurate_cpu_trace")));
    const bool bAcquisitionInProgress = bScheduledScanInProgress || bGpuDepthScanInProgress;
    if (bAcquisitionInProgress && NowSeconds >= NextScheduledScanTime)
    {
        // Do not move the deadline beyond the active acquisition here. The
        // scheduler polls completion later in the same Tick; advancing the
        // deadline first introduced an artificial idle period of one complete
        // ML-X scan interval. Record the miss once and let the post-poll
        // PrepareScheduledScan call admit the next coherent capture.
        if (!bDeadlineMissRecordedForActiveAcquisition)
        {
            ++RuntimeStatus.BudgetSkippedAcquisitionFrameCount;
            ++RuntimeStatus.DeadlineMissCount;
            bDeadlineMissRecordedForActiveAcquisition = true;
        }
    }
    if (!bAcquisitionInProgress && NowSeconds >= NextScheduledScanTime)
    {
        do { NextScheduledScanTime += SafeInterval; } while (NextScheduledScanTime <= NowSeconds);
        bDeadlineMissRecordedForActiveAcquisition = false;
        ActiveAcquisitionBackend = ResolveAcquisitionBackend(AcquisitionBackendFallbackReason);
        if (ActiveAcquisitionBackend == EVirtualLidarAcquisitionBackend::GpuDepthProjection)
        {
            if (!BeginGpuDepthScan(NowSeconds))
            {
                ActiveAcquisitionBackend = EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
                RuntimeStatus.ActiveAcquisitionBackend = TEXT("accurate_cpu_trace");
                RuntimeStatus.AcquisitionBackendMessage = AcquisitionBackendFallbackReason;
                BeginScheduledScan(NowSeconds);
            }
        }
        else
        {
            RuntimeStatus.ActiveAcquisitionBackend = TEXT("accurate_cpu_trace");
            RuntimeStatus.AcquisitionBackendMessage = AcquisitionBackendFallbackReason;
            BeginScheduledScan(NowSeconds);
        }
    }
}

void UVirtualLidarScanComponent::BeginScheduledScan(double NowSeconds)
{
	if (auto* Slab = GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()) ScheduledSlabContext=Slab->CaptureContext(SensorId,FrameId+1);
    ActiveAcquisitionBackend = EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
    ScheduledScanWidth = FMath::Max(1, HorizontalSamples);
    ScheduledScanHeight = FMath::Max(1, VerticalChannels);
    ScheduledNextRayIndex = 0;
    ScheduledScanTransform = GetComponentTransform();
    ScheduledScanStartTime = FPlatformTime::Seconds();
    ScheduledAcquisitionStartUnixNanoseconds = UtcNowUnixNanoseconds();
    ScheduledPoints.Reset();
    ScheduledHitPointCount = 0;
    ScheduledSemanticCounts.Reset();
    const bool bProfileMultiEcho = bUseProfileEchoCapability && DeviceSpec.MaxEchoesPerPixel > 1;
    const int32 EffectiveMaxEchoes = bProfileMultiEcho
        ? FMath::Clamp(DeviceSpec.MaxEchoesPerPixel, 1, 2)
        : (bUseMultiHit ? FMath::Max(1, MaxHitsPerRay) : 1);
    ScheduledPoints.Reserve(ScheduledScanWidth * ScheduledScanHeight * EffectiveMaxEchoes);
    ScheduledHeatmapPixels.SetNumZeroed(ScheduledScanWidth * ScheduledScanHeight * 4);
    BuildBeamAngleTables(ScheduledScanWidth, ScheduledScanHeight, ScheduledHorizontalAnglesDegrees, ScheduledVerticalAnglesDegrees);
    bScheduledScanInProgress = true;
    RuntimeStatus.bAcquisitionInFlight = true;
}

int32 UVirtualLidarScanComponent::ProcessScheduledScanChunk(int32 MaxRays)
{
    if (bGpuDepthScanInProgress)
    {
        return ProcessGpuDepthScan();
    }
    if (!bScheduledScanInProgress || MaxRays <= 0 || !GetWorld()) return 0;
    UWorld* World = GetWorld();
    const int32 TotalRays = ScheduledScanWidth * ScheduledScanHeight;
    const int32 EndRay = FMath::Min(TotalRays, ScheduledNextRayIndex + FMath::Clamp(MaxRays, 128, 1024));
    const FVector Origin = ScheduledScanTransform.GetLocation();
    const FRotator BaseRotation = ScheduledScanTransform.Rotator();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VirtualLidarScheduledSensor), false, GetOwner());
    const bool bProfileMultiEcho = bUseProfileEchoCapability && DeviceSpec.MaxEchoesPerPixel > 1;
    const bool bEffectiveMultiHit = bUseMultiHit || bProfileMultiEcho;
    const int32 EffectiveMaxHits = bProfileMultiEcho
        ? FMath::Clamp(DeviceSpec.MaxEchoesPerPixel, 1, 2)
        : FMath::Max(1, MaxHitsPerRay);

    for (int32 RayIndex = ScheduledNextRayIndex; RayIndex < EndRay; ++RayIndex)
    {
        const int32 V = RayIndex / ScheduledScanWidth;
        const int32 X = RayIndex % ScheduledScanWidth;
        const float Pitch = ScheduledVerticalAnglesDegrees[V];
        const float Yaw = ScheduledHorizontalAnglesDegrees[X];
        const FVector Direction = (BaseRotation + FRotator(Pitch, Yaw, 0.0f)).Vector();
        const FVector End = Origin + Direction * MaxDistance;
        FVirtualLidarPoint FirstPoint;
        InitializePhysicalPoint(
            FirstPoint,
            V,
            X,
            RayIndex,
            TotalRays,
            ScheduledScanTransform.InverseTransformVectorNoScale(Direction).GetSafeNormal());
        FirstPoint.Distance = MaxDistance;
        FirstPoint.WorldLocation = End;

        if (bEffectiveMultiHit)
        {
            TArray<FHitResult> Hits;
            World->LineTraceMultiByChannel(Hits, Origin, End, TraceChannel, Params);
            int32 Added = 0;
            const int32 EchoStartIndex = ScheduledPoints.Num();
            for (const FHitResult& Hit : Hits)
            {
                if (ShouldIgnoreHitActor(Hit.GetActor())) continue;
                FVirtualLidarPoint Point = FirstPoint;
                if (!ApplyPhysicalHitModel(Point, Hit, Direction, ScheduledScanTransform, RayIndex, Added)) continue;
                PopulatePointSemanticMetadata(Point, Hit);
                ScheduledPoints.Add(Point);
                ++ScheduledHitPointCount;
                ScheduledSemanticCounts.FindOrAdd(Point.SemanticLabel.IsNone() ? TEXT("Unclassified") : Point.SemanticLabel.ToString())++;
                if (!FirstPoint.bHit) FirstPoint = Point;
                if (++Added >= EffectiveMaxHits) break;
            }
            if (Added == 0)
            {
                ScheduledPoints.Add(FirstPoint);
            }
            else
            {
                FinalizeEchoMetadata(ScheduledPoints, EchoStartIndex, Added);
                FirstPoint = ScheduledPoints[EchoStartIndex];
            }
        }
        else
        {
            FHitResult Hit;
            bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, TraceChannel, Params);
            if (bHit && ShouldIgnoreHitActor(Hit.GetActor())) bHit = false;
            if (bHit)
            {
                bHit = ApplyPhysicalHitModel(FirstPoint, Hit, Direction, ScheduledScanTransform, RayIndex, 0);
            }
            if (bHit)
            {
                PopulatePointSemanticMetadata(FirstPoint, Hit);
                FirstPoint.EchoCount = 1;
                FirstPoint.EchoType = EVirtualLidarEchoType::Single;
            }
            ScheduledPoints.Add(FirstPoint);
            if (bHit)
            {
                ++ScheduledHitPointCount;
                ScheduledSemanticCounts.FindOrAdd(FirstPoint.SemanticLabel.IsNone() ? TEXT("Unclassified") : FirstPoint.SemanticLabel.ToString())++;
            }
        }
        WriteHeatmapPixel(ScheduledHeatmapPixels, GetHeatmapPixelIndex(X, V, ScheduledScanWidth, ScheduledScanHeight), FirstPoint);
        if (bDrawDebugRays) DrawDebugLine(World, Origin, FirstPoint.WorldLocation, FirstPoint.bHit ? ResolveSemanticColor(FirstPoint).ToFColor(true) : FColor::Silver, false, ScanInterval, 0, 0.5f);
    }

    const int32 Processed = EndRay - ScheduledNextRayIndex;
    ScheduledNextRayIndex = EndRay;
    if (ScheduledNextRayIndex >= TotalRays) CompleteScheduledScan(GetWorld()->GetTimeSeconds());
    return Processed;
}

void UVirtualLidarScanComponent::CompleteScheduledScan(double NowSeconds)
{
    bScheduledScanInProgress = false;
    RuntimeStatus.bAcquisitionInFlight = false;
    RuntimeStatus.LastAcquisitionDurationMs = static_cast<float>((FPlatformTime::Seconds() - ScheduledScanStartTime) * 1000.0);
    ++FrameId;
    LastPointStorage = MakeShared<TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(ScheduledPoints));
	PublishLastFrameSnapshot(ScheduledScanTransform, ScheduledScanWidth, ScheduledScanHeight, MaxDistance);
    const TArray<FVirtualLidarPoint>& LastPoints = GetLastPoints();
    LastHitPointCount = ScheduledHitPointCount;
    LastSemanticCounts = MoveTemp(ScheduledSemanticCounts);
    LastSlabAnalysis = AnalyzeSlabPoints(LastPoints);
    LastServerPayloadPointCount = CountServerPayloadPoints(LastPoints);
    LastPreviewPointCount = CountPreviewPoints(LastPoints);
    LastPerformanceWarning = BuildPerformanceWarning();
    const bool bRefreshPreview = ActiveAcquisitionBackend == EVirtualLidarAcquisitionBackend::GpuDepthProjection
        ? bScheduledGenerateHeatmap
        : (GetWorld() && GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()
            ? GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()->ShouldRefreshLidarPreview(this)
            : true);
    if (bRefreshPreview)
    {
        UpdateLidarViewTexture(ScheduledHeatmapPixels);
        // V2 actors route world visualization through
        // UVirtualLidarVisualizationComponent from HandleFrameAcquired. Do not
        // also rebuild the legacy CPU ISM here; that duplicated every preview
        // update and made a FullSpec scan pay the render cost twice.
        const AVirtualLidarSensorActor* SensorOwner = Cast<AVirtualLidarSensorActor>(GetOwner());
        if (!SensorOwner || !SensorOwner->VisualizationComponent)
        {
            RefreshPointCloudPreview();
        }
    }

    RuntimeStatus.MeasuredCompletionRateHz = LastScheduledCompletionTime >= 0.0 ? static_cast<float>(1.0 / FMath::Max(0.001, NowSeconds - LastScheduledCompletionTime)) : 0.0f;
    RuntimeStatus.MeasuredAcquisitionRateHz = RuntimeStatus.MeasuredCompletionRateHz;
    LastScheduledCompletionTime = NowSeconds;
	UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
	OnFrameAcquired.Broadcast(FrameId);
	OnFrameAcquiredNative.Broadcast(FrameId);
	if (bInteractivePreviewMode && bSuppressInteractiveDerivedOutput)
	{
		RuntimeStatus.LastPostProcessDurationMs = 0.0f;
		UpdateRuntimeStatusAfterScan(0);
		OnScanCompleted.Broadcast(FString(), LidarViewTexture);
		return;
	}
    if (bScheduledPayloadBuildInFlight)
    {
        ++RuntimeStatus.DroppedDerivedFrameCount;
        bScheduledPayloadRefreshPending = true;
        RuntimeStatus.bDerivedWorkInFlight = true;
        UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
        return;
    }
    QueueScheduledPayloadBuild(FrameId, ScheduledScanStartTime);
}

void UVirtualLidarScanComponent::QueueScheduledPayloadBuild(int64 CapturedFrameId, double AcquisitionStartedSeconds)
{
    if (GVirtualLidarJsonJobs.fetch_add(1, std::memory_order_acq_rel) >= GVirtualLidarJsonJobLimit)
    {
        GVirtualLidarJsonJobs.fetch_sub(1, std::memory_order_acq_rel);
        ++RuntimeStatus.QueueOverflowCount;
        ++RuntimeStatus.DroppedDerivedFrameCount;
        RuntimeStatus.bDerivedWorkInFlight = false;
        UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
        return;
    }
    bScheduledPayloadBuildInFlight = true;
    RuntimeStatus.bDerivedWorkInFlight = true;
    const int32 Generation = ScheduledGeneration;
    FLidarScheduledPayloadSnapshot Snapshot;
    Snapshot.Points = GetLastPointSnapshot();
    Snapshot.SensorId = SensorId;
    Snapshot.Manufacturer = DeviceSpec.Manufacturer;
    Snapshot.Model = DeviceSpec.Model;
    Snapshot.PreviewBackend = GetPreviewBackendName();
    Snapshot.FrameId = CapturedFrameId;
    Snapshot.HorizontalSamples = HorizontalSamples;
    Snapshot.VerticalChannels = VerticalChannels;
    Snapshot.MaxDistance = MaxDistance;
    Snapshot.ServerPayloadStride = ServerPayloadStride;
    Snapshot.MaxServerPayloadPoints = MaxServerPayloadPoints;
    Snapshot.bIncludeMissPoints = bIncludeMissPointsInServerPayload;
    Snapshot.PreviewPointStride = PreviewPointStride;
    Snapshot.MaxPreviewPoints = MaxPreviewPoints;
    Snapshot.bPreviewHitOnly = bPointCloudPreviewHitOnly;
    Snapshot.bSemanticClassification = bEnableSemanticClassification;
    Snapshot.bGpuRequested = IsGpuPreviewBackendRequested();
    Snapshot.bGpuActive = IsGpuPreviewBackendActive();
    Snapshot.bExperimentalGpuOptIn = bAllowExperimentalGpuPreviewBackend;
    Snapshot.bIncludeSlabAnalysis = bIncludeSlabAnalysisInPayload;
    Snapshot.SensorTransform = ScheduledScanTransform;
    Snapshot.SlabAnalysis = LastSlabAnalysis;
    TWeakObjectPtr<UVirtualLidarScanComponent> WeakThis(this);

    Async(EAsyncExecution::ThreadPool, [WeakThis, Generation, CapturedFrameId, Snapshot = MoveTemp(Snapshot), AcquisitionStartedSeconds]() mutable
    {
        FString Payload = BuildScheduledLidarJson(Snapshot);
        GVirtualLidarJsonJobs.fetch_sub(1, std::memory_order_acq_rel);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation, CapturedFrameId, Payload = MoveTemp(Payload), AcquisitionStartedSeconds]() mutable
        {
            if (!WeakThis.IsValid() || WeakThis->ScheduledGeneration != Generation) return;
            WeakThis->CompleteScheduledPayloadBuild(CapturedFrameId, MoveTemp(Payload), AcquisitionStartedSeconds);
        });
    });
}

void UVirtualLidarScanComponent::CompleteScheduledPayloadBuild(int64 CapturedFrameId, FString&& JsonPayload, double AcquisitionStartedSeconds)
{
    bScheduledPayloadBuildInFlight = false;
    RuntimeStatus.bDerivedWorkInFlight = false;
    RuntimeStatus.LastPostProcessDurationMs = static_cast<float>((FPlatformTime::Seconds() - AcquisitionStartedSeconds) * 1000.0);
    if (bScheduledPayloadRefreshPending && CapturedFrameId < FrameId)
    {
        bScheduledPayloadRefreshPending = false;
        QueueScheduledPayloadBuild(FrameId, ScheduledScanStartTime);
        return;
    }
    bScheduledPayloadRefreshPending = false;
    if (JsonPayload.IsEmpty())
    {
        ++RuntimeStatus.DroppedDerivedFrameCount;
        UpdateRuntimeStatusAfterScan(0);
        return;
    }
    LastJsonPayload = MoveTemp(JsonPayload);
    DispatchPayload(LastJsonPayload);
    const double OutputNowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    RuntimeStatus.MeasuredOutputRateHz = LastScheduledOutputTime >= 0.0
        ? static_cast<float>(1.0 / FMath::Max(0.001, OutputNowSeconds - LastScheduledOutputTime))
        : 0.0f;
    LastScheduledOutputTime = OutputNowSeconds;
    UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
    if (RecorderComponent) RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_lidar"), CapturedFrameId, LastJsonPayload);
    QueueScheduledAutoExports();
    OnScanCompleted.Broadcast(LastJsonPayload, LidarViewTexture);
}

void UVirtualLidarScanComponent::QueueScheduledAutoExports()
{
    if (!bExportCsvOnScan && !bExportJsonLinesOnScan && !bExportPcdOnScan) return;
    if (bScheduledAutoExportInFlight)
    {
        ++RuntimeStatus.DroppedDerivedFrameCount;
        return;
    }

    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FDateTime Now = FDateTime::UtcNow();
    const FString Stamp = FString::Printf(TEXT("%s_%03d_%lld"), *Now.ToString(TEXT("%Y%m%d_%H%M%S")), Now.GetMillisecond(), Now.GetTicks());
    const FString Prefix = FString::Printf(TEXT("%s_%s"), *SensorId, *Stamp);
    const FString CsvPath = bExportCsvOnScan ? FPaths::Combine(Directory, Prefix + TEXT(".csv")) : FString();
    const FString JsonlPath = bExportJsonLinesOnScan ? FPaths::Combine(Directory, Prefix + TEXT(".jsonl")) : FString();
    const FString PcdPath = bExportPcdOnScan ? FPaths::Combine(Directory, Prefix + TEXT(".pcd")) : FString();
    LastPointCloudExportPath = !PcdPath.IsEmpty() ? PcdPath : (!JsonlPath.IsEmpty() ? JsonlPath : CsvPath);
    const bool bHitOnly = bExportHitOnlyPointCloud;
    const int32 Generation = ScheduledGeneration;
    TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> Points = GetLastPointSnapshot();
    TWeakObjectPtr<UVirtualLidarScanComponent> WeakThis(this);
    bScheduledAutoExportInFlight = true;
    RuntimeStatus.bDerivedWorkInFlight = true;

    Async(EAsyncExecution::ThreadPool, [WeakThis, Generation, Points = MoveTemp(Points), CsvPath, JsonlPath, PcdPath, bHitOnly]() mutable
    {
        TArray<const FVirtualLidarPoint*> ExportPoints;
        if (Points.IsValid()) for (const FVirtualLidarPoint& Point : *Points) if (!bHitOnly || Point.bHit) ExportPoints.Add(&Point);
        bool bSuccess = true;
        if (!CsvPath.IsEmpty())
        {
            FString Text = TEXT("sensor_x_m,sensor_y_m,sensor_z_m,range_mm,intensity,ring,horizontal_index,echo_index,echo_count,time_offset_ns,validity,confidence,world_x_cm,world_y_cm,world_z_cm,actor,actor_class,semantic_label,tags\n");
            for (const FVirtualLidarPoint* Point : ExportPoints)
            {
                Text += FString::Printf(
                    TEXT("%.9f,%.9f,%.9f,%d,%d,%d,%d,%d,%d,%lld,%d,%.6f,%.6f,%.6f,%.6f,%s,%s,%s,%s\n"),
                    Point->SensorLocalPositionMeters.X, Point->SensorLocalPositionMeters.Y, Point->SensorLocalPositionMeters.Z,
                    Point->RangeMillimeters, Point->RawIntensity, Point->Ring, Point->HorizontalIndex,
                    Point->EchoIndex, Point->EchoCount, Point->PointTimeOffsetNanoseconds,
                    static_cast<int32>(Point->Validity), Point->Confidence,
                    Point->WorldLocation.X, Point->WorldLocation.Y, Point->WorldLocation.Z,
                    *Point->HitActorName.ToString(), *Point->HitActorClassName.ToString(),
                    *Point->SemanticLabel.ToString(), *JoinNames(Point->HitActorTags));
            }
            bSuccess = FFileHelper::SaveStringToFile(Text, *CsvPath) && bSuccess;
        }
        if (!JsonlPath.IsEmpty())
        {
            FString Text;
            for (const FVirtualLidarPoint* Point : ExportPoints)
            {
                Text += FString::Printf(
                    TEXT("{\"sensorPositionMeters\":[%.9f,%.9f,%.9f],\"rangeMillimeters\":%d,\"intensity\":%d,\"ring\":%d,\"horizontalIndex\":%d,\"echoIndex\":%d,\"echoCount\":%d,\"timeOffsetNanoseconds\":\"%lld\",\"validity\":%d,\"confidence\":%.6f,\"worldCentimeters\":[%.6f,%.6f,%.6f],\"hit\":%s,\"semanticLabel\":\"%s\"}\n"),
                    Point->SensorLocalPositionMeters.X, Point->SensorLocalPositionMeters.Y, Point->SensorLocalPositionMeters.Z,
                    Point->RangeMillimeters, Point->RawIntensity, Point->Ring, Point->HorizontalIndex,
                    Point->EchoIndex, Point->EchoCount, Point->PointTimeOffsetNanoseconds,
                    static_cast<int32>(Point->Validity), Point->Confidence,
                    Point->WorldLocation.X, Point->WorldLocation.Y, Point->WorldLocation.Z,
                    Point->bHit ? TEXT("true") : TEXT("false"), *Point->SemanticLabel.ToString());
            }
            bSuccess = FFileHelper::SaveStringToFile(Text, *JsonlPath) && bSuccess;
        }
        if (!PcdPath.IsEmpty())
        {
            FString Text = FString::Printf(
                TEXT("# .PCD v0.7\nVERSION 0.7\nFIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence\n")
                TEXT("SIZE 4 4 4 2 2 2 1 1 8 1 4\nTYPE F F F U U U U U I U F\nCOUNT 1 1 1 1 1 1 1 1 1 1 1\nWIDTH %d\nHEIGHT 1\nPOINTS %d\nDATA ascii\n"),
                ExportPoints.Num(), ExportPoints.Num());
            for (const FVirtualLidarPoint* Point : ExportPoints)
            {
                Text += FString::Printf(
                    TEXT("%.9f %.9f %.9f %d %d %d %d %d %lld %d %.6f\n"),
                    Point->SensorLocalPositionMeters.X, Point->SensorLocalPositionMeters.Y, Point->SensorLocalPositionMeters.Z,
                    Point->RawIntensity, Point->Ring, Point->HorizontalIndex, Point->EchoIndex, Point->EchoCount,
                    Point->PointTimeOffsetNanoseconds, static_cast<int32>(Point->Validity), Point->Confidence);
            }
            bSuccess = FFileHelper::SaveStringToFile(Text, *PcdPath) && bSuccess;
        }
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation, bSuccess]()
        {
            if (!WeakThis.IsValid() || WeakThis->ScheduledGeneration != Generation) return;
            WeakThis->bScheduledAutoExportInFlight = false;
            WeakThis->RuntimeStatus.bDerivedWorkInFlight = WeakThis->bScheduledPayloadBuildInFlight;
            if (!bSuccess) ++WeakThis->RuntimeStatus.DroppedDerivedFrameCount;
        });
    });
}
void UVirtualLidarScanComponent::SetTransportComponent(UVirtualSensorTransportComponent* InTransportComponent) { TransportComponent = InTransportComponent; }
void UVirtualLidarScanComponent::SetRecorderComponent(UVirtualSensorRecorderComponent* InRecorderComponent) { RecorderComponent = InRecorderComponent; }

void UVirtualLidarScanComponent::SetServerPayloadPolicy(int32 InStride, int32 InMaxPoints, bool bInIncludeMissPoints)
{
    ServerPayloadStride = FMath::Clamp(InStride, 1, 100);
    MaxServerPayloadPoints = FMath::Clamp(InMaxPoints, 0, 1000000);
    bIncludeMissPointsInServerPayload = bInIncludeMissPoints;
    PayloadPointStride = ServerPayloadStride;
    MaxPayloadPoints = MaxServerPayloadPoints;
    bIncludeMissPointsInPayload = bIncludeMissPointsInServerPayload;
}

void UVirtualLidarScanComponent::SetPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly)
{
    PreviewPointStride = FMath::Clamp(InStride, 1, 100);
    MaxPreviewPoints = FMath::Clamp(InMaxPoints, 0, 1000000);
    bPointCloudPreviewHitOnly = bInHitOnly;
    PointCloudPreviewStride = PreviewPointStride;
    MaxPointCloudPreviewInstances = MaxPreviewPoints;
    RefreshPointCloudPreview();
	if (AVirtualLidarSensorActor* SensorActor = Cast<AVirtualLidarSensorActor>(GetOwner()))
	{
		if (SensorActor->VisualizationComponent) SensorActor->VisualizationComponent->RefreshLatestFrame();
	}
}

void UVirtualLidarScanComponent::SetPreviewPolicyState(const FVirtualLidarPreviewPolicy& InPolicy)
{
	SetPreviewPolicy(InPolicy.PointStride, InPolicy.MaxPoints, InPolicy.bHitOnly);
}

FVirtualLidarPreviewPolicy UVirtualLidarScanComponent::GetPreviewPolicyState() const
{
	FVirtualLidarPreviewPolicy Policy;
	Policy.PointStride = PreviewPointStride;
	Policy.MaxPoints = MaxPreviewPoints;
	Policy.bHitOnly = bPointCloudPreviewHitOnly;
	return Policy;
}

void UVirtualLidarScanComponent::SetPreviewBackend(ELidarPointCloudPreviewBackend InBackend, bool bAllowExperimentalGpuBackend)
{
    PreviewBackend = InBackend;
    bAllowExperimentalGpuPreviewBackend = bAllowExperimentalGpuBackend;
    LastPerformanceWarning = BuildPerformanceWarning();
    RefreshPointCloudPreview();
}

FString UVirtualLidarScanComponent::GetPreviewBackendName() const
{
    if (bGpuPreviewBackendRuntimeActive) return TEXT("NiagaraGpuSprites");
    switch (PreviewBackend)
    {
    case ELidarPointCloudPreviewBackend::NiagaraCandidate:
        return TEXT("NiagaraCandidateCpuFallback");
    case ELidarPointCloudPreviewBackend::CustomGpuCandidate:
        return TEXT("CustomGpuCandidateCpuFallback");
    case ELidarPointCloudPreviewBackend::CpuInstancedMesh:
    default:
        return TEXT("CpuInstancedMesh");
    }
}

bool UVirtualLidarScanComponent::IsGpuPreviewBackendRequested() const
{
    return bGpuPreviewBackendRuntimeRequested ||
        PreviewBackend == ELidarPointCloudPreviewBackend::NiagaraCandidate ||
        PreviewBackend == ELidarPointCloudPreviewBackend::CustomGpuCandidate;
}

bool UVirtualLidarScanComponent::IsGpuPreviewBackendActive() const
{
    return bGpuPreviewBackendRuntimeActive;
}

void UVirtualLidarScanComponent::SetGpuPreviewBackendRuntimeState(bool bActive, const FString& InFallbackReason)
{
    bGpuPreviewBackendRuntimeActive = bActive;
    bGpuPreviewBackendRuntimeRequested = bActive || !InFallbackReason.IsEmpty();
    GpuPreviewFallbackReason = InFallbackReason;
}

void UVirtualLidarScanComponent::SetPointCloudPreviewEnabled(bool bEnabled)
{
    bPointCloudPreviewEnabled = bEnabled;
    if (bPointCloudPreviewEnabled) { EnsurePointCloudPreviewComponent(); ApplyPointCloudPreviewStyle(); RefreshPointCloudPreview(); }
    else { ClearPointCloudPreview(); }
}
void UVirtualLidarScanComponent::ClearPointCloudPreview()
{
    if (PointCloudPreviewComponent) { PointCloudPreviewComponent->ClearInstances(); PointCloudPreviewComponent->SetHiddenInGame(true); PointCloudPreviewComponent->SetVisibility(false, true); }
}
void UVirtualLidarScanComponent::ApplyPointCloudPreviewStyle()
{
    if (PointCloudPreviewComponent && PointCloudPreviewMaterial) PointCloudPreviewComponent->SetMaterial(0, PointCloudPreviewMaterial);
}

void UVirtualLidarScanComponent::ResetDefaultSemanticClassRules()
{
    SemanticClassRules.Reset();
    auto AddRule = [this](FName Label, const FLinearColor& Color, TArray<FName> Tags, TArray<FName> Classes, TArray<FString> Names, TArray<FString> Comps)
    {
        FVirtualLidarSemanticClassRule R; R.Label = Label; R.DisplayColor = Color; R.ActorTags = Tags; R.ActorClassNames = Classes; R.ActorNameContains = Names; R.ComponentNameContains = Comps; SemanticClassRules.Add(R);
    };
    AddRule(TEXT("Slab"), FLinearColor(1.0f, 0.25f, 0.02f, 1.0f), {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("SteelSlab"), TEXT("Plate")}, {}, {TEXT("Slab"), TEXT("slab"), TEXT("SLAB"), TEXT("SteelSlab"), TEXT("Plate")}, {TEXT("Slab"), TEXT("slab"), TEXT("Plate")});
    AddRule(TEXT("Roller"), FLinearColor(0.0f, 0.85f, 1.0f, 1.0f), {TEXT("Roller"), TEXT("roller"), TEXT("Rollers")}, {}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll"), TEXT("roll")}, {TEXT("Roller"), TEXT("roller"), TEXT("Roll")});
    AddRule(TEXT("Conveyor"), FLinearColor(0.05f, 1.0f, 0.25f, 1.0f), {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Equipment")}, {}, {TEXT("Conveyor"), TEXT("conveyor"), TEXT("Frame"), TEXT("Equipment")}, {TEXT("Conveyor"), TEXT("Frame"), TEXT("Equipment")});
    AddRule(TEXT("Floor"), FLinearColor(0.2f, 0.25f, 1.0f, 1.0f), {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground")}, {}, {TEXT("Floor"), TEXT("floor"), TEXT("Ground"), TEXT("ground"), TEXT("Grid")}, {TEXT("Floor"), TEXT("Ground")});
    AddRule(TEXT("Sensor"), FLinearColor(0.75f, 0.15f, 1.0f, 1.0f), {TEXT("Sensor"), TEXT("Lidar"), TEXT("LiDAR"), TEXT("Camera")}, {}, {TEXT("Sensor"), TEXT("Lidar"), TEXT("LiDAR"), TEXT("Camera")}, {TEXT("Sensor"), TEXT("Lidar"), TEXT("Camera")});
}

FLinearColor UVirtualLidarScanComponent::GetSemanticColorForLabel(FName SemanticLabel) const
{
    for (const FVirtualLidarSemanticClassRule& R : SemanticClassRules) { if (!R.Label.IsNone() && R.Label == SemanticLabel) return R.DisplayColor; }
    return DefaultSemanticColor;
}

bool UVirtualLidarScanComponent::SemanticRuleMatches(const FVirtualLidarSemanticClassRule& Rule, const AActor* Actor, const UPrimitiveComponent* Component) const
{
    if (!Actor) return false;
    for (const FName& Tag : Rule.ActorTags) { if (!Tag.IsNone() && Actor->ActorHasTag(Tag)) return true; }
    const FString ActorName = Actor->GetName();
    for (const FString& Pattern : Rule.ActorNameContains) { if (!Pattern.IsEmpty() && ActorName.Contains(Pattern, ESearchCase::IgnoreCase)) return true; }
    const FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
    for (const FName& ClassRule : Rule.ActorClassNames) { const FString Pattern = ClassRule.ToString(); if (!Pattern.IsEmpty() && (ClassName.Equals(Pattern, ESearchCase::IgnoreCase) || ClassName.Contains(Pattern, ESearchCase::IgnoreCase))) return true; }
    if (Component) { const FString CompName = Component->GetName(); for (const FString& Pattern : Rule.ComponentNameContains) { if (!Pattern.IsEmpty() && CompName.Contains(Pattern, ESearchCase::IgnoreCase)) return true; } }
    return false;
}

FName UVirtualLidarScanComponent::ResolveSemanticLabel(const FHitResult& Hit) const
{
    if (!bEnableSemanticClassification || !Hit.GetActor()) return DefaultSemanticLabel;
    for (const FVirtualLidarSemanticClassRule& R : SemanticClassRules) { if (SemanticRuleMatches(R, Hit.GetActor(), Hit.GetComponent())) return R.Label; }
    return DefaultSemanticLabel;
}

FLinearColor UVirtualLidarScanComponent::ResolveSemanticColor(const FVirtualLidarPoint& Point) const
{
    if (!Point.bHit) return FLinearColor(0.01f, 0.01f, 0.02f, 1.0f);
    return GetSemanticColorForLabel(Point.SemanticLabel.IsNone() ? DefaultSemanticLabel : Point.SemanticLabel);
}

void UVirtualLidarScanComponent::PopulatePointSemanticMetadata(FVirtualLidarPoint& Point, const FHitResult& Hit) const
{
    const AActor* A = Hit.GetActor();
    Point.HitActorName = A ? A->GetFName() : NAME_None;
    Point.HitActorClassName = (A && A->GetClass()) ? A->GetClass()->GetFName() : NAME_None;
    Point.HitActorTags.Reset();
    if (A) Point.HitActorTags = A->Tags;
    Point.SemanticLabel = A ? ResolveSemanticLabel(Hit) : NAME_None;
}

void UVirtualLidarScanComponent::ApplyPreset(EVirtualLidarPreset NewPreset)
{
    Preset = NewPreset;
    if (Preset == EVirtualLidarPreset::LowDebug) { HorizontalSamples = 90; VerticalChannels = 8; ScanInterval = 1.0f; PreviewPointStride = 1; MaxPreviewPoints = 2000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
    else if (Preset == EVirtualLidarPreset::MediumPreview) { HorizontalSamples = 180; VerticalChannels = 16; ScanInterval = 0.5f; PreviewPointStride = 2; MaxPreviewPoints = 3000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
    else if (Preset == EVirtualLidarPreset::HighQuality) { HorizontalSamples = 360; VerticalChannels = 32; ScanInterval = 0.25f; PreviewPointStride = 4; MaxPreviewPoints = 5000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
}

void UVirtualLidarScanComponent::ApplyDeviceProfile(EVirtualLidarDeviceProfile NewProfile)
{
    DeviceProfile = NewProfile;
    ApplySimulationQuality(SimulationQuality);
}

void UVirtualLidarScanComponent::ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality)
{
    SimulationQuality = NewQuality;
    if (SimulationQuality != EVirtualSensorSimulationQuality::Custom)
    {
        const FVirtualLidarProfilePreset Resolved = ResolveProfilePreset(DeviceProfile, SimulationQuality);
        DeviceSpec = Resolved.DeviceSpec;
        ProfileClass = Resolved.ProfileClass;
        FidelityMode = Resolved.FidelityMode;
        AcquisitionBackend = Resolved.RecommendedBackend;
        CalibrationId = Resolved.CalibrationId;
        ScanInterval = Resolved.ScanIntervalSeconds;
        MaxDistance = Resolved.MaxDistanceCm;
        HorizontalSamples = Resolved.HorizontalSamples;
        VerticalChannels = Resolved.VerticalChannels;
        HorizontalFov = Resolved.HorizontalFovDegrees;
        MinVerticalAngle = Resolved.MinVerticalAngleDegrees;
        MaxVerticalAngle = Resolved.MaxVerticalAngleDegrees;
        PreviewPointStride = Resolved.PreviewPointStride;
        MaxPreviewPoints = Resolved.MaxPreviewPoints;
        Preset = EVirtualLidarPreset::Custom;
    }

    PointCloudPreviewStride = PreviewPointStride;
    MaxPointCloudPreviewInstances = MaxPreviewPoints;
    PayloadPointStride = ServerPayloadStride;
    MaxPayloadPoints = MaxServerPayloadPoints;
}

FVirtualLidarProfilePreset UVirtualLidarScanComponent::ResolveProfilePreset(
    EVirtualLidarDeviceProfile Profile,
    EVirtualSensorSimulationQuality Quality)
{
    FVirtualLidarProfilePreset Result;
    Result.DeviceSpec.Manufacturer = TEXT("Generic");
    Result.DeviceSpec.Model = TEXT("Generic LiDAR");
    Result.DeviceSpec.HorizontalFovDegrees = 360.0f;
    Result.DeviceSpec.VerticalFovDegrees = 59.0f;
    Result.DeviceSpec.MinRangeCm = 10.0f;
    Result.DeviceSpec.TypicalRangeCm = 4000.0f;
    Result.DeviceSpec.MaxRangeCm = 20000.0f;
    Result.ProfileClass = EVirtualLidarProfileClass::Generic;
    Result.FidelityMode = EVirtualSensorFidelityMode::IdealTruth;
    Result.RecommendedBackend = EVirtualLidarAcquisitionBackend::Auto;
    Result.ProfileKey = TEXT("generic");
    Result.CalibrationId = TEXT("none");

    if (Profile == EVirtualLidarDeviceProfile::LivoxMid360S)
    {
        Result.DeviceSpec.Manufacturer = TEXT("Livox");
        Result.DeviceSpec.Model = TEXT("Mid-360S");
        Result.DeviceSpec.HorizontalFovDegrees = 360.0f;
        Result.DeviceSpec.VerticalFovDegrees = 59.0f;
        Result.DeviceSpec.MinRangeCm = 10.0f;
        Result.DeviceSpec.TypicalRangeCm = 4000.0f;
        Result.DeviceSpec.MaxRangeCm = 10000.0f;
        Result.DeviceSpec.FrameRateHz = 10.0f;
        Result.DeviceSpec.PointRate = 200000;
        Result.DeviceSpec.OutputFields = TEXT("XYZ");
        Result.DeviceSpec.Notes = TEXT("Livox Mid-360S: 40m at 10% reflectivity and 100m cutoff.");
        Result.ProfileKey = TEXT("livox-mid360s");
        Result.MaxDistanceCm = 4000.0f;
        Result.HorizontalFovDegrees = 360.0f;
        Result.MinVerticalAngleDegrees = -7.0f;
        Result.MaxVerticalAngleDegrees = 52.0f;
    }
    else if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80)
    {
        Result.DeviceSpec.Manufacturer = TEXT("IYOBOT");
        Result.DeviceSpec.Model = TEXT("ML-X(80) Integration 200");
        Result.DeviceSpec.HorizontalFovDegrees = 80.0f;
        Result.DeviceSpec.VerticalFovDegrees = 23.3f;
        Result.DeviceSpec.MinRangeCm = 10.0f;
        Result.DeviceSpec.TypicalRangeCm = 15000.0f;
        Result.DeviceSpec.MaxRangeCm = 15000.0f;
        Result.DeviceSpec.FrameRateHz = 20.0f;
        Result.DeviceSpec.PointRate = 224000;
        Result.DeviceSpec.Width = 200;
        Result.DeviceSpec.Height = 56;
        Result.DeviceSpec.HorizontalAngularResolutionDegrees = 0.4f;
        Result.DeviceSpec.VerticalAngularResolutionDegrees = 23.3f / 55.0f;
        Result.DeviceSpec.MaxEchoesPerPixel = 2;
        Result.DeviceSpec.MaxDistanceErrorMillimeters = 30.0f;
        Result.DeviceSpec.WavelengthNanometers = 940.0f;
        Result.DeviceSpec.ReferenceReflectivityPercent = 20.0f;
        Result.DeviceSpec.ReferenceAmbientLux = 100000.0f;
        Result.DeviceSpec.OutputFields = TEXT("XYZ + intensity");
        Result.DeviceSpec.Notes = TEXT("Project integration/downsample mode: 200x56 at 20Hz. Optical limits follow the public ML-X(80) specification; this is not the public native point density.");
        Result.ProfileClass = EVirtualLidarProfileClass::IntegrationDownsampled;
        Result.FidelityMode = EVirtualSensorFidelityMode::PublicSpecBased;
        Result.ProfileKey = TEXT("iyobot-mlx80-integration200");
        Result.CalibrationId = TEXT("public-spec-derived-2026");
        Result.MaxDistanceCm = 15000.0f;
        Result.HorizontalFovDegrees = 80.0f;
        Result.MinVerticalAngleDegrees = -11.65f;
        Result.MaxVerticalAngleDegrees = 11.65f;
    }
    else if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE)
    {
        Result.DeviceSpec.Manufacturer = TEXT("IYOBOT");
        Result.DeviceSpec.Model = TEXT("ML-X(80) Native");
        Result.DeviceSpec.HorizontalFovDegrees = 80.0f;
        Result.DeviceSpec.VerticalFovDegrees = 23.3f;
        Result.DeviceSpec.MinRangeCm = 10.0f;
        Result.DeviceSpec.TypicalRangeCm = 15000.0f;
        Result.DeviceSpec.MaxRangeCm = 15000.0f;
        Result.DeviceSpec.FrameRateHz = 20.0f;
        Result.DeviceSpec.PointRate = 645120;
        Result.DeviceSpec.Width = 576;
        Result.DeviceSpec.Height = 56;
        Result.DeviceSpec.HorizontalAngularResolutionDegrees = 0.139f;
        Result.DeviceSpec.VerticalAngularResolutionDegrees = 0.417f;
        Result.DeviceSpec.MaxEchoesPerPixel = 2;
        Result.DeviceSpec.MaxDistanceErrorMillimeters = 30.0f;
        Result.DeviceSpec.WavelengthNanometers = 940.0f;
        Result.DeviceSpec.ReferenceReflectivityPercent = 20.0f;
        Result.DeviceSpec.ReferenceAmbientLux = 100000.0f;
        Result.DeviceSpec.OutputFields = TEXT("XYZ + intensity");
        Result.DeviceSpec.Notes = TEXT("Public-spec-derived native density: approximately 576x56 at 20Hz (645,120 points/s), up to 25Hz. Exact calibration and vendor packet layout require hardware evidence.");
        Result.ProfileClass = EVirtualLidarProfileClass::PublicSpecNative;
        Result.FidelityMode = EVirtualSensorFidelityMode::PublicSpecBased;
        Result.ProfileKey = TEXT("iyobot-mlx80-native");
        Result.CalibrationId = TEXT("public-spec-derived-2026");
        Result.MaxDistanceCm = 15000.0f;
        Result.HorizontalFovDegrees = 80.0f;
        Result.MinVerticalAngleDegrees = -11.65f;
        Result.MaxVerticalAngleDegrees = 11.65f;
    }

    if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80)
    {
        if (Quality == EVirtualSensorSimulationQuality::Debug) { Result.HorizontalSamples = 50; Result.VerticalChannels = 14; Result.ScanIntervalSeconds = 0.2f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 1000; }
        else if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) { Result.HorizontalSamples = 100; Result.VerticalChannels = 28; Result.ScanIntervalSeconds = 0.1f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 2800; }
        else if (Quality == EVirtualSensorSimulationQuality::Balanced) { Result.HorizontalSamples = 160; Result.VerticalChannels = 42; Result.ScanIntervalSeconds = 1.0f / 15.0f; Result.PreviewPointStride = 2; Result.MaxPreviewPoints = 5000; }
        else { Result.HorizontalSamples = 200; Result.VerticalChannels = 56; Result.ScanIntervalSeconds = 0.05f; Result.PreviewPointStride = 3; Result.MaxPreviewPoints = 5000; }
    }
    else if (Profile == EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE)
    {
        if (Quality == EVirtualSensorSimulationQuality::Debug) { Result.HorizontalSamples = 144; Result.VerticalChannels = 14; Result.ScanIntervalSeconds = 0.2f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 2016; }
        else if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) { Result.HorizontalSamples = 288; Result.VerticalChannels = 28; Result.ScanIntervalSeconds = 0.1f; Result.PreviewPointStride = 2; Result.MaxPreviewPoints = 4032; }
        else if (Quality == EVirtualSensorSimulationQuality::Balanced) { Result.HorizontalSamples = 432; Result.VerticalChannels = 42; Result.ScanIntervalSeconds = 1.0f / 15.0f; Result.PreviewPointStride = 4; Result.MaxPreviewPoints = 5000; }
        else { Result.HorizontalSamples = 576; Result.VerticalChannels = 56; Result.ScanIntervalSeconds = 0.05f; Result.PreviewPointStride = 7; Result.MaxPreviewPoints = 5000; }
    }
    else
    {
        if (Quality == EVirtualSensorSimulationQuality::Debug) { Result.HorizontalSamples = 60; Result.VerticalChannels = 8; Result.ScanIntervalSeconds = 0.5f; Result.PreviewPointStride = 1; Result.MaxPreviewPoints = 1000; }
        else if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) { Result.HorizontalSamples = 120; Result.VerticalChannels = 24; Result.ScanIntervalSeconds = 0.25f; Result.PreviewPointStride = 2; Result.MaxPreviewPoints = 3000; }
        else if (Quality == EVirtualSensorSimulationQuality::Balanced) { Result.HorizontalSamples = 240; Result.VerticalChannels = 32; Result.ScanIntervalSeconds = 0.2f; Result.PreviewPointStride = 3; Result.MaxPreviewPoints = 5000; }
        else { Result.HorizontalSamples = 360; Result.VerticalChannels = 60; Result.ScanIntervalSeconds = 0.1f; Result.PreviewPointStride = 6; Result.MaxPreviewPoints = 5000; }
    }
    return Result;
}

void UVirtualLidarScanComponent::ScanAndSend()
{
    ++FrameId;
    ScheduledAcquisitionStartUnixNanoseconds = UtcNowUnixNanoseconds();

    TArray<uint8> HeatmapPixels;
    TArray<FVirtualLidarPoint> NewPoints;
    ExecuteScan(NewPoints, HeatmapPixels);
    LastPointStorage = MakeShared<TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(NewPoints));
	PublishLastFrameSnapshot(GetComponentTransform(), HorizontalSamples, VerticalChannels, MaxDistance);
    const TArray<FVirtualLidarPoint>& LastPoints = GetLastPoints();
    RebuildLastPointStatistics();
    LastSlabAnalysis = AnalyzeSlabPoints(LastPoints);
    LastServerPayloadPointCount = CountServerPayloadPoints(LastPoints);
    LastPreviewPointCount = CountPreviewPoints(LastPoints);
    LastPerformanceWarning = BuildPerformanceWarning();
    UpdateLidarViewTexture(HeatmapPixels);
    RefreshPointCloudPreview();
	UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
	OnFrameAcquired.Broadcast(FrameId);
	OnFrameAcquiredNative.Broadcast(FrameId);

    const FString JsonPayload = BuildJsonPayload(LastPoints);
    LastJsonPayload = JsonPayload;
    DispatchPayload(JsonPayload);
    UpdateRuntimeStatusAfterScan(JsonPayload.Len());

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_lidar"), FrameId, JsonPayload);
    }

    ExportEnabledPointCloudFormats();
    OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
}

void UVirtualLidarScanComponent::InjectPointCloudFrame(const TArray<FVirtualLidarPoint>& Points, bool bSendTransport)
{
    ++FrameId;
    LastPointStorage = MakeShared<TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(Points);
	PublishLastFrameSnapshot(GetComponentTransform(), HorizontalSamples, VerticalChannels, MaxDistance);
    const TArray<FVirtualLidarPoint>& LastPoints = GetLastPoints();
    RebuildLastPointStatistics();
    LastSlabAnalysis = AnalyzeSlabPoints(LastPoints);
    LastServerPayloadPointCount = CountServerPayloadPoints(LastPoints);
    LastPreviewPointCount = CountPreviewPoints(LastPoints);
    LastPerformanceWarning = BuildPerformanceWarning();

    const int32 W = FMath::Max(1, HorizontalSamples);
    const int32 Hn = FMath::Max(1, VerticalChannels);
    TArray<uint8> HeatmapPixels;
    HeatmapPixels.SetNumZeroed(W * Hn * 4);
    const int32 MaxRenderablePoints = FMath::Min(LastPoints.Num(), W * Hn);
    for (int32 PointIndex = 0; PointIndex < MaxRenderablePoints; ++PointIndex)
    {
        WriteHeatmapPixel(HeatmapPixels, GetHeatmapPixelIndex(PointIndex % W, PointIndex / W, W, Hn), LastPoints[PointIndex]);
    }
    UpdateLidarViewTexture(HeatmapPixels);
    RefreshPointCloudPreview();
	UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
	OnFrameAcquired.Broadcast(FrameId);
	OnFrameAcquiredNative.Broadcast(FrameId);

    const FString JsonPayload = BuildJsonPayload(LastPoints);
    LastJsonPayload = JsonPayload;
    if (bSendTransport)
    {
        DispatchPayload(JsonPayload);
    }
    UpdateRuntimeStatusAfterScan(JsonPayload.Len());

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_lidar_replay"), FrameId, JsonPayload);
    }

    OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
}

void UVirtualLidarScanComponent::PublishLastFrameSnapshot(
	const FTransform& AcquisitionTransform,
	int32 InHorizontalSamples,
	int32 InVerticalChannels,
	float InMaxDistanceCm)
{
	TSharedPtr<FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe>();
	Snapshot->Points = GetLastPointSnapshot();
    Snapshot->SchemaVersion = TEXT("virtual-lidar.v2");
    Snapshot->ProfileKey = DeviceProfile == EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE
        ? TEXT("iyobot-mlx80-native")
        : (DeviceProfile == EVirtualLidarDeviceProfile::IYOBOT_MLX80
            ? TEXT("iyobot-mlx80-integration200")
            : (DeviceProfile == EVirtualLidarDeviceProfile::LivoxMid360S ? TEXT("livox-mid360s") : TEXT("generic")));
    Snapshot->CalibrationId = CalibrationId;
    Snapshot->FirmwareVersion = TEXT("simulation-public-spec");
    Snapshot->CoordinateConvention = TEXT("sensor-local RH X-forward Y-left Z-up, meters");
    Snapshot->AcquisitionStartUnixNanoseconds = ScheduledAcquisitionStartUnixNanoseconds > 0
        ? ScheduledAcquisitionStartUnixNanoseconds
        : UtcNowUnixNanoseconds();
    Snapshot->AcquisitionEndUnixNanoseconds = UtcNowUnixNanoseconds();
    Snapshot->RequestedRayCount = FMath::Max(1, InHorizontalSamples) * FMath::Max(1, InVerticalChannels);
    Snapshot->FidelityMode = FidelityMode;
    Snapshot->AcquisitionBackend = ActiveAcquisitionBackend;
    Snapshot->TimeSyncState = EVirtualLidarTimeSyncState::SimulationClock;
    Snapshot->bProtocolVerifiedAgainstHardware = DeviceSpec.bProtocolVerifiedAgainstHardware;
	Snapshot->AcquisitionTransform = AcquisitionTransform;
	Snapshot->SlabContext=ScheduledSlabContext;
	if (GetWorld()) if (auto* Slab = GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()) Slab->CompleteAcquisition(SensorId,FrameId);
	ScheduledSlabContext=FVirtualSlabFrameContext();
	Snapshot->FrameId = FrameId;
	Snapshot->HorizontalSamples = FMath::Max(1, InHorizontalSamples);
	Snapshot->VerticalChannels = FMath::Max(1, InVerticalChannels);
	Snapshot->MaxDistanceCm = FMath::Max(1.0f, InMaxDistanceCm);
	Snapshot->SettingsRevision = ++FrameSettingsRevision;
    RebuildPhysicalFrameStatistics(*Snapshot);
	LastFrameSnapshot = StaticCastSharedPtr<const FVirtualLidarFrameSnapshot>(Snapshot);
    ScheduledAcquisitionStartUnixNanoseconds = 0;
}

void UVirtualLidarScanComponent::ExecuteScan(TArray<FVirtualLidarPoint>& OutPoints, TArray<uint8>& OutHeatmapPixels)
{
    OutPoints.Reset();
    const int32 W = FMath::Max(1, HorizontalSamples);
    const int32 H = FMath::Max(1, VerticalChannels);
    const int32 TotalRays = W * H;
    const bool bProfileMultiEcho = bUseProfileEchoCapability && DeviceSpec.MaxEchoesPerPixel > 1;
    const bool bEffectiveMultiHit = bUseMultiHit || bProfileMultiEcho;
    const int32 EffectiveMaxHits = bProfileMultiEcho
        ? FMath::Clamp(DeviceSpec.MaxEchoesPerPixel, 1, 2)
        : FMath::Max(1, MaxHitsPerRay);
    OutPoints.Reserve(TotalRays * (bEffectiveMultiHit ? EffectiveMaxHits : 1));
    OutHeatmapPixels.SetNumZeroed(TotalRays * 4);

    UWorld* World = GetWorld();
    if (!World) return;
    const FTransform AcquisitionTransform = GetComponentTransform();
    const FVector Origin = AcquisitionTransform.GetLocation();
    const FRotator BaseRotation = AcquisitionTransform.Rotator();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VirtualLidarSensor), false, GetOwner());
    TArray<float> HorizontalAngles;
    TArray<float> VerticalAngles;
    BuildBeamAngleTables(W, H, HorizontalAngles, VerticalAngles);

    for (int32 V = 0; V < H; ++V)
    {
        const float Pitch = VerticalAngles[V];
        for (int32 X = 0; X < W; ++X)
        {
            const int32 RayIndex = V * W + X;
            const float Yaw = HorizontalAngles[X];
            const FVector Direction = (BaseRotation + FRotator(Pitch, Yaw, 0.0f)).Vector();
            const FVector End = Origin + Direction * MaxDistance;
            FVirtualLidarPoint FirstPoint;
            InitializePhysicalPoint(
                FirstPoint,
                V,
                X,
                RayIndex,
                TotalRays,
                AcquisitionTransform.InverseTransformVectorNoScale(Direction).GetSafeNormal());
            FirstPoint.Distance = MaxDistance;
            FirstPoint.WorldLocation = End;

            if (bEffectiveMultiHit)
            {
                TArray<FHitResult> Hits;
                World->LineTraceMultiByChannel(Hits, Origin, End, TraceChannel, Params);
                int32 Added = 0;
                const int32 EchoStartIndex = OutPoints.Num();
                for (const FHitResult& Hit : Hits)
                {
                    if (ShouldIgnoreHitActor(Hit.GetActor())) continue;
                    FVirtualLidarPoint Point = FirstPoint;
                    if (!ApplyPhysicalHitModel(Point, Hit, Direction, AcquisitionTransform, RayIndex, Added)) continue;
                    PopulatePointSemanticMetadata(Point, Hit);
                    OutPoints.Add(Point);
                    if (!FirstPoint.bHit) FirstPoint = Point;
                    if (++Added >= EffectiveMaxHits) break;
                }
                if (Added == 0)
                {
                    OutPoints.Add(FirstPoint);
                }
                else
                {
                    FinalizeEchoMetadata(OutPoints, EchoStartIndex, Added);
                    FirstPoint = OutPoints[EchoStartIndex];
                }
            }
            else
            {
                FHitResult Hit;
                bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, TraceChannel, Params);
                if (bHit && ShouldIgnoreHitActor(Hit.GetActor())) bHit = false;
                if (bHit)
                {
                    bHit = ApplyPhysicalHitModel(FirstPoint, Hit, Direction, AcquisitionTransform, RayIndex, 0);
                }
                if (bHit)
                {
                    PopulatePointSemanticMetadata(FirstPoint, Hit);
                    FirstPoint.EchoCount = 1;
                    FirstPoint.EchoType = EVirtualLidarEchoType::Single;
                }
                OutPoints.Add(FirstPoint);
            }
            WriteHeatmapPixel(OutHeatmapPixels, GetHeatmapPixelIndex(X, V, W, H), FirstPoint);
            if (bDrawDebugRays) DrawDebugLine(World, Origin, FirstPoint.WorldLocation, FirstPoint.bHit ? ResolveSemanticColor(FirstPoint).ToFColor(true) : FColor::Silver, false, ScanInterval, 0, 0.5f);
        }
    }
}

FVirtualLidarSlabAnalysisResult UVirtualLidarScanComponent::AnalyzeSlabPoints(const TArray<FVirtualLidarPoint>& Points) const
{
    FVirtualLidarSlabAnalysisResult Result;
    Result.ReferenceYawDegrees = ReferenceSlabYawDegrees;

    TArray<FVector> SlabPoints;
    SlabPoints.Reserve(Points.Num());
    for (const FVirtualLidarPoint& Point : Points)
    {
        if (Point.bHit && Point.SemanticLabel == SlabSemanticLabel)
        {
            SlabPoints.Add(Point.WorldLocation);
        }
    }

    Result.SlabHitPointCount = SlabPoints.Num();
    if (SlabPoints.Num() < FMath::Max(3, MinSlabPointsForAnalysis))
    {
        Result.StatusMessage = FString::Printf(TEXT("insufficient slab points: %d/%d"), SlabPoints.Num(), FMath::Max(3, MinSlabPointsForAnalysis));
        return Result;
    }

    FVector Sum = FVector::ZeroVector;
    Result.BoundsMin = FVector(FLT_MAX);
    Result.BoundsMax = FVector(-FLT_MAX);
    for (const FVector& Point : SlabPoints)
    {
        Sum += Point;
        Result.BoundsMin.X = FMath::Min(Result.BoundsMin.X, Point.X);
        Result.BoundsMin.Y = FMath::Min(Result.BoundsMin.Y, Point.Y);
        Result.BoundsMin.Z = FMath::Min(Result.BoundsMin.Z, Point.Z);
        Result.BoundsMax.X = FMath::Max(Result.BoundsMax.X, Point.X);
        Result.BoundsMax.Y = FMath::Max(Result.BoundsMax.Y, Point.Y);
        Result.BoundsMax.Z = FMath::Max(Result.BoundsMax.Z, Point.Z);
    }

    Result.Center = Sum / static_cast<float>(SlabPoints.Num());

    double XX = 0.0;
    double XY = 0.0;
    double YY = 0.0;
    for (const FVector& Point : SlabPoints)
    {
        const double DX = static_cast<double>(Point.X - Result.Center.X);
        const double DY = static_cast<double>(Point.Y - Result.Center.Y);
        XX += DX * DX;
        XY += DX * DY;
        YY += DY * DY;
    }

    const double Count = static_cast<double>(SlabPoints.Num());
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

    Result.EstimatedYawDegrees = NormalizeAxisAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X)));
    Result.AngleDeviationDegrees = NormalizeAxisAngleDegrees(Result.EstimatedYawDegrees - ReferenceSlabYawDegrees);
    Result.Confidence = static_cast<float>(FMath::Clamp((LambdaMajor - LambdaMinor) / FMath::Max(1.0, LambdaMajor), 0.0, 1.0));
    Result.bValid = true;
    Result.StatusMessage = TEXT("ok");
    return Result;
}

int32 UVirtualLidarScanComponent::CountServerPayloadPoints(const TArray<FVirtualLidarPoint>& Points) const
{
    const int32 SafeStride = FMath::Max(1, ServerPayloadStride);
    const int32 SafeMax = FMath::Max(0, MaxServerPayloadPoints);
    int32 Count = 0;
    for (int32 Index = 0; Index < Points.Num(); Index += SafeStride)
    {
        if (SafeMax > 0 && Count >= SafeMax) break;
        if (!bIncludeMissPointsInServerPayload && !Points[Index].bHit) continue;
        ++Count;
    }
    return Count;
}

int32 UVirtualLidarScanComponent::CountPreviewPoints(const TArray<FVirtualLidarPoint>& Points) const
{
    const int32 SafeStride = FMath::Max(1, PreviewPointStride);
    const int32 SafeMax = FMath::Max(0, MaxPreviewPoints);
    int32 Count = 0;
    for (int32 Index = 0; Index < Points.Num(); Index += SafeStride)
    {
        if (SafeMax > 0 && Count >= SafeMax) break;
        if (bPointCloudPreviewHitOnly && !Points[Index].bHit) continue;
        ++Count;
    }
    return Count;
}

FString UVirtualLidarScanComponent::BuildPerformanceWarning() const
{
    TArray<FString> Warnings;
    if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec && bUseMultiHit)
    {
        Warnings.Add(TEXT("FullSpec+MultiHit is expensive"));
    }
    if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec && (bExportCsvOnScan || bExportJsonLinesOnScan || bExportPcdOnScan))
    {
        Warnings.Add(TEXT("FullSpec export-on-scan may stall the editor"));
    }
    if (bPointCloudPreviewEnabled && MaxPreviewPoints == 0 && PreviewPointStride <= 1)
    {
        Warnings.Add(TEXT("Preview is uncapped"));
    }
    if (MaxServerPayloadPoints == 0)
    {
        Warnings.Add(TEXT("Server payload is uncapped (MaxServerPayloadPoints=0)"));
    }
    if (MaxDistance >= 15000.0f)
    {
        Warnings.Add(TEXT("장거리 LiDAR trace가 활성화되어 복잡한 레벨에서는 처리 비용이 증가할 수 있습니다"));
    }
    if (IsGpuPreviewBackendRequested())
    {
        Warnings.Add(TEXT("GPU preview backend is a candidate only; CPU fallback is active"));
    }
    return FString::Join(Warnings, TEXT("; "));
}

FString UVirtualLidarScanComponent::BuildJsonPayload(const TArray<FVirtualLidarPoint>& Points) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    int32 HitPointCount = 0;
    for (const FVirtualLidarPoint& Point : Points)
    {
        if (Point.bHit)
        {
            ++HitPointCount;
        }
    }

    Root->SetStringField(TEXT("schemaVersion"), TEXT("virtual-lidar.v1"));
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_lidar"));
    Root->SetStringField(TEXT("sensorId"), SensorId);
    Root->SetStringField(TEXT("manufacturer"), DeviceSpec.Manufacturer);
    Root->SetStringField(TEXT("model"), DeviceSpec.Model);
    Root->SetNumberField(TEXT("frameId"), static_cast<double>(FrameId));
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("horizontalSamples"), HorizontalSamples);
    Root->SetNumberField(TEXT("verticalChannels"), VerticalChannels);
    Root->SetNumberField(TEXT("rayCount"), HorizontalSamples * VerticalChannels);
    Root->SetNumberField(TEXT("totalPointCount"), Points.Num());
    Root->SetNumberField(TEXT("hitPointCount"), HitPointCount);
    Root->SetNumberField(TEXT("maxDistance"), MaxDistance);
    Root->SetBoolField(TEXT("semanticClassification"), bEnableSemanticClassification);
    Root->SetNumberField(TEXT("serverPayloadStride"), ServerPayloadStride);
    Root->SetNumberField(TEXT("maxServerPayloadPoints"), MaxServerPayloadPoints);
    Root->SetBoolField(TEXT("includeMissPointsInServerPayload"), bIncludeMissPointsInServerPayload);
    Root->SetNumberField(TEXT("previewPointStride"), PreviewPointStride);
    Root->SetNumberField(TEXT("maxPreviewPoints"), MaxPreviewPoints);
    Root->SetStringField(TEXT("previewBackend"), GetPreviewBackendName());
    Root->SetBoolField(TEXT("gpuPreviewBackendRequested"), IsGpuPreviewBackendRequested());
    Root->SetBoolField(TEXT("gpuPreviewBackendActive"), IsGpuPreviewBackendActive());

    TSharedRef<FJsonObject> PayloadPolicyObject = MakeShared<FJsonObject>();
    PayloadPolicyObject->SetNumberField(TEXT("stride"), ServerPayloadStride);
    PayloadPolicyObject->SetNumberField(TEXT("maxPoints"), MaxServerPayloadPoints);
    PayloadPolicyObject->SetBoolField(TEXT("includeMissPoints"), bIncludeMissPointsInServerPayload);
    PayloadPolicyObject->SetStringField(TEXT("pointSelection"), bIncludeMissPointsInServerPayload ? TEXT("hit_and_miss") : TEXT("hit_only"));
    Root->SetObjectField(TEXT("payloadPolicy"), PayloadPolicyObject);

    TSharedRef<FJsonObject> PreviewPolicyObject = MakeShared<FJsonObject>();
    PreviewPolicyObject->SetNumberField(TEXT("stride"), PreviewPointStride);
    PreviewPolicyObject->SetNumberField(TEXT("maxPoints"), MaxPreviewPoints);
    PreviewPolicyObject->SetBoolField(TEXT("hitOnly"), bPointCloudPreviewHitOnly);
    PreviewPolicyObject->SetStringField(TEXT("backend"), GetPreviewBackendName());
    PreviewPolicyObject->SetBoolField(TEXT("gpuRequested"), IsGpuPreviewBackendRequested());
    PreviewPolicyObject->SetBoolField(TEXT("gpuActive"), IsGpuPreviewBackendActive());
    PreviewPolicyObject->SetBoolField(TEXT("experimentalGpuOptIn"), bAllowExperimentalGpuPreviewBackend);
    PreviewPolicyObject->SetStringField(TEXT("activePath"), IsGpuPreviewBackendActive() ? TEXT("gpu") : TEXT("cpu_instanced_mesh_fallback"));
    Root->SetObjectField(TEXT("previewPolicy"), PreviewPolicyObject);

    TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    AddVectorArray(TransformObject, TEXT("location"), GetComponentLocation());
    const FRotator Rotation = GetComponentRotation();
    TArray<TSharedPtr<FJsonValue>> RotationJson;
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    TransformObject->SetArrayField(TEXT("rotation"), RotationJson);
    AddVectorArray(TransformObject, TEXT("forward"), GetForwardVector());
    AddVectorArray(TransformObject, TEXT("up"), GetUpVector());
    Root->SetObjectField(TEXT("sensorTransform"), TransformObject);

    if (bIncludeSlabAnalysisInPayload)
    {
        TSharedRef<FJsonObject> SlabObject = MakeShared<FJsonObject>();
        SlabObject->SetBoolField(TEXT("valid"), LastSlabAnalysis.bValid);
        SlabObject->SetNumberField(TEXT("slabHitPointCount"), LastSlabAnalysis.SlabHitPointCount);
        AddVectorArray(SlabObject, TEXT("boundsMin"), LastSlabAnalysis.BoundsMin);
        AddVectorArray(SlabObject, TEXT("boundsMax"), LastSlabAnalysis.BoundsMax);
        AddVectorArray(SlabObject, TEXT("center"), LastSlabAnalysis.Center);
        SlabObject->SetNumberField(TEXT("estimatedYawDegrees"), LastSlabAnalysis.EstimatedYawDegrees);
        SlabObject->SetNumberField(TEXT("referenceYawDegrees"), LastSlabAnalysis.ReferenceYawDegrees);
        SlabObject->SetNumberField(TEXT("angleDeviationDegrees"), LastSlabAnalysis.AngleDeviationDegrees);
        SlabObject->SetNumberField(TEXT("confidence"), LastSlabAnalysis.Confidence);
        SlabObject->SetStringField(TEXT("status"), LastSlabAnalysis.StatusMessage);
        Root->SetObjectField(TEXT("slabAnalysis"), SlabObject);
    }

    TArray<TSharedPtr<FJsonValue>> JsonPoints;
    const int32 SafeStride = FMath::Max(1, ServerPayloadStride);
    const int32 SafeMax = FMath::Max(0, MaxServerPayloadPoints);
    int32 Added = 0;
    for (int32 I = 0; I < Points.Num(); I += SafeStride)
    {
        if (SafeMax > 0 && Added >= SafeMax)
        {
            break;
        }

        const FVirtualLidarPoint& P = Points[I];
        if (!bIncludeMissPointsInServerPayload && !P.bHit)
        {
            continue;
        }

        const int32 DerivedRow = HorizontalSamples > 0 ? I / HorizontalSamples : 0;
        const int32 DerivedCol = HorizontalSamples > 0 ? I % HorizontalSamples : I;
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("pointIndex"), I);
        O->SetNumberField(TEXT("row"), P.bHasGridCoord ? P.Row : DerivedRow);
        O->SetNumberField(TEXT("col"), P.bHasGridCoord ? P.Col : DerivedCol);
        O->SetNumberField(TEXT("returnIndex"), P.ReturnIndex);
        O->SetBoolField(TEXT("gridCoordValid"), P.bHasGridCoord);
        O->SetStringField(TEXT("gridCoordSource"), P.bHasGridCoord ? TEXT("point_metadata") : TEXT("derived_from_point_index"));
        O->SetBoolField(TEXT("hit"), P.bHit);
        O->SetNumberField(TEXT("distance"), P.Distance);
        O->SetStringField(TEXT("hitActor"), P.HitActorName.ToString());
        O->SetStringField(TEXT("hitActorClass"), P.HitActorClassName.ToString());
        O->SetStringField(TEXT("semanticLabel"), P.SemanticLabel.ToString());

        TArray<TSharedPtr<FJsonValue>> Tags;
        for (const FName& T : P.HitActorTags)
        {
            Tags.Add(MakeShared<FJsonValueString>(T.ToString()));
        }
        O->SetArrayField(TEXT("hitActorTags"), Tags);
        AddVectorArray(O, TEXT("worldLocation"), P.WorldLocation);
        AddVectorArray(O, TEXT("localDirection"), P.LocalDirection);
        JsonPoints.Add(MakeShared<FJsonValueObject>(O));
        ++Added;
    }
    Root->SetNumberField(TEXT("payloadPointCount"), Added);
    Root->SetArrayField(TEXT("points"), JsonPoints);

    FString Out;
    TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root, W);
    return Out;
}

void UVirtualLidarScanComponent::WriteHeatmapPixel(TArray<uint8>& Pixels, int32 PixelIndex, const FVirtualLidarPoint& Point) const
{
    if (!Pixels.IsValidIndex(PixelIndex + 3)) return;
    const float N = FMath::Clamp(Point.Distance / FMath::Max(1.0f, MaxDistance), 0.0f, 1.0f); const uint8 I = Point.bHit ? (uint8)((1.0f - N) * 255.0f) : 0; FColor C;
    if (ViewMode == EVirtualLidarViewMode::HitMask) C = Point.bHit ? FColor::White : FColor::Black;
    else if (ViewMode == EVirtualLidarViewMode::DepthGradient) C = Point.bHit ? FColor(255, I, (uint8)(N * 255.0f), 255) : FColor::Black;
    else if (ViewMode == EVirtualLidarViewMode::ActorClassColor) C = Point.bHit ? ResolveSemanticColor(Point).ToFColor(true) : FColor(3, 8, 10, 255);
    else C = FColor(I, I, I, 255);
    Pixels[PixelIndex + 0] = C.B; Pixels[PixelIndex + 1] = C.G; Pixels[PixelIndex + 2] = C.R; Pixels[PixelIndex + 3] = 255;
}

bool UVirtualLidarScanComponent::ShouldIgnoreHitActor(const AActor* Actor) const { if (!Actor) return false; for (const FName& Tag : IgnoreActorTags) if (!Tag.IsNone() && Actor->ActorHasTag(Tag)) return true; return false; }
int32 UVirtualLidarScanComponent::GetHeatmapPixelIndex(int32 H, int32 V, int32 W, int32 Ht) const { const int32 DH = bFlipLidarViewHorizontal ? W - 1 - H : H; const int32 DV = bFlipLidarViewVertical ? Ht - 1 - V : V; return (DV * W + DH) * 4; }

void UVirtualLidarScanComponent::UpdateLidarViewTexture(const TArray<uint8>& Pixels)
{
    const int32 W = FMath::Max(1, HorizontalSamples); const int32 H = FMath::Max(1, VerticalChannels); if (Pixels.Num() < W * H * 4) return;
    if (!LidarViewTexture || LidarViewTexture->GetSizeX() != W || LidarViewTexture->GetSizeY() != H)
    {
        LidarViewTexture = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
        if (LidarViewTexture)
        {
            LidarViewTexture->SRGB = true;
            LidarViewTexture->CompressionSettings = TC_VectorDisplacementmap;
            LidarViewTexture->UpdateResource();
        }
    }
    if (!LidarViewTexture || !LidarViewTexture->GetResource()) return;
    uint8* UploadData = new uint8[Pixels.Num()];
    FMemory::Memcpy(UploadData, Pixels.GetData(), Pixels.Num());
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, W, H);
    LidarViewTexture->UpdateTextureRegions(0, 1, Region, W * 4, 4, UploadData,
        [Region](uint8* Data, const FUpdateTextureRegion2D*)
        {
            delete[] Data;
            delete Region;
        });
}

void UVirtualLidarScanComponent::UpdateRuntimeStatusAfterScan(int32 PayloadLength)
{
    const TArray<FVirtualLidarPoint>& LastPoints = GetLastPoints();
    RuntimeStatus.SensorId = SensorId;
    RuntimeStatus.SensorType = TEXT("virtual_lidar");
    RuntimeStatus.FrameId = FrameId;
    RuntimeStatus.LastUpdateUtc = FDateTime::UtcNow();
    RuntimeStatus.LastPayloadLength = PayloadLength;
    RuntimeStatus.TotalPointCount = LastPoints.Num();
    RuntimeStatus.HitPointCount = 0;
    RuntimeStatus.ServerPayloadPointCount = LastServerPayloadPointCount;
    RuntimeStatus.PreviewPointCount = LastPreviewPointCount;
    RuntimeStatus.PerformanceWarning = LastPerformanceWarning;
    RuntimeStatus.SlabAnalysis = LastSlabAnalysis;

    RuntimeStatus.HitPointCount = LastHitPointCount;

    RuntimeStatus.LastMessage = FString::Printf(
        TEXT("Quality=%d Rays=%d Hits=%d ServerPoints=%d ServerStride=%d MaxServer=%d Preview=%s PreviewPoints=%d PreviewStride=%d PreviewBackend=%s GpuActive=%s Slab=%s Angle=%.2f Dev=%.2f Conf=%.2f Warning=%s"),
        static_cast<int32>(SimulationQuality),
        HorizontalSamples * VerticalChannels,
        RuntimeStatus.HitPointCount,
        RuntimeStatus.ServerPayloadPointCount,
        ServerPayloadStride,
        MaxServerPayloadPoints,
        bPointCloudPreviewEnabled ? TEXT("On") : TEXT("Off"),
        RuntimeStatus.PreviewPointCount,
        PreviewPointStride,
        *GetPreviewBackendName(),
        IsGpuPreviewBackendActive() ? TEXT("true") : TEXT("false"),
        LastSlabAnalysis.bValid ? TEXT("Valid") : TEXT("Invalid"),
        LastSlabAnalysis.EstimatedYawDegrees,
        LastSlabAnalysis.AngleDeviationDegrees,
        LastSlabAnalysis.Confidence,
        LastPerformanceWarning.IsEmpty() ? TEXT("None") : *LastPerformanceWarning);
}

void UVirtualLidarScanComponent::ExportEnabledPointCloudFormats() const
{
    if (bExportCsvOnScan)
    {
        ExportLastPointCloudCsv();
    }
    if (bExportJsonLinesOnScan)
    {
        ExportLastPointCloudJsonLines();
    }
    if (bExportPcdOnScan)
    {
        ExportLastPointCloudPcd();
    }
}

void UVirtualLidarScanComponent::DispatchPayload(const FString& JsonPayload) const
{
    if (TransportComponent)
    {
        TransportComponent->SendJson(SensorId, TEXT("virtual_lidar"), JsonPayload);
        return;
    }

    if (OutputMode == EVirtualLidarOutputMode::LogOnly)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] Payload length=%d"), *SensorId, JsonPayload.Len());
    }
    else if (OutputMode == EVirtualLidarOutputMode::SaveJson)
    {
        SaveJsonToDisk(JsonPayload);
    }
    else if (OutputMode == EVirtualLidarOutputMode::HttpPost)
    {
        PostJson(JsonPayload);
    }
}
void UVirtualLidarScanComponent::PostJson(const FString& JsonPayload) const
{
    if (HttpEndpoint.IsEmpty()) return; TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest(); Request->SetURL(HttpEndpoint); Request->SetVerb(TEXT("POST")); Request->SetHeader(TEXT("Content-Type"), TEXT("application/json")); Request->SetContentAsString(JsonPayload); Request->ProcessRequest();
}
void UVirtualLidarScanComponent::SaveJsonToDisk(const FString& JsonPayload) const { const FString Path = BuildExportPath(TEXT("json"), SensorId); FFileHelper::SaveStringToFile(JsonPayload, *Path); }

FString UVirtualLidarScanComponent::BuildExportPath(const FString& Extension, const FString& FileNamePrefix) const
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud")); IFileManager::Get().MakeDirectory(*Dir, true); const FString Prefix = FileNamePrefix.IsEmpty() ? SensorId : FileNamePrefix; const FDateTime N = FDateTime::UtcNow(); const FString Ts = FString::Printf(TEXT("%s_%03d_%lld"), *N.ToString(TEXT("%Y%m%d_%H%M%S")), N.GetMillisecond(), N.GetTicks()); LastPointCloudExportPath = FPaths::Combine(Dir, FString::Printf(TEXT("%s_%s.%s"), *Prefix, *Ts, *Extension)); return LastPointCloudExportPath;
}
void UVirtualLidarScanComponent::RebuildLastPointStatistics()
{
    LastHitPointCount = 0;
    LastSemanticCounts.Reset();
    for (const FVirtualLidarPoint& Point : GetLastPoints())
    {
        if (!Point.bHit) continue;
        ++LastHitPointCount;
        LastSemanticCounts.FindOrAdd(Point.SemanticLabel.IsNone() ? TEXT("Unclassified") : Point.SemanticLabel.ToString())++;
    }
}

void UVirtualLidarScanComponent::CollectExportPoints(TArray<const FVirtualLidarPoint*>& Out) const { Out.Reset(); for (const FVirtualLidarPoint& P : GetLastPoints()) if (!bExportHitOnlyPointCloud || P.bHit) Out.Add(&P); }

void UVirtualLidarScanComponent::LogLastPointCloud(int32 MaxPointsToLog, bool bHitOnly) const
{
    const TArray<FVirtualLidarPoint>& LastPoints = GetLastPoints();
    int32 Logged = 0, Candidates = 0; UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PointCloud frame=%lld total=%d"), *SensorId, FrameId, LastPoints.Num());
    for (int32 Idx = 0; Idx < LastPoints.Num(); ++Idx) { const FVirtualLidarPoint& P = LastPoints[Idx]; if (bHitOnly && !P.bHit) continue; ++Candidates; if (MaxPointsToLog > 0 && Logged >= MaxPointsToLog) continue; UE_LOG(LogTemp, Warning, TEXT("[%d] x=%.3f y=%.3f z=%.3f d=%.3f hit=%d actor=%s class=%s label=%s tags=%s"), Idx, P.WorldLocation.X, P.WorldLocation.Y, P.WorldLocation.Z, P.Distance, P.bHit ? 1 : 0, *P.HitActorName.ToString(), *P.HitActorClassName.ToString(), *P.SemanticLabel.ToString(), *JoinNames(P.HitActorTags)); ++Logged; }
    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PointCloud log complete. candidates=%d logged=%d"), *SensorId, Candidates, Logged);
}

bool UVirtualLidarScanComponent::ExportLastPointCloudCsv(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> Points;
    CollectExportPoints(Points);
    FString Text = TEXT("x,y,z,distance,hit,sensor_x_m,sensor_y_m,sensor_z_m,range_mm,intensity,intensity_normalized,ring,horizontal_index,echo_index,echo_count,point_time_offset_ns,validity,confidence,actor,actor_class,semantic_label,tags\n");
    for (const FVirtualLidarPoint* P : Points)
    {
        if (!P) continue;
        Text += FString::Printf(
            TEXT("%f,%f,%f,%f,%d,%.9f,%.9f,%.9f,%d,%d,%.6f,%d,%d,%d,%d,%lld,%d,%.6f,%s,%s,%s,%s\n"),
            P->WorldLocation.X,
            P->WorldLocation.Y,
            P->WorldLocation.Z,
            P->Distance,
            P->bHit ? 1 : 0,
            P->SensorLocalPositionMeters.X,
            P->SensorLocalPositionMeters.Y,
            P->SensorLocalPositionMeters.Z,
            P->RangeMillimeters,
            P->RawIntensity,
            P->NormalizedIntensity,
            P->Ring,
            P->HorizontalIndex,
            P->EchoIndex,
            P->EchoCount,
            P->PointTimeOffsetNanoseconds,
            static_cast<int32>(P->Validity),
            P->Confidence,
            *P->HitActorName.ToString(),
            *P->HitActorClassName.ToString(),
            *P->SemanticLabel.ToString(),
            *JoinNames(P->HitActorTags));
    }
    const FString Path = BuildExportPath(TEXT("csv"), FileNamePrefix);
    const bool bFileSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] CSV saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] CSV failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarScanComponent::ExportLastPointCloudJsonLines(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> Points; CollectExportPoints(Points); FString Text;
    for (const FVirtualLidarPoint* P : Points)
    {
        if (!P) continue;
        Text += FString::Printf(
            TEXT("{\"x\":%f,\"y\":%f,\"z\":%f,\"distance\":%f,\"hit\":%s,\"sensorPositionMeters\":[%.9f,%.9f,%.9f],\"rangeMillimeters\":%d,\"intensity\":%d,\"intensityNormalized\":%.6f,\"ring\":%d,\"horizontalIndex\":%d,\"echoIndex\":%d,\"echoCount\":%d,\"pointTimeOffsetNanoseconds\":\"%lld\",\"validity\":%d,\"confidence\":%.6f,\"actor\":\"%s\",\"actorClass\":\"%s\",\"semanticLabel\":\"%s\",\"tags\":\"%s\"}\n"),
            P->WorldLocation.X,
            P->WorldLocation.Y,
            P->WorldLocation.Z,
            P->Distance,
            P->bHit ? TEXT("true") : TEXT("false"),
            P->SensorLocalPositionMeters.X,
            P->SensorLocalPositionMeters.Y,
            P->SensorLocalPositionMeters.Z,
            P->RangeMillimeters,
            P->RawIntensity,
            P->NormalizedIntensity,
            P->Ring,
            P->HorizontalIndex,
            P->EchoIndex,
            P->EchoCount,
            P->PointTimeOffsetNanoseconds,
            static_cast<int32>(P->Validity),
            P->Confidence,
            *P->HitActorName.ToString(),
            *P->HitActorClassName.ToString(),
            *P->SemanticLabel.ToString(),
            *JoinNames(P->HitActorTags));
    }
    const FString Path = BuildExportPath(TEXT("jsonl"), FileNamePrefix);
    const bool bFileSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] JSONL saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] JSONL failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarScanComponent::ExportLastPointCloudPcd(const FString& FileNamePrefix) const
{
    TArray<const FVirtualLidarPoint*> Points;
    CollectExportPoints(Points);
    FString Text = FString::Printf(
        TEXT("# .PCD v0.7\nVERSION 0.7\nFIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence\nSIZE 4 4 4 2 2 2 1 1 8 1 4\nTYPE F F F U U U U U I U F\nCOUNT 1 1 1 1 1 1 1 1 1 1 1\nWIDTH %d\nHEIGHT 1\nVIEWPOINT 0 0 0 1 0 0 0\nPOINTS %d\nDATA ascii\n"),
        Points.Num(),
        Points.Num());
    for (const FVirtualLidarPoint* P : Points)
    {
        if (!P) continue;
        Text += FString::Printf(
            TEXT("%.9f %.9f %.9f %d %d %d %d %d %lld %d %.6f\n"),
            P->SensorLocalPositionMeters.X,
            P->SensorLocalPositionMeters.Y,
            P->SensorLocalPositionMeters.Z,
            P->RawIntensity,
            P->Ring,
            P->HorizontalIndex,
            P->EchoIndex,
            P->EchoCount,
            P->PointTimeOffsetNanoseconds,
            static_cast<int32>(P->Validity),
            P->Confidence);
    }
    const FString Path = BuildExportPath(TEXT("pcd"), FileNamePrefix);
    const bool bFileSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] PCD saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] PCD failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarScanComponent::ExportLastPointCloudLas(const FString& FileNamePrefix) const
{
    return ExportLastPointCloudLasToPath(BuildExportPath(TEXT("las"), FileNamePrefix));
}

bool UVirtualLidarScanComponent::ExportLastPointCloudLasToPath(const FString& Path) const
{
    TArray<const FVirtualLidarPoint*> Points; CollectExportPoints(Points); if (Points.Num() <= 0) return false;
    FVector Min(FLT_MAX), Max(-FLT_MAX); for (const FVirtualLidarPoint* P : Points) if (P) { Min.X = FMath::Min(Min.X, P->WorldLocation.X); Min.Y = FMath::Min(Min.Y, P->WorldLocation.Y); Min.Z = FMath::Min(Min.Z, P->WorldLocation.Z); Max.X = FMath::Max(Max.X, P->WorldLocation.X); Max.Y = FMath::Max(Max.Y, P->WorldLocation.Y); Max.Z = FMath::Max(Max.Z, P->WorldLocation.Z); }
    const double Scale = 0.001, CmToM = 0.01, OX = Min.X * CmToM, OY = Min.Y * CmToM, OZ = Min.Z * CmToM; FBufferArchive A; const uint8 Sig[4] = {'L','A','S','F'}; WriteLasBytes(A, Sig, 4); WriteLasValue<uint16>(A,0); WriteLasValue<uint16>(A,0); WriteLasValue<uint32>(A,0); WriteLasValue<uint16>(A,0); WriteLasValue<uint16>(A,0); for(int32 i=0;i<8;++i) WriteLasValue<uint8>(A,0); WriteLasValue<uint8>(A,1); WriteLasValue<uint8>(A,2); WriteLasFixedString(A,"UE-DT-Project",32); WriteLasFixedString(A,"VirtualLidar",32); const FDateTime Now = FDateTime::Now(); WriteLasValue<uint16>(A,(uint16)Now.GetDayOfYear()); WriteLasValue<uint16>(A,(uint16)Now.GetYear()); WriteLasValue<uint16>(A,227); WriteLasValue<uint32>(A,227); WriteLasValue<uint32>(A,0); WriteLasValue<uint8>(A,0); WriteLasValue<uint16>(A,20); WriteLasValue<uint32>(A,(uint32)Points.Num()); WriteLasValue<uint32>(A,(uint32)Points.Num()); for(int32 i=1;i<5;++i) WriteLasValue<uint32>(A,0); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,Scale); WriteLasValue<double>(A,OX); WriteLasValue<double>(A,OY); WriteLasValue<double>(A,OZ); WriteLasValue<double>(A,Max.X*CmToM); WriteLasValue<double>(A,Min.X*CmToM); WriteLasValue<double>(A,Max.Y*CmToM); WriteLasValue<double>(A,Min.Y*CmToM); WriteLasValue<double>(A,Max.Z*CmToM); WriteLasValue<double>(A,Min.Z*CmToM);
    for (const FVirtualLidarPoint* P : Points) if (P) { const int32 X=(int32)FMath::RoundToDouble(((P->WorldLocation.X*CmToM)-OX)/Scale); const int32 Y=(int32)FMath::RoundToDouble(((P->WorldLocation.Y*CmToM)-OY)/Scale); const int32 Z=(int32)FMath::RoundToDouble(((P->WorldLocation.Z*CmToM)-OZ)/Scale); WriteLasValue<int32>(A,X); WriteLasValue<int32>(A,Y); WriteLasValue<int32>(A,Z); WriteLasValue<uint16>(A,static_cast<uint16>(FMath::Clamp(P->RawIntensity,0,65535))); const uint8 ReturnBits=static_cast<uint8>((FMath::Clamp(P->EchoIndex+1,1,7)&0x07)|((FMath::Clamp(P->EchoCount,1,7)&0x07)<<3)); WriteLasValue<uint8>(A,ReturnBits); WriteLasValue<uint8>(A,1); WriteLasValue<int8>(A,0); WriteLasValue<uint8>(A,0); WriteLasValue<uint16>(A,static_cast<uint16>(FMath::Clamp(P->Ring,0,65535))); }
    const bool bFileSaved = FFileHelper::SaveArrayToFile(A, *Path);
    if (bFileSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] LAS saved: %s points=%d"), *SensorId, *Path, Points.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] LAS failed: %s"), *SensorId, *Path);
    }
    return bFileSaved;
}

bool UVirtualLidarScanComponent::RunExternalLazCompressor(const FString& LasSourcePath, const FString& LazOutputPath) const
{
    bLastLazExternalCompressorAttempted = true;
    bLastLazExternalCompressorSucceeded = false;
    bLastLazExportPlaceholderOnly = false;
    LastLazLasSourcePath = FPaths::ConvertRelativePathToFull(LasSourcePath);
    LastLazOutputPath = FPaths::ConvertRelativePathToFull(LazOutputPath);

    if (!bUseExternalLazCompressor)
    {
        LastLazExportWarningText = TEXT("ExternalCompressorDisabled");
        LastLazExportStatusText = TEXT("ExternalCompressorDisabled");
        return false;
    }
    if (ExternalLazCompressorPath.IsEmpty() || !FPaths::FileExists(ExternalLazCompressorPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ compressor is enabled but executable is missing: %s"), *SensorId, *ExternalLazCompressorPath);
        LastLazExportWarningText = FString::Printf(TEXT("External LAZ compressor executable is missing: %s"), *ExternalLazCompressorPath);
        LastLazExportStatusText = FString::Printf(TEXT("ExternalCompressorMissing: %s"), *ExternalLazCompressorPath);
        return false;
    }

    FString Arguments = ExternalLazCompressorArguments.IsEmpty()
        ? TEXT("-i {input} -o {output}")
        : ExternalLazCompressorArguments;
    if (!Arguments.Contains(TEXT("{input}")) || !Arguments.Contains(TEXT("{output}")))
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ compressor arguments must include both {input} and {output}: %s"), *SensorId, *Arguments);
        LastLazExportWarningText = TEXT("External LAZ compressor arguments must include both {input} and {output}.");
        LastLazExportStatusText = TEXT("ExternalCompressorArgumentsMissingPlaceholders");
        return false;
    }
    FString FullLasSourcePath = FPaths::ConvertRelativePathToFull(LasSourcePath);
    FString FullLazOutputPath = FPaths::ConvertRelativePathToFull(LazOutputPath);
    FPaths::MakePlatformFilename(FullLasSourcePath);
    FPaths::MakePlatformFilename(FullLazOutputPath);
    if (FullLasSourcePath == FullLazOutputPath)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ output path must differ from LAS source path: %s"), *SensorId, *LazOutputPath);
        LastLazExportWarningText = TEXT("External LAZ output path must differ from LAS source path.");
        LastLazExportStatusText = TEXT("ExternalCompressorOutputPathMatchesSource");
        return false;
    }
    Arguments.ReplaceInline(TEXT("{input}"), *FString::Printf(TEXT("\"%s\""), *FullLasSourcePath));
    Arguments.ReplaceInline(TEXT("{output}"), *FString::Printf(TEXT("\"%s\""), *FullLazOutputPath));

    int32 ReturnCode = INDEX_NONE;
    FString StdOut;
    FString StdErr;
    UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] Running external LAZ compressor: %s %s"), *SensorId, *ExternalLazCompressorPath, *Arguments);
    const bool bProcessRan = FPlatformProcess::ExecProcess(*ExternalLazCompressorPath, *Arguments, &ReturnCode, &StdOut, &StdErr);
    const bool bOutputExists = FPaths::FileExists(FullLazOutputPath);
    const int64 OutputSize = bOutputExists ? IFileManager::Get().FileSize(*FullLazOutputPath) : 0;
    LastLazExternalCompressorReturnCode = ReturnCode;
    LastLazOutputSizeBytes = OutputSize;
    bLastLazProducedOutputFile = bOutputExists && OutputSize > 0;
    if (!bProcessRan || ReturnCode != 0 || !bOutputExists || OutputSize <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] External LAZ compressor failed. ran=%s code=%d outputExists=%s outputSize=%lld stdout=%s stderr=%s"), *SensorId, bProcessRan ? TEXT("true") : TEXT("false"), ReturnCode, bOutputExists ? TEXT("true") : TEXT("false"), OutputSize, *StdOut, *StdErr);
        LastLazExportWarningText = FString::Printf(TEXT("External LAZ compressor failed. ran=%s code=%d outputExists=%s outputSize=%lld"),
            bProcessRan ? TEXT("true") : TEXT("false"),
            ReturnCode,
            bOutputExists ? TEXT("true") : TEXT("false"),
            OutputSize);
        LastLazExportStatusText = FString::Printf(TEXT("ExternalCompressorFailed: ran=%s code=%d outputExists=%s outputSize=%lld"),
            bProcessRan ? TEXT("true") : TEXT("false"),
            ReturnCode,
            bOutputExists ? TEXT("true") : TEXT("false"),
            OutputSize);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] LAZ saved: %s source=%s"), *SensorId, *FullLazOutputPath, *FullLasSourcePath);
    bLastLazExternalCompressorSucceeded = true;
    bLastLazProducedOutputFile = true;
    bLastLazExportSucceeded = true;
    LastLazOutputPath = FullLazOutputPath;
    LastLazLasSourcePath = FullLasSourcePath;
    LastLazExportStatusText = FString::Printf(TEXT("ExternalCompressorSucceeded: output=%s source=%s"), *FullLazOutputPath, *FullLasSourcePath);
    return true;
}

bool UVirtualLidarScanComponent::ExportLastPointCloudLaz(const FString& FileNamePrefix) const
{
    ResetLastLazExportStatus(TEXT("NotStarted"));
    bLastLazExportAttempted = true;
    bLastLazExternalCompressorRequested = bUseExternalLazCompressor;
    TArray<const FVirtualLidarPoint*> ExportPoints;
    CollectExportPoints(ExportPoints);
    LastLazExportedPointCount = ExportPoints.Num();
    const FString Prefix = FileNamePrefix.IsEmpty() ? TEXT("laz_source") : FileNamePrefix + TEXT("_laz_source");
    const FString LasSourcePath = BuildExportPath(TEXT("las"), Prefix);
    LastLazLasSourcePath = FPaths::ConvertRelativePathToFull(LasSourcePath);
    if (!ExportLastPointCloudLasToPath(LasSourcePath))
    {
        LastLazExportWarningText = TEXT("LAS source write failed before LAZ compression could run.");
        LastLazExportStatusText = TEXT("LasSourceWriteFailed");
        return false;
    }

    if (!bUseExternalLazCompressor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] LAZ compression is not integrated. Writing LAS-compatible source file with prefix '%s'."), *SensorId, *Prefix);
        bLastLazExportPlaceholderOnly = true;
        bLastLazExportSucceeded = true;
        LastLazExportWarningText = TEXT("LAZ compression is not integrated; wrote LAS-compatible source only.");
        LastLazExportStatusText = FString::Printf(TEXT("PlaceholderOnlyLasSource: %s"), *LastLazLasSourcePath);
        return true;
    }

    const FString LazOutputPath = BuildExportPath(TEXT("laz"), FileNamePrefix);
    return RunExternalLazCompressor(LasSourcePath, LazOutputPath);
}
bool UVirtualLidarScanComponent::ExportLastPointCloudCsvLasLaz(const FString& FileNamePrefix) const { const bool A = ExportLastPointCloudCsv(FileNamePrefix); const bool B = ExportLastPointCloudLas(FileNamePrefix); const bool C = ExportLastPointCloudLaz(FileNamePrefix); return A || B || C; }

void UVirtualLidarScanComponent::ResetLastLazExportStatus(const FString& StatusText) const
{
    LastLazExportStatusText = StatusText;
    LastLazExportWarningText.Reset();
    LastLazLasSourcePath.Reset();
    LastLazOutputPath.Reset();
    bLastLazExportAttempted = false;
    bLastLazExportSucceeded = false;
    bLastLazExportPlaceholderOnly = false;
    bLastLazExternalCompressorRequested = false;
    bLastLazExternalCompressorAttempted = false;
    bLastLazExternalCompressorSucceeded = false;
    bLastLazProducedOutputFile = false;
    bLastLazTrueCompressionValidated = false;
    LastLazExportedPointCount = 0;
    LastLazExternalCompressorReturnCode = INDEX_NONE;
    LastLazOutputSizeBytes = 0;
}

UInstancedStaticMeshComponent* UVirtualLidarScanComponent::EnsurePointCloudPreviewComponent()
{
    if (!PointCloudPreviewComponent)
    {
        UObject* PreviewOuter = GetOwner() ? Cast<UObject>(GetOwner()) : Cast<UObject>(this);
        PointCloudPreviewComponent = NewObject<UInstancedStaticMeshComponent>(PreviewOuter, TEXT("VirtualLidarPointCloudPreview"));
        if (PointCloudPreviewComponent) { PointCloudPreviewComponent->SetupAttachment(this); PointCloudPreviewComponent->NumCustomDataFloats = 4; PointCloudPreviewComponent->SetCastShadow(false); PointCloudPreviewComponent->RegisterComponent(); if (!PointCloudPreviewMesh) PointCloudPreviewMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))); if (PointCloudPreviewMesh) PointCloudPreviewComponent->SetStaticMesh(PointCloudPreviewMesh); if (!PointCloudPreviewMaterial && GEngine) PointCloudPreviewMaterial = GEngine->DebugMeshMaterial; ApplyPointCloudPreviewStyle(); }
    }
    return PointCloudPreviewComponent;
}

void UVirtualLidarScanComponent::RefreshPointCloudPreview()
{
    UWorld* World = GetWorld(); if (!bPointCloudPreviewEnabled || !World) { ClearPointCloudPreview(); return; }
    const TArray<FVirtualLidarPoint>& LastPoints = GetLastPoints();
    UInstancedStaticMeshComponent* Comp = EnsurePointCloudPreviewComponent(); if (!Comp) return; Comp->SetHiddenInGame(false); Comp->SetVisibility(true, true);
    const int32 Stride = FMath::Max(1, PreviewPointStride); int32 Added = 0;
    const int32 PreviewCapacity = MaxPreviewPoints > 0 ? FMath::Min(MaxPreviewPoints, LastPoints.Num()) : LastPoints.Num();
    const int32 DebugPointStride = FMath::Max(1, FMath::DivideAndRoundUp(PreviewCapacity, 512));
    TArray<FTransform> InstanceTransforms;
    InstanceTransforms.Reserve(PreviewCapacity);
    const AVirtualLidarSensorActor* SensorOwner = Cast<AVirtualLidarSensorActor>(GetOwner());
    const UVirtualLidarVisualizationComponent* Visualizer = SensorOwner ? SensorOwner->VisualizationComponent : nullptr;
    auto DisplayColor = [&](const FVirtualLidarPoint& P) { return Visualizer ? Visualizer->GetPointDisplayColor(P) : ((bUseSemanticColorInPointCloudPreview && P.bHit) ? ResolveSemanticColor(P) : PointCloudPreviewColor); };
    for (int32 I = 0; I < LastPoints.Num(); I += Stride)
    {
        const FVirtualLidarPoint& P = LastPoints[I]; if (bPointCloudPreviewHitOnly && !P.bHit) continue; if (MaxPreviewPoints > 0 && Added >= MaxPreviewPoints) break;
        InstanceTransforms.Add(FTransform(FRotator::ZeroRotator, P.WorldLocation, FVector(PointCloudPreviewPointScale)));
        if (bDrawPointCloudPreviewDebugPoints && Added % DebugPointStride == 0) DrawDebugPoint(World, P.WorldLocation, PointCloudPreviewDebugPointSize, DisplayColor(P).ToFColor(true), false, FMath::Max(ScanInterval, 0.2f));
        ++Added;
    }
    const int32 ExistingCount = Comp->GetInstanceCount();
    const int32 NewCount = InstanceTransforms.Num();
    const int32 CommonCount = FMath::Min(ExistingCount, NewCount);
    if (CommonCount > 0)
    {
        // LastPoints stores world-space hit locations. Treating these as local-space
        // transforms applies the sensor transform twice and moves the cloud out of view.
        Comp->BatchUpdateInstancesTransforms(0, TArrayView<const FTransform>(InstanceTransforms.GetData(), CommonCount), true, false, true);
    }
    if (NewCount > ExistingCount)
    {
        TArray<FTransform> AddedTransforms;
        AddedTransforms.Append(InstanceTransforms.GetData() + ExistingCount, NewCount - ExistingCount);
        Comp->AddInstances(AddedTransforms, true, true);
    }
    else if (ExistingCount > NewCount)
    {
        TArray<int32> RemovedIndices;
        RemovedIndices.Reserve(ExistingCount - NewCount);
        for (int32 Index = ExistingCount - 1; Index >= NewCount; --Index) RemovedIndices.Add(Index);
        Comp->RemoveInstances(RemovedIndices, true);
    }
    for (int32 InstanceIndex = 0, PointIndex = 0; InstanceIndex < NewCount && PointIndex < LastPoints.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = LastPoints[PointIndex];
        if (PointIndex % Stride != 0 || (bPointCloudPreviewHitOnly && !Point.bHit)) continue;
        const FLinearColor Color = DisplayColor(Point);
        Comp->SetCustomDataValue(InstanceIndex, 0, Color.R, false);
        Comp->SetCustomDataValue(InstanceIndex, 1, Color.G, false);
        Comp->SetCustomDataValue(InstanceIndex, 2, Color.B, false);
        Comp->SetCustomDataValue(InstanceIndex, 3, Color.A, InstanceIndex == NewCount - 1);
        ++InstanceIndex;
    }
    Comp->MarkRenderStateDirty();
}

void UVirtualLidarScanComponent::TryAutoRegisterToManager()
{
    if (!bAutoRegisterToManager || !GetWorld()) return; for (TActorIterator<AVirtualSensorCoordinator> It(GetWorld()); It; ++It) { if (*It) { It->RegisterLidar(this); return; } }
}
