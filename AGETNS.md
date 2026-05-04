# AGETNS.md - Real Sensor Performance Optimization Branch Notes

> Branch: `feature/real-sensor-performance-optimizations`  
> Base branch: `feature/real-sensor-device-profiles`  
> Project: UE-DT-Project / Unreal Engine Digital Twin virtual sensor platform

This document summarizes the current implementation state of the branch and gives future contributors or automation agents practical guidance for modifying the virtual camera / LiDAR sensor system safely.

## 1. Branch purpose

This branch continues the real sensor profile work and focuses on solving a major runtime performance issue: applying real hardware specifications directly to Unreal simulation caused the editor/runtime to become too slow.

The key design decision is:

```text
DeviceProfile = real hardware metadata and physical expectations
SimulationQuality = runtime simulation cost and preview/capture quality
```

Real-world sensor identity, FOV, range, frame metadata, and payload structure should remain aligned with the target hardware, but Unreal should not necessarily execute the full real-world point rate or camera frame rate during interactive preview.

## 2. Target real-world devices

### Camera

- Manufacturer: Intel
- Model: RealSense D455
- Implemented enum: `EVirtualCameraDeviceProfile::IntelRealSenseD455`
- Component: `UVirtualCameraComp`

### LiDAR

- Manufacturer: Livox
- Model: Mid-360S / Mid-360S-style profile
- Implemented enum: `EVirtualLidarDeviceProfile::LivoxMid360S`
- Component: `UVirtualLidarSensorComp`

The project currently simulates these devices. It does not yet read real hardware data through Intel RealSense SDK, Livox SDK, ROS2, UDP packets, or vendor bridge middleware.

## 3. Main architectural components

### `AVirtualSensorManager`

Path:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.cpp
```

Responsibilities:

- Discover virtual camera and LiDAR components in the level.
- Register cameras and LiDAR sensors.
- Maintain selected camera and selected LiDAR.
- Bind the selected sensors to `UVirtualSensorMonitorWidget`.
- Start/stop all sensors.
- Run synchronized capture if enabled.
- Provide sensor summaries and health summaries.
- Own shared runtime services:
  - `SharedSensorTransport`
  - `SharedSensorRecorder`

Important properties:

```cpp
bDiscoverOnBeginPlay
bStartSensorsOnBeginPlay
bUseSynchronizedCapture
SynchronizedInterval
StaleSensorSeconds
SharedTransportComponent
SharedRecorderComponent
```

Current behavior:

```text
AVirtualSensorManager
├─ SharedSensorTransport
├─ SharedSensorRecorder
├─ registered UVirtualCameraComp instances
└─ registered UVirtualLidarSensorComp instances
```

When a camera or LiDAR is registered, the manager injects both shared services into the sensor component.

### `UVirtualCameraComp`

Path:

```text
Source/m7at10_dt/M7AT10/Camera/VirtualCameraComp.h
Source/m7at10_dt/M7AT10/Camera/VirtualCameraComp.cpp
```

Responsibilities:

- Scene capture via `USceneCaptureComponent2D`.
- RenderTarget creation and management.
- JPEG compression and Base64 encoding.
- JSON payload generation.
- Transport dispatch through `UVirtualSensorDataTransportComp`.
- Recording through `UVirtualSensorRecorderComp`.
- Runtime status updates for UI/health monitoring.

Important settings:

```cpp
DeviceProfile = EVirtualCameraDeviceProfile::IntelRealSenseD455
SimulationQuality = EVirtualSensorSimulationQuality::RealTimePreview
CaptureMode = EVirtualCameraCaptureMode::PayloadAndOutput
CaptureResolution
CaptureInterval
JpegQuality
bApplyDeviceProfileOnBeginPlay
bAutoRegisterToManager
TransportComponent
RecorderComponent
```

Camera capture modes:

```text
PreviewOnly      = RenderTarget preview only; no JSON/JPEG payload generation.
Payload          = Build JSON payload but do not dispatch via output mode.
PayloadAndOutput = Build payload and dispatch through transport/output path.
```

### `UVirtualLidarSensorComp`

Path:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h
Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.cpp
```

