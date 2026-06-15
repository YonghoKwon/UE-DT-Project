# Real Sensor Adapter Plan

This document defines the next implementation path for real sensor input without modifying DTCore in the current scope.

## Goal

Support two runtime paths with the same downstream processing model:

```text
Virtual simulation scan -> normalized sensor frame -> transport/recorder/judgment server
Real sensor input       -> normalized sensor frame -> transport/recorder/judgment server
```

## DT-Project Source Interface

Keep adapter implementations in DT-Project first. If shared abstractions become stable, move them to DTCore later as a separate task.

Current common base:

```text
URealSensorSourceComponent
URealSensorSourceComp
```

Current concrete/placeholder components:

```text
ULidarCsvReplaySourceComp
ULidarJsonLinesReplaySourceComp
UCameraJsonLiveSourceComp
ULidarJsonLiveSourceComp
ULidarHttpJsonLiveSourceComp
ULidarUdpJsonLiveSourceComp
ULidarJsonLiveFrameTC
URos2SensorBridgeSourceComp
ULivoxLidarSourceComp
URealSenseCameraSourceComp
```

`URos2SensorBridgeSourceComp`, `ULivoxLidarSourceComp`, and `URealSenseCameraSourceComp` are intentional placeholders. They expose configuration fields and state messages, but `StartSource` and `PushFrameOnce` return false until SDK/bridge integration is implemented.

Suggested normalized frame structs:

```text
FNormalizedLidarFrame
FNormalizedCameraFrame
FNormalizedSensorFrameMetadata
```

## LiDAR Normalization

Map real LiDAR packets into the virtual LiDAR schema where possible:

```text
frameId
timestampUtc
sensorTransform
pointIndex
row
col
returnIndex
gridCoordValid
gridCoordSource
worldLocation
distance
hit
semanticLabel
```

Adapters should preserve sensor/replay grid coordinates in `FVirtualLidarPoint`
whenever the upstream packet contains them. `row` and `col` describe the source
scan grid, `returnIndex` describes multi-return order for the same ray, and
`gridCoordSource` is emitted as `point_metadata` when that coordinate is
available. Frames that do not carry this metadata are still accepted, but the
server payload marks them as `derived_from_point_index`.

Real sensors may not have `hitActor` or `semanticLabel`. Use:

```text
hitActor = ""
hitActorClass = ""
semanticLabel = Unknown
```

Semantic classification can be added later by geometry matching, zone maps, or perception results.

## Camera Normalization

Map real camera frames into the virtual camera payload model:

```text
frameId
timestampUtc
sensorId
width
height
encoding
imageBase64 or file path
```

`UVirtualCameraComp::InjectExternalJsonPayload` accepts an already-normalized
`virtual-camera.v1` JSON payload from a real camera bridge, validates the
schema, caches it as the camera's latest server payload, updates runtime status,
records the JSON frame when a recorder is assigned, and optionally sends it
through the same JSON transport path as virtual camera captures. It does not
require a render-target readback, does not update the preview render target, and
does not send the optional binary JPEG side channel used by virtual captures.

`UCameraJsonLiveSourceComp` is the first DT-Project camera live bridge. It
buffers one `virtual-camera.v1` payload and pushes it into a target
`UVirtualCameraComp` through `InjectExternalJsonPayload`. This gives future
RealSense, ROS2 image, HTTP, WebSocket, or Blueprint camera adapters a shared
handoff point without modifying DTCore source.

`M7AT10.RealSensorSource.CameraJsonLiveBridgePushFrame` verifies that an
external camera JSON payload reaches the target camera cache and runtime status
without renderer-dependent SceneCapture readback. It also verifies JSON
transport, recorder capture, payload `sensorId`/`frameId` propagation, decoded
JPEG byte count rejection, invalid base64/JPEG rejection, and cached-payload
preservation after rejected frames.

## Implemented First Step

DT-Project now includes file replay adapters:

```text
ULidarCsvReplaySourceComp
ULidarJsonLinesReplaySourceComp
```

Purpose:

```text
CSV point cloud -> FVirtualLidarPoint[] -> URealSensorSourceComp::PushPointFrameToTarget
JSONL point cloud -> FVirtualLidarPoint[] -> URealSensorSourceComp::PushPointFrameToTarget
Buffered JSON live lines -> FVirtualLidarPoint[] -> URealSensorSourceComp::PushPointFrameToTarget
```

