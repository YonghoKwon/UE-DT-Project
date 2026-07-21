# Server Transport Contract

This document tracks the current DT-Project transport contract for sending
virtual or replayed sensor payloads to a judging server.

## Current Modes

`UVirtualSensorTransportComponent` currently supports:

```text
None
LogOnly
SaveToFile
HttpPost
StompWebSocket
```

- `None` disables submission and reports that transport is disabled.
- `LogOnly` keeps runtime/editor smoke tests safe by logging payload size only.
- `SaveToFile` writes JSON or binary payloads under `Saved/<SaveSubDirectory>/`.
- `HttpPost` submits JSON payloads with `Content-Type: application/json`.
- `StompWebSocket` publishes to Artemis multicast topics and records the broker receipt separately from consumer acknowledgement.

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

Manual exported files use the same endpoint with `Content-Type: application/octet-stream` plus `X-Data-Kind: manual-export` and `X-File-Extension`. The optional Bearer token is session-only and is never written to the UI SaveGame.

## Artemis STOMP/WebSocket

The development broker in `Tools/Artemis` exposes STOMP/WebSocket on port 61616 and multicast addresses for camera, LiDAR, and export traffic. Each message includes sensor ID/type, data kind, frame ID, request ID, content type, and `destination-type=MULTICAST`. A successful STOMP receipt means the broker accepted the message. It does not prove a consumer processed it; an application-level ACK Topic is required for that distinction.

The CaptureExport panel persists non-secret broker URL, topics, username, endpoint, and message limit. Passwords and tokens remain in memory for the PIE session only. Export files are Base64-wrapped for STOMP because the UE STOMP interface sends text frames; HTTP uses raw binary.

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

Local outbound transport evidence:

```text
MA0T10.SensorTransport.HttpPostLoopbackAcceptance
```

This automation starts a localhost mock judging-server route with Unreal
`HTTPServer`, sends payloads through `UVirtualSensorTransportComponent` in
`HttpPost` mode, verifies `POST`, `Content-Type: application/json`,
`X-Sensor-Id`, `X-Sensor-Type`, and `virtual-lidar.v1` body identity, then
checks that HTTP 202 sets `bAccepted=true` while HTTP 400 preserves the response
body and sets `bAccepted=false`.

Current implementation file:

```text
Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.cpp
```

## Inbound HTTP Live Bridge

Inbound real-sensor HTTP is intentionally separate from outbound judging-server
`HttpPost` transport.

```text
component: ULidarHttpJsonLiveSourceComponent
method: POST
default route: /ma0t10/lidar/live
body: LIDAR_JSON_LIVE_FRAME-compatible JSON payload
handoff: AppendLivePayloadJson -> PushFrameOnce
threading: HTTP callback marshals payload processing to the game thread
```

This listener is a DT-Project bridge for feeding live LiDAR frames into the same
normalized path used by WebSocket, UDP, replay, and virtual scans. It does not
replace the outbound `HttpPost` transport and does not prove judging-server
acceptance. Deployment still needs explicit endpoint exposure, credentials,
host firewall/network policy, rate-limit/backpressure, and operator smoke
evidence. In UE 5.3, Unreal's `HTTPServer` default listener bind address is
`localhost`. Wider exposure is controlled by engine ini settings under
`[HTTPServer.Listeners]`, including `DefaultBindAddress` and per-port
`ListenerOverrides` entries such as `BindAddress=any` or an explicit IP
address. This project component intentionally does not duplicate that bind
address setting as a Blueprint property; deployment configs and firewall policy
must be reviewed together.

Local evidence:

```text
MA0T10.RealSensorSource.HttpJsonLiveBridgeLoopbackPost
```

This automation starts a DT-Project HTTP live source on a probed localhost TCP
port, sends the shared `LIDAR_JSON_LIVE_FRAME` payload through Unreal's HTTP
client, expects HTTP 202, and waits for target LiDAR handoff. It proves local
route/request/response wiring only; it is not deployment network exposure or
judging-server acceptance evidence.

## Outbound Backpressure

`UVirtualSensorTransportComponent::MaxInFlightHttpRequests` caps concurrent HTTP
POST work. A frame above the cap returns `bSubmitted=false`,
`bAccepted=false`, and `bBackpressureRejected=true`; it is not queued or sent.
`InFlightHttpRequestCount` and `BackpressureRejectedRequestCount` expose runtime
telemetry.

This protects Unreal runtime memory but does not settle the final retry,
batching, rate-limit, or frame-loss policy.

The local retry baseline is bounded by `MaxHttpRetryAttempts`. Connection
failures and 5xx responses can retry when enabled; 4xx responses remain final.
`RetryAttemptCount` and `TotalHttpRetryAttemptCount` expose per-result and
cumulative retry telemetry. This baseline still requires judging-server owner
approval before production use.
Final failures increment `FailedHttpRequestCount`. Retry-eligible failures that
consume the configured budget additionally set `bRetryExhausted` and increment
`RetryExhaustedRequestCount`.

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

`Scripts/export_payload_contract_report.ps1` also exports a server acceptance
readiness matrix for these open items. It separates local fixture/mock/loopback
evidence from real judging-server acceptance and keeps
`RealJudgingServerAcceptancePresent=false` until an owned endpoint accepts the
payloads.

`Scripts/export_judging_server_acceptance_template.ps1` exports a read-only
fillable evidence template for the real judging-server handoff. The template
records evidence paths, reviewer metadata, accepted/rejected response evidence,
retry/timeout behavior, batching/backpressure decisions, and secret-scan proof,
but it must not contain endpoint URL values, tokens, passwords, secrets, or
credentials. It does not modify config and does not stage files.

## Review Commands

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_fixtures.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_contract.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_schema_review_policy.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_judging_server_acceptance_template.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_payload_contract_report.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_server_transport_contract.ps1"
```

Use `SaveToFile` when you need a concrete payload artifact for the judging-server
team before the final endpoint is available.

## Bounded sensor streams

The V2 capture/export panel can publish three independent latest-frame streams:
`virtual-lidar.v1` JSON, `virtual-camera.v1` JSON containing one Base64 JPEG,
and a `virtual-pointcloud.v1` envelope containing the selected point-cloud
format. Each sensor/stream key is bounded to one processing item and one
replaceable latest item. Automatic sends sample broker receipts (default every
10 messages); consumer completion requires a correlated ACK topic response.
See `docs/sensor_streaming.ko.md` and `Scripts/run_artemis_stream_smoke.ps1`.
