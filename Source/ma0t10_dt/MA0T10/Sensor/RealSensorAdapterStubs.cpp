#include "ma0t10_dt/MA0T10/Sensor/RealSensorAdapterStubs.h"

URos2SensorBridgeSourceComponent::URos2SensorBridgeSourceComponent()
{
    SourceKind = ERealSensorSourceKind::Ros2Bridge;
    SourceId = TEXT("ROS2Bridge");
    LastSourceMessage = TEXT("ROS2 bridge source is a DT-Project placeholder. SDK/bridge integration is not implemented yet.");
}

bool URos2SensorBridgeSourceComponent::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("ROS2 bridge integration is not implemented yet."));
    return false;
}

bool URos2SensorBridgeSourceComponent::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("ROS2 bridge frame push is not implemented yet."));
    return false;
}

ULivoxLidarSourceComponent::ULivoxLidarSourceComponent()
{
    SourceKind = ERealSensorSourceKind::LivoxLidar;
    SourceId = TEXT("LivoxLidar");
    LastSourceMessage = TEXT("Livox source is a DT-Project placeholder. Livox SDK integration is not implemented yet.");
}

bool ULivoxLidarSourceComponent::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Livox SDK integration is not implemented yet."));
    return false;
}

bool ULivoxLidarSourceComponent::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Livox frame push is not implemented yet."));
    return false;
}

URealSenseCameraSourceComponent::URealSenseCameraSourceComponent()
{
    SourceKind = ERealSensorSourceKind::RealSenseCamera;
    SourceId = TEXT("RealSenseCamera");
    LastSourceMessage = TEXT("RealSense source is a DT-Project placeholder. RealSense SDK integration is not implemented yet.");
}

bool URealSenseCameraSourceComponent::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("RealSense SDK integration is not implemented yet."));
    return false;
}

bool URealSenseCameraSourceComponent::PushFrameOnce(bool bSendTransport)
{
    SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("RealSense frame push is not implemented yet."));
    return false;
}