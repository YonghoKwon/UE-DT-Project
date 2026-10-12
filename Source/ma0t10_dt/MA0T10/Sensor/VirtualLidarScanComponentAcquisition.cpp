#include "VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"

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

std::atomic<int32> GVirtualLidarJsonJobs{0};
// Four supported FullSpec LiDARs may complete in the same scheduling window.
// One immutable latest-frame JSON job per sensor avoids queue rejection while
// preserving the per-sensor in-flight bound.
constexpr int32 GVirtualLidarJsonJobLimit = 4;
int64 UtcNowUnixNanoseconds()
{
    static const FDateTime UnixEpoch(1970, 1, 1);
    return (FDateTime::UtcNow() - UnixEpoch).GetTicks() * 100;
}

struct FLidarScheduledPayloadSnapshot
{
    TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> Points;
    FString SensorId;
    FString Manufacturer;
    FString Model;
    FString PreviewBackend;
    int64 FrameId = 0;
    FDateTime TimestampUtc;
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
#include "VirtualLidarScanComponentEncoding.inl"

}


struct FVirtualLidarDerivedOutputJob
{
    FLidarScheduledPayloadSnapshot Payload;
    FVirtualSensorFrameEnvelope Frame;
    int32 Generation=0;
    double StartedSeconds=0;
    double QueuedSeconds=0,EncodeStartedSeconds=0,EncodeFinishedSeconds=0;
    FString RequestId;
    bool bAutoExportAtAcquisition=false;
    bool bReserved=false;
    std::atomic<bool> Cancelled{false};
};

