#include "VirtualLidarScanComponent.h"
#include "IndustrialLidarProfiles.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHitchDiagnostics.h"
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

template <typename T> void WriteLasValue(FBufferArchive& A, const T& V) { A.Serialize(const_cast<T*>(&V), sizeof(T)); }
void WriteLasFixedString(FBufferArchive& A, const ANSICHAR* Text, int32 Len) { TArray<ANSICHAR> B; B.SetNumZeroed(Len); if (Text) { FCStringAnsi::Strncpy(B.GetData(), Text, Len); } A.Serialize(B.GetData(), Len); }
void WriteLasBytes(FBufferArchive& A, const uint8* Bytes, int32 Len) { A.Serialize(const_cast<uint8*>(Bytes), Len); }
FString JoinNames(const TArray<FName>& Names) { FString R; for (int32 I = 0; I < Names.Num(); ++I) { if (I > 0) R += TEXT("|"); R += Names[I].ToString(); } return R; }
}


#if WITH_EDITOR
void UVirtualLidarScanComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    FVirtualSensorHitchScope Scope(EVirtualSensorHitchSection::EditorProperty);
    Super::PostEditChangeProperty(Event);
}
#endif
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
    CompatibilityConsumers.Reset();
    ++ScheduledGeneration;
    ScheduledPoints.Reset();
    ScheduledHitPointCount = 0;
    ScheduledSemanticCounts.Reset();
    ScheduledHeatmapPixels.Reset();
    ClearPointCloudPreview();
    if (PointCloudPreviewComponent) { PointCloudPreviewComponent->DestroyComponent(); PointCloudPreviewComponent = nullptr; }
    Super::EndPlay(EndPlayReason);
}

void UVirtualLidarScanComponent::CompleteScheduledScan(double NowSeconds)
{
    FVirtualSensorHitchScope HitchScope(EVirtualSensorHitchSection::SensorComplete);
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
    RuntimeStatus.LastAcquisitionProgressWorldSeconds = NowSeconds;
    LastScheduledCompletionTime = NowSeconds;
	UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
	OnFrameAcquired.Broadcast(FrameId);
	OnFrameAcquiredNative.Broadcast(FrameId);
	if (bInteractivePreviewMode && bSuppressInteractiveDerivedOutput)
	{
		RuntimeStatus.LastPostProcessDurationMs = 0.0f;
		UpdateRuntimeStatusAfterScan(0);
        if(CompatibilityOutputPolicy==EVirtualLidarCompatibilityOutputPolicy::LegacyAlways)OnScanCompleted.Broadcast(FString(), LidarViewTexture);
		return;
	}
    for(auto It=CompatibilityConsumers.CreateIterator();It;++It)if(!It.Value().IsValid())It.RemoveCurrent();
    if(CompatibilityOutputPolicy==EVirtualLidarCompatibilityOutputPolicy::WhenRequested)QueueScheduledAutoExports(LastFrameSnapshot);
    if(HasCompatibilityJsonDemand(LastFrameSnapshot->SlabContext))QueueScheduledPayloadBuild(FrameId, ScheduledScanStartTime);
    else {++CompatibilityStatus.SkippedUnrequested;RuntimeStatus.MeasuredOutputRateHz=0;LastScheduledOutputTime=-1;}
}

void UVirtualLidarScanComponent::QueueScheduledAutoExports(TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> Frame)
{
    if (!bExportCsvOnScan && !bExportJsonLinesOnScan && !bExportPcdOnScan) return;
    if (bScheduledAutoExportInFlight)
    {
        ++RuntimeStatus.DroppedDerivedFrameCount;
        return;
    }

    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::CaptureDirectory, SensorId, VirtualSensorDefaults::PointCloudSubdirectory);
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
    TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> Points = Frame.IsValid()?Frame->Points:GetLastPointSnapshot();
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

