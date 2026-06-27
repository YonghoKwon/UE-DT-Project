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
- DTCore submodule must remain pinned to
  `2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7`; use
  `Scripts/validate_dtcore_submodule_guard.ps1` before commits.

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
  - Keep the current local config change out of source control by default.
  - Current local state: empty `[DTCoreRuntimeOverride]` values.
  - Recommended default: keep untracked as local-only runtime override unless
    shared endpoint defaults are explicitly required.
  - Static readiness:
    `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1"`.
  - Current report status: `KeepLocal`, `KeepLocalByDecision`, and
    `ManualConfigOwnerDecisionStillRequired=false` for the empty override shape.
  - Evidence needed only if committing later: inspect diff, confirm there are no
    endpoint or credential values, and document why the setting belongs in repo.
- `Content/ChemicalPlantEnv/`
  - Unused large environment pack.
  - Current decision: keep out of source control; optionally remove manually
    from the local project after Unreal reference/dependency checks.
- `Content/Mega_Crane/`
  - Unused local crane asset pack.
  - Current decision: keep out of source control; optionally remove manually
    from the local project after reference/dependency checks.
- `Content/Materials/`, `Content/Meshes/`, `Content/Textures/`
  - Unused local material/mesh/texture folders.
  - Current decision: keep out of source control; optionally remove manually
    from the local project after reference/dependency checks.
- `Samples/PixelStreaming/`
  - Sample or third-party content.
  - Current local state: tracked only as a local sample/third-party staging
    boundary with extension counts and largest-file details in
    `Scripts/report_local_project_status.ps1`.
  - Current scope decision: ignore PixelStreaming for the DT-Project
    LiDAR/virtual-sensor work. It must remain untracked, but its ownership and
    redistribution acceptance no longer count as current remaining work.
  - Current pre-commit summary: one sample/third-party candidate, about 3.3 MB,
    with `ExcludedFromCurrentScope = true` and
    `CountsTowardRemainingWork = false`.
  - Repository-side setup alternative: `docs/pixel_streaming_setup.md`.
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
  It now also exports `ReviewPriority`, `CommitBlocker`, `BlockingReason`,
  `NextReviewAction`, and an `ActionPlan` sorted by the suggested owner-review
  order.
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
  The evidence template now includes a `Summary` with decision counts, pending
  evidence item counts, and `TopBlockingPaths` so owner review starts from the
  highest-priority blockers without implicitly accepting local files.
  Large `Content/*` asset folders currently confirmed as unused are treated as
  local cleanup candidates, not repository-acceptance candidates. They should
  stay untracked or be removed manually after a map/WBP dependency check. WBP still
  requires manual editor review, while the current empty `Game.ini` override is
  treated as KeepLocal unless non-empty endpoint or credential values are added.
  The exported review bundle groups
  paths into `ReadyToStage`, `NeedsOwnerDecision`, and `KeepLocal` queues and
  also shows top blocking actions.
- `Scripts/report_precommit_summary.ps1` now consumes the large-content
  decision report plus the read-only cleanup plan and shows the confirmed-unused
  cleanup candidate count, total cleanup size, largest cleanup candidate,
  cleanup-plan dry-run/deletion safety fields, sample/third-party candidate
  count and size, and the repository-acceptance paths that still need owner
  decisions. Cleanup candidate still means keep ignored or manually remove after
  map/WBP dependency checks; sample/third-party candidate still means keep
  untracked unless project ownership, redistribution approval, and documentation
  alternative are accepted.
- The large content decision report now exports per-path `RequiredAcceptance`,
  `DecisionBlockers`, `NextReviewAction`, and `TopBlockers`. High-risk binary
  asset packs and content over 100 MB are kept on explicit owner/source/license,
  dependency, and storage acceptance before repository inclusion.
- The report also flags `BuiltDataHeavy`, `LargestFileRisk`,
  `StorageRiskReason`, `RedistributionReviewRequired`, and `SampleRiskReason`
  so very large map build data, single-file storage risk, and copied sample
  redistribution questions are visible before owner acceptance.
- `Scripts/validate_large_content_decision_policy.ps1` supports
  `-LocalProjectRoot` so the source checkout can validate docs/scripts while the
  actual untracked Unreal content is scanned from `C:\Unreal Projects\m7at10_dt`.
- `Scripts/export_large_content_cleanup_plan.ps1` exports a dry-run cleanup plan
  for the confirmed-unused large Content folders. It records recoverable local
  disk size, required pre-delete checks, and per-candidate safety fields such as
  `DryRunOnly=true`, `DeletesFiles=false`, `ModifiesAssets=false`,
  `ManualDeletionOnly=true`, and `SafeToDelete=false`; it never deletes files or
  modifies Unreal assets.
- `Scripts/invoke_unused_content_archive.ps1` exports a preview-only archive
  action report for those same unused local folders. It keeps
  `PreviewOnly=true`, `DeletesFiles=false`, `StagesFiles=false`, and
  `ModifiesAssets=false` by default. Actual local archive moves require
  `-Execute`, `-ConfirmReferenceChecks`, and an explicit `-ArchiveRoot` outside
  `C:\Unreal Projects\m7at10_dt`; the tool never performs permanent deletion or
  git staging.
  If the archive move is executed locally, the current cleanup-candidate count
  should drop because the folders are no longer inside the Unreal project. This
  is tracked as local cleanup state, not a source-control deletion. The
  pre-commit summary reports `PresentKnownUnusedCleanupCandidateCount`, the
  known unused-candidate max, and absent-or-archived count so the archive can be
  verified without staging those asset folders.
- `Scripts/export_unused_content_archive_evidence.ps1` verifies the local
  post-archive state. It checks that known unused folders are absent from
  `C:\Unreal Projects\m7at10_dt`, present under the archive root, the archive
  root is outside the Unreal project, archive files are not staged, and DTCore
  has not been touched. This evidence is local-only; it is not repository
  acceptance, deletion approval, or permission to stage archived asset folders.
- `Scripts/export_sample_content_decision_report.ps1` exports the current copied
  sample/third-party decision for `Samples/PixelStreaming`. It keeps
  `RecommendedDecision=KeepLocalUnlessOwned`, `MustRemainUntracked=true`, and
  `SafeToStage=false` until project ownership, license/redistribution approval,
  and setup-documentation alternative evidence are recorded. The report points
  to `docs/pixel_streaming_setup.md` so copied sample files can stay local while
  the repository keeps reproducible setup notes.