Responsibilities:

- LiDAR-style ray sampling using Unreal collision traces.
- Optional multi-hit tracing.
- Heatmap/texture generation for widget preview.
- JSON payload generation.
- Point cloud export as CSV / JSONL / PCD.
- Transport dispatch through `UVirtualSensorDataTransportComp`.
- Recording through `UVirtualSensorRecorderComp`.
- Runtime status updates for UI/health monitoring.

Important settings:

```cpp
DeviceProfile = EVirtualLidarDeviceProfile::LivoxMid360S
SimulationQuality = EVirtualSensorSimulationQuality::RealTimePreview
PayloadPointStride
MaxPayloadPoints
bIncludeMissPointsInPayload
HorizontalSamples
VerticalChannels
ScanInterval
HorizontalFov
MinVerticalAngle
MaxVerticalAngle
MaxDistance
bUseMultiHit
MaxHitsPerRay
bFlipLidarViewHorizontal
bFlipLidarViewVertical
bExportCsvOnScan
bExportJsonLinesOnScan
bExportPcdOnScan
IgnoreActorTags
TransportComponent
RecorderComponent
```

### `UVirtualSensorDataTransportComp`

Path:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.cpp
```

Responsibilities:

- Centralized JSON and binary data output.
- Log-only output.
- Save-to-file output.
- HTTP POST output.
- Store last transport result for UI/runtime status.

Transport modes:

```text
None
LogOnly
SaveToFile
HttpPost
```

Important APIs:

```cpp
FVirtualSensorTransportResult SendJson(...)
FVirtualSensorTransportResult SendBinary(...)
```

### `UVirtualSensorRecorderComp`

Path:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorRecorderComp.h
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorRecorderComp.cpp
```

Responsibilities:

- Record camera/LiDAR JSON payload frames.
- Save recording sessions to JSON.
- Load session files.
- Replay recorded frames by broadcasting `OnReplayFrame`.

Important APIs:

```cpp
StartRecording()
StopRecording()
ClearRecording()
RecordJsonFrame(...)
SaveSession(...)
LoadSessionFromFile(...)
StartReplay(...)
StopReplay()
```

Current replay limitation:

```text
Replay currently emits recorded payload frames as events.
It does not yet reconstruct camera images or LiDAR heatmaps into the live monitor widget automatically.
```

## 4. Performance model

### Why the branch exists

A full Livox-style configuration such as:

```text
HorizontalSamples = 360
VerticalChannels = 60
ScanInterval = 0.1 sec
```

produces:

```text
360 * 60 = 21,600 traces per scan
21,600 * 10Hz = 216,000 traces/sec
```

This is expensive in Unreal because each simulated LiDAR point requires collision/ray work, result processing, payload construction, optional texture updates, UI refresh, export, transport, and recording.

The same applies to camera simulation. A full D455-style `1280x720 @ 30fps` capture with RenderTarget readback, JPEG compression, and Base64 encoding is much heavier than merely displaying a camera view.

### Simulation quality presets

