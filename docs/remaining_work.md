# Remaining Work

This document tracks what is still open after the current LiDAR/virtual sensor
branch work. It is intentionally practical: each item should either name the
decision needed, the implementation task, or the evidence required to call the
work complete.

## Current Baseline

Implemented in DT-Project:

- Virtual LiDAR scan/replay path with separated server payload and UI preview
  policies.
- Virtual camera payload export and monitor status text.
- Slab angle analysis v1 from semantic LiDAR points.
- CSV/JSONL point-cloud replay sources.
- Sensor manager point-cloud-only view policy.
- Native monitor widget fallback and monitor host actor.
- Local monitor camera capture pending-state guards for GPU readback and async
  JPEG write paths.
- Local smoke runner and project readiness wrapper.
- Local asset decision report with generated-output, category, and
  unclassified-untracked gates.

Out of scope for the current DT-Project-only work:

- DTCore source changes.
- Real SDK/bridge implementation inside DTCore.

## Local Project Decisions

The local project currently contains intentional untracked decision points. Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

Open decisions:

- `Content/M7AT10/UI/WBP_VirtualSensorMonitor.uasset`
  - Decide whether this binary WBP is the intended production monitor asset.
  - Current local state: binary Designer widget decision point, approximately
    40 KB in the local project.
  - Evidence needed: open in Unreal Editor, verify bindings, run PIE smoke test,
    then commit only if accepted.
- `Config/Game.ini`
  - Decide whether the local config change is intentional project config.
  - Current local state: empty `[DTCoreRuntimeOverride]` values.
  - Recommended default: keep untracked as local-only runtime override unless
    shared endpoint defaults are explicitly required.
  - Static readiness:
    `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1"`.
  - Evidence needed for commit: inspect diff, confirm there are no endpoint or
    credential values, and document why the setting belongs in repo.
- `Content/ChemicalPlantEnv/`
  - Large environment pack.
  - Evidence needed: explicit asset/vendor decision, size acceptance, and map
    dependency confirmation.
- `Content/Mega_Crane/`
  - Large crane asset pack.
  - Evidence needed: explicit asset/vendor decision and whether crane example
    remains part of this branch.
- `Content/Materials/`, `Content/Meshes/`, `Content/Textures/`
  - Shared content folders.
  - Evidence needed: map/WBP dependency check and asset ownership decision.
- `Samples/PixelStreaming/`
  - Sample or third-party content.
  - Current local state: tracked as a sample/third-party decision point with
    extension counts and largest-file details in
    `Scripts/report_local_project_status.ps1`.
  - Evidence needed: explicit decision that Pixel Streaming samples are part of
    this repository.
- Static readiness:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1"`.
- Strict content-review gate:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1" -FailIfPresent`.
- Decision report export:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1"`.
- Durable review artifact export:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1" -MarkdownPath ".\Saved\Reports\local_asset_decisions.md" -JsonPath ".\Saved\Reports\local_asset_decisions.json"`.
- The local asset report now includes `GitState`, `CommitReadiness`,
  `ReviewQueue`, `DecisionOwner`, `DecisionStatus`, `EvidenceNeeded`, and a
  `DecisionChecklist` for each known decision point. It also supports
  `DecisionEvidence`, `EvidenceStatus`, and `EvidenceSatisfied` through
  `docs/local_asset_decisions.evidence.json`.
  DecisionOwner is review-routing metadata, not accepted ownership.
  Current unresolved asset/sample paths should stay on `AssetOwnerRequired` or
  `ProjectOwnerRequired` until the required authority accepts the path.
  `EvidenceNeeded must be complete before ReadyToStage`; use
  `PendingOwnerDecision` or `EvidencePending` until evidence exists, and reserve
  `AcceptedForRepository` for explicitly accepted paths.
  ReadyToStage requires AcceptedForRepository. ReadyToStage requires complete evidence. AcceptedForRepository requires complete EvidenceNeeded.
  Recorded evidence must name reviewer, date, and source. Each completed evidence item must also include a non-empty source. Generated output remains KeepLocal.
  `Scripts/validate_local_asset_decision_evidence_workflow.ps1` now verifies
  the evidence state machine, including the Staged decision gate for blocked and
  evidence-ready decision paths. Duplicate normalized evidence paths are invalid
  and fail before any owner decision is accepted.
  Large content decisions require owner/source/license, dependency, size, and
  storage/versioning evidence before staging. WBP and `Game.ini` decisions
  still require manual editor/config review. The exported review bundle groups
  paths into `ReadyToStage`, `NeedsOwnerDecision`, and `KeepLocal` queues.