- Runtime config validation now supports `-LocalProjectRoot` so the source repo
  policy can inspect the real local Unreal project `Config/Game.ini`. The JSON
  output includes `RecommendedDecision`; the current empty
  `[DTCoreRuntimeOverride]` shape should remain `KeepLocal` unless a config
  owner explicitly accepts blank shared defaults.
- Runtime config decision report tooling now reuses the local asset decision
  engine. `Scripts/export_runtime_config_decision_report.ps1` reports
  `ReviewQueue`, `CommitReadiness`, `EvidenceStatus`, `MissingEvidenceCount`,
  `ReadyToStage`, redacted runtime override key state, and a manual acceptance
  checklist for `Config/Game.ini`. It accepts `-EvidencePath` for a candidate
  `LocalAssetDecisionEvidenceV1` file and `-FailOnIncompleteEvidence` for an
  opt-in config pre-commit gate.
- `Scripts/export_runtime_config_acceptance_template.ps1` exports a read-only
  fillable runtime config evidence template. It records key names, key counts,
  config hash, secret-scan/log fields, shared-default owner decision fields, and
  policy validation evidence with `ValuesRedacted=true`, `ModifiesConfig=false`,
  and `StagesConfig=false`; endpoint, credential, token, password, and secret
  values should never be written to the template output.
- Monitor WBP decision report tooling is available through
  `Scripts/export_monitor_wbp_decision_report.ps1`. It records local
  `WBP_VirtualSensorMonitor.uasset` metadata, Git state, setup-document contract
  checks, `RecommendedDecision`, `ReviewQueue`, `CommitReadiness`,
  `EvidenceStatus`, `MissingEvidenceCount`, `ReadyToStage`, and an evidence
  draft while keeping Unreal Editor verification as the remaining acceptance
  gate. It accepts `-EvidencePath` for a candidate
  `LocalAssetDecisionEvidenceV1` file and `-FailOnIncompleteEvidence` for an
  opt-in WBP pre-commit gate.
- `Scripts/export_monitor_wbp_acceptance_template.ps1` exports a read-only
  fillable evidence template for editor-open, optional-binding, PIE-smoke, and
  production-owner acceptance. It records the current WBP asset hash and fields
  such as `EvidenceRunId`, operator, map/PIE session, log path, screenshot path,
  optional binding rows, exported payload path, and owner acceptance metadata
  without modifying or staging the binary `.uasset`.
  The template also separates `MonitorWbpAssetPresent` from
  `MonitorWbpAssetStageAllowed` and keeps `ReadyToStageMonitorWbpAsset = false`
  until manual evidence is complete. It now exposes manual acceptance sections:
  `EditorOpenEvidence`, `WidgetBindingEvidence`, `PieSmokeEvidence`,
  `SensorSelectionEvidence`, `LidarStatusPanelEvidence`,
  `SlabAnalysisPanelEvidence`, `NoCrashEvidence`, and `OwnerAcceptance`.
  A local untracked WBP file is therefore not treated as repository-ready just
  because the asset exists on disk.
- `Scripts/export_monitor_wbp_preflight_report.ps1` exports a read-only
  preflight report before manual Editor/PIE review. It checks the current WBP
  hash, Git state, setup-document contract, acceptance-template availability,
  missing evidence count, and post-archive context. Preflight readiness is not
  WBP acceptance and does not permit staging the binary asset.
- `Scripts/export_monitor_wbp_post_edit_hash_report.ps1` records post-edit WBP
  SHA256 evidence after the asset is saved through Unreal Editor. It writes only
  under `Saved/Reports/MonitorWbpPostEdit`, compares the current hash with the
  pre-edit review/backup when available, and provides the exact hash fields to
  copy into the acceptance evidence before strict validation.
- `Scripts/update_monitor_wbp_acceptance_hash_evidence.ps1` copies the
  post-edit hash report path and current SHA256 into
  `monitor_wbp_acceptance.evidence.json`. It updates only the local Saved
  evidence JSON, creates a timestamped backup, and does not modify or stage the
  binary WBP asset.
- `Scripts/export_monitor_wbp_acceptance_package.ps1` now preserves an existing
  `monitor_wbp_acceptance.evidence.json` and writes a fresh comparison template
  to `monitor_wbp_acceptance.template.fresh.json`, so rerunning the package does
  not erase hash or manual evidence already collected under `Saved`.
- `Scripts/validate_monitor_wbp_acceptance_evidence.ps1` and
  `Scripts/export_monitor_wbp_acceptance_package.ps1` now emit
  `MissingEvidenceActions`. Each failed WBP acceptance check is mapped to an
  `EvidencePhase`, `BlockingStage`, `EvidenceTarget`, and a concrete
  `NextAction`. The package also writes
  `monitor_wbp_missing_evidence_actions.json` and
  `monitor_wbp_missing_evidence_actions.md`, so the manual Editor/PIE pass has
  a focused checklist instead of only a missing-count summary.
- `Scripts/export_monitor_wbp_evidence_todo.ps1` exports
  `monitor_wbp_evidence_todo.json` and `monitor_wbp_evidence_todo.md` from the
  current acceptance validator output. The Markdown report groups missing
  evidence by phase and includes checkbox rows for the manual WBP reviewer.
- `Scripts/update_monitor_wbp_manual_evidence_paths.ps1` merges collected
  Editor/PIE/DisplayData log, screenshot, exported payload, map, session, and
  operator metadata into the Saved WBP acceptance evidence JSON. It validates
  referenced files and still does not modify or stage the binary WBP asset.
- `Scripts/update_monitor_wbp_manual_acceptance_sections.ps1` marks individual
  `ManualAcceptanceSections` present/accepted from real evidence files. It
  intentionally requires `-AcceptOwnerAcceptance` before touching the owner
  acceptance section, so repository acceptance cannot be set accidentally.
- `Scripts/export_monitor_wbp_acceptance_runbook.ps1` writes
  `monitor_wbp_acceptance_runbook.json` and `.md` with the exact command order
  for package refresh, Editor/PIE evidence collection, hash copy, evidence path
  merge, manual section update, and strict validation.
