# LiDAR Payload Schema v1

Schema version:

```text
virtual-lidar.v1
```

This schema is the current DT-Project contract for virtual LiDAR frames. It is intentionally explicit about the difference between server payload data and Unreal preview data.

## Top-Level Fields

```text
schemaVersion
sensorType
sensorId
manufacturer
model
frameId
timestampUtc
horizontalSamples
verticalChannels
rayCount
totalPointCount
hitPointCount
payloadPointCount
maxDistance
semanticClassification
payloadPolicy
previewPolicy
sensorTransform
slabAnalysis
points
```

## Payload Policy

`payloadPolicy` describes the point selection used for `points[]`.

```json
{
  "stride": 1,
  "maxPoints": 0,
  "includeMissPoints": false,
  "pointSelection": "hit_only"
}
```

Rules:

- `stride = 1` means no stride downsampling.
- `maxPoints = 0` means no max-point cap.
- `includeMissPoints = false` means server payload contains hit points only.
- Preview settings never change this object.

## Preview Policy

`previewPolicy` describes Unreal Editor/runtime display only.

```json
{
  "stride": 2,
  "maxPoints": 5000,
  "hitOnly": true
}
```

Rules:

- This policy exists so the UI can render fewer points than the server receives.
- `PointCloudOnly` mode may reduce preview density for performance.
- Server-side judgment should not use `previewPolicy` as measurement truth.

## Point Fields

Each point in `points[]` contains:

```text
pointIndex
row
col
hit
distance
hitActor
hitActorClass
hitActorTags
semanticLabel
worldLocation
localDirection
```

`row` and `col` are derived from `pointIndex` and `horizontalSamples`.

## Slab Analysis

`slabAnalysis` is generated from hit points where `semanticLabel == SlabSemanticLabel`.

```text
valid
slabHitPointCount
boundsMin
boundsMax
center
estimatedYawDegrees
referenceYawDegrees
angleDeviationDegrees
confidence
status
```

`valid = false` usually means there were fewer points than `MinSlabPointsForAnalysis`.
