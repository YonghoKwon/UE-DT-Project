# Editor Smoke Test

This checklist verifies the current virtual camera, virtual LiDAR, point-cloud-only view, preview policy, and Slab analysis path.

## Map Setup

Use `BasicMap`, `TestMap`, or a small dedicated validation map.

Required actors/components:

```text
AVirtualSensorManager
AVirtualSensorAct
AVirtualCameraAct
WBP_VirtualSensorMonitor
```

Recommended manager settings:

```text
bDiscoverOnBeginPlay = true
bStartSensorsOnBeginPlay = true
SharedSensorTransport.TransportMode = LogOnly
bPointCloudOnlyHideWorld = true
bPointCloudOnlyAutoSelectLidarView = true
```

Recommended LiDAR settings:

```text
SimulationQuality = RealTimePreview
bUseMultiHit = false
bDrawDebugRays = false
ExportOnScan = false
ServerPayloadStride = 1
MaxServerPayloadPoints = 0
bIncludeMissPointsInServerPayload = false
PreviewPointStride = 2
MaxPreviewPoints = 5000
bPointCloudPreviewHitOnly = true
```

## Slab Setup

At least one actor or component should match a Slab semantic rule.

Recommended actor tag:

```text
Slab
```

Recommended LiDAR settings:

```text
SlabSemanticLabel = Slab
ReferenceSlabYawDegrees = 0
MinSlabPointsForAnalysis = 12
bIncludeSlabAnalysisInPayload = true
```

## Checks

1. Start PIE.
2. Confirm the monitor shows selected camera and LiDAR status.
3. Click or call `CaptureSelectedOnce`.
4. Confirm LiDAR `FrameId` increments.
5. Confirm `Server Payload` count and `Preview` count are shown separately.
6. Toggle point-cloud-only mode.
7. Confirm world visibility hides, collision-based LiDAR hits continue, and point cloud remains visible.
8. Click or call preview more/less.
9. Confirm preview count or preview policy changes without changing server payload policy.
10. Confirm Slab status is valid when enough tagged points are hit.

## Automation Tests

Run the full local smoke gate:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

Use `-SkipBuild` when the editor binary was already built:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1" -SkipBuild
```

When `-SkipBuild` is not used, the script checks for a running `UnrealEditor.exe` before building. Close the editor first to avoid Live Coding or DLL lock failures. Use `-AllowOpenEditor` only when you intentionally want to bypass this guard.

Replay tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorReplay; Quit" -TestExit="Automation Test Queue Empty"
```

`M7AT10.SensorReplay.TransportSaveToFilePayload` verifies that a replay-injected LiDAR server payload is saved through `UVirtualSensorDataTransportComp` in `SaveToFile` mode and that the file content matches `GetLastJsonPayload()`.

Recorder save/load tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorRecorder; Quit" -TestExit="Automation Test Queue Empty"
```

`M7AT10.SensorRecorder.SaveLoadSession` verifies that recorded camera/LiDAR JSON frames are saved to a session file, loaded back, and exposed through `GetRecordedFrame()`.

Map asset and sensor composition smoke tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.EditorSmoke; Quit" -TestExit="Automation Test Queue Empty"
```

`M7AT10.EditorSmoke.MapAssetsLoad` verifies `BasicMap` and `TestMap` can load in headless editor automation.
`M7AT10.EditorSmoke.MapSensorComposition` verifies the validation map set includes at least one `AVirtualSensorManager`, `AVirtualSensorAct`, and `AVirtualCameraAct`.

Real sensor source base tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.RealSensorSource; Quit" -TestExit="Automation Test Queue Empty"
```

WebSocket live LiDAR sample contract:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_websocket_lidar_live_sample.ps1"
```

After the project WebSocket data table includes `LIDAR_JSON_LIVE_FRAME`, send
`Samples/websocket/lidar_json_live_frame_sample.json` through the deployment
WebSocket broker in PIE. Confirm the matching `ULidarJsonLiveSourceComp` updates
its source frame/point counts and that the target LiDAR exposes a cached server
payload when `SEND_TRANSPORT` is enabled.

Sensor manager point-cloud-only policy tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorManager; Quit" -TestExit="Automation Test Queue Empty"
```

Monitor host fallback tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorMonitor; Quit" -TestExit="Automation Test Queue Empty"
```

`M7AT10.SensorManager.PointCloudOnlyPreservesPayloadPolicy` verifies that point-cloud-only mode changes preview density and selected preview visibility without changing LiDAR server payload policy.
`M7AT10.SensorManager.SharedServicesAssigned` verifies that registered camera and LiDAR components receive the manager's shared transport and recorder components.
`M7AT10.RealSensorSource.JsonLiveBridgePushFrame` verifies that buffered JSON live LiDAR lines can be converted into `FVirtualLidarPoint` frames and injected into the target LiDAR through the normalized real-sensor handoff path.
`M7AT10.RealSensorSource.JsonLiveTransactionParse` verifies that `LIDAR_JSON_LIVE_FRAME` WebSocket payloads can be parsed into JSON live LiDAR lines before DTCore dispatches them on the game thread, and that empty or whitespace-only live frame payloads are rejected before routing.
`M7AT10.SensorMonitor.CameraStatusTextContract` verifies that the monitor camera view exposes sensor id, `virtual-camera.v1`, resolution, capture mode, cached payload state, and server payload export hint.
`M7AT10.SensorMonitor.LidarStatusTextContract` verifies that the monitor status includes sensor id, frame id, scan/ray counts, server payload count/byte size, preview count, Slab analysis, warning, view mode, and CSV row/return contract.
`M7AT10.SensorMonitor.ServerPayloadExport` verifies that monitor server payload export writes a JSON file matching the cached LiDAR payload. Camera payload export uses the same monitor export function, but should also be checked in PIE because render-target readback is renderer-dependent.

Static preview-policy readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_point_cloud_preview_policy.ps1"
```

This check keeps the current CPU/instance preview safety policy explicit until a
GPU/Niagara/custom high-density renderer is selected.

Manual PIE payload checks:

- In local monitor capture mode, confirm that a failed or skipped camera
  readback does not leave `Capture Pending: Camera=true` forever and that the
  next capture interval can continue.

```text
1. Open the monitor in camera view and press Export Payload.
2. Confirm Saved/SensorCaptures/<CameraSensorId>/ServerPayload contains a virtual-camera.v1 JSON file.
3. Switch to LiDAR view and press Export Payload.
4. Confirm Saved/SensorCaptures/<LidarSensorId>/ServerPayload contains a virtual-lidar.v1 JSON file.
```

Local project status and asset decision inventory:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

Use this before committing local Unreal assets so `WBP_VirtualSensorMonitor.uasset`, environment packs, packaged `Windows` output, and launcher files are intentionally included or left untracked.
The report classifies each path as a review candidate, large content candidate, sample/third-party candidate, generated output, or local config so packaging outputs are not mixed into code commits by accident.
Use `-Json` when an automated job needs machine-readable output. Use `-FailOnGeneratedOutput` when packaged output directories or archives should fail the validation gate. Use `-FailOnCategory` when a gate should fail on a specific decision category such as `LargeContentCandidate` or `SampleOrThirdParty`.
`Windows/`, `Windows.zip`, and `launcher.config.json` are ignored by git, but this report still checks their filesystem presence so local generated output is visible during handoff.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```

## Expected Warnings

The current DTCore plugin may emit:

```text
Plugin 'DTCore' does not list plugin 'EnhancedInput' as a dependency
```

This is outside the DT-Project-only change scope.