- `Scripts/report_precommit_summary.ps1` now includes a Monitor WBP decision
  and preflight section with WBP Git state, review queue, missing evidence
  count, missing acceptance items, setup-doc contract status, preflight blocked
  checks, and the boundary that the binary asset stays untracked until editor
  open, optional binding check, PIE smoke, and production WBP acceptance evidence
  are recorded.
  The summary also reports `MonitorWbpAssetPresent`,
  `MonitorWbpAssetTracked`, `MonitorWbpAssetStageAllowed`,
  `ReadyToStageMonitorWbpAsset`, `EditorManualAcceptancePresent`, and
  `MonitorWbpManualAcceptanceComplete` so asset presence cannot be confused with
  stage permission.
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
  smoke evidence without pretending to connect to the broker itself. It now
  requires concrete evidence fields before `BrokerSmokeComplete` can become
  true: evidence run id, operator, notes, map name, PIE session, log path,
  source frame before/after counts, target point count, cached payload bytes or
  hash, and broker client command.
- `Scripts/run_websocket_lidar_smoke_evidence.ps1` wraps sample validation,
  registration checks, commandlet dry run, optional evidence automation, and
  brokerless dispatch automation, and broker smoke reporting into one
  repeatable workflow. Its summary separates core broker observations, evidence
  field completeness, external broker requirement, and deployment readiness.
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
- The real-sensor readiness report now ranks deployment path candidates. Current
  recommendation: use file replay as the baseline, use HTTP JSON live as the
  first local/live deployment bridge, and prefer DTCore WebSocket only when the
  broker endpoint, credentials, topic, and deployment smoke ownership are ready.
- The readiness report now also exports a `DeploymentActionPlan` with
  per-candidate `RequiredEvidence`, `DeploymentBlockers`, and `NextAction`
  fields. File replay is the baseline path; HTTP/WebSocket/UDP/SDK paths remain
  blocked until their deployment owner supplies endpoint, credential, network,
  broker, trust-boundary, or real-device evidence. WebSocket broker smoke is
  still blocked unless the required evidence schema is complete, not merely when
  observation flags are set.
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
- `Scripts/export_real_sensor_adapter_deployment_package.ps1` creates a local
  `Saved/Reports/RealSensorAdapterDeployment` package with readiness, adapter
  plan validation, WebSocket sample validation, transaction registration,
  broker-smoke draft, manual steps, and follow-up commands. It does not connect
  to the external broker or SDKs, modify assets, stage files, or write endpoint
  or credential values.
- The package now also writes a fillable
  `RealSensorAdapterDeploymentEvidenceV1` draft and validates it with
  `Scripts/validate_real_sensor_adapter_deployment_evidence.ps1`. Real
  deployment readiness remains false until `BrokerPieSmoke`,
  `HttpDeploymentSmoke`, `UdpDeploymentSmoke`, `SdkRos2Evidence`,
  `LivoxEvidence`, `RealSenseEvidence`, `CredentialRedaction`, and
  `OwnerAcceptance` evidence are resolved for the selected deployment path.
  The draft now also includes `DeploymentPathEvidenceSections` for
  `ReplayBaseline`, `HttpJsonLive`, `WebSocketDTCore`, `UdpJsonLive`,
  `Ros2Bridge`, `LivoxSdk`, and `RealSenseSdk`, so selected-path acceptance and
  not-selected SDK/bridge acceptance are tracked separately.
  Brokerless DTCore dispatch, sample validation, and commandlet dry runs remain
  pre-deployment evidence; they do not replace real STOMP/WebSocket broker PIE
  smoke or SDK hardware evidence.
  The deployment template/package/pre-commit summary now also expose
  `PreDeploymentEvidenceOnly = true`,
  `BrokerlessDispatchIsDeploymentEvidence = false`,
  `LoopbackSmokeIsDeploymentBrokerEvidence = false`,
  `StaticTransactionRegistrationIsBrokerAcceptance = false`,
  `AcceptancePackageIsEvidenceShell = true`,
  `AcceptancePackageIsDeploymentProof = false`,
  `GeneratedReportDoesNotMeanDeploymentPassed = true`, and
  `DoesNotModifyDTCore = true`, so generated reports, loopback smokes, and
  static registrations cannot be mistaken for external broker/SDK deployment
  acceptance.
- `URealSensorSourceComp::GetDeploymentReadinessSummaryText()` and
  `RequiresExternalDeploymentEvidence()` expose the same deployment boundary in
  UE runtime code. File replay is marked as a schema baseline, while JSON live,
  ROS2, Livox, RealSense, and custom sources report that external deployment
  evidence is still required.
- `Scripts/export_real_sensor_adapter_gap_summary.ps1` refreshes the deployment
  package and validator output, then lists missing evidence checks by phase plus
  the next manual action. It writes only under `Saved`, never connects to
  brokers or SDKs, never writes endpoint/credential values, does not modify
  DTCore/assets, and does not stage files.

Next implementation steps:

- Define the final normalized frame contract for LiDAR and camera input.
- Export the deployment evidence package before deployment-owner review:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_real_sensor_adapter_deployment_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"`.
- Smoke-test the `LIDAR_JSON_LIVE_FRAME` route with the deployment STOMP/WebSocket
  broker using
  `Samples/websocket/lidar_json_live_frame_sample.json`.
- Export a completed broker smoke report with source frame, target point, and
  cached payload observations plus the required evidence fields: evidence run
  id, map name, PIE session, log path, before/after source frame counts, target
  point count, cached payload bytes or hash, broker client command, operator,
  and notes.
- Run the smoke evidence wrapper with `-RunEvidenceAutomation` and
  `-RunBrokerlessDTCoreDispatchAutomation` during deployment verification.
- Decide deployment ownership for HTTP/UDP listener exposure, host firewall
  rules, credentials, rate limits, and whether HTTP, UDP, WebSocket, or SDK
  input is the selected production live bridge shape.
- Confirm whether the deployment should follow the readiness report's default
  priority: file replay baseline, HTTP JSON live first, WebSocket via DTCore
  when broker ownership is ready, then UDP/SDK paths as specialized follow-ups.
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
- Deployment evidence package exports without touching broker/SDK/config:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_real_sensor_adapter_deployment_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json`.
- Deployment gap summary exports the next real-sensor handoff action:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_real_sensor_adapter_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json`.
- Deployment package summary should report seven deployment path sections,
  `SelectedDeploymentPathCount = 0`, and
  `CurrentReadyToClaimRealSensorDeployment = false` until deployment-owner
  evidence is filled.