FVirtualSensorResourceSnapshot UVirtualLidarScanComponent::GetResourceSnapshot() const
{
    check(IsInGameThread());
    FVirtualSensorResourceSnapshot S; S.Owner=SensorId+TEXT("/lidar");
    S.Counts={{TEXT("scan_in_flight"),bScheduledScanInProgress},{TEXT("payload_in_flight"),bScheduledPayloadBuildInFlight},
        {TEXT("export_in_flight"),bScheduledAutoExportInFlight},{TEXT("global_json_jobs"),GVirtualLidarJsonJobs.load()},
        {TEXT("surface_cache_entries"),SurfaceResponseCache.Num()},{TEXT("preview_instances"),PointCloudPreviewComponent?PointCloudPreviewComponent->GetInstanceCount():0}};
    S.OwnedCpuBytes.Add(TEXT("scheduled_scan_arrays"),ScheduledPoints.GetAllocatedSize()+ScheduledHeatmapPixels.GetAllocatedSize());
    S.OwnedCpuBytes.Add(TEXT("ray_lookup"),GpuDepthLocalDirections.GetAllocatedSize()+GpuDepthPixelIndices.GetAllocatedSize()+ScheduledHorizontalAnglesDegrees.GetAllocatedSize()+ScheduledVerticalAnglesDegrees.GetAllocatedSize());
    S.OwnedCpuBytes.Add(TEXT("surface_cache_container"),SurfaceResponseCache.GetAllocatedSize());
    S.OwnedCpuBytes.Add(TEXT("latest_json"),LastJsonPayload.GetAllocatedSize());
    S.SharedCpuBytes.Add(TEXT("latest_points"),LastPointStorage.IsValid()?LastPointStorage->GetAllocatedSize():0);
    // LastFrameSnapshot references LastPointStorage: do not count the same points again.
    return S;
}
void UVirtualLidarScanComponent::StopScan()
{
    for(const auto& Job:{ActiveDerivedJob,PendingDerivedJob})if(Job){Job->Cancelled=true;ReleaseDerivedReservation(Job);++RuntimeStatus.DiscardedDerivedGenerationCount;if(!Job->RequestId.IsEmpty())OnCompatibilityRequestCompletedNative.Broadcast(Job->RequestId,false,Job->Frame,TEXT("측정 종료로 JSON 요청이 취소됐습니다."));}
    ActiveDerivedJob.Reset();PendingDerivedJob.Reset();
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
    RuntimeStatus.bDerivedWorkInFlight=false;
    RuntimeStatus.bAcquisitionInFlight = false;
    ScheduledPoints.Reset();
    ScheduledHeatmapPixels.Reset();
    ++ScheduledGeneration;
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
    BuildBeamAngleTables(Request.HorizontalSamples, Request.VerticalChannels, Request.HorizontalAngles, Request.VerticalAngles);
    Request.bRequestSemantics=IsSemanticCaptureRequested();

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
    RuntimeStatus.AcquisitionBackendMessage = Request.bRequestSemantics
        ? TEXT("GPU 깊이·의미 분류 캡처 중 (첫 표면, 다중 Echo 미지원)")
        : TEXT("GPU 깊이 측정 중 (첫 표면, 다중 Echo 미지원)");
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
FVirtualLidarCompatibilityOutputStatus UVirtualLidarScanComponent::GetCompatibilityOutputStatus() const
{
    auto Status=CompatibilityStatus;Status.bActive=ActiveDerivedJob.IsValid();Status.bPending=PendingDerivedJob.IsValid();
    Status.bRequested=HasCompatibilityJsonDemand(LastFrameSnapshot.IsValid()?LastFrameSnapshot->SlabContext:FVirtualSlabFrameContext(),&Status.Reason);
    if(bInteractivePreviewMode&&bSuppressInteractiveDerivedOutput){Status.bRequested=false;Status.Reason=TEXT("조작 미리보기 · JSON/수집 출력 제외");}
    if((ActiveDerivedJob&&!ActiveDerivedJob->RequestId.IsEmpty())||(PendingDerivedJob&&!PendingDerivedJob->RequestId.IsEmpty())){Status.bRequested=true;Status.Reason=TEXT("수동 JSON 1회 생성 중");}
    return Status;
}
bool UVirtualLidarScanComponent::RequestCompatibilityPayloadFromLatestFrame(FString& RequestId,FString& Reason)
{
    RequestId.Reset();
    if(bInteractivePreviewMode){Reason=TEXT("조작을 종료하고 원규격 미리보기 갱신 후 요청하세요.");return false;}
    if(!LastFrameSnapshot.IsValid()){Reason=TEXT("완료된 측정 프레임이 없습니다.");return false;}
    if(LastFrameSnapshot->PipelineGeneration!=ScheduledGeneration){Reason=TEXT("측정 종료·설정 변경 전 프레임입니다. 미리보기의 새 프레임을 받은 뒤 요청하세요.");return false;}
    if(ActiveDerivedJob||PendingDerivedJob){Reason=TEXT("JSON 처리 중입니다. 완료 후 다시 요청하세요.");return false;}
    if(GVirtualLidarJsonJobs.load(std::memory_order_acquire)>=GVirtualLidarJsonJobLimit){Reason=TEXT("전역 JSON 처리 슬롯이 사용 중입니다. 완료 후 다시 요청하세요.");return false;}
    RequestId=FGuid::NewGuid().ToString();QueueScheduledPayloadBuild(LastFrameSnapshot->FrameId,FPlatformTime::Seconds(),RequestId);Reason=TEXT("JSON 생성 요청 접수");return true;
}
void UVirtualLidarScanComponent::QueueScheduledPayloadBuild(int64 CapturedFrameId, double AcquisitionStartedSeconds,const FString& RequestId)
{
    if(!LastFrameSnapshot.IsValid()||LastFrameSnapshot->FrameId!=CapturedFrameId)return;
    if(RequestId.IsEmpty()&&PendingDerivedJob&&!PendingDerivedJob->RequestId.IsEmpty())return;
    auto Job=MakeShared<FVirtualLidarDerivedOutputJob,ESPMode::ThreadSafe>();Job->Generation=ScheduledGeneration;Job->StartedSeconds=AcquisitionStartedSeconds;
    Job->QueuedSeconds=FPlatformTime::Seconds();Job->RequestId=RequestId;
    Job->bAutoExportAtAcquisition=CompatibilityOutputPolicy==EVirtualLidarCompatibilityOutputPolicy::WhenRequested;
    auto& Frame=Job->Frame;Frame.SensorId=SensorId;Frame.SensorKind=EVirtualSensorKind::Lidar;Frame.FrameId=CapturedFrameId;
    Frame.LidarFrameSnapshot=LastFrameSnapshot;Frame.PointSnapshot=LastFrameSnapshot->Points;Frame.SlabContext=LastFrameSnapshot->SlabContext;
    Frame.TimestampUtc=LastFrameSnapshot->GetAcquisitionStartUtc();Frame.SchemaVersion=TEXT("virtual-lidar.v1");Frame.bLidarCompatibilityDerived=true;
    auto& Snapshot=Job->Payload;Snapshot.TimestampUtc=Frame.TimestampUtc;
    Snapshot.Points = Frame.PointSnapshot;
    Snapshot.SensorId = SensorId;
    Snapshot.Manufacturer = DeviceSpec.Manufacturer;
    Snapshot.Model = DeviceSpec.Model;
    Snapshot.PreviewBackend = GetPreviewBackendName();
    Snapshot.FrameId = CapturedFrameId;
    Snapshot.HorizontalSamples = LastFrameSnapshot->HorizontalSamples;
    Snapshot.VerticalChannels = LastFrameSnapshot->VerticalChannels;
    Snapshot.MaxDistance = LastFrameSnapshot->MaxDistanceCm;
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
    Snapshot.SensorTransform = LastFrameSnapshot->AcquisitionTransform;
    Snapshot.SlabAnalysis = LastSlabAnalysis;
    if(GetWorld())if(auto* Slab=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>())Job->bReserved=Slab->BeginCompatibilityDerivation(SensorId,CapturedFrameId,Frame.SlabContext);
    if(PendingDerivedJob){ReleaseDerivedReservation(PendingDerivedJob);++RuntimeStatus.DroppedDerivedFrameCount;++RuntimeStatus.CompatibilityPendingReplacementCount;}
    PendingDerivedJob=Job;RuntimeStatus.bDerivedWorkInFlight=true;TryStartPendingPayloadBuild();
}
void UVirtualLidarScanComponent::TryStartPendingPayloadBuild()
{
    if(ActiveDerivedJob||!PendingDerivedJob)return;
    if(PendingDerivedJob->RequestId.IsEmpty()&&!PendingDerivedJob->bReserved&&!HasCompatibilityJsonDemand(PendingDerivedJob->Frame.SlabContext))
    {ReleaseDerivedReservation(PendingDerivedJob);PendingDerivedJob.Reset();++CompatibilityStatus.CancelledPending;RuntimeStatus.bDerivedWorkInFlight=false;return;}
    if(GVirtualLidarJsonJobs.fetch_add(1,std::memory_order_acq_rel)>=GVirtualLidarJsonJobLimit){GVirtualLidarJsonJobs.fetch_sub(1,std::memory_order_acq_rel);return;}
    auto Job=MoveTemp(PendingDerivedJob);ActiveDerivedJob=Job;bScheduledPayloadBuildInFlight=true;
    ++CompatibilityStatus.Started;
    TWeakObjectPtr<UVirtualLidarScanComponent> WeakThis(this);
    int32 Delay=0;
#if WITH_DEV_AUTOMATION_TESTS
    Delay=JsonEncodeDelayMsForTests;
#endif
    Async(EAsyncExecution::ThreadPool, [WeakThis, Job,Delay]() mutable
    {
        Job->EncodeStartedSeconds=FPlatformTime::Seconds();
        if(Delay>0)FPlatformProcess::Sleep(Delay/1000.f);
        FString Payload;if(!Job->Cancelled.load())Payload=BuildScheduledLidarJson(Job->Payload);
        Job->EncodeFinishedSeconds=FPlatformTime::Seconds();
        GVirtualLidarJsonJobs.fetch_sub(1, std::memory_order_acq_rel);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Job,Payload = MoveTemp(Payload)]() mutable
        {
            if (!WeakThis.IsValid() || WeakThis->ScheduledGeneration != Job->Generation||Job->Cancelled.load()) return;
            WeakThis->CompleteScheduledPayloadBuild(Job,MoveTemp(Payload));
        });
    });
}
void UVirtualLidarScanComponent::ReleaseDerivedReservation(const TSharedPtr<FVirtualLidarDerivedOutputJob,ESPMode::ThreadSafe>& Job)
{if(Job&&Job->bReserved){if(GetWorld())if(auto* Slab=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>())Slab->EndCompatibilityDerivation(Job->Frame.SlabContext.RunId,Job->Frame.SensorId,Job->Frame.FrameId);Job->bReserved=false;}}

