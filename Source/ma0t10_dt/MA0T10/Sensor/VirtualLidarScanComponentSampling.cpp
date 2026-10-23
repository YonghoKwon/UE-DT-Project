#include "VirtualLidarScanComponent.h"
#include "HAL/IConsoleManager.h"
#include "IndustrialLidarProfiles.h"
#include "VirtualLidarSemanticDiagnostics.h"
#include "Dom/JsonObject.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"

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
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Misc/Crc.h"
#include <atomic>

namespace
{
TAutoConsoleVariable<int32> CVarLidarPhaseSpread(TEXT("ma0t10.LidarPhaseSpread"),1,
    TEXT("Even automatic same-rate LiDAR phases;0 retains hashed phases. Restart scans after changing."));
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

int64 UVirtualLidarScanComponent::CalculatePointTimeOffsetNanoseconds(int32 RayIndex, int32 RayCount) const
{
    if (!bUseRollingPointTimestamps || RayCount <= 1)
    {
        return 0;
    }
    const int32 Width = FMath::Max(1, HorizontalSamples);
    const int32 Height = FMath::Max(1, RayCount / Width);
    return VirtualLidarFidelity::PointOffset(DeviceProfile, RayIndex, Width, Height, ScanInterval);
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
    if(const auto* Frozen=HardwareSurfaceIdentities.Find(const_cast<UPrimitiveComponent*>(Primitive)))return Frozen->Surface;
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
    if (bEnableMaterialResponse)
    {
        const auto Surface = VirtualLidarFidelity::ResolveSurface(Primitive);
        Resolved.Reflectivity = Surface.Reflectivity;
        Resolved.IntensityGain = Surface.IntensityGain;
        Resolved.DetectionProbabilityScale = Surface.DetectionProbabilityScale;
        Resolved.bTwoSided = Surface.bTwoSided;
        Resolved.bAllowSaturation = Surface.bAllowSaturation;
    }
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
    float ModelRange = Hit.Distance;
    float ModelIncidence = SignedIncidence;
    if (bEnableMotionDistortion && bUseRollingPointTimestamps
        && !ResolveRollingHit(Hit, SafeDirection, AcquisitionTransform, Point.PointTimeOffsetNanoseconds * 1.e-9, ModelRange, ModelIncidence))
        return false;
    if (bEnableMotionDistortion && bUseRollingPointTimestamps && (ModelRange > MaxDistance || ModelRange < DeviceSpec.MinRangeCm)) return false;
    return CapturePointModel().Apply(Point,Surface,SafeDirection,AcquisitionTransform,ModelRange,ModelIncidence,RayIndex,ReturnIndex);
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

void UVirtualLidarScanComponent::StartScan()
{
    if (!GetWorld() || ScanInterval <= 0.0f) return;
    GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    AutomaticCadenceStartedAt=FPlatformTime::Seconds();bAutomaticCadencePhasePending=true;
    bAutomaticCadencePhaseEnabled=true;
    ++AutomaticCadenceGeneration;
    ScheduledCadence.Start(AutomaticCadenceStartedAt, ScanInterval, (GetTypeHash(SensorId) % 1000) / 1000.0);
    bSingleScheduledScanRequested = false;
    if (!bRegisteredWithPerformanceSubsystem)
    {
        RuntimeStatus.LastAcquisitionProgressWorldSeconds = -1.0;
        RuntimeStatus.MeasuredAcquisitionRateHz = 0.0f;
        LastScheduledCompletionTime = -1.0;
    }
    RegisterWithPerformanceSubsystem();
}

void UVirtualLidarScanComponent::RequestImmediateScheduledScan()
{
	if (IsScanRunning() && GetWorld())
	{
		ScheduledCadence.RequestImmediate(FPlatformTime::Seconds());
		bAutomaticCadencePhasePending=false; // Explicit immediate/one-shot keeps its own grid.
        bAutomaticCadencePhaseEnabled=false;
	}
}

void UVirtualLidarScanComponent::BindGpuDepthProjectionComponent(
    UVirtualLidarGpuDepthProjectionComponent* InComponent)
{
    GpuDepthProjectionComponent = InComponent;
}

void UVirtualLidarScanComponent::ConvertGpuDepthFrame(const FVirtualLidarDepthAcquisitionFrame& Frame)
{
	RuntimeStatus.AcquisitionBackendMessage = Frame.SemanticStatus;
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
    TMap<int32, int32> SemanticIdCounts;
    if (bEnableMaterialResponse || (bEnableMotionDistortion && bUseRollingPointTimestamps))
    {
        FidelityDepthTangents.X = FMath::Tan(FMath::DegreesToRadians(Frame.Request.HorizontalFovDegrees * .5));
        FidelityDepthTangents.Y = FidelityDepthTangents.X * double(Frame.CaptureHeight) / FMath::Max(1,Frame.CaptureWidth);
    }

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

            FVirtualLidarPoint& Point = ScheduledPoints.AddDefaulted_GetRef();
            InitializePhysicalPoint(Point, Ring, Column, RayIndex, TotalRays, LocalDirection);
            Point.Distance = MaxDistance;
            Point.WorldLocation = ScheduledScanTransform.GetLocation() + WorldDirection * MaxDistance;

            const bool bDepthValid = FMath::IsFinite(RadialDistanceCm)
                && ForwardDepthCm > FMath::Max(0.1f, DeviceSpec.MinRangeCm)
                && RadialDistanceCm <= MaxDistance;
            if (bDepthValid && (bEnableMaterialResponse || (bEnableMotionDistortion && bUseRollingPointTimestamps)))
            {
                ApplyGpuFidelityPoint(Point, Frame, DepthIndex, RayIndex, RadialDistanceCm);
                if (Point.bHit)
                {
                    ++ScheduledHitPointCount;
                    if (!Point.SemanticLabel.IsNone()) ++SemanticIdCounts.FindOrAdd(FMath::RoundToInt(Frame.SemanticIdDepth[DepthIndex].R));
                }
            }
            else if (bDepthValid)
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
                    if (Frame.SemanticIdDepth.IsValidIndex(DepthIndex))
                    {
                        const FLinearColor IdDepth = Frame.SemanticIdDepth[DepthIndex];
                        // ID and depth come from one proxy capture. An unsupported occluder
                        // must never inherit the identity of a different surface behind it.
                        if (FMath::IsFinite(IdDepth.R) && FMath::IsFinite(IdDepth.A) && FMath::Abs(IdDepth.A - ForwardDepthCm) <= 1.0f)
                        {
                            if (const auto* Identity = Frame.SemanticIdentities.Find(FMath::RoundToInt(IdDepth.R)))
                            {
                                Point.SemanticLabel = Identity->Label;
                                Point.HitActorName = Identity->ActorName;
                                Point.HitActorClassName = Identity->ActorClass;
                                Point.HitActorTags = Identity->ActorTags;
                                if(Identity->bHasOpticalOverride)
                                {
                                    // Values were copied with the existing ID/depth acquisition, never read from the live Actor at completion.
                                    Point.SurfaceReflectivity=Identity->Reflectivity940Nm;
                                    Point.NormalizedIntensity=FMath::Clamp(Identity->Reflectivity940Nm*Attenuation*5.f*Identity->IntensityGain,0.f,1.f);
                                    Point.RawIntensity=FMath::RoundToInt(Point.NormalizedIntensity*65535.f);
                                    Point.Validity=Point.NormalizedIntensity>=1.f?EVirtualLidarPointValidity::Saturated:EVirtualLidarPointValidity::Valid;
                                }
                                ++SemanticIdCounts.FindOrAdd(FMath::RoundToInt(IdDepth.R));
                            }
                        }
                    }
                    ++ScheduledHitPointCount;
                }
                else
                {
                    Point.Validity = EVirtualLidarPointValidity::BelowSignalThreshold;
                    Point.Confidence = DetectionProbability;
                }
            }
            if (bScheduledGenerateHeatmap)
            {
                WriteHeatmapPixel(
                    ScheduledHeatmapPixels,
                    GetHeatmapPixelIndex(Column, Ring, ScheduledScanWidth, ScheduledScanHeight),
                    Point);
            }
        }
    }
    // Convert names once per surface, not once per 32,256-point scan element.
    for (const auto& Count : SemanticIdCounts)
        if (const auto* Identity = Frame.SemanticIdentities.Find(Count.Key))
            ScheduledSemanticCounts.FindOrAdd(Identity->Label.ToString()) += Count.Value;
    if(Frame.Request.bRequestSemantics&&!bDepthSemanticDiagnosticIssued&&VirtualLidarSemanticDiagnostics::IsEnabled())
    {
        bDepthSemanticDiagnosticIssued=true;
        auto Report=VirtualLidarSemanticDiagnostics::BuildDepthReport(Frame,GpuDepthPixelIndices,ScheduledPoints);
        Report->SetBoolField(TEXT("semantic_classification_enabled"),bEnableSemanticClassification);
        Report->SetBoolField(TEXT("dataset_semantic_demand"),bDatasetSemanticDemand);
        Report->SetNumberField(TEXT("requested_backend"),int32(AcquisitionBackend));
        Report->SetNumberField(TEXT("active_backend"),int32(ActiveAcquisitionBackend));
        VirtualLidarSemanticDiagnostics::Persist(this,SensorId,FrameId+1,ScheduledGeneration,
            TEXT("gpu_depth_projection"),TEXT("converted"),Report);
    }
}