- Deployment package summary should also report `PreDeploymentEvidenceOnly =
  true`, `BrokerlessDispatchIsDeploymentEvidence = false`,
  `LoopbackSmokeIsDeploymentBrokerEvidence = false`,
  `StaticTransactionRegistrationIsBrokerAcceptance = false`,
  `RealBrokerOrSdkAcceptanceEvidencePresent = false`,
  `AcceptancePackageIsEvidenceShell = true`,
  `AcceptancePackageIsDeploymentProof = false`, and `DoesNotModifyDTCore =
  true`.
- Deployment evidence validation reports missing evidence without touching
  broker/SDK/config:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_deployment_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -EvidencePath "C:\Unreal Projects\m7at10_dt\Saved\Reports\RealSensorAdapterDeployment\real_sensor_adapter_deployment.evidence.json" -Json`.
- Deployment evidence can be made a strict gate only after the evidence draft is
  filled:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_real_sensor_adapter_deployment_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -EvidencePath "C:\Unreal Projects\m7at10_dt\Saved\Reports\RealSensorAdapterDeployment\real_sensor_adapter_deployment.evidence.json" -FailOnIncompleteEvidence`.
- WebSocket registration checklist exports:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1"`.
- Read-only WebSocket registration checklist passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_websocket_transaction_registration_report.ps1" -NoWrite`.
- Data-table evidence automation passes:
  `M7AT10.Evidence.WebSocketTransactionRegistration`.
- Deployment broker smoke evidence shows the sample payload reaches
  `ULidarJsonLiveSourceComp` in PIE.
- Broker smoke report exists under `Saved/WebSocketBrokerSmoke/` with the
  observation flags and required evidence fields backed by a real run.
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
- LiDAR `payloadPolicy.pointSelection` is fixed to canonical
  `hit_only | hit_and_miss` values. The schema review policy now checks the
  docs, fixture, mock contract validator, and C++ serializer for the same
  server payload policy instead of accepting the older `all_points` alias.
- LiDAR point `row`/`col` are now preserved from scan/replay metadata when
  available, with `returnIndex`, `gridCoordValid`, and `gridCoordSource`
  documenting multi-return and fallback cases.
- `Scripts/export_payload_contract_report.ps1` exports JSON and Markdown review
  artifacts under `Saved/PayloadContractReports/` for judging-server handoff.
  The report now includes a server acceptance readiness matrix for endpoint
  ownership, authentication, retry/timeout, batching/backpressure, and response
  schema decisions, while keeping real judging-server acceptance separate from
  local mock and loopback evidence.
- `Scripts/export_judging_server_acceptance_template.ps1` exports a read-only
  fillable evidence template for the real judging-server handoff. It requires
  endpoint ownership, authentication policy, LiDAR/camera accepted-response
  evidence, rejected-payload behavior, retry/timeout behavior,
  batching/backpressure evidence, and a secret scan before
  `RealJudgingServerAcceptancePresent` can be treated as complete. The template
  records evidence paths and reviewer metadata only; endpoint URLs, tokens,
  passwords, secrets, and credential values must stay out of repository files.
- The judging-server acceptance template now includes structured
  `EvidenceSections`: `EndpointOwnership`, `AuthenticationPolicy`,
  `ResponseSchema`, `RealEndpointSmoke`, `RateBackpressure`,
  `SecretRedaction`, and `OwnerAcceptance`. These sections are acceptance
  metadata, not proof of real server acceptance by themselves; the contract
  remains unclaimed until owned endpoint response evidence and owner acceptance
  are recorded without endpoint or credential leakage.
  Endpoint and credential values are owner-held external evidence, not
  repository content.
- `Scripts/export_judging_server_acceptance_package.ps1` now creates a local
  `Saved/Reports/JudgingServerAcceptance` package with the payload contract
  report, server transport contract validation, fillable judging-server
  acceptance template, manual steps, follow-up commands, and a sensitive-pattern
  scan of generated files. The package is evidence preparation only; it does
  not contact the server, write endpoint values, modify config, or stage files.
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
- HTTP transport now caps concurrent requests with
  `MaxInFlightHttpRequests`, reports local backpressure rejection explicitly,
  and exposes in-flight/rejection telemetry without creating an unbounded
  request queue.
- `M7AT10.SensorTransport.HttpPostLoopbackAcceptance` verifies outbound
  `HttpPost` against a local mock judging-server route, including POST shape,
  JSON content type, sensor headers, `virtual-lidar.v1` body identity, 2xx
  acceptance, non-2xx rejection, status code, and response body capture.
- The readiness wrapper runs both fixture validation and mock contract
  validation before smoke tests unless those gates are explicitly skipped, and
  now also runs the schema review policy and server transport contract gates.
- The schema is implemented enough for local export and smoke tests, but the
  final judging server contract is not confirmed.

Next implementation steps:

- Confirm required field names, units, coordinate frame, timestamp format, and
  compression/transport requirements with the judging server.
- Export the judging-server acceptance package before the server-owner review:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_judging_server_acceptance_package.ps1" -ProjectRoot "."`.
- Decide final transport endpoint, authentication, retry, batching, and
  backpressure behavior with the judging-server team.
- Replace the local mock HTTP endpoint acceptance test with real judging-server
  acceptance evidence once the expected response schema is available.
- Add a fixture-based payload contract test if the server contract becomes
  stable.

Completion evidence:

- Approved example payloads exist.
- Contract tests validate required fields.
- Transport mode can produce payloads accepted by the real judging server. The
  local mock contract validator is only pre-approval evidence and is not enough
  to claim real server acceptance.
- Outbound local mock transport acceptance passes:
  `M7AT10.SensorTransport.HttpPostLoopbackAcceptance`.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_schema_review_policy.ps1"`.
- Payload contract review exports:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_payload_contract_report.ps1" -Json`.
- Judging-server acceptance template exports:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_judging_server_acceptance_template.ps1" -Json`.
- The template JSON should show `EvidenceSectionCount = 7`,
  `CurrentReadyToClaimRealServerAcceptance = false`, and no endpoint or
  credential values.