TFunction<FString()> UVirtualLidarScanComponent::CreateLocalFilePayloadEncoder(TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> Frame) const
{
	check(IsInGameThread()); if(!Frame.IsValid()||!Frame->Points.IsValid()) return {};
	FLidarScheduledPayloadSnapshot Snapshot;
	Snapshot.Points=Frame->Points; Snapshot.SensorId=SensorId; Snapshot.Manufacturer=DeviceSpec.Manufacturer; Snapshot.Model=DeviceSpec.Model;
	Snapshot.TimestampUtc=Frame->AcquisitionStartUnixNanoseconds>0?FDateTime(1970,1,1)+FTimespan(Frame->AcquisitionStartUnixNanoseconds/100):FDateTime::UtcNow();
	Snapshot.PreviewBackend=GetPreviewBackendName();Snapshot.FrameId=Frame->FrameId;Snapshot.HorizontalSamples=Frame->HorizontalSamples;Snapshot.VerticalChannels=Frame->VerticalChannels;Snapshot.MaxDistance=Frame->MaxDistanceCm;
	Snapshot.ServerPayloadStride=ServerPayloadStride;Snapshot.MaxServerPayloadPoints=MaxServerPayloadPoints;Snapshot.bIncludeMissPoints=bIncludeMissPointsInServerPayload;
	Snapshot.PreviewPointStride=PreviewPointStride;Snapshot.MaxPreviewPoints=MaxPreviewPoints;Snapshot.bPreviewHitOnly=bPointCloudPreviewHitOnly;
	Snapshot.bSemanticClassification=bEnableSemanticClassification;Snapshot.bGpuRequested=IsGpuPreviewBackendRequested();Snapshot.bGpuActive=IsGpuPreviewBackendActive();Snapshot.bExperimentalGpuOptIn=bAllowExperimentalGpuPreviewBackend;
	Snapshot.bIncludeSlabAnalysis=bIncludeSlabAnalysisInPayload;Snapshot.SensorTransform=Frame->AcquisitionTransform;Snapshot.SlabAnalysis=LastSlabAnalysis;
	return [Snapshot=MoveTemp(Snapshot)](){return BuildScheduledLidarJson(Snapshot);};
}