Defined in:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDeviceProfileTypes.h
```

Enum:

```cpp
EVirtualSensorSimulationQuality
```

Values:

```text
Debug
RealTimePreview
Balanced
FullSpec
Custom
```

### Recommended runtime defaults

Use these for editor and packaged smoke tests:

```text
Camera SimulationQuality = RealTimePreview
LiDAR SimulationQuality  = RealTimePreview
TransportMode            = LogOnly
LiDAR MultiHit           = false
LiDAR ExportOnScan       = false
Camera CaptureMode       = PayloadAndOutput, or PreviewOnly for visual-only tests
```

### FullSpec warning

`FullSpec` exists for short validation passes only. Do not use it as the default editor/runtime mode unless the machine is known to handle it.

## 5. Current recommended editor setup

Place the following actors in the level:

```text
AVirtualSensorManager
AVirtualCameraAct
AVirtualSensorAct
```

### Camera settings

Select `AVirtualCameraAct`, then configure `VirtualCameraComp`:

```text
DeviceProfile = IntelRealSenseD455
SimulationQuality = RealTimePreview
bApplyDeviceProfileOnBeginPlay = true
CaptureMode = PayloadAndOutput
bAutoRegisterToManager = true
```

For performance troubleshooting:

```text
CaptureMode = PreviewOnly
SimulationQuality = Debug
```

### LiDAR settings

Select `AVirtualSensorAct`, then configure `LidarSensorComp`:

```text
DeviceProfile = LivoxMid360S
SimulationQuality = RealTimePreview
bApplyDeviceProfileOnBeginPlay = true
bAutoRegisterToManager = true
bUseMultiHit = false
bDrawDebugRays = false
bExportCsvOnScan = false
bExportJsonLinesOnScan = false
bExportPcdOnScan = false
```

For an inverted LiDAR monitor view, test these flags:

```text
bFlipLidarViewVertical
bFlipLidarViewHorizontal
```

Current default is vertical flip enabled.

### Manager settings

Select `AVirtualSensorManager`:

```text
bDiscoverOnBeginPlay = true
bStartSensorsOnBeginPlay = true
SharedSensorTransport.TransportMode = LogOnly
```

For file output:

```text
SharedSensorTransport.TransportMode = SaveToFile
```

For HTTP output:

```text
SharedSensorTransport.TransportMode = HttpPost
SharedSensorTransport.HttpEndpoint = http://host:port/path
```

## 6. Widget setup

Main widget class:

```text
UVirtualSensorMonitorWidget
```

Expected optional widget names in Designer:

```text
TitleText
ViewImage
StatusText
ToggleButton
ToggleButtonText
NextCameraButton
NextLidarButton
```

All named widgets should be marked `Is Variable` when used in Blueprint.

Recommended Level Blueprint flow:

```text
Event BeginPlay
→ Create Widget: WBP_VirtualSensorMonitor
→ Add to Viewport
→ Get Actor of Class: AVirtualSensorManager
→ BindSensorManager
```

Avoid directly binding camera/LiDAR from Level Blueprint when the manager is present. Let `AVirtualSensorManager` choose and bind the selected sensors.

## 7. File output locations

Runtime saved data generally goes under the project or packaged app `Saved` directory.

Examples:

```text
Saved/SensorCaptures/{SensorId}/
Saved/SensorCaptures/{SensorId}/PointCloud/
Saved/SensorRecordings/
```

Generated artifacts may include:

```text
*.json   sensor payloads / recording sessions
*.jpg    camera frames
*.csv    LiDAR point cloud export
*.jsonl  LiDAR point cloud export
*.pcd    LiDAR point cloud export
```

## 8. Packaging guidance

The virtual sensor path is intended to be package-friendly. However, always perform a local package build before merging or releasing.

Recommended first packaging smoke test:

```text
Camera SimulationQuality = RealTimePreview
LiDAR SimulationQuality = RealTimePreview
TransportMode = LogOnly
MultiHit = false
ExportOnScan = false
```

Then test heavier modes one by one:

```text
SaveToFile
HttpPost
Balanced
FullSpec
MultiHit
ExportOnScan
```

Do not enable all heavy options at the same time when diagnosing performance or packaging issues.

## 9. Known limitations

### Real hardware is not integrated yet

Current implementation is simulation-based:

```text
Camera = Unreal SceneCapture-based virtual camera
LiDAR  = Unreal trace-based virtual LiDAR
```

Not yet implemented:

```text
Intel RealSense SDK input
Livox SDK input
ROS2 bridge input
UDP point cloud input
Real hardware timestamp synchronization
Real hardware calibration import
```

### Replay is data-event based

`UVirtualSensorRecorderComp` can record/save/load/replay payload frames, but the replay path currently broadcasts payload events. It does not yet automatically reconstruct visual camera frames or LiDAR heatmap textures in the live monitor widget.

### FullSpec is not real-time guaranteed

Even with a high-end PC, full hardware-equivalent simulation can be expensive because Unreal is computing scene intersection and payload generation, not reading precomputed hardware sensor frames.

## 10. Development rules for future changes

### Keep device profile and simulation quality separate

Do not make real hardware profile application directly force expensive runtime sampling by default.

Good:

```text
DeviceProfile = IntelRealSenseD455
SimulationQuality = RealTimePreview
```

Bad:

```text
DeviceProfile = IntelRealSenseD455 automatically forces 1280x720 @ 30fps at all times
```

Good:

```text
DeviceProfile = LivoxMid360S
SimulationQuality = RealTimePreview
```

Bad:

```text
DeviceProfile = LivoxMid360S automatically forces 216,000 traces/sec in editor
```

### Prefer manager-owned shared services

Use `AVirtualSensorManager` to provide shared services:

```text
SharedSensorTransport
SharedSensorRecorder
```

Avoid each sensor creating unrelated transport/recorder instances unless there is a strong reason.

### Avoid heavy defaults

Defaults should keep the editor responsive.

Preferred defaults:

```text
RealTimePreview
LogOnly
MultiHit false
ExportOnScan false
DrawDebugRays false
```

### Make expensive features explicit

Expensive operations should be opt-in:

```text
FullSpec
MultiHit
CSV/JSONL/PCD ExportOnScan
HTTP high-rate streaming
High-resolution JPEG payloads
```

### Keep UI resilient

`UVirtualSensorMonitorWidget` should continue using optional widget bindings. Avoid requiring every optional button/text field to exist in the Blueprint widget.

## 11. Quick troubleshooting

### The editor becomes slow

Try in this order:

```text
1. LiDAR SimulationQuality = Debug
2. Camera CaptureMode = PreviewOnly
3. SharedSensorTransport.TransportMode = LogOnly
4. LiDAR bUseMultiHit = false
5. Disable all ExportOnScan flags
6. Disable bDrawDebugRays
```

### LiDAR view looks upside down

Toggle:

```text
bFlipLidarViewVertical
```

### LiDAR view looks mirrored

Toggle:

```text
bFlipLidarViewHorizontal
```

### Payload is too large

Adjust:

```text
PayloadPointStride
MaxPayloadPoints
bIncludeMissPointsInPayload
```

### Recording file is empty

Check:

```text
SharedRecorderComponent.StartRecording() was called
Camera/LiDAR are registered to manager
Camera/LiDAR RecorderComponent is assigned
Capture/scan is actually running
```

### HTTP does not send

Check:

```text
SharedSensorTransport.TransportMode = HttpPost
SharedSensorTransport.HttpEndpoint is valid
Network/firewall allows outbound request
Server accepts application/json
```

## 12. Recommended next implementation steps

1. Add a replay visualization mode that reconstructs camera images and LiDAR heatmaps from recorded payloads.
2. Add binary point cloud transport for high-rate LiDAR instead of JSON-only streaming.
3. Add `VirtualSimulation`, `RealHardware`, and `Replay` source modes under the manager.
4. Add RealSense SDK / Livox SDK / ROS2 bridge adapters as separate source components.
5. Add calibration import for real device extrinsics/intrinsics.
6. Add automated smoke test map and documented packaging test procedure.
7. Add UI controls for `SimulationQuality`, recording, replay, export, and transport mode.
8. Add clear warnings in UI when FullSpec or ExportOnScan is enabled.

## 13. Summary

This branch makes the virtual sensor platform more practical by keeping real device identity and payload metadata aligned with target hardware while avoiding full hardware-equivalent simulation cost by default.

The intended operating model is:

```text
RealTimePreview = editor/runtime interactive operation
Balanced        = higher quality preview or short tests
FullSpec        = short validation capture only
Replay          = data-event playback foundation
RealHardware    = future SDK/bridge integration
```

When modifying this branch, preserve this separation between hardware profile fidelity and runtime simulation cost.
