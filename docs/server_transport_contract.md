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

Current implementation file:

```text
Source/m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.cpp
```

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
