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
worldLocation
distance
hit
semanticLabel
```

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
```

This allows saved measurement data to enter the same LiDAR payload, recorder, transport, preview, and Slab analysis path as a virtual scan.

`PushPointFrameToTarget` is the DT-Project-side normalized LiDAR frame handoff point for SDK adapters. ROS2, Livox, and other future LiDAR sources should normalize packets into `FVirtualLidarPoint[]` and call this helper instead of calling `UVirtualLidarSensorComp::InjectPointCloudFrame` directly. That keeps target resolution, optional LiDAR dimension updates, frame counters, point counts, and source state messages consistent.

Supported CSV formats:

```text
row,col,x,y,z
x,y,z
```

Supported JSONL formats:

```text
{"x":900,"y":-260,"z":0,"distance":936.8,"hit":true,"semanticLabel":"Slab"}
{"worldLocation":[900,-260,0],"localDirection":[1,0,0],"hit":true,"semanticLabel":"Slab"}
```

Recommended use:

1. Add `ULidarCsvReplaySourceComp` to the same actor as a `UVirtualLidarSensorComp`, or set `TargetLidar` manually.
2. Set `CsvFilePath`.
3. Set `ReplaySemanticLabel = Slab` for Slab angle tests.
4. Call `PushFrameOnce`, or enable `bAutoStartReplay`.

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
```

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
