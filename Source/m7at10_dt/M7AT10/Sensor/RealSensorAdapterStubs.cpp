#include "m7at10_dt/M7AT10/Sensor/RealSensorAdapterStubs.h"

URos2SensorBridgeSourceComp::URos2SensorBridgeSourceComp()
{
    SourceKind = ERealSensorSourceKind::Ros2Bridge;
    SourceId = TEXT("ROS2Bridge");
    LastSourceMessage = TEXT("ROS2 bridge source is a DT-Project placeholder. SDK/bridge integration is not implemented yet.");
}

bool URos2SensorBridgeSourceComp::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("ROS2 bridge integration is not implemented yet."));
    return false;
}

bool URos2SensorBridgeSourceComp::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("ROS2 bridge frame push is not implemented yet."));
    return false;
}

ULivoxLidarSourceComp::ULivoxLidarSourceComp()
{
    SourceKind = ERealSensorSourceKind::LivoxLidar;
    SourceId = TEXT("LivoxLidar");
    LastSourceMessage = TEXT("Livox source is a DT-Project placeholder. Livox SDK integration is not implemented yet.");
}

bool ULivoxLidarSourceComp::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Livox SDK integration is not implemented yet."));
    return false;
}

bool ULivoxLidarSourceComp::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Livox frame push is not implemented yet."));
    return false;
}

URealSenseCameraSourceComp::URealSenseCameraSourceComp()
{
    SourceKind = ERealSensorSourceKind::RealSenseCamera;
    SourceId = TEXT("RealSenseCamera");
    LastSourceMessage = TEXT("RealSense source is a DT-Project placeholder. RealSense SDK integration is not implemented yet.");
}

bool URealSenseCameraSourceComp::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("RealSense SDK integration is not implemented yet."));
    return false;
}

bool URealSenseCameraSourceComp::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("RealSense frame push is not implemented yet."));
    return false;
}
