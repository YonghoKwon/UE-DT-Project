#pragma once

#include "CoreTypes.h"

// Literal-only defaults. No project services, UObject types, or runtime policy.
namespace VirtualSensorDefaults
{
inline constexpr TCHAR CameraTopic[] = TEXT("topic.virtual.sensor.camera.0");
inline constexpr TCHAR LidarTopic[] = TEXT("topic.virtual.sensor.lidar.0");
inline constexpr TCHAR PointCloudTopic[] = TEXT("topic.virtual.sensor.export.0");
// Default Rev7 x3 measured PCD demand is about 75.9 MiB/s at 20 Hz.
inline constexpr float PointCloudBandwidthMiBPerSecond = 80.0f;

inline constexpr TCHAR UiSlotV1[] = TEXT("MA0T10_VirtualSensorUI_v1");
inline constexpr TCHAR UiSlotV2[] = TEXT("MA0T10_VirtualSensorUI_v2");
inline constexpr TCHAR UiSlotV3[] = TEXT("MA0T10_VirtualSensorUI_v3");
inline constexpr TCHAR UiSlotV4[] = TEXT("MA0T10_VirtualSensorUI_v4");
inline constexpr TCHAR UiSlotV5[] = TEXT("MA0T10_VirtualSensorUI_v5");
inline constexpr TCHAR UiSlotV6[] = TEXT("MA0T10_VirtualSensorUI_v6");
inline constexpr TCHAR WorkspaceSlot[] = TEXT("MA0T10_SensorToolWorkspace_v1");
inline constexpr TCHAR AppearanceSlot[] = TEXT("MA0T10_SensorToolAppearance_v1");

inline constexpr TCHAR CaptureDirectory[] = TEXT("SensorCaptures");
inline constexpr TCHAR RecordingDirectory[] = TEXT("SensorRecordings");
inline constexpr TCHAR PlacementDirectory[] = TEXT("SensorPlacements");
inline constexpr TCHAR LayoutDirectory[] = TEXT("SensorLayouts");
inline constexpr TCHAR StreamTempDirectory[] = TEXT("SensorStreamTemp");
inline constexpr TCHAR ReportsDirectory[] = TEXT("Reports");
inline constexpr TCHAR ScenarioRunReportsDirectory[] = TEXT("Reports/ScenarioRuns");
inline constexpr TCHAR TimedCaptureDirectory[] = TEXT("SensorCaptures/LocalTimedCapture");
inline constexpr TCHAR TimedCaptureAutomationDirectory[] = TEXT("SensorCaptures/LocalTimedCapture/Automation");
inline constexpr TCHAR PointCloudSubdirectory[] = TEXT("PointCloud");
inline constexpr TCHAR ServerPayloadSubdirectory[] = TEXT("ServerPayload");
}