void UVirtualLidarScanComponent::EnsureGpuDepthBeamLookup(const FVirtualLidarDepthAcquisitionFrame& Frame)
{
    const uint32 CalibrationHash = HashCombine(
        FCrc::MemCrc32(Frame.Request.HorizontalAngles.GetData(), Frame.Request.HorizontalAngles.Num() * sizeof(float)),
        FCrc::MemCrc32(Frame.Request.VerticalAngles.GetData(), Frame.Request.VerticalAngles.Num() * sizeof(float)));
    const bool bLookupMatches =
        GpuDepthLookupCalibrationHash == CalibrationHash
        && GpuDepthLookupHorizontalSamples == ScheduledScanWidth
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
    if (Frame.Request.HorizontalAngles.Num() == ScheduledScanWidth) HorizontalAngles = Frame.Request.HorizontalAngles;
    if (Frame.Request.VerticalAngles.Num() == ScheduledScanHeight) VerticalAngles = Frame.Request.VerticalAngles;
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
    GpuDepthLookupCalibrationHash = CalibrationHash;
    GpuDepthLookupVerticalChannels = ScheduledScanHeight;
    GpuDepthLookupCaptureWidth = Frame.CaptureWidth;
    GpuDepthLookupCaptureHeight = Frame.CaptureHeight;
    GpuDepthLookupHorizontalFov = Frame.Request.HorizontalFovDegrees;
    GpuDepthLookupMinVerticalAngle = Frame.Request.MinVerticalAngleDegrees;
    GpuDepthLookupMaxVerticalAngle = Frame.Request.MaxVerticalAngleDegrees;
    GpuDepthLookupHorizontalCalibrationCount = HorizontalCalibrationAnglesDegrees.Num();
    GpuDepthLookupVerticalCalibrationCount = VerticalCalibrationAnglesDegrees.Num();
}