void UVirtualLidarScanComponent::CompleteScheduledPayloadBuild(TSharedPtr<FVirtualLidarDerivedOutputJob,ESPMode::ThreadSafe> Job,FString&& JsonPayload)
{
    if(ActiveDerivedJob!=Job)return;
    ActiveDerivedJob.Reset();
    bScheduledPayloadBuildInFlight = false;
    RuntimeStatus.bDerivedWorkInFlight = false;
    RuntimeStatus.LastPostProcessDurationMs = static_cast<float>((FPlatformTime::Seconds() - Job->StartedSeconds) * 1000.0);
    const double CompletionStarted=FPlatformTime::Seconds();
    CompatibilityStatus.QueueWaitMs=(Job->EncodeStartedSeconds-Job->QueuedSeconds)*1000;
    CompatibilityStatus.EncodeMs=(Job->EncodeFinishedSeconds-Job->EncodeStartedSeconds)*1000;
    if (JsonPayload.IsEmpty())
    {
        ++RuntimeStatus.DroppedDerivedFrameCount;
        ++RuntimeStatus.FailedDerivedEncodingCount;
        if(Job->bReserved&&GetWorld()){GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->EndCompatibilityDerivation(Job->Frame.SlabContext.RunId,Job->Frame.SensorId,Job->Frame.FrameId,true);Job->bReserved=false;}
        ReleaseDerivedReservation(Job);TryStartPendingPayloadBuild();
        UpdateRuntimeStatusAfterScan(0);
        if(!Job->RequestId.IsEmpty())OnCompatibilityRequestCompletedNative.Broadcast(Job->RequestId,false,Job->Frame,TEXT("JSON 생성 실패"));
        return;
    }
    if(Job->RequestId.IsEmpty()&&LastCompletedDerivedFrame.SensorId==Job->Frame.SensorId&&LastCompletedDerivedFrame.FrameId>=Job->Frame.FrameId&&LastCompletedDerivedFrame.HasJsonPayload())
    {++RuntimeStatus.DiscardedDerivedGenerationCount;ReleaseDerivedReservation(Job);TryStartPendingPayloadBuild();return;}
    Job->Frame.JsonPayload=MakeShared<const FString,ESPMode::ThreadSafe>(MoveTemp(JsonPayload));
    LastJsonPayload = *Job->Frame.JsonPayload;
    ++CompatibilityStatus.Completed;
    if(!Job->RequestId.IsEmpty())
    {
        LastCompletedDerivedFrame=Job->Frame;
        OnCompatibilityRequestCompletedNative.Broadcast(Job->RequestId,true,Job->Frame,TEXT("JSON 생성 완료"));
        ReleaseDerivedReservation(Job);TryStartPendingPayloadBuild();CompatibilityStatus.CompletionMs=(FPlatformTime::Seconds()-CompletionStarted)*1000;return;
    }
    DispatchPayload(LastJsonPayload);
    const double OutputNowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    RuntimeStatus.MeasuredOutputRateHz = LastScheduledOutputTime >= 0.0
        ? static_cast<float>(1.0 / FMath::Max(0.001, OutputNowSeconds - LastScheduledOutputTime))
        : 0.0f;
    LastScheduledOutputTime = OutputNowSeconds;
    UpdateRuntimeStatusAfterScan(LastJsonPayload.Len());
    if (RecorderComponent) RecorderComponent->RecordJsonFrame(Job->Frame.SensorId, TEXT("virtual_lidar"), Job->Frame.FrameId, LastJsonPayload);
    if(!Job->bAutoExportAtAcquisition)QueueScheduledAutoExports(Job->Frame.LidarFrameSnapshot);
    CompleteDerivedFrame(Job->Frame);
    OnScanCompleted.Broadcast(LastJsonPayload, LidarViewTexture);
    ReleaseDerivedReservation(Job);TryStartPendingPayloadBuild();RuntimeStatus.bDerivedWorkInFlight=ActiveDerivedJob.IsValid()||PendingDerivedJob.IsValid();
    CompatibilityStatus.CompletionMs=(FPlatformTime::Seconds()-CompletionStarted)*1000;
}

