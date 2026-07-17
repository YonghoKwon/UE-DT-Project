#include "ma0t10_dt/MA0T10/Sensor/RealSensorSourceComponent.h"

#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

namespace
{
FString RealSensorSourceKindToText(ERealSensorSourceKind SourceKind)
{
    switch (SourceKind)
    {
    case ERealSensorSourceKind::FileReplay:
        return TEXT("FileReplay");
    case ERealSensorSourceKind::JsonLiveBridge:
        return TEXT("JsonLiveBridge");
    case ERealSensorSourceKind::Ros2Bridge:
        return TEXT("Ros2Bridge");
    case ERealSensorSourceKind::LivoxLidar:
        return TEXT("LivoxLidar");
    case ERealSensorSourceKind::RealSenseCamera:
        return TEXT("RealSenseCamera");
    default:
        return TEXT("Custom");
    }
}

FString RealSensorConnectionStateToText(ERealSensorSourceConnectionState State)
{
    switch (State)
    {
    case ERealSensorSourceConnectionState::Stopped:
        return TEXT("Stopped");
    case ERealSensorSourceConnectionState::Starting:
        return TEXT("Starting");
    case ERealSensorSourceConnectionState::Running:
        return TEXT("Running");
    case ERealSensorSourceConnectionState::Error:
        return TEXT("Error");
    default:
        return TEXT("Unknown");
    }
}
}

URealSensorSourceComponent::URealSensorSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URealSensorSourceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoStartSource)
    {
        StartSource();
    }
}

void URealSensorSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopSource();
    Super::EndPlay(EndPlayReason);
}

bool URealSensorSourceComponent::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Running, TEXT("Source started."));
    return true;
}

void URealSensorSourceComponent::StopSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("Source stopped."));
}

bool URealSensorSourceComponent::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("PushFrameOnce is not implemented for this source."));
    return false;
}

FString URealSensorSourceComponent::GetDeploymentReadinessSummaryText() const
{
    const bool bNeedsExternalEvidence = RequiresExternalDeploymentEvidence();
    FString NextAction = TEXT("Use replay as baseline and confirm payload handoff.");

    switch (SourceKind)
    {
    case ERealSensorSourceKind::FileReplay:
        NextAction = TEXT("Replay baseline can validate schema; production live evidence is still separate.");
        break;
    case ERealSensorSourceKind::JsonLiveBridge:
        NextAction = TEXT("Record live HTTP/WebSocket/UDP broker smoke and judging-server handoff evidence.");
        break;
    case ERealSensorSourceKind::Ros2Bridge:
        NextAction = TEXT("Implement ROS2 bridge and record real message/schema/calibration smoke evidence.");
        break;
    case ERealSensorSourceKind::LivoxLidar:
        NextAction = TEXT("Implement Livox SDK path and record device/calibration/live-frame smoke evidence.");
        break;
    case ERealSensorSourceKind::RealSenseCamera:
        NextAction = TEXT("Implement RealSense path and record camera/depth live-frame smoke evidence.");
        break;
    default:
        NextAction = TEXT("Define owner, payload contract, live smoke, and acceptance evidence for this custom source.");
        break;
    }

    return FString::Printf(TEXT("Deployment Gate: Source=%s Kind=%s State=%s Frame=%lld Points=%d ExternalEvidenceRequired=%s Next=%s Last=%s"),
        *SourceId,
        *RealSensorSourceKindToText(SourceKind),
        *RealSensorConnectionStateToText(ConnectionState),
        LastSourceFrameId,
        LastSourcePointCount,
        bNeedsExternalEvidence ? TEXT("true") : TEXT("false"),
        *NextAction,
        LastSourceMessage.IsEmpty() ? TEXT("None") : *LastSourceMessage);
}

bool URealSensorSourceComponent::RequiresExternalDeploymentEvidence() const
{
    return SourceKind != ERealSensorSourceKind::FileReplay;
}

AVirtualSensorActorBase* URealSensorSourceComponent::ResolveTargetSensorActor() const
{
    if (TargetSensorActor)
    {
        return TargetSensorActor;
    }
    if (TargetLidar)
    {
        return Cast<AVirtualSensorActorBase>(TargetLidar->GetOwner());
    }
    return Cast<AVirtualSensorActorBase>(GetOwner());
}

UVirtualLidarScanComponent* URealSensorSourceComponent::ResolveTargetLidar() const
{
    if (TargetLidar)
    {
        return TargetLidar;
    }
    return GetOwner() ? GetOwner()->FindComponentByClass<UVirtualLidarScanComponent>() : nullptr;
}

bool URealSensorSourceComponent::PushPointFrameToTarget(const TArray<FVirtualLidarPoint>& Points, bool bSendTransport, const FString& SuccessMessage, int32 Rows, int32 Cols, bool bUpdateLidarDimensions)
{
    UVirtualLidarScanComponent* LidarComp = ResolveTargetLidar();
    AVirtualSensorActorBase* SensorActor = ResolveTargetSensorActor();
    if (!LidarComp && !SensorActor)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Target LiDAR is not set."));
        return false;
    }
    if (Points.Num() <= 0)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Point frame is empty."));
        return false;
    }

    if (LidarComp && bUpdateLidarDimensions && Rows > 0 && Cols > 0)
    {
        LidarComp->VerticalChannels = FMath::Clamp(Rows, 1, 256);
        LidarComp->HorizontalSamples = FMath::Clamp(Cols, 1, 1440);
    }

    if (SensorActor)
    {
        FVirtualSensorFrameEnvelope Frame;
        Frame.SensorId = SensorActor->GetSensorId();
        Frame.SensorKind = EVirtualSensorKind::Lidar;
        Frame.FrameId = LastSourceFrameId + 1;
        Frame.TimestampUtc = FDateTime::UtcNow();
        Frame.SchemaVersion = TEXT("virtual-lidar.v1");
        Frame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(Points);
        if (!SensorActor->SubmitExternalFrame(Frame, bSendTransport))
        {
            SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Target LiDAR rejected the point frame."));
            return false;
        }
    }
    else
    {
        LidarComp->InjectPointCloudFrame(Points, bSendTransport);
    }
    MarkFramePushed(Points.Num(), SuccessMessage);
    return true;
}

void URealSensorSourceComponent::SetSourceState(ERealSensorSourceConnectionState NewState, const FString& Message)
{
    ConnectionState = NewState;
    LastSourceMessage = Message;
}

void URealSensorSourceComponent::MarkFramePushed(int32 PointCount, const FString& Message)
{
    ++LastSourceFrameId;
    LastSourcePointCount = PointCount;
    SetSourceState(ERealSensorSourceConnectionState::Running, Message);
}
