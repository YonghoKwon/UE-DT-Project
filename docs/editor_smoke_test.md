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
`M7AT10.SensorMonitor.LidarStatusTextContract` verifies that the monitor status includes sensor id, frame id, scan/ray counts, server payload count, preview count, Slab analysis, warning, view mode, and CSV row contract.

Local project status and asset decision inventory:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

Use this before committing local Unreal assets so `WBP_VirtualSensorMonitor.uasset`, environment packs, packaged `Windows` output, and launcher files are intentionally included or left untracked.

## Expected Warnings

The current DTCore plugin may emit:

```text
Plugin 'DTCore' does not list plugin 'EnhancedInput' as a dependency
```

This is outside the DT-Project-only change scope.