This allows saved or bridge-fed measurement data to enter the same LiDAR payload, recorder, transport, preview, and Slab analysis path as a virtual scan.

`PushPointFrameToTarget` is the DT-Project-side normalized LiDAR frame handoff point for SDK adapters. ROS2, Livox, WebSocket, HTTP, UDP, and other future LiDAR sources should normalize packets into `FVirtualLidarPoint[]` and call this helper instead of calling `UVirtualLidarSensorComp::InjectPointCloudFrame` directly. That keeps target resolution, optional LiDAR dimension updates, frame counters, point counts, and source state messages consistent. If a bridge already receives the shared JSON live payload shape, call `ULidarJsonLiveSourceComp::AppendLivePayloadJson` and then `PushFrameOnce` instead of using the WebSocket-named helper directly.

`ULidarJsonLiveSourceComp` is the first live bridge step. It does not open a
socket by itself; instead, a DTCore WebSocket route, HTTP handler, UDP listener,
or Blueprint integration can append one JSON point per line with
`AppendJsonLine`/`AppendJsonLines`, then call `PushFrameOnce`. The pushed frame
uses the same payload, transport, recorder, preview, and Slab analysis path as
file replay and virtual scans.

`ULidarJsonLiveFrameTC` is the DT-Project WebSocket transaction handler for
DTCore's `UDxDataSubsystem`. Register it in the DTCore WebSocket data table with
`TransactionCodeName = LIDAR_JSON_LIVE_FRAME` and
`TransactionCodeMessageClass = ULidarJsonLiveFrameTC`. The handler accepts either
`POINTS`/`points` arrays or a `JSON_LINES`/`jsonLines` string, then forwards the
frame to the matching `ULidarJsonLiveSourceComp`. Set `SOURCE_ID` for normal
operation. If it is omitted, routing is allowed only when exactly one
`ULidarJsonLiveSourceComp` exists in the current World; multiple live sources
without `SOURCE_ID` are rejected to avoid sending a real frame to the wrong
sensor.

Example WebSocket payload:

```json
{
  "MESSAGE_ID": "LIDAR_JSON_LIVE_FRAME",
  "DATA_MAP": {
    "SOURCE_ID": "JsonLiveLidarBridge",
    "SEND_TRANSPORT": true,
    "PUSH_FRAME": true,
    "POINTS": [
      { "row": 0, "col": 0, "returnIndex": 0, "x": 900, "y": -260, "z": 0, "hit": true, "semanticLabel": "Slab" }
    ]
  }
}
```

The checked-in sample lives at:

```text
Samples/websocket/lidar_json_live_frame_sample.json
```

Validate the sample and handler contract before data-table registration:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_websocket_lidar_live_sample.ps1"
```

Export the static registration checklist before touching the binary data table:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1" -NoWrite
```

The registration report writes JSON and Markdown files under
`Saved/WebSocketTransactionRegistration/`. It confirms the configured
`WebSocketDataTable` path, the expected row name
`LIDAR_JSON_LIVE_FRAME`, the expected handler class
`/Script/m7at10_dt.LidarJsonLiveFrameTC`, and the sample payload metadata. It
does not edit or deserialize `DT_TransactionCode.uasset`; verify the binary row
in Unreal Editor before calling the WebSocket route complete.
Use `-NoWrite` when you only need a read-only check and do not want to create
`Saved/WebSocketTransactionRegistration/` artifacts.

After adding or confirming the row, run the optional evidence automation:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -NullRHI -Unattended -NoSplash -NoSound -ExecCmds="Automation RunTests M7AT10.Evidence.WebSocketTransactionRegistration; Quit" -TestExit="Automation Test Queue Empty"
```

This automation loads `UDTCoreSettings::WebSocketDataTable`, finds the
`LIDAR_JSON_LIVE_FRAME` row, verifies `TransactionCodeName`, and checks that
`TransactionCodeMessageClass` resolves to `ULidarJsonLiveFrameTC`. It also
instantiates the row handler through the `UTransactionCodeMessage` base class and
processes a sample payload into `ULidarJsonLiveSourceComp`. It is kept out of the
default real-sensor test group because it is evidence for the binary data-table
route rather than a pure component-unit test.

To create or repair the row in a reproducible DT-Project-only way, run the
editor-only commandlet:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\path\to\m7at10_dt.uproject" -run=EnsureLidarJsonLiveFrameTransaction -unattended -nop4
```

