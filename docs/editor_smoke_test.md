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

Outbound transport tests:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.SensorTransport; Quit" -TestExit="Automation Test Queue Empty"
```

`M7AT10.SensorTransport.HttpPostLoopbackAcceptance` verifies the outbound
judging-server `HttpPost` path against a localhost mock route, including sensor
headers, JSON content type, `virtual-lidar.v1` body identity, 2xx acceptance,
non-2xx rejection, status code, and response body capture.

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
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1" -NoWrite
```

The registration report is a static checklist for
`/Game/M7AT10/Common/DataTables/DT_TransactionCode`. It expects a row named
`LIDAR_JSON_LIVE_FRAME` with `TransactionCodeMessageClass` set to
`/Script/m7at10_dt.LidarJsonLiveFrameTC`. It intentionally does not mutate the
binary `.uasset`; use it as the pre-editor evidence, then verify the row in
Unreal Editor. Use `-NoWrite` for read-only readiness checks.

Optional data-table registration evidence test:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.Evidence.WebSocketTransactionRegistration; Quit" -TestExit="Automation Test Queue Empty"
```

This test is intentionally separate from `M7AT10.RealSensorSource` because it
verifies the binary data-table asset. Run it after the row has been added or
confirmed in Unreal Editor.

If the row is missing, create it through the DT-Project commandlet:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -run=EnsureLidarJsonLiveFrameTransaction -unattended -nop4
```

After the project WebSocket data table includes `LIDAR_JSON_LIVE_FRAME`, send
`Samples/websocket/lidar_json_live_frame_sample.json` through the deployment
WebSocket broker in PIE. Confirm the matching `ULidarJsonLiveSourceComp` updates
its source frame/point counts and that the target LiDAR exposes a cached server
payload when `SEND_TRANSPORT` is enabled.

Record the broker smoke result:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_broker_smoke_report.ps1" -BrokerUrl "ws://host:61616" -Topic "topic.cep.output.0" -ObservedSourceFrame -ObservedTargetPoints -ObservedCachedPayload -Operator "name"
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_websocket_lidar_smoke_evidence.ps1" -ProjectRoot "C:\path\to\m7at10_dt" -RunCommandletDryRun -RunEvidenceAutomation -RunBrokerlessDTCoreDispatchAutomation -WriteReports -ObservedSourceFrame -ObservedTargetPoints -ObservedCachedPayload -Operator "name"
```

Use `-NoWrite` for a read-only prerequisite check. The report does not connect to
the broker; it captures evidence from a real PIE/broker run.

Before data-table registration, use the same component-level smoke path:

```text
ULidarJsonLiveSourceComp -> AppendSampleWebSocketFrameInEditor
ULidarJsonLiveSourceComp -> PushBufferedFrameNoTransportInEditor
```

`AppendSampleWebSocketFrameInEditor` replaces the current live buffer with the
checked sample payload. The no-transport push verifies sample payload parsing and
target LiDAR handoff without sending to the judging server.
HTTP, UDP, or Blueprint bridge prototypes can call
`ULidarJsonLiveSourceComp::AppendLivePayloadJson` with the same payload shape,
then call `PushFrameOnce(false)` for a no-transport local handoff check.
`M7AT10.RealSensorSource.HttpJsonLiveBridgePayload` verifies that the optional
HTTP wrapper reuses this handoff path, supports auto-push and buffer-only
modes, and stays separate from outbound judging-server HTTP POST transport.
`M7AT10.RealSensorSource.HttpJsonLiveBridgeLoopbackPost` verifies the actual
localhost HTTP POST route using Unreal's HTTP client and waits for HTTP 202 plus
target LiDAR point updates.
`M7AT10.RealSensorSource.UdpJsonLiveBridgePayload` verifies that the optional
UDP wrapper reuses this handoff path without opening a real test port.
`M7AT10.RealSensorSource.UdpJsonLiveBridgeDatagram` verifies the local socket
path with a real loopback UDP datagram and an ephemeral port.

Brokerless DTCore dispatch automation:

```text
M7AT10.RealSensorSource.JsonLiveDTCoreDispatch
```

This PIE automation starts a GameInstance-backed world, queues the checked
`LIDAR_JSON_LIVE_FRAME` sample through `UDxDataSubsystem::EnqueueWebSocketData`,
and waits for the matching `ULidarJsonLiveSourceComp` and target LiDAR to update.
It does not replace the deployment broker smoke because it bypasses STOMP
endpoint, credential, subscription, and network receive checks.

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
`M7AT10.RealSensorSource.CameraJsonLiveBridgePushFrame` verifies that an external `virtual-camera.v1` JSON payload can be injected into a target `UVirtualCameraComp` without renderer-dependent capture.
`M7AT10.RealSensorSource.HttpJsonLiveBridgePayload` verifies that the inbound HTTP JSON live wrapper feeds the same shared payload shape into the normalized handoff path without requiring a real listener in automation.
`M7AT10.RealSensorSource.HttpJsonLiveBridgeLoopbackPost` verifies that the inbound HTTP JSON live wrapper can accept a real loopback POST through `HTTPServer` and return an accepted response after target LiDAR handoff.
`M7AT10.RealSensorSource.JsonLiveTransactionParse` verifies that `LIDAR_JSON_LIVE_FRAME` WebSocket payloads can be parsed into JSON live LiDAR lines before DTCore dispatches them on the game thread, and that empty or whitespace-only live frame payloads are rejected before routing.
`M7AT10.RealSensorSource.JsonLiveTransactionRouting` verifies that the WebSocket transaction handler routes by `SOURCE_ID`, honors `PUSH_FRAME=false`, rejects ambiguous no-`SOURCE_ID` multi-source routing, and can push a matched frame into the target LiDAR.
`M7AT10.Evidence.WebSocketTransactionRegistration` verifies that the configured DTCore WebSocket data table contains the `LIDAR_JSON_LIVE_FRAME` row, that its handler class resolves to `ULidarJsonLiveFrameTC`, and that a row-instantiated handler can parse/process a sample payload into `ULidarJsonLiveSourceComp`.
`M7AT10.SensorMonitor.CameraStatusTextContract` verifies that the monitor camera view exposes sensor id, `virtual-camera.v1`, resolution, capture mode, cached payload state, and server payload export hint.
`M7AT10.SensorMonitor.LidarStatusTextContract` verifies that the monitor status includes sensor id, frame id, scan/ray counts, server payload count/byte size, preview count, Slab analysis, warning, view mode, and CSV row/return contract.
`M7AT10.SensorMonitor.ServerPayloadExport` verifies that monitor server payload export writes a JSON file matching the cached LiDAR payload. Camera payload export uses the same monitor export function, but should also be checked in PIE because render-target readback is renderer-dependent.

Static preview-policy readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_point_cloud_preview_policy.ps1"
```

