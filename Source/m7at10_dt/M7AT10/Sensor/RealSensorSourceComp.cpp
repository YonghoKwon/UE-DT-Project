#include "m7at10_dt/M7AT10/Sensor/RealSensorSourceComp.h"

#include "GameFramework/Actor.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

URealSensorSourceComp::URealSensorSourceComp()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URealSensorSourceComp::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoStartSource)
    {
        StartSource();
    }
}

void URealSensorSourceComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopSource();
    Super::EndPlay(EndPlayReason);
}

bool URealSensorSourceComp::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Running, TEXT("Source started."));
    return true;
}

void URealSensorSourceComp::StopSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("Source stopped."));
}

bool URealSensorSourceComp::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("PushFrameOnce is not implemented for this source."));
    return false;
}

UVirtualLidarSensorComp* URealSensorSourceComp::ResolveTargetLidar() const
{
    if (TargetLidar)
    {
        return TargetLidar;
    }
    return GetOwner() ? GetOwner()->FindComponentByClass<UVirtualLidarSensorComp>() : nullptr;
}

void URealSensorSourceComp::SetSourceState(ERealSensorSourceConnectionState NewState, const FString& Message)
{
    ConnectionState = NewState;
    LastSourceMessage = Message;
}

void URealSensorSourceComp::MarkFramePushed(int32 PointCount, const FString& Message)
{
    ++LastSourceFrameId;
    LastSourcePointCount = PointCount;
    SetSourceState(ERealSensorSourceConnectionState::Running, Message);
}