Use `-NoSave` for a dry run. The commandlet updates only the project WebSocket
data table configured by `UDTCoreSettings::WebSocketDataTable`; it does not
modify DTCore source.

External broker connectivity is still a deployment smoke item.
`M7AT10.RealSensorSource.JsonLiveDTCoreDispatch` covers the brokerless
GameInstance-backed DTCore dispatch path by enqueueing the checked
`LIDAR_JSON_LIVE_FRAME` sample through `UDxDataSubsystem::EnqueueWebSocketData`
in PIE and polling until the target LiDAR receives the frame. This proves the
data-table/subsystem/handler handoff without validating broker endpoint,
credentials, subscription, or network receive.

After `DT_TransactionCode.uasset` contains the `LIDAR_JSON_LIVE_FRAME` row,
send this sample through the deployment WebSocket broker and confirm the target
`ULidarJsonLiveSourceComp` frame count, target LiDAR point count, and optional
transport payload update in PIE.

Record deployment broker smoke evidence with:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_broker_smoke_report.ps1" -BrokerUrl "ws://host:61616" -Topic "topic.cep.output.0" -ObservedSourceFrame -ObservedTargetPoints -ObservedCachedPayload -Operator "name" -Notes "PIE map and broker details"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_broker_smoke_report.ps1" -NoWrite
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_websocket_lidar_smoke_evidence.ps1" -ProjectRoot "C:\path\to\m7at10_dt" -NoWrite
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_websocket_lidar_smoke_evidence.ps1" -ProjectRoot "C:\path\to\m7at10_dt" -RunCommandletDryRun -RunEvidenceAutomation -RunBrokerlessDTCoreDispatchAutomation -WriteReports -ObservedSourceFrame -ObservedTargetPoints -ObservedCachedPayload -Operator "name"
```

This report validates local prerequisites and records operator observations. It
does not connect to the broker by itself and should not be treated as STOMP
network proof unless the observation flags are backed by a real PIE run.
The wrapper runs the sample validator, registration checklist, optional row
commandlet dry run, optional evidence automation, optional brokerless dispatch
automation, and broker smoke report in a consistent order. Its default mode is
read-only: Unreal commandlets, automation, and report writes are opt-in.

The DT-Project side now owns conservative HTTP and UDP listener wrappers over
the transport-neutral JSON live handoff entry point,
`ULidarJsonLiveSourceComp::AppendLivePayloadJson`. Both wrappers feed the same
sample payload shape without going through the WebSocket-specific function
name. Deployment ownership is still open for endpoint exposure, authentication,
rate limits, and whether HTTP, UDP, WebSocket, or SDK input is the production
bridge.

`ULidarHttpJsonLiveSourceComp` is the optional inbound HTTP wrapper. It keeps
`bAutoStartSource=false`, binds one POST route with Unreal's `HTTPServer`
module when `StartSource` is called, caps request body size with
`MaxRequestBytes`, and pushes accepted bodies through `AppendLivePayloadJson`
plus optional `PushFrameOnce`. HTTP callbacks marshal request processing back to
the game thread before touching component state or target LiDAR data. Treat it
as DT-Project-owned local bridge coverage. It is separate from outbound
judging-server `HttpPost` transport. In UE 5.3, Unreal's `HTTPServer` listener
defaults to `localhost`; all-interface or explicit-address exposure is changed
through engine ini settings such as `[HTTPServer.Listeners]`,
`DefaultBindAddress`, or per-port `ListenerOverrides` entries with
`BindAddress=any` or an IP address. That bind choice is deployment
configuration, not a component property, so production exposure must be
controlled by port selection, host firewall/network policy, and ini review.
`M7AT10.RealSensorSource.HttpJsonLiveBridgePayload` verifies that the wrapper
reuses the shared JSON payload handoff and can either auto-push or buffer an
incoming frame. It also checks route bind/unbind lifecycle state.
`M7AT10.RealSensorSource.HttpJsonLiveBridgeLoopbackPost` starts the route on a
unique localhost path and high test port, sends a real HTTP POST through
`FHttpModule`, and waits for the target LiDAR frame update and HTTP 202
response.

`ULidarUdpJsonLiveSourceComp` is the first optional UDP wrapper for that path.
It binds conservatively to loopback by default, keeps `bAutoStartSource=false`,
marshals received datagrams back to the game thread, and feeds text payloads
through `AppendLivePayloadJson` plus optional `PushFrameOnce`. Treat it as local
bridge smoke coverage, not as deployment broker evidence.
`M7AT10.RealSensorSource.UdpJsonLiveBridgeDatagram` binds to an ephemeral
loopback port, sends one UTF-8 UDP datagram, and waits for the target LiDAR to
receive the frame.

Supported CSV formats:

```text
row,col,x,y,z
x,y,z
```

The `row,col,x,y,z` CSV form preserves grid coordinates. The `x,y,z` form is
accepted for loose point-cloud replay, but those points are emitted with
`gridCoordValid = false` and `gridCoordSource = derived_from_point_index`.

Supported JSONL formats:

```text
{"row":0,"col":0,"returnIndex":0,"x":900,"y":-260,"z":0,"distance":936.8,"hit":true,"semanticLabel":"Slab"}
{"worldLocation":[900,-260,0],"localDirection":[1,0,0],"hit":true,"semanticLabel":"Slab"}
```

When JSONL contains `gridCoordValid` or `gridCoordSource`, replay preserves that
validity marker instead of promoting fallback coordinates to source metadata.

Recommended use:

1. Add `ULidarCsvReplaySourceComp` to the same actor as a `UVirtualLidarSensorComp`, or set `TargetLidar` manually.
2. Set `CsvFilePath`.
3. Set `ReplaySemanticLabel = Slab` for Slab angle tests.
4. Call `PushFrameOnce`, or enable `bAutoStartReplay`.

Recommended live bridge use:

1. Add `ULidarJsonLiveSourceComp` to the same actor as a `UVirtualLidarSensorComp`, or set `TargetLidar` manually.
2. Set a stable `SourceId`, especially when more than one live LiDAR bridge exists.
3. Call `StartSource` when the external bridge is ready.
4. Append incoming JSON point lines until one sensor frame is buffered.
5. Call `PushFrameOnce` to inject the frame and optionally send transport.

Recommended pre-registration editor smoke:

1. Add `ULidarJsonLiveSourceComp` to the target LiDAR actor.
2. Keep `SampleWebSocketPayloadPath = Samples/websocket/lidar_json_live_frame_sample.json`.
3. Click `AppendSampleWebSocketFrameInEditor`; this replaces the current buffer
   with the checked sample payload.
4. Click `PushBufferedFrameNoTransportInEditor`.
5. Confirm the target LiDAR point count and `LastSourceFrameId` update without needing the DTCore WebSocket data-table row yet.

Included sample:

```text
Samples/slab_replay_sample.csv
Samples/slab_replay_sample.jsonl
Samples/websocket/lidar_json_live_frame_sample.json
```

Editor-callable helpers:

```text
PushFrameOnceInEditor
PushFrameOnceNoTransportInEditor
AppendSampleWebSocketFrameInEditor
PushBufferedFrameInEditor
PushBufferedFrameNoTransportInEditor
ClearBufferedFrameInEditor
StartReplayInEditor
StopReplayInEditor
```

Regression coverage:

```text
M7AT10.RealSensorSource.PushFrameToTarget
M7AT10.RealSensorSource.JsonLiveBridgePushFrame
M7AT10.RealSensorSource.JsonLiveTransactionParse
M7AT10.Evidence.WebSocketTransactionRegistration
```

Static readiness coverage:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_plan.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_plan.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1"
```

The static readiness script checks that replay adapters, placeholder adapters,
the normalized LiDAR handoff API, replay samples, WebSocket sample payload,
automation test names, and this plan document remain in sync before the real SDK
adapters are implemented.

## Adapter Priority

1. File replay adapter for saved JSON/JSONL/CSV frames.
2. ROS2 bridge adapter, if the deployment environment already has ROS2 topics.
3. Livox SDK adapter for direct LiDAR packet capture.
4. RealSense SDK adapter for direct RGB/depth camera capture.

File replay should come first because it tests the same downstream judgment server contract without hardware.

## Open Decisions

- Final server payload schema approval.
- Whether real sensor frames should be visualized with the same point-cloud preview path.
- Time synchronization source between camera, LiDAR, and simulation state.
- Coordinate transform calibration storage.
- Whether DTCore should own the final normalized interfaces.