This check keeps the current CPU/instance preview safety policy explicit until a
GPU/Niagara/custom high-density renderer is selected.

Headless CSV preview coverage:

```text
Automation RunTests M7AT10.Sensor.CsvPointCloudPreview
```

Full headless command:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.Sensor.CsvPointCloudPreview; Quit" -TestExit="Automation Test Queue Empty"
```

`M7AT10.Sensor.CsvPointCloudPreview.ProceduralHighDensityLoad` verifies a
120,000-point procedural CSV preview load without opening the Unreal Editor GUI.
`M7AT10.Sensor.CsvPointCloudPreview.InstancedBatchLoad` keeps the instanced
fallback path covered.
`M7AT10.Sensor.CsvPointCloudPreview.ProceduralPerformanceBudget` verifies a
250,000-point procedural CSV preview load with a generous timing regression
guard and records parse/build/load telemetry.

The CSV preview actor exposes `GetLastPreviewTelemetryText()` plus
`LastInputLineCount`, `LastAcceptedPointCount`, `LastPreviewSectionCount`,
`LastPreviewInstanceCount`, `LastRenderModeName`, `LastPreviewStatus`,
`LastParseDurationMs`, `LastBuildDurationMs`, and `LastLoadDurationMs` so
headless runs can distinguish renderer counts from observational timing data.

After the headless CSV preview automation run, export the latest local telemetry
evidence:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_csv_preview_performance_report.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_csv_preview_performance_report.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -RequireAutomationSuccess
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_csv_preview_performance_report.ps1" -LogPath "C:\Unreal Projects\m7at10_dt\Saved\Logs\m7at10_dt.log" -RequireAutomationSuccess
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_csv_preview_performance_report.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -MarkdownPath ".\Saved\Reports\csv_preview_performance.md" -JsonPath ".\Saved\Reports\csv_preview_performance.json"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_decision_report.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -RequireCsvPerformanceEvidence
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_decision_report.ps1" -LogPath "C:\Unreal Projects\m7at10_dt\Saved\Logs\m7at10_dt.log" -RequireCsvPerformanceEvidence
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_csv_preview_performance_evidence.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -SkipBuild
```

This report proves the current CPU preview fallback telemetry was emitted from
the automation log and that the automation run completed successfully. It does
not replace the future GPU/Niagara viewport smoke evidence. The renderer
decision report consumes the same local evidence and sets
`CpuFallbackPerformanceEvidencePresent` when the instanced, 120,000-point
procedural, and 250,000-point procedural budget scenarios plus
scenario-specific success lines and `TEST COMPLETE. EXIT CODE: 0` are all
present inside the selected CSV preview run block. The generated Markdown and
JSON include `EvidenceRunStartLine`, each metric `EvidenceLine`, each
scenario success line, `TestCompleteLine`, and `EvidenceLinesWithinRun`.

GPU/Niagara renderer smoke:

```text
1. Open the target map with the candidate GPU/Niagara point renderer enabled.
2. Load or replay a dense LiDAR frame and record map name, sensor id, renderer name, preview point count, and server payload point count.
3. Capture a viewport screenshot and verify it contains nonblank point pixels, not only UI or an empty background.
4. Toggle or force the CPU/ISM fallback path and confirm it still renders a usable preview.
5. Record whether the dense frame caused an editor stall, overlap, clipping, or monitor UI obstruction.
6. Export the renderer decision report with the viewport smoke fields.
```

Example evidence command after a GPU path exists:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_decision_report.ps1" -ViewportScreenshotPath "C:\path\to\gpu_viewport.png" -ViewportScreenshotBytes 123456 -NonBlankPixelCount 1000 -GpuSmokePointCount 120000 -GpuSmokeMapName "TestMap" -GpuSmokeSensorId "Lidar01" -GpuSmokeRendererName "Niagara point renderer" -GpuSmokeOperator "name" -GpuSmokeNotes "dense frame viewport smoke" -ObservedDenseFrameNoStall -ObservedFallbackToggle
```

Until a GPU renderer is actually integrated, the renderer decision report should
remain in `RendererPhase = PreGpuSpike`. After GPU code or assets are detected,
it should move to `GpuIntegratedEvidencePending` until viewport smoke, fallback
preservation, and dense-frame evidence are all recorded.

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