- Runtime config validation now supports `-LocalProjectRoot` so the source repo
  policy can inspect the real local Unreal project `Config/Game.ini`. The JSON
  output includes `RecommendedDecision`; the current empty
  `[DTCoreRuntimeOverride]` shape should remain `KeepLocal` unless a config
  owner explicitly accepts blank shared defaults.
- Monitor WBP decision report tooling is available through
  `Scripts/export_monitor_wbp_decision_report.ps1`. It records local
  `WBP_VirtualSensorMonitor.uasset` metadata, Git state, setup-document contract
  checks, `RecommendedDecision`, and an evidence draft while keeping Unreal
  Editor verification as the remaining acceptance gate.
- Large content decision report tooling is available through
  `Scripts/export_large_content_decision_report.ps1`. It summarizes
  `LargeContentCandidate` and `SampleOrThirdParty` paths by size, extension
  counts, largest files, risk, `RecommendedDecision`, and evidence drafts. Large
  content over 1 GB and copied samples default to `KeepLocal` until
  source/license/dependency/storage evidence is accepted.

Generated/local-output items are ignored by Git but still reported:

- `Windows/`
- `Windows.zip`
- `launcher.config.json`

These should stay out of commits unless a separate packaging workflow explicitly
requires a different policy.

## Implementation Gaps

### Real Sensor Adapters

Current state:

- `URos2SensorBridgeSourceComp`, `ULivoxLidarSourceComp`, and
  `URealSenseCameraSourceComp` are placeholders.
- `ULidarJsonLiveSourceComp` accepts buffered JSON point lines from a future
  WebSocket/HTTP/UDP/Blueprint bridge and pushes them through the normalized
  LiDAR handoff path.
- `ULidarJsonLiveFrameTC` can be registered in the DTCore WebSocket data table
  with `MESSAGE_ID = LIDAR_JSON_LIVE_FRAME` to route DTCore WebSocket payloads
  into `ULidarJsonLiveSourceComp`.
- `Samples/websocket/lidar_json_live_frame_sample.json` and
  `Scripts/validate_websocket_lidar_live_sample.ps1` define the checked sample
  payload that should be sent through the broker after data-table registration.
- `Scripts/export_websocket_transaction_registration_report.ps1` exports the
  expected `DT_TransactionCode` row name, handler class, sample metadata, and
  manual PIE smoke steps without mutating the binary data table asset. Its
  `-NoWrite` mode supports read-only readiness checks without creating `Saved/`
  report artifacts.
- `M7AT10.Evidence.WebSocketTransactionRegistration` is an optional automation
  test that loads the configured DTCore WebSocket data table and verifies the
  `LIDAR_JSON_LIVE_FRAME` row resolves to `ULidarJsonLiveFrameTC` after the
  binary row exists.
- `UEnsureLidarJsonLiveFrameTransactionCommandlet` can create or repair the
  project data-table row without modifying DTCore source.
- `DT_TransactionCode.uasset` now contains the `LIDAR_JSON_LIVE_FRAME` row, and
  local evidence automation has passed against that binary row and row-based
  handler parse/process path.
- `Scripts/export_websocket_broker_smoke_report.ps1` records deployment broker
  smoke evidence without pretending to connect to the broker itself.
- `Scripts/run_websocket_lidar_smoke_evidence.ps1` wraps sample validation,
  registration checks, commandlet dry run, optional evidence automation, and
  brokerless dispatch automation, and broker smoke reporting into one
  repeatable workflow.
- `M7AT10.RealSensorSource.JsonLiveDTCoreDispatch` now starts a PIE world,
  injects the checked `LIDAR_JSON_LIVE_FRAME` sample into the DTCore
  `UDxDataSubsystem` WebSocket queue, and verifies the target LiDAR receives the
  brokerless frame without requiring an external broker.