void UVirtualLidarScanComponent::ApplyPreset(EVirtualLidarPreset NewPreset)
{
    Preset = NewPreset;
    if (Preset == EVirtualLidarPreset::LowDebug) { HorizontalSamples = 90; VerticalChannels = 8; ScanInterval = 1.0f; PreviewPointStride = 1; MaxPreviewPoints = 2000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
    else if (Preset == EVirtualLidarPreset::MediumPreview) { HorizontalSamples = 180; VerticalChannels = 16; ScanInterval = 0.5f; PreviewPointStride = 2; MaxPreviewPoints = 3000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
    else if (Preset == EVirtualLidarPreset::HighQuality) { HorizontalSamples = 360; VerticalChannels = 32; ScanInterval = 0.25f; PreviewPointStride = 4; MaxPreviewPoints = 5000; PointCloudPreviewStride = PreviewPointStride; MaxPointCloudPreviewInstances = MaxPreviewPoints; PayloadPointStride = ServerPayloadStride; MaxPayloadPoints = MaxServerPayloadPoints; }
}

void UVirtualLidarScanComponent::ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality)
{
    SimulationQuality = NewQuality;
    if (SimulationQuality != EVirtualSensorSimulationQuality::Custom)
    {
        const FVirtualLidarProfilePreset Resolved = ResolveProfilePreset(DeviceProfile, SimulationQuality);
        DeviceSpec = Resolved.DeviceSpec;
        if(const auto* Industrial=IndustrialLidar::Find(DeviceProfile))
        {
            if(!PreIndustrialRangeNoise.IsSet())PreIndustrialRangeNoise=RangeNoiseStandardDeviationMillimeters;
            RangeNoiseStandardDeviationMillimeters=Industrial->PrecisionMM;
        }
        else if(PreIndustrialRangeNoise.IsSet())
        {RangeNoiseStandardDeviationMillimeters=PreIndustrialRangeNoise.GetValue();PreIndustrialRangeNoise.Reset();}
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
void UVirtualLidarScanComponent::SaveJsonToDisk(const FString& JsonPayload) const { const FString Path = BuildExportPath(TEXT("json"), SensorId); FFileHelper::SaveStringToFile(JsonPayload, *Path); }

FString UVirtualLidarScanComponent::BuildExportPath(const FString& Extension, const FString& FileNamePrefix) const
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::CaptureDirectory, SensorId, VirtualSensorDefaults::PointCloudSubdirectory); IFileManager::Get().MakeDirectory(*Dir, true); const FString Prefix = FileNamePrefix.IsEmpty() ? SensorId : FileNamePrefix; const FDateTime N = FDateTime::UtcNow(); const FString Ts = FString::Printf(TEXT("%s_%03d_%lld"), *N.ToString(TEXT("%Y%m%d_%H%M%S")), N.GetMillisecond(), N.GetTicks()); LastPointCloudExportPath = FPaths::Combine(Dir, FString::Printf(TEXT("%s_%s.%s"), *Prefix, *Ts, *Extension)); return LastPointCloudExportPath;
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
        if (PointCloudPreviewComponent)
        {
            // Preview instances are diagnostic markers, never physical scene objects.
            // Configure before registration so thousands of bodies/nav updates are
            // not created and moved during repeated FullSpec preview refreshes.
            PointCloudPreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PointCloudPreviewComponent->SetGenerateOverlapEvents(false);
            PointCloudPreviewComponent->SetCanEverAffectNavigation(false);
            PointCloudPreviewComponent->SetHiddenInSceneCapture(true);
            PointCloudPreviewComponent->SetupAttachment(this);PointCloudPreviewComponent->NumCustomDataFloats=4;PointCloudPreviewComponent->SetCastShadow(false);PointCloudPreviewComponent->RegisterComponent();
            if(!PointCloudPreviewMesh)PointCloudPreviewMesh=Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(),nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            if(PointCloudPreviewMesh)PointCloudPreviewComponent->SetStaticMesh(PointCloudPreviewMesh);
            if(!PointCloudPreviewMaterial&&GEngine)PointCloudPreviewMaterial=GEngine->DebugMeshMaterial;ApplyPointCloudPreviewStyle();
        }
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