- Judging-server acceptance package exports local review evidence without
  endpoint or credential values:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_judging_server_acceptance_package.ps1" -ProjectRoot "." -Json`.
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
- `UVirtualLidarSensorComp` now exposes last-export telemetry through
  `GetLastLazExportStatusText()`, `GetLastLazLasSourcePath()`,
  `GetLastLazOutputPath()`, export attempt/success flags,
  placeholder-only state, external compressor requested/attempted/succeeded
  flags, produced-output state, true-validation state, exported point count,
  compressor return code, output size, and warning text, so UI and automation
  can distinguish placeholder-only LAS source export, missing/attempted
  compressor paths, and external process success without claiming readable LAZ
  acceptance.
- `M7AT10.SensorReplay.LazExternalCompressorFakeWritesOutput` covers the
  positive external process path with a local copy-command surrogate. This proves the
  `{input}`/`{output}` process contract and output-file validation, not true LAZ
  compression.
- `Scripts/validate_laz_placeholder_policy.ps1` checks that code, docs, monitor
  settings, and automation continue to describe this as a placeholder until true
  LAZ compression is integrated.
- `Scripts/export_laz_compression_decision_report.ps1` exports the current
  placeholder evidence, native-library / external-CLI / server-post-process
  candidate paths, and the acceptance evidence required before replacing the
  placeholder.
- `Scripts/export_laz_compressor_readiness_report.ps1` detects local compressor
  and reader candidates such as `laszip`, `las2las`, `lasinfo`, and `pdal`, and
  keeps the boundary clear: tool readiness is not the same as readable
  compressed `.laz` evidence.
  It now reports `ToolCandidateDiscoveryOnly = true`,
  `CompressorCandidateIsAcceptanceEvidence = false`, and
  `ReaderCandidateIsAcceptanceEvidence = false`, so candidate paths and version
  probes cannot be mistaken for acceptance evidence.
- The readiness report can now accept a produced `.laz` evidence file through
  `-LazEvidencePath` and can run an independent reader check with
  `-RunReaderProbe`. `ReadableOutputEvidencePresent` remains false unless the
  file exists, has a `.laz` extension, is non-empty, and the reader probe
  succeeds with a known point-cloud reader such as `lasinfo` or `pdal`.
  Requested-but-blocked probes are reported separately through
  `ReaderProbeBlockedReason`.
- `Scripts/report_precommit_summary.ps1 -IncludeReadiness` now surfaces the LAZ
  decision/readiness boundary directly, including `TrueCompressionIntegrated`,
  `ReadableOutputEvidencePresent`, `ReadyForRealLazAutomation`, and
  `ReadyToClaimTrueLaz`, so the pre-commit report does not confuse process
  contract evidence with readable compressed-output evidence.
- `Scripts/export_laz_compression_acceptance_package.ps1` creates a local
  `Saved/Reports/LazCompressionAcceptance` package with the decision report,
  compressor/readiness report, placeholder policy validation, manual acceptance
  steps, and follow-up commands. It does not install tools, run a compressor,
  modify assets, or stage files; reader probing runs only when explicitly
  requested.
  The package is an evidence shell, not readable LAZ proof:
  `AcceptancePackageIsEvidenceShell = true`,
  `AcceptancePackageIsReadableLazProof = false`, and
  `GeneratedReportDoesNotMeanAcceptancePassed = true` until produced `.laz`,
  known-reader validation, repeatable command, and owner acceptance evidence are
  filled.
- The package now also writes a fillable
  `LazCompressionAcceptanceEvidenceV1` draft and validates it with
  `Scripts/validate_laz_compression_acceptance_evidence.ps1`. True LAZ remains
  unclaimed until compressor selection, produced `.laz` output, known-reader
  validation, placeholder distinction, repeatable command, and owner acceptance
  evidence are all recorded.
- The evidence draft now includes structured `EvidenceSections`:
  `CompressorSelection`, `ProducedLazEvidence`, `KnownReaderValidation`,
  `PlaceholderDistinction`, `RepeatableCommand`, and `OwnerAcceptance`. The
  validator checks each section separately, so a copied LAS placeholder,
  unverified tool path, missing reader probe, or missing owner acceptance cannot
  accidentally mark `ReadyToClaimTrueLaz`.
- The validator now also checks that produced evidence has a `.laz` extension,
  is non-empty, has a recorded byte size matching the file, differs from the LAS
  source path, was produced by `ExportLastPointCloudLaz()` or an accepted
  post-process path, and was read by a known reader with exit code 0 against the
  same `.laz`. The package exposes `TopMissingAcceptanceChecks` and
  `ReadyToClaimTrueLazBlockers` so missing proof cannot be hidden in a generated
  report.
- The LAZ acceptance package now emits `EvidenceCopyHints` for the fillable
  acceptance JSON. These hints map readiness-report values into
  `CompressorSelection`, `ProducedLazEvidence`, `KnownReaderValidation`, and
  `PlaceholderDistinction`, while keeping `ReadyToAutoFillAcceptanceEvidence`
  false until readable `.laz` evidence and a successful known-reader probe exist.
- A selected compressor/workflow is acceptance metadata, not proof of true
  compressed output. `ReadyToClaimTrueLaz` still requires a produced `.laz`,
  known-reader validation, repeatable command evidence, placeholder distinction,
  and owner acceptance together.
- `Scripts/export_laz_compression_acceptance_runbook.ps1` writes
  `laz_compression_acceptance_runbook.json` and `.md` with the command order for
  placeholder policy validation, decision review, compressor/reader readiness,
  produced `.laz` evidence, known-reader probing, evidence fill, and strict
  validation.
- `Scripts/export_laz_compression_gap_summary.ps1` refreshes the LAZ package,
  runbook, and validator output, then lists missing acceptance checks by phase
  plus the next manual action. It writes only under `Saved`, does not install or
  run compressors, does not write `.laz` output, and does not stage files.
- Tool version probes are opt-in through `-ProbeToolVersions` on
  `Scripts/export_laz_compressor_readiness_report.ps1`; the default acceptance
  package does not run a compressor, write `.laz` output, or probe tool
  versions.

Next implementation steps:

- Decide whether true LAZ is required for this project.
- Export the LAZ acceptance package before compressor/tool owner review:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compression_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"`.
- Choose whether the external compressor path is sufficient, or whether a native
  library / server-side post-processing workflow is required.
- Configure an accepted compressor executable and argument template, then
  validate readable `.laz` output.