- `ULidarJsonLiveSourceComp::AppendLivePayloadJson` provides a
  transport-neutral handoff for HTTP, UDP, or Blueprint bridges that already
  receive the shared JSON live payload shape.
- `UVirtualCameraComp::InjectExternalJsonPayload` and
  `UCameraJsonLiveSourceComp` provide the first camera-side JSON live handoff
  for already-normalized `virtual-camera.v1` payloads without render-target
  readback. This updates server payload/status/JSON transport, not the camera
  preview render target or binary JPEG side channel. The current validation
  checks schema/type/encoding, identity metadata, UTC-looking timestamp format,
  integral dimensions/frame id, positive FOV values, base64 image decode, JPEG
  magic bytes, decoded byte count, and required transform arrays. Accepted
  payloads propagate the payload `sensorId` and `frameId` to runtime status,
  recorder frames, and transport file naming; rejected payloads preserve the
  previous cached payload and do not add recorder frames.
- Camera JSON live automation now rejects malformed external payloads for wrong
  schema, wrong sensor type, wrong encoding, invalid timestamp, fractional
  frame id, invalid dimensions, malformed transform arrays, invalid base64,
  invalid `simulationQuality`, and decoded byte-size mismatch.
- `ULidarHttpJsonLiveSourceComp` provides an optional inbound HTTP POST wrapper
  over the same handoff path. It is explicit-start by default, caps request body
  size, uses Unreal's `HTTPServer` module, marshals request processing back to
  the game thread, and is covered by
  `M7AT10.RealSensorSource.HttpJsonLiveBridgePayload` plus loopback HTTP POST
  smoke coverage in `M7AT10.RealSensorSource.HttpJsonLiveBridgeLoopbackPost`.
- `ULidarUdpJsonLiveSourceComp` provides an optional loopback-first UDP JSON
  live bridge wrapper over the same handoff path.
- `M7AT10.RealSensorSource.UdpJsonLiveBridgeDatagram` provides local UDP
  datagram smoke coverage using an ephemeral loopback port.
- `ULidarJsonLiveSourceComp` has editor helpers to append the checked sample
  payload and push the buffered frame without transport before the DTCore
  WebSocket data-table row exists.
- `M7AT10.RealSensorSource.JsonLiveTransactionRouting` covers handler
  `SOURCE_ID` routing, append-only payloads, ambiguous no-source rejection, and
  matched push into the target LiDAR.
- They expose configuration/state but do not connect to real SDKs or bridges.
- `Scripts/validate_real_sensor_adapter_plan.ps1` checks that replay adapters,
  placeholder adapters, sample files, WebSocket sample payload, handoff API
  text, automation test names, and the adapter plan remain in sync.

Next implementation steps:

- Define the final normalized frame contract for LiDAR and camera input.
- Smoke-test the `LIDAR_JSON_LIVE_FRAME` route with the deployment STOMP/WebSocket
  broker using
  `Samples/websocket/lidar_json_live_frame_sample.json`.
- Export a completed broker smoke report with source frame, target point, and
  cached payload observations.
- Run the smoke evidence wrapper with `-RunEvidenceAutomation` and
  `-RunBrokerlessDTCoreDispatchAutomation` during deployment verification.
- Decide deployment ownership for HTTP/UDP listener exposure, host firewall
  rules, credentials, rate limits, and whether HTTP, UDP, WebSocket, or SDK
  input is the selected production live bridge shape.
- Implement a ROS2 bridge adapter that converts incoming messages to
  `FVirtualLidarPoint` frames or camera payloads.
- Implement a Livox SDK adapter that normalizes packet streams into the same
  frame handoff path.
- Implement a RealSense adapter for camera/depth frames if required.

Completion evidence:

- Adapter starts and stops without editor crashes.
- Recorded real or replayed SDK sample data reaches the target LiDAR/camera
  component.
- Server payload/export path is shared with virtual/replay sources.
- Automation or editor smoke coverage exists for connection failure and at least
  one successful frame handoff path.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_plan.ps1"`.
