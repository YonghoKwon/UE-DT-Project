#include "VirtualLidarSensorActor.h"

#include "Components/ArrowComponent.h"
#include "VirtualLidarScanComponent.h"
#include "VirtualLidarAnalysisComponent.h"
#include "VirtualLidarExportComponent.h"
#include "VirtualLidarVisualizationComponent.h"
#include "VirtualSensorOutputComponent.h"
#include "VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"

AVirtualLidarSensorActor::AVirtualLidarSensorActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ScanComponent = CreateDefaultSubobject<UVirtualLidarScanComponent>(TEXT("LidarScanComponent"));
    RootComponent = ScanComponent;
    AnalysisComponent = CreateDefaultSubobject<UVirtualLidarAnalysisComponent>(TEXT("LidarAnalysisComponent"));
    VisualizationComponent = CreateDefaultSubobject<UVirtualLidarVisualizationComponent>(TEXT("LidarVisualizationComponent"));
    ExportComponent = CreateDefaultSubobject<UVirtualLidarExportComponent>(TEXT("LidarExportComponent"));

    EditorForwardArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("EditorForwardArrowComp"));
    EditorForwardArrowComp->SetupAttachment(RootComponent);
    EditorForwardArrowComp->ArrowSize = 2.0f;
    EditorForwardArrowComp->ArrowLength = 250.0f;
    EditorForwardArrowComp->bIsScreenSizeScaled = true;
}

void AVirtualLidarSensorActor::BeginPlay()
{
    Super::BeginPlay();
    if (ScanComponent)
    {
        ScanComponent->SetTransportComponent(nullptr);
        ScanComponent->SetRecorderComponent(nullptr);
        ScanComponent->OutputMode = EVirtualLidarOutputMode::None;
        ScanComponent->OnScanCompleted.AddDynamic(this, &AVirtualLidarSensorActor::HandleLidarFrame);
        if (AnalysisComponent) AnalysisComponent->BindScanComponent(ScanComponent);
        if (VisualizationComponent) VisualizationComponent->BindScanComponent(ScanComponent);
        if (ExportComponent) ExportComponent->BindScanComponent(ScanComponent);
    }
}

FString AVirtualLidarSensorActor::GetSensorId() const { return ScanComponent ? ScanComponent->SensorId : FString(); }
EVirtualSensorKind AVirtualLidarSensorActor::GetSensorKind() const { return EVirtualSensorKind::Lidar; }
bool AVirtualLidarSensorActor::IsSensorRunning() const { return ScanComponent && ScanComponent->IsScanRunning(); }
void AVirtualLidarSensorActor::StartSensor() { if (ScanComponent) ScanComponent->StartScan(); }
void AVirtualLidarSensorActor::StopSensor() { if (ScanComponent) ScanComponent->StopScan(); }
void AVirtualLidarSensorActor::CaptureSensorOnce() { if (ScanComponent) ScanComponent->ScanAndSend(); }
UTexture* AVirtualLidarSensorActor::GetSensorPreviewTexture() const { return ScanComponent ? ScanComponent->GetLidarViewTexture() : nullptr; }
FVirtualSensorRuntimeStatus AVirtualLidarSensorActor::GetSensorRuntimeStatus() const { return ScanComponent ? ScanComponent->GetRuntimeStatus() : FVirtualSensorRuntimeStatus(); }

bool AVirtualLidarSensorActor::SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport)
{
    if (!ScanComponent || Frame.SensorKind != EVirtualSensorKind::Lidar || !Frame.PointSnapshot.IsValid()) return false;
    PendingExternalSendTransport = bSendTransport;
    ScanComponent->InjectPointCloudFrame(*Frame.PointSnapshot, false);
    return true;
}

bool AVirtualLidarSensorActor::ReadEditableState(FVirtualSensorEditableState& OutState) const
{
    if (!ScanComponent) return false;
    OutState.TargetKind = EVirtualSensorTargetKind::Lidar;
    OutState.OriginalSensorId = ScanComponent->SensorId;
    OutState.SensorId = ScanComponent->SensorId;
    OutState.ActorTransform = GetActorTransform();
    OutState.SimulationQuality = ScanComponent->SimulationQuality;
    OutState.LidarProfile = ScanComponent->DeviceProfile;
    OutState.LidarScanInterval = ScanComponent->ScanInterval;
    OutState.LidarMaxDistance = ScanComponent->MaxDistance;
    OutState.LidarHorizontalSamples = ScanComponent->HorizontalSamples;
    OutState.LidarVerticalChannels = ScanComponent->VerticalChannels;
    OutState.LidarHorizontalFov = ScanComponent->HorizontalFov;
    OutState.LidarMinVerticalAngle = ScanComponent->MinVerticalAngle;
    OutState.LidarMaxVerticalAngle = ScanComponent->MaxVerticalAngle;
    OutState.ServerPayloadStride = ScanComponent->ServerPayloadStride;
    OutState.MaxServerPayloadPoints = ScanComponent->MaxServerPayloadPoints;
    OutState.bIncludeMissPointsInServerPayload = ScanComponent->bIncludeMissPointsInServerPayload;
    OutState.PreviewPointStride = ScanComponent->PreviewPointStride;
    OutState.MaxPreviewPoints = ScanComponent->MaxPreviewPoints;
    OutState.bPreviewHitOnly = ScanComponent->bPointCloudPreviewHitOnly;
    OutState.bUseMultiHit = ScanComponent->bUseMultiHit;
    OutState.MaxHitsPerRay = ScanComponent->MaxHitsPerRay;
    OutState.bExportCsvOnScan = ScanComponent->bExportCsvOnScan;
    OutState.bExportJsonLinesOnScan = ScanComponent->bExportJsonLinesOnScan;
    OutState.bExportPcdOnScan = ScanComponent->bExportPcdOnScan;
    return true;
}

