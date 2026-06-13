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

Replay tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorReplay; Quit" -TestExit="Automation Test Queue Empty"
```

Real sensor source base tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.RealSensorSource; Quit" -TestExit="Automation Test Queue Empty"
```

## Expected Warnings

The current DTCore plugin may emit:

```text
Plugin 'DTCore' does not list plugin 'EnhancedInput' as a dependency
```

This is outside the DT-Project-only change scope.