- Run the compressor readiness report after selecting or installing a candidate
  tool, optionally passing explicit compressor/reader paths.
- Capture readable-output evidence with the same report after producing a real
  `.laz` file.
- Keep current warning behavior until a real compressor is configured and
  verified.

Completion evidence:

- `.laz` output is actually compressed and readable by a known point-cloud tool.
- Automation verifies `.laz` creation separately from LAS source export.
- Missing-compressor guard passes:
  `M7AT10.SensorReplay.LazExternalCompressorMissingFails`.
- External process success guard passes:
  `M7AT10.SensorReplay.LazExternalCompressorFakeWritesOutput`.
- Static placeholder readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_laz_placeholder_policy.ps1"`.
- Pre-commit LAZ boundary appears:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -IncludeReadiness`.
- Local compressor readiness exports:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compressor_readiness_report.ps1"`.
- LAZ acceptance package exports local review evidence without running a
  compressor:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compression_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json`.
- LAZ acceptance gap summary exports the next true-LAZ handoff action:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compression_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json`.
- LAZ acceptance evidence validation reports missing evidence without modifying
  assets or staging files:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_laz_compression_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -EvidencePath "C:\Unreal Projects\m7at10_dt\Saved\Reports\LazCompressionAcceptance\laz_compression_acceptance.evidence.json" -Json`.
- LAZ acceptance evidence validation includes section-level checks for
  compressor selection, produced LAZ evidence, known-reader validation,
  placeholder distinction, repeatable command, and owner acceptance.
- LAZ acceptance evidence can be made a strict gate only after evidence is
  filled:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_laz_compression_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -EvidencePath "C:\Unreal Projects\m7at10_dt\Saved\Reports\LazCompressionAcceptance\laz_compression_acceptance.evidence.json" -FailOnIncompleteEvidence`.
- Explicit compressor/reader readiness exports:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compressor_readiness_report.ps1" -CompressorPath "C:\path\to\laszip.exe" -ReaderPath "C:\path\to\lasinfo.exe"`.
- Readable-output evidence export:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compressor_readiness_report.ps1" -CompressorPath "C:\path\to\laszip.exe" -ReaderPath "C:\path\to\lasinfo.exe" -LazEvidencePath "C:\path\to\frame.laz" -RunReaderProbe`.

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
- The renderer decision report now ranks candidate paths and recommends
  `Niagara point renderer` as the first GPU spike while preserving the CPU ISM
  fallback. It also exports decision gates for CPU fallback evidence, first GPU
  spike selection, GPU implementation, and fallback preservation.
- `UVirtualLidarSensorComp` now exposes `ELidarPointCloudPreviewBackend`,
  `PreviewBackend`, and `bAllowExperimentalGpuPreviewBackend`. The Niagara and
  custom-GPU values are candidate-only selectors; `IsGpuPreviewBackendActive()`
  remains false and the active path is reported as
  `cpu_instanced_mesh_fallback` until a real renderer is implemented and
  accepted.
- The renderer decision report now separates `RendererPhase` into
  `PreGpuSpike`, `GpuIntegratedEvidencePending`, and `GpuEvidenceReady`. This
  prevents a future Niagara/GPU integration from making the report fail merely
  because GPU code exists; once a GPU renderer is detected, the report requires
  viewport smoke, fallback preservation, and dense-frame evidence before it can
  become ready.
- The report also exports a `GpuSpikeActionPlan` and
  `GpuViewportSmokeEvidence` object. Required evidence includes screenshot path,
  screenshot byte count, nonblank pixel count, map name, sensor id, renderer
  name, point count, operator/notes, dense-frame no-stall observation, and CPU
  fallback toggle observation.
- The renderer decision report can now read the latest local CSV preview
  performance report and mark `CpuPreviewFallbackEvidencePresent` only when the
  required instanced smoke, 120,000-point procedural dense, and 250,000-point
  procedural budget scenarios are present in the selected automation run block
  without failure/error/fatal lines.
- CPU evidence is split into `CpuIsmFallbackSmokePresent` for the small
  InstancedMesh fallback smoke and `CpuProceduralDenseEvidencePresent` for the
  dense ProceduralMesh fallback path, so the report no longer treats the 250k
  procedural evidence as ISM evidence.
- `Scripts/run_csv_preview_performance_evidence.ps1` runs the dedicated
  headless CSV preview automation group, exports CSV and renderer decision
  reports under `Saved/Reports`, and requires both telemetry rows and automation
  success evidence before accepting the CPU fallback performance result.
- CSV evidence reports now record the selected run block and line-level proof:
  `EvidenceRunStartLine`, per-scenario telemetry `EvidenceLine`,
  per-scenario success lines, `TestCompleteLine`, and
  `EvidenceLinesWithinRun`. They also stop at the selected run completion and
  report `FailureEvidencePresent` / `FailureLineCount` for failed, error, or
  fatal lines inside that block.
- `M7AT10.Sensor.CsvPointCloudPreview.ProceduralHighDensityLoad` covers a
  120,000-point procedural CSV preview load without requiring the Unreal Editor
  GUI, and `M7AT10.Sensor.CsvPointCloudPreview.InstancedBatchLoad` keeps the
  instanced fallback path covered.
- CSV preview loads now expose runtime telemetry for input line count, accepted
  point count, procedural section count, instanced instance count, active render
  mode, load status, parse duration, build duration, and total load duration.
- `M7AT10.Sensor.CsvPointCloudPreview.ProceduralPerformanceBudget` covers a
  250,000-point procedural CSV preview load in headless automation with a
  generous regression guard while treating timing values as observational
  telemetry.
- `M7AT10.Sensor.CsvPointCloudPreview.AutoPromoteLargeInstanced` covers the
  large-instanced safety path. When an operator requests instanced CSV preview
  above the configured threshold, the actor promotes the effective renderer to
  procedural mesh and records requested/effective mode telemetry.
- `Scripts/export_csv_preview_performance_report.ps1` reads the latest Unreal
  automation log and exports the CSV preview telemetry for the instanced,
  120,000-point procedural, and 250,000-point procedural budget scenarios.
  This gives the CPU fallback path repeatable dense-frame evidence before the
  GPU/Niagara renderer decision is made.