bool AVirtualLidarSensorActor::ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError)
{
    if (!ValidateEditableState(State, OutError))
    {
        return false;
    }
    SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
    ScanComponent->StopScan();
    ScanComponent->ApplyDeviceProfile(State.LidarProfile);
    ScanComponent->ApplySimulationQuality(State.SimulationQuality);
    ScanComponent->SensorId = State.SensorId;
    ScanComponent->ScanInterval = State.LidarScanInterval;
    ScanComponent->MaxDistance = State.LidarMaxDistance;
    ScanComponent->HorizontalSamples = State.LidarHorizontalSamples;
    ScanComponent->VerticalChannels = State.LidarVerticalChannels;
    ScanComponent->HorizontalFov = State.LidarHorizontalFov;
    ScanComponent->MinVerticalAngle = State.LidarMinVerticalAngle;
    ScanComponent->MaxVerticalAngle = State.LidarMaxVerticalAngle;
    ScanComponent->SetServerPayloadPolicy(State.ServerPayloadStride, State.MaxServerPayloadPoints, State.bIncludeMissPointsInServerPayload);
    ScanComponent->SetPreviewPolicy(State.PreviewPointStride, State.MaxPreviewPoints, State.bPreviewHitOnly);
    ScanComponent->bUseMultiHit = State.bUseMultiHit;
    ScanComponent->MaxHitsPerRay = State.MaxHitsPerRay;
    ScanComponent->bExportCsvOnScan = State.bExportCsvOnScan;
    ScanComponent->bExportJsonLinesOnScan = State.bExportJsonLinesOnScan;
    ScanComponent->bExportPcdOnScan = State.bExportPcdOnScan;
    ScanComponent->StartScan();
    return true;
}

bool AVirtualLidarSensorActor::ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError)
{
    if (!ScanComponent || RequestedState.TargetKind != EVirtualSensorTargetKind::Lidar)
    {
        OutError = TEXT("선택한 LiDAR 센서를 사용할 수 없습니다.");
        return false;
    }
    const bool bWasRunning = ScanComponent->IsScanRunning();
    ScanComponent->StopScan();
    ScanComponent->ApplyDeviceProfile(RequestedState.LidarProfile);
    ScanComponent->ApplySimulationQuality(RequestedState.SimulationQuality);
    if (bWasRunning) ScanComponent->StartScan();
    return ReadEditableState(OutAppliedState);
}

bool AVirtualLidarSensorActor::ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const
{
    if (!ScanComponent || State.TargetKind != EVirtualSensorTargetKind::Lidar)
    {
        OutError = TEXT("선택한 LiDAR 센서를 사용할 수 없습니다.");
        return false;
    }
    if (State.SensorId.TrimStartAndEnd().IsEmpty() || State.ActorTransform.ContainsNaN())
    {
        OutError = TEXT("SensorId 또는 Transform 값이 올바르지 않습니다.");
        return false;
    }
    if (State.LidarScanInterval < 0.033f || State.LidarScanInterval > 60.0f ||
        State.LidarMaxDistance < 10.0f || State.LidarMaxDistance > 10000.0f ||
        State.LidarHorizontalSamples < 1 || State.LidarHorizontalSamples > 1440 ||
        State.LidarVerticalChannels < 1 || State.LidarVerticalChannels > 256 ||
        State.LidarHorizontalFov < 1.0f || State.LidarHorizontalFov > 360.0f ||
        State.LidarMinVerticalAngle < -90.0f || State.LidarMaxVerticalAngle > 90.0f ||
        State.LidarMinVerticalAngle >= State.LidarMaxVerticalAngle ||
        State.ServerPayloadStride < 1 || State.ServerPayloadStride > 100 ||
        State.MaxServerPayloadPoints < 0 || State.MaxServerPayloadPoints > 1000000 ||
        State.PreviewPointStride < 1 || State.PreviewPointStride > 100 ||
        State.MaxPreviewPoints < 0 || State.MaxPreviewPoints > 1000000 ||
        State.MaxHitsPerRay < 1 || State.MaxHitsPerRay > 16)
    {
        OutError = TEXT("LiDAR 설정이 지원 범위를 벗어났습니다.");
        return false;
    }
    return true;
}

void AVirtualLidarSensorActor::HandleLidarFrame(const FString& JsonPayload, UTexture2D* ViewTexture)
{
    if (!ScanComponent) return;
    if (AnalysisComponent) AnalysisComponent->ApplyPrecomputedStatistics(ScanComponent->GetLastHitPointCount(), ScanComponent->GetLastSemanticCounts());
    if (VisualizationComponent)
    {
        const UVirtualSensorSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>() : nullptr;
        if (!Scheduler || Scheduler->ShouldRefreshLidarPreview(ScanComponent)) VisualizationComponent->RefreshLatestFrame();
    }
    if (!OutputComponent || JsonPayload.IsEmpty()) return;

    FVirtualSensorFrameEnvelope Frame;
    Frame.SensorId = ScanComponent->SensorId;
    Frame.SensorKind = EVirtualSensorKind::Lidar;
    Frame.FrameId = ScanComponent->GetRuntimeStatus().FrameId;
    Frame.TimestampUtc = FDateTime::UtcNow();
    Frame.SchemaVersion = TEXT("virtual-lidar.v1");
    Frame.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(JsonPayload);
    Frame.PointSnapshot = ScanComponent->GetLastPointSnapshot();
    Frame.bSendTransport = PendingExternalSendTransport.IsSet() ? PendingExternalSendTransport.GetValue() : true;
    Frame.bRecord = true;
    PendingExternalSendTransport.Reset();
    OutputComponent->RouteFrame(Frame);
}
