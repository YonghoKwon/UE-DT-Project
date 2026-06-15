# Server Transport Contract

This document tracks the current DT-Project transport contract for sending
virtual or replayed sensor payloads to a judging server.

## Current Modes

`UVirtualSensorDataTransportComp` currently supports:

```text
None
LogOnly
SaveToFile
HttpPost
```

- `None` disables submission and reports that transport is disabled.
- `LogOnly` keeps runtime/editor smoke tests safe by logging payload size only.
- `SaveToFile` writes JSON or binary payloads under `Saved/<SaveSubDirectory>/`.
- `HttpPost` submits JSON payloads with `Content-Type: application/json`.

## HTTP POST Request

Current request shape:

```text
method: POST
content-type: application/json
headers:
  X-Sensor-Id: <sensorId>
  X-Sensor-Type: <sensorType>
body: virtual-lidar.v1 or virtual-camera.v1 JSON
timeout: HttpTimeoutSeconds, clamped to at least 1 second
```

HTTP callback behavior:

```text
bSubmitted: request/response exchange completed with a response object
bAccepted: response status is in the 2xx range
HttpStatusCode: response code, or 0 when no response was available
ResponseBody: response body text for acceptance/error evidence
```

The async callback uses a weak component pointer before updating `LastResult` or
broadcasting `OnDataSent`, so an HTTP response arriving after component teardown
does not dereference a destroyed component.

Current implementation file:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.cpp
```

## Inbound HTTP Live Bridge

Inbound real-sensor HTTP is intentionally separate from outbound judging-server
`HttpPost` transport.

```text
component: ULidarHttpJsonLiveSourceComp
method: POST
default route: /m7at10/lidar/live
body: LIDAR_JSON_LIVE_FRAME-compatible JSON payload
handoff: AppendLivePayloadJson -> PushFrameOnce
```

This listener is a DT-Project bridge for feeding live LiDAR frames into the same
normalized path used by WebSocket, UDP, replay, and virtual scans. It does not
replace the outbound `HttpPost` transport and does not prove judging-server
acceptance. Deployment still needs explicit endpoint exposure, credentials,
rate-limit/backpressure, and operator smoke evidence.

## Decisions Still Required

The following are intentionally not frozen until the judging server contract is
approved:

- Final endpoint URL ownership and environment override policy.
- Authentication header or token strategy.
- Retry policy for timeout, network failure, and 5xx responses.
- Batching policy for high-frequency LiDAR frames.
- Backpressure behavior when capture rate exceeds server acceptance rate.
- Response body schema and whether server-side analysis results are returned.

Until those decisions are complete, the safe default for editor smoke tests is
`LogOnly` or `SaveToFile`.

## Review Commands

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_fixtures.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_contract.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_schema_review_policy.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_server_transport_contract.ps1"
```

Use `SaveToFile` when you need a concrete payload artifact for the judging-server
team before the final endpoint is available.