void UVirtualLidarScanComponent::ScanAndSend()
{
    ++FrameId;
    ScheduledScanStartTime=FPlatformTime::Seconds();
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
    FVirtualSensorFrameEnvelope Completed;Completed.SensorId=SensorId;Completed.SensorKind=EVirtualSensorKind::Lidar;Completed.FrameId=FrameId;Completed.TimestampUtc=LastFrameSnapshot->GetAcquisitionStartUtc();Completed.SchemaVersion=TEXT("virtual-lidar.v1");Completed.LidarFrameSnapshot=LastFrameSnapshot;Completed.PointSnapshot=LastFrameSnapshot->Points;Completed.SlabContext=LastFrameSnapshot->SlabContext;Completed.JsonPayload=MakeShared<const FString,ESPMode::ThreadSafe>(JsonPayload);Completed.bLidarCompatibilityDerived=true;
    CompleteDerivedFrame(Completed);OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
}

void UVirtualLidarScanComponent::InjectPointCloudFrame(const TArray<FVirtualLidarPoint>& Points, bool bSendTransport)
{
    ++FrameId;
    ScheduledScanStartTime=FPlatformTime::Seconds();ScheduledAcquisitionStartUnixNanoseconds=UtcNowUnixNanoseconds();
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

    FVirtualSensorFrameEnvelope Completed;Completed.SensorId=SensorId;Completed.SensorKind=EVirtualSensorKind::Lidar;Completed.FrameId=FrameId;Completed.TimestampUtc=LastFrameSnapshot->GetAcquisitionStartUtc();Completed.SchemaVersion=TEXT("virtual-lidar.v1");Completed.LidarFrameSnapshot=LastFrameSnapshot;Completed.PointSnapshot=LastFrameSnapshot->Points;Completed.SlabContext=LastFrameSnapshot->SlabContext;Completed.JsonPayload=MakeShared<const FString,ESPMode::ThreadSafe>(JsonPayload);Completed.bLidarCompatibilityDerived=true;Completed.bSendTransport=bSendTransport;
    CompleteDerivedFrame(Completed);OnScanCompleted.Broadcast(JsonPayload, LidarViewTexture);
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
    Snapshot->AcquisitionStartedMonotonic=ScheduledScanStartTime;
    Snapshot->PipelineGeneration=ScheduledGeneration;
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
    Root->SetStringField(TEXT("timestampUtc"), (LastFrameSnapshot.IsValid()&&LastFrameSnapshot->FrameId==FrameId?LastFrameSnapshot->GetAcquisitionStartUtc():FDateTime::UtcNow()).ToIso8601());
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