- `Scripts/report_precommit_summary.ps1 -IncludeReadiness` now surfaces the
  renderer decision/readiness boundary directly, including `RendererPhase`,
  `GpuViewportSmokeEvidencePresent`, `GpuFallbackPreservationEvidencePresent`,
  `GpuDenseFrameEvidencePresent`, and `ReadyToClaimGpuDensePreview`.
- `Scripts/export_point_cloud_renderer_acceptance_package.ps1` now writes a
  local `Saved/Reports/PointCloudRendererAcceptance` bundle with the renderer
  decision report, static preview-policy validation, optional CSV preview
  performance evidence, manual GPU smoke follow-up commands, and a manifest
  that explicitly marks `DoesNotIntegrateGpuRenderer`, `DoesNotModifyAssets`,
  `CreatesNiagaraAssets = false`, `RunsGpuViewportSmoke = false`, and
  `StagesFiles = false`.
  The manifest separates a written CSV performance report shell from accepted
  automation evidence. `CsvPreviewPerformanceReportShellWritten = true` only
  means a review artifact exists; performance acceptance still requires
  `CsvPreviewPerformanceAutomationEvidencePresent = true`,
  `CsvPreviewPerformanceReportValid = true`, and
  `ReadyToClaimCsvPreviewPerformance = true`.
  Missing evidence is surfaced as
  `CsvPreviewPerformanceEvidenceMissingReason`.

Next implementation steps:

- Decide whether to use Niagara, GPU buffers, or a custom renderer.
- Start with the Niagara point-renderer spike unless a concrete density,
  packaging, or data-interface constraint invalidates it.
- Keep `PreviewBackend = NiagaraCandidate` or `CustomGpuCandidate` as a planning
  marker only until viewport smoke, fallback preservation, and dense-frame
  evidence exist; do not treat the selector as GPU integration evidence by
  itself.
- Preserve the current split: server payload can stay dense while preview is
  downsampled or GPU-rendered.
- Add pixel/screenshot smoke checks if a GPU renderer is added.
- When the GPU path lands, export the renderer decision report with viewport
  evidence fields such as `-ViewportScreenshotPath`, `-NonBlankPixelCount`,
  `-GpuSmokePointCount`, `-GpuSmokeMapName`, `-GpuSmokeSensorId`,
  `-GpuSmokeRendererName`, `-ObservedDenseFrameNoStall`, and
  `-ObservedFallbackToggle`.

Completion evidence:

- High-density scan can be previewed without editor stalls.
- Point-cloud-only mode still preserves collision/trace behavior.
- Dense server payload count is independent from preview point count.
- Headless CSV preview automation passes:
  `Automation RunTests M7AT10.Sensor.CsvPointCloudPreview`.
- CSV preview telemetry distinguishes parse/build/load timings from renderer
  counts and confirms the active CPU preview path.
- CSV preview performance evidence exports from the local automation log:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_csv_preview_performance_report.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt"`.
- CSV preview performance evidence can require automation completion evidence:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_csv_preview_performance_report.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -RequireAutomationSuccess`.
- CSV preview performance evidence can target an explicit log file:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_csv_preview_performance_report.ps1" -LogPath "C:\Unreal Projects\m7at10_dt\Saved\Logs\m7at10_dt.log" -RequireAutomationSuccess`.
- Renderer decision evidence can be required against the same local log:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_decision_report.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -RequireCsvPerformanceEvidence`.
- Renderer decision evidence can also target an explicit log file:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_decision_report.ps1" -LogPath "C:\Unreal Projects\m7at10_dt\Saved\Logs\m7at10_dt.log" -RequireCsvPerformanceEvidence`.
- Renderer acceptance package exports the current CPU/GPU evidence boundary:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt"`.
- Renderer acceptance package can also consume the explicit CSV automation log:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -LogPath "C:\Unreal Projects\m7at10_dt\Saved\Logs\m7at10_dt.log"`.
- GPU renderer decision evidence records viewport smoke once a GPU path exists:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_decision_report.ps1" -ViewportScreenshotPath "C:\path\to\gpu_viewport.png" -ViewportScreenshotBytes 123456 -NonBlankPixelCount 1000 -GpuSmokePointCount 120000 -GpuSmokeMapName "TestMap" -GpuSmokeSensorId "Lidar01" -GpuSmokeRendererName "Niagara point renderer" -GpuSmokeOperator "name" -GpuSmokeNotes "dense frame viewport smoke" -ObservedDenseFrameNoStall -ObservedFallbackToggle`.
- GPU evidence readiness requires a nonblank viewport screenshot, point count,
  renderer identity, dense-frame no-stall observation, and fallback toggle
  observation; otherwise `RendererPhase` remains
  `GpuIntegratedEvidencePending`.
- Pre-commit renderer boundary appears:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -IncludeReadiness`.
- End-to-end local CPU fallback evidence workflow passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\run_csv_preview_performance_evidence.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -SkipBuild`.
- Static readiness passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_point_cloud_preview_policy.ps1"`.
- Static readiness also confirms that candidate GPU preview backends are
  blocked from claiming active GPU rendering and continue to report CPU fallback
  as the active path.

### Production Monitor WBP

Current state:

- Native fallback exists and is covered by automation.
- Designer-authored `WBP_VirtualSensorMonitor` is still a local binary decision.
- `Scripts/validate_monitor_widget_policy.ps1` checks that optional bindings,
  native fallback, local WBP decision guards, setup docs, and monitor automation
  names remain in sync before the binary WBP is committed.
- `Scripts/validate_monitor_wbp_acceptance_evidence.ps1` now provides a
  read-only acceptance-evidence gate for the local WBP. It keeps missing
  evidence as an incomplete report by default and only fails the command when
  `-FailOnIncompleteEvidence` is explicitly requested.
- `Scripts/export_monitor_wbp_acceptance_package.ps1` now creates a local
  `Saved/Reports/MonitorWbpAcceptance` package with preflight, decision,
  fillable evidence, validation, manual steps, and strict follow-up commands.
  This package is still not WBP acceptance; it only reduces the manual Editor
  evidence collection gap.
- Codex can safely modify native `UVirtualSensorMonitorWidget` bindings,
  Blueprint-callable APIs, host actor behavior, and the WBP acceptance tooling.
  It must not byte-patch `WBP_VirtualSensorMonitor.uasset` directly. Actual WBP
  Designer/layout edits require Unreal Editor, pre-edit hash/backup, compile and
  save evidence, post-edit hash, PIE smoke, and owner acceptance before staging.