- WebSocket registration checklist exports:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1"`.
- Read-only WebSocket registration checklist passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1" -NoWrite`.
- Data-table evidence automation passes:
  `M7AT10.Evidence.WebSocketTransactionRegistration`.
- Deployment broker smoke evidence shows the sample payload reaches
  `ULidarJsonLiveSourceComp` in PIE.
- Broker smoke report exists under `Saved/WebSocketBrokerSmoke/` with the
  observation flags backed by a real run.
- Wrapper summary shows sample validation, registration checklist, commandlet
  dry run, and optional evidence automation status.
- Brokerless dispatch automation passes:
  `M7AT10.RealSensorSource.JsonLiveDTCoreDispatch`.
- Brokerless dispatch automation is not a replacement for broker evidence; it
  does not validate endpoint, credentials, subscription, or network receive.

### Server Payload Schema Approval

Current state:

- LiDAR schema is documented as `virtual-lidar.v1`.
- Camera schema is documented as `virtual-camera.v1`.
- Reference fixtures exist under `Samples/payload_fixtures/` and can be checked
  with `Scripts/validate_payload_fixtures.ps1`.
- A local mock contract validator checks the fixtures against acceptance rules
  that approximate likely judging-server rejections before the final server
  contract is approved.
- Camera fixture and mock contract validation now decode the base64 image,
  verify JPEG magic bytes, enforce decoded `byteSize` consistency, and check
  UTC-looking timestamps, integral dimensions/frame ids, positive FOVs,
  documented `simulationQuality` string enum values, and required transform
  arrays.
- Schema compatibility examples are documented for LiDAR and camera payloads,
  including timestamp, unit, coordinate-frame, and preview/server semantics.
- LiDAR point `row`/`col` are now preserved from scan/replay metadata when
  available, with `returnIndex`, `gridCoordValid`, and `gridCoordSource`
  documenting multi-return and fallback cases.
- `Scripts/export_payload_contract_report.ps1` exports JSON and Markdown review
  artifacts under `Saved/PayloadContractReports/` for judging-server handoff.
- `Scripts/validate_payload_schema_review_policy.ps1` checks that schema review
  notes stay present while the final server contract is still open.
- `docs/server_transport_contract.md` records the current `LogOnly`,
  `SaveToFile`, and `HttpPost` behavior plus the server transport contract
  decisions that still need judging-server approval.
- `Scripts/validate_server_transport_contract.ps1` keeps the documented
  transport request shape and open endpoint/auth/retry/batching decisions in
  sync with the current DT-Project code.
- HTTP transport now uses a weak callback target, captures response body text,
  and separates request submission from 2xx server acceptance with `bAccepted`.
- The readiness wrapper runs both fixture validation and mock contract
  validation before smoke tests unless those gates are explicitly skipped, and
  now also runs the schema review policy and server transport contract gates.
- The schema is implemented enough for local export and smoke tests, but the
  final judging server contract is not confirmed.

Next implementation steps:

- Confirm required field names, units, coordinate frame, timestamp format, and
  compression/transport requirements with the judging server.
- Decide final transport endpoint, authentication, retry, batching, and
  backpressure behavior with the judging-server team.
- Add a real or local mock HTTP endpoint acceptance test once the expected
  response schema is available.
- Add a fixture-based payload contract test if the server contract becomes
  stable.

Completion evidence:

- Approved example payloads exist.
- Contract tests validate required fields.
- Transport mode can produce payloads accepted by the server or the local mock
  contract validator.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_schema_review_policy.ps1"`.
- Server transport readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_server_transport_contract.ps1"`.

### True LAZ Compression

Current state:

- `ExportLastPointCloudLaz()` is intentionally a placeholder.
- It writes a LAS-compatible `*_laz_source_*.las` file and logs a warning.
- An opt-in external compressor path is available through
  `bUseExternalLazCompressor`, `ExternalLazCompressorPath`, and
  `ExternalLazCompressorArguments`. The default remains the safe LAS source
  placeholder until a compressor is configured and verified.
- The external compressor path requires `{input}` and `{output}` tokens, writes
  a separate `.laz` output path, and fails if the configured process does not
  create a non-empty output file.