bool UVirtualLidarScanComponent::UsesAutomaticCadencePhase() const
{
    return IsScanRunning() && bAutomaticCadencePhaseEnabled && !bSingleScheduledScanRequested
        && CVarLidarPhaseSpread.GetValueOnGameThread()!=0 && FMath::IsFinite(ScanInterval) && ScanInterval>0;
}

void UVirtualLidarScanComponent::ApplyAutomaticCadencePhase(double WallSeconds, double Phase, bool bInitial)
{
    const double Interval=FMath::Max(.001,double(ScanInterval));
    if(bInitial) ScheduledCadence.StartAligned(AutomaticCadenceStartedAt,Interval,Phase);
    else ScheduledCadence.RephaseNextSlot(WallSeconds,Interval,Phase);
    bAutomaticCadencePhasePending=false;
}

void UVirtualLidarScanComponent::PrepareScheduledScan(double NowSeconds)
{
    TryStartPendingPayloadBuild();
#if WITH_DEV_AUTOMATION_TESTS
    if(FreezeAcquisitionForTests)return;
#endif
    if (!IsScanRunning()) return;
    const double WallSeconds = FPlatformTime::Seconds();
    if (GetWorld() && GetWorld()->IsPaused()) { ScheduledCadence.Suspend(WallSeconds); return; }
    const double SafeInterval = FMath::Max(0.001, static_cast<double>(ScanInterval));
    if(bAutomaticCadencePhasePending)
    {
        if(auto* Scheduler=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>():nullptr)
            Scheduler->RefreshLidarCadencePhases(WallSeconds);
        bAutomaticCadencePhasePending=false;
    }
    RuntimeStatus.RequestedAcquisitionRateHz = static_cast<float>(1.0 / SafeInterval);
    RuntimeStatus.RequestedAcquisitionBackend = AcquisitionBackend == EVirtualLidarAcquisitionBackend::Auto
        ? TEXT("auto")
        : (AcquisitionBackend == EVirtualLidarAcquisitionBackend::GpuDepthProjection
            ? TEXT("gpu_depth_projection")
            : (AcquisitionBackend == EVirtualLidarAcquisitionBackend::HardwareRayTracing
                ? TEXT("hardware_ray_tracing")
                : TEXT("accurate_cpu_trace")));
    const bool bAcquisitionInProgress = bScheduledScanInProgress || bGpuDepthScanInProgress;
#if WITH_DEV_AUTOMATION_TESTS
    const double TraceDue=ScheduledCadence.GetNextDue();
    if(bTimingTraceForTests&&bAcquisitionInProgress&&WallSeconds>=TraceDue&&ActiveTimingSampleForTests.LastBusyFrame!=GFrameCounter)
    {++ActiveTimingSampleForTests.DueBusyFrames;ActiveTimingSampleForTests.LastBusyFrame=GFrameCounter;}
#endif
    const auto Decision = ScheduledCadence.Poll(WallSeconds, SafeInterval, GFrameCounter, bAcquisitionInProgress);
    // Keep the original per-active-acquisition semantics, independently of how
    // many slots expire (including repeated pre/post completion polls).
    if (Decision.bDelayedAcquisitionEvent)
    {
        RuntimeStatus.BudgetSkippedAcquisitionFrameCount += RuntimeStatus.BudgetSkippedAcquisitionFrameCount < MAX_int32;
        RuntimeStatus.DeadlineMissCount += RuntimeStatus.DeadlineMissCount < MAX_int32;
    }
    RuntimeStatus.ExpiredCadenceSlotCount += static_cast<int64>(FMath::Min<uint64>(
        Decision.SkippedSlots, MAX_int64 - RuntimeStatus.ExpiredCadenceSlotCount));
    if (Decision.bAdmit)
    {
#if WITH_DEV_AUTOMATION_TESTS
        if(bTimingTraceForTests){ActiveTimingSampleForTests=FVirtualLidarTimingSample();ActiveTimingSampleForTests.FrameId=FrameId+1;ActiveTimingSampleForTests.Due=TraceDue;ActiveTimingSampleForTests.Admitted=WallSeconds;ActiveTimingSampleForTests.AdmissionFrame=GFrameCounter;}
#endif
        if (bSingleScheduledScanRequested)
        {
            ScheduledCadence.HoldAdmissions();
            bSingleScheduledScanRequested = false;
        }
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

int32 UVirtualLidarScanComponent::ProcessScheduledScanChunk(int32 MaxRays)
{
    if (bGpuDepthScanInProgress)
    {
        return ProcessGpuDepthScan();
    }
    if (!bScheduledScanInProgress || MaxRays <= 0 || !GetWorld()) return 0;
    if(ScheduledRayQuery&&!PollSceneQueryBatch())return 0;
    {const int32 Completed=CompleteHardwareNormalizedFrame();if(Completed>=0)return Completed;}
    const double SceneWorkStarted=ScheduledRayQuery?FPlatformTime::Seconds():0;
    UWorld* World = GetWorld();
    const int32 TotalRays = ScheduledScanWidth * ScheduledScanHeight;
    const int32 EndRay = FMath::Min(ScheduledRayQuery?SceneQueryReadyRays():TotalRays, ScheduledNextRayIndex + FMath::Clamp(MaxRays, 128, 1024));
    const FVector Origin = ScheduledScanTransform.GetLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VirtualLidarScheduledSensor), false, GetOwner());
    const bool bProfileMultiEcho = bUseProfileEchoCapability && DeviceSpec.MaxEchoesPerPixel > 1;
    const bool bEffectiveMultiHit = bUseMultiHit || bProfileMultiEcho;
    const int32 EffectiveMaxHits = bProfileMultiEcho
        ? FMath::Clamp(DeviceSpec.MaxEchoesPerPixel, 1, 3)
        : FMath::Max(1, MaxHitsPerRay);

    for (int32 RayIndex = ScheduledNextRayIndex; RayIndex < EndRay; ++RayIndex)
    {
        const int32 V = RayIndex / ScheduledScanWidth;
        const int32 X = RayIndex % ScheduledScanWidth;
        const float Pitch = ScheduledVerticalAnglesDegrees[V];
        const float Yaw = ScheduledHorizontalAnglesDegrees[X];
        const FVector LocalDirection=bSceneQueryDirectionsCached?FVector(SceneQueryDirections[RayIndex]):IndustrialLidar::Beam(DeviceProfile,RayIndex,ScheduledScanWidth,ScheduledScanHeight,FrameId+1,Pitch,Yaw).Vector();
        const FVector Direction = ScheduledScanTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
        const float BeamRange=DeviceProfile==EVirtualLidarDeviceProfile::HesaiXT32&&(V<8||V>=24)?FMath::Min(MaxDistance,5000.f):MaxDistance;
        const FVector End = Origin + Direction * BeamRange;
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
            auto& Hits=SceneQueryHits;Hits.Reset();
            if(ScheduledRayQuery){FHitResult Hit;if(ResolveSceneQueryRay(RayIndex,Origin,End,Hit))Hits.Add(Hit);}else World->LineTraceMultiByChannel(Hits, Origin, End, TraceChannel, Params);
            int32 Added = 0;
            const int32 EchoStartIndex = ScheduledPoints.Num();
            for (const FHitResult& Hit : Hits)
            {
                if (ShouldIgnoreScheduledHit(Hit)) continue;
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
            bool bHit = ScheduledRayQuery?ResolveSceneQueryRay(RayIndex,Origin,End,Hit):World->LineTraceSingleByChannel(Hit, Origin, End, TraceChannel, Params);
            if (bHit && ShouldIgnoreScheduledHit(Hit)) bHit = false;
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
    if(ScheduledRayQuery)RecordSceneQueryWork(FPlatformTime::Seconds()-SceneWorkStarted,ScheduledNextRayIndex>=TotalRays);
    if (ScheduledNextRayIndex >= TotalRays) CompleteScheduledScan(GetWorld()->GetTimeSeconds());
    return Processed;
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
        ? FMath::Clamp(DeviceSpec.MaxEchoesPerPixel, 1, 3)
        : FMath::Max(1, MaxHitsPerRay);
    OutPoints.Reserve(TotalRays * (bEffectiveMultiHit ? EffectiveMaxHits : 1));
    OutHeatmapPixels.SetNumZeroed(TotalRays * 4);

    UWorld* World = GetWorld();
    if (!World) return;
    const FTransform AcquisitionTransform = GetComponentTransform();
    BeginFidelityAcquisition(true);
    const FVector Origin = AcquisitionTransform.GetLocation();
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
            const FVector Direction = AcquisitionTransform.TransformVectorNoScale(IndustrialLidar::Beam(DeviceProfile,RayIndex,W,H,FrameId+1,Pitch,Yaw).Vector()).GetSafeNormal();
            const float BeamRange=DeviceProfile==EVirtualLidarDeviceProfile::HesaiXT32&&(V<8||V>=24)?FMath::Min(MaxDistance,5000.f):MaxDistance;
        const FVector End = Origin + Direction * BeamRange;
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
int32 UVirtualLidarScanComponent::GetHeatmapPixelIndex(int32 H, int32 V, int32 W, int32 Ht) const { const int32 DH = bFlipLidarViewHorizontal ? W - 1 - H : H; const int32 DV = bFlipLidarViewVertical ? Ht - 1 - V : V; return (DV * W + DH) * 4; }

VirtualLidarPointModel::FConfig UVirtualLidarScanComponent::CapturePointModel() const
{
    VirtualLidarPointModel::FConfig C;C.MaxDistance=MaxDistance;C.AmbientLightLux=AmbientLightLux;
    C.RangeNoiseStandardDeviationMillimeters=RangeNoiseStandardDeviationMillimeters;C.MaximumRangeErrorMillimeters=MaximumRangeErrorMillimeters;
    C.bEnableMaterialResponse=bEnableMaterialResponse;C.bEnableRangeNoise=bEnableRangeNoise;C.FidelityMode=FidelityMode;
    C.RandomSequence=DatasetNoiseSequence?HashCombineFast(GetTypeHash(DatasetNoiseSequence->Key),GetTypeHash(DatasetNoiseSequence->Value)):HashCombineFast(GetTypeHash(DeterministicNoiseSeed),GetTypeHash(FrameId+1));
    return C;
}
