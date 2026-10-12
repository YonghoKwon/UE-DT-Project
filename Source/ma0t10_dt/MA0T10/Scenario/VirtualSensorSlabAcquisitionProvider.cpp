#include "VirtualSensorSlabContextSubsystem.h"
#include "SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"

namespace
{
EVirtualSensorStreamKind StreamKind(EVirtualSensorAcquisitionOutput Output)
{
    switch (Output)
    {
    case EVirtualSensorAcquisitionOutput::LidarTelemetry: return EVirtualSensorStreamKind::LidarPayload;
    case EVirtualSensorAcquisitionOutput::CameraImage: return EVirtualSensorStreamKind::CameraImage;
    case EVirtualSensorAcquisitionOutput::PointCloud: return EVirtualSensorStreamKind::PointCloud;
    }
    checkNoEntry();
    return EVirtualSensorStreamKind::LidarPayload;
}
}
FVirtualSensorAcquisitionContext UVirtualSensorSlabContextSubsystem::CaptureAcquisitionContext(const FString& SensorId, int64 FrameId)
{
    check(IsInGameThread());
    return CaptureContext(SensorId, FrameId);
}
bool UVirtualSensorSlabContextSubsystem::BeginContextDerivation(const FString& SensorId, int64 FrameId, const FVirtualSensorAcquisitionContext& Context)
{
    check(IsInGameThread());
    return BeginCompatibilityDerivation(SensorId, FrameId, FVirtualSlabFrameContext(Context));
}
bool UVirtualSensorSlabContextSubsystem::AllowsAcquisitionFrame(const FString& SensorId, const FVirtualSensorAcquisitionContext& Context) const
{
    check(IsInGameThread());
    return AllowsFrame(SensorId, FVirtualSlabFrameContext(Context));
}
bool UVirtualSensorSlabContextSubsystem::AllowsOutputFrame(const FString& SensorId, EVirtualSensorAcquisitionOutput Output, const FVirtualSensorAcquisitionContext& Context) const
{
    check(IsInGameThread());
    return AllowsStreamFrame(SensorId, StreamKind(Output), FVirtualSlabFrameContext(Context));
}
bool UVirtualSensorSlabContextSubsystem::AllowsOutputDemand(const FString& SensorId, EVirtualSensorAcquisitionOutput Output) const
{
    check(IsInGameThread());
    return AllowsStreamDemand(SensorId, StreamKind(Output));
}
bool UVirtualSensorSlabContextSubsystem::IsAcquisitionSessionBusy() const
{
    check(IsInGameThread());
    return Status.State == EVirtualSlabSessionState::Preparing || Status.State == EVirtualSlabSessionState::Ready ||
        Status.State == EVirtualSlabSessionState::Running || Status.State == EVirtualSlabSessionState::Paused || Status.State == EVirtualSlabSessionState::Draining;
}
void UVirtualSensorSlabContextSubsystem::RecordAcquisitionOutputFailure(const FString& RunId, const FString& SensorId, EVirtualSensorAcquisitionOutput Output, int64 FrameId, const FString& Reason)
{
    check(IsInGameThread());
    GetWorld()->GetSubsystem<USlabRunResultsSubsystem>()->RecordOutputFailure(RunId, SensorId, static_cast<int32>(StreamKind(Output)), FrameId, Reason);
}