- `Scripts/validate_laz_placeholder_policy.ps1` checks that code, docs, monitor
  settings, and automation continue to describe this as a placeholder until true
  LAZ compression is integrated.
- `Scripts/export_laz_compression_decision_report.ps1` exports the current
  placeholder evidence, native-library / external-CLI / server-post-process
  candidate paths, and the acceptance evidence required before replacing the
  placeholder.

Next implementation steps:

- Decide whether true LAZ is required for this project.
- Choose whether the external compressor path is sufficient, or whether a native
  library / server-side post-processing workflow is required.
- Configure an accepted compressor executable and argument template, then
  validate readable `.laz` output.
- Keep current warning behavior until a real compressor is configured and
  verified.

Completion evidence:

- `.laz` output is actually compressed and readable by a known point-cloud tool.
- Automation verifies `.laz` creation separately from LAS source export.
- Missing-compressor guard passes:
  `M7AT10.SensorReplay.LazExternalCompressorMissingFails`.
- Static placeholder readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_laz_placeholder_policy.ps1"`.

### Large Point Cloud Renderer

Current state:

- Preview is CPU/instance oriented and protected by preview stride/max point
  policies.
- Live LiDAR preview now uploads selected preview transforms with batched
  `UInstancedStaticMeshComponent::AddInstances` instead of per-point
  `AddInstance` calls.
- This is enough for editor feedback but not ideal for very large point clouds.
- `Scripts/validate_point_cloud_preview_policy.ps1` checks that the current
  server/preview split, runtime warnings, point-cloud-only preview clamps, and
  automation test names remain in sync while the GPU renderer decision is open.
- `Scripts/export_point_cloud_renderer_decision_report.ps1` exports the current
  CPU/ISM fallback evidence, Niagara/custom-GPU/external-viewer candidate
  renderers, and the acceptance evidence needed before replacing the preview
  path.
- `M7AT10.Sensor.CsvPointCloudPreview.ProceduralHighDensityLoad` covers a
  120,000-point procedural CSV preview load without requiring the Unreal Editor
  GUI, and `M7AT10.Sensor.CsvPointCloudPreview.InstancedBatchLoad` keeps the
  instanced fallback path covered.

Next implementation steps:

- Decide whether to use Niagara, GPU buffers, or a custom renderer.
- Preserve the current split: server payload can stay dense while preview is
  downsampled or GPU-rendered.
- Add pixel/screenshot smoke checks if a GPU renderer is added.

Completion evidence:

- High-density scan can be previewed without editor stalls.
- Point-cloud-only mode still preserves collision/trace behavior.
- Dense server payload count is independent from preview point count.
- Headless CSV preview automation passes:
  `Automation RunTests M7AT10.Sensor.CsvPointCloudPreview`.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_point_cloud_preview_policy.ps1"`.

### Production Monitor WBP

Current state:

- Native fallback exists and is covered by automation.
- Designer-authored `WBP_VirtualSensorMonitor` is still a local binary decision.
- `Scripts/validate_monitor_widget_policy.ps1` checks that optional bindings,
  native fallback, local WBP decision guards, setup docs, and monitor automation
  names remain in sync before the binary WBP is committed.

Next implementation steps:

- Open the WBP in Unreal Editor and verify optional bindings.
- Confirm camera/LiDAR switching, preview budget controls, hit-only toggle,
  server payload export, and slab analysis text.
- Commit the WBP only after manual editor verification.

Completion evidence:

- PIE smoke test demonstrates the WBP in a real map.
- No missing binding crashes.
- Operator-facing status text exposes selected sensor, frame/ray counts, payload
  counts, preview counts, warnings, and slab angle result.
- Local camera capture failure paths clear pending state after failed GPU
  readbacks and completed JPEG writes.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_widget_policy.ps1"`.

## Routine Verification

Fast local readiness check:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
```

Pre-commit progress and remaining-work summary:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -Json
```

The percentages are planning snapshots derived from the current remaining-work
areas. They are not a substitute for build, smoke, editor, or server acceptance
evidence.

Full local readiness check:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
```

Strict asset gates for clean-content review:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnStagedDecisionPoints
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnLargeContentCandidates
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```