- The native monitor now exposes WBP-facing state helpers for Designer bindings:
  `IsShowingLidar`, `HasBoundCamera`, `HasBoundLidar`,
  `GetLastManualExportMessage`, and `GetLazExportSummaryText`. The
  `FVirtualSensorMonitorDisplayData` struct also includes `LazExportText`, so a
  WBP can show the LAZ placeholder/compressor/true-validation boundary without
  parsing the full debug status string.
- `GetAcceptanceGateSummaryText` and
  `FVirtualSensorMonitorDisplayData::AcceptanceGateText` expose WBP/manual PIE,
  server payload, true-LAZ, and real-sensor deployment acceptance state from UE
  runtime code, keeping the operator monitor aligned with the external
  gap-summary reports.
- `BindRealSensorSource`, `GetRealSensorDeploymentSummaryText`, and
  `FVirtualSensorMonitorDisplayData::RealSensorText` now let the monitor show
  the selected real-sensor source deployment readiness directly from
  `URealSensorSourceComp` without parsing the full debug status string.
- `AVirtualSensorManager` now discovers/registers `URealSensorSourceComp`
  instances, exposes real-source summaries, and binds the source that targets
  the selected LiDAR into the monitor automatically.
- `FVirtualSensorHealthSummary` now includes real-source counts, running/error
  counts, and external-deployment-evidence counts so the monitor `Health:` row
  can show real-source readiness without parsing individual source strings.
- `AVirtualSensorManager` now exposes separate real-source start, stop, and
  selected-source push controls. They remain separate from virtual sensor
  capture controls to avoid duplicate virtual scans during replay/live input.
- `UVirtualSensorMonitorWidget` now exposes the same controls through
  Blueprint-callable functions and optional start/stop/push buttons, while
  preserving existing WBP compatibility when those buttons are absent.
- The native fallback toolbar exposes the same controls, allowing code-native
  replay/live-source smoke before manual WBP acceptance.

Next implementation steps:

- Export the WBP acceptance package before the manual Editor pass:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."`.
- Open the WBP in Unreal Editor and verify optional bindings.
- If the WBP layout must change, perform the edit through Unreal Editor only;
  do not manually patch the binary `.uasset`.
- After saving the WBP in Unreal Editor, export post-edit hash evidence with
  `Scripts/export_monitor_wbp_post_edit_hash_report.ps1`, then run
  `Scripts/update_monitor_wbp_acceptance_hash_evidence.ps1` to copy the
  reported hash fields into `monitor_wbp_acceptance.evidence.json`.
- Confirm camera/LiDAR switching, preview budget controls, hit-only toggle,
  server payload export, and slab analysis text.
- Export the focused WBP decision report with `-EvidencePath` after filling
  editor-open, optional-binding, PIE-smoke, and production-acceptance evidence.
- Run the WBP acceptance evidence validator against the filled evidence file and
  confirm `ReadyToStageCandidate=true` before considering the binary asset for
  repository staging.
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
- Focused WBP evidence gate passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -EvidencePath ".\docs\local_asset_decisions.evidence.json" -FailOnIncompleteEvidence`.
- WBP acceptance evidence validator passes:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_wbp_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -EvidencePath ".\docs\local_asset_decisions.evidence.json" -FailOnIncompleteEvidence`.
- WBP acceptance template exports:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_template.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json`.
- WBP acceptance package exports a local review bundle without touching assets:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json`.

## Routine Verification

Fast local readiness check:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SourceRepoRoot "." -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SkipSmoke
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SourceRepoRoot "." -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SkipSmoke -Json
```

Use the `-SourceRepoRoot` form when running from the source checkout while the
actual untracked Unreal asset/config decisions are in the local project. The
wrapper keeps policy/docs validation on the source checkout and scans local
asset, runtime config, and large-content decisions from `-ProjectRoot`.

Pre-commit progress and remaining-work summary:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_local_decision_precommit_gate_policy.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -SourceRepoRoot "." -ProjectRoot "C:\Unreal Projects\m7at10_dt" -IncludeReadiness
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -SourceRepoRoot "." -ProjectRoot "C:\Unreal Projects\m7at10_dt" -IncludeReadiness -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_goal_progress_blocker_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_goal_progress_blocker_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -ObservedCodexUsagePercent 84
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
```

`export_goal_progress_blocker_report.ps1` writes
`Saved/Reports/GoalProgress` with the current overall percent, remaining
percent, blocked external-evidence items, PixelStreaming exclusion,
unused-content scope exclusion, rough remaining calendar-time expectation, and
Codex-thread reset guidance. It also separates Codex-advanceable blockers from
external-only blockers and records the next recommended Codex work area. It is
progress evidence only and does not modify assets or stage files.
When the Codex UI shows a usage percentage, pass it as
`-ObservedCodexUsagePercent`. This records user-observed usage, remaining usage,
and reset guidance separately from project progress. The script cannot read
Codex GPT Plus quota directly and should not be used to promise exact remaining
calendar weeks.
`export_monitor_wbp_gap_summary.ps1` is the quickest WBP handoff view: it
refreshes the package, TODO, runbook, and validator output under `Saved`, then
lists missing evidence by phase plus the next manual Editor/PIE action. It does
not modify assets or stage `WBP_VirtualSensorMonitor.uasset`.

The percentages are planning snapshots derived from the current remaining-work
areas. They are not a substitute for build, smoke, editor, or server acceptance
evidence. Use `-IncludeReadiness` before a commit when the percent summary
should also show the fast readiness JSON result and skipped gate list. The
`ReadinessSummary.ReadinessMode = FastStaticPrecommit` result is a quick
pre-commit signal; `FastReadinessPassed` means the fast/static gates passed, not
that full PIE, broker, LAZ, editor, or deployment evidence is complete. Skipped
steps are reported with evidence boundaries and still require manual, editor,
deployment, or full smoke evidence before the related work is complete.
PixelStreaming is intentionally excluded from these percentages for the current
LiDAR/virtual-sensor scope; the local sample folder is only guarded against
accidental staging.

Full local readiness check:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
```

Strict asset gates for clean-content review:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_large_content_cleanup_plan.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_sample_content_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnStagedDecisionPoints
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnLargeContentCandidates
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```
