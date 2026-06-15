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
ULidarJsonLiveSourceComp
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

`PushPointFrameToTarget` is the DT-Project-side normalized LiDAR frame handoff point for SDK adapters. ROS2, Livox, WebSocket, HTTP, UDP, and other future LiDAR sources should normalize packets into `FVirtualLidarPoint[]` and call this helper instead of calling `UVirtualLidarSensorComp::InjectPointCloudFrame` directly. That keeps target resolution, optional LiDAR dimension updates, frame counters, point counts, and source state messages consistent.

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

Included sample:

```text
Samples/slab_replay_sample.csv
Samples/slab_replay_sample.jsonl
```

Editor-callable helpers:

```text
PushFrameOnceInEditor
PushFrameOnceNoTransportInEditor
StartReplayInEditor
StopReplayInEditor
```

Regression coverage:

```text
M7AT10.RealSensorSource.PushFrameToTarget
M7AT10.RealSensorSource.JsonLiveBridgePushFrame
M7AT10.RealSensorSource.JsonLiveTransactionParse
```

Static readiness coverage:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_plan.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_plan.ps1" -Json
```

The static readiness script checks that replay adapters, placeholder adapters,
the normalized LiDAR handoff API, replay samples, automation test names, and
this plan document remain in sync before the real SDK adapters are implemented.

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
