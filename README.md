# UE-DT-Project

## CSV Preview Safety

`ACsvPointCloudPreviewActor` can auto-promote a large preview requested as
`InstancedMesh` to the procedural preview path when
`bAutoPromoteLargeInstancedPreviewToProcedural` is enabled. Telemetry records
both requested and effective render modes, plus whether auto-promotion happened.

## Point Cloud Renderer Evidence Boundary

`export_point_cloud_renderer_acceptance_package.ps1` can write a CSV preview
performance report shell even when Unreal automation log evidence is missing.
Treat CSV preview performance as accepted only when
`CsvPreviewPerformanceAutomationEvidencePresent`,
`CsvPreviewPerformanceReportValid`, and `ReadyToClaimCsvPreviewPerformance` are
all true.

Unreal Engine 5.3 기반 Digital Twin 프로젝트입니다. 철강 제조 환경을 대상으로 가상 센서와 가상 카메라를 배치하고, 시뮬레이션 또는 실제 센서 입력을 Unreal 환경에 재현한 뒤 판단 서버로 측정 데이터를 전달하는 것을 목표로 합니다.

현재 브랜치의 중심 기능은 가상 LiDAR/카메라, point-cloud-only view, Slab 각도 분석 v1, CSV/JSONL replay source, sensor monitor widget/host actor, 로컬 smoke test 및 asset 상태 점검 스크립트입니다. Livox, RealSense, ROS2 직접 입력은 placeholder component까지 준비되어 있고 실제 SDK/bridge 연결은 후속 작업입니다.

## HTTP Transport Backpressure

`UVirtualSensorDataTransportComp` caps concurrent HTTP POST requests with
`MaxInFlightHttpRequests`. Frames above the cap are rejected locally with
`bBackpressureRejected=true` instead of growing an unbounded request queue.
Runtime telemetry exposes current in-flight and cumulative rejection counts.
`MaxHttpRetryAttempts` applies only to connection failures and 5xx responses
when their policy toggles are enabled. 4xx responses are final and are not
retried.
`FailedHttpRequestCount` counts final non-accepted responses, while
`RetryExhaustedRequestCount` identifies retry-eligible failures that consumed
their configured retry budget.

## 목표 기능

1. 시뮬레이션 데이터를 Unreal에 재현하고 가상 카메라/LiDAR 측정값을 판단 서버로 전송합니다.
2. 실제 센서/카메라 데이터를 받아 Unreal 환경에 재현합니다.
3. 철강 공정의 Slab 각도 틀어짐을 LiDAR point cloud 기반으로 분석합니다.
4. 판단 서버용 원본/고밀도 데이터와 에디터 UI용 간략 preview 데이터를 분리합니다.

## 저장소 구성

```text
Source/m7at10_dt/M7AT10/Camera   가상 카메라 component/actor
Source/m7at10_dt/M7AT10/Sensor   가상 LiDAR, 센서 매니저, replay source, transport/recorder
Source/m7at10_dt/M7AT10/UI       센서 모니터 widget 및 host actor
Source/m7at10_dt/M7AT10/Crane    크레인 예제 구현
Plugins/DTCore                   공통 Core 플러그인 submodule
Content/M7AT10                   map, widget, Blueprint asset
docs                             payload schema, smoke test, adapter plan, widget setup
Samples                          replay sample data
Scripts                          smoke/status helper scripts
```

## DTCore Submodule

처음 받은 뒤에는 반드시 submodule을 초기화합니다.

```powershell
git submodule update --init --recursive
git submodule status
```

기준 commit:

```text
2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7 Plugins/DTCore
```

이번 작업 범위에서는 DTCore 내부 소스를 직접 수정하지 않습니다. 현재 DTCore 쪽에서 `EnhancedInput` dependency 경고가 보일 수 있지만, DTCore 수정 허용 전까지 DT-Project 쪽에서는 우회하지 않습니다.

커밋 전 DTCore submodule guard:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_dtcore_submodule_guard.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_dtcore_submodule_guard.ps1" -FailOnViolation
```

이 검사는 read-only입니다. `Plugins/DTCore`를 수정하거나 stage하지 않고,
기대 커밋 `2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7` 및 parent/submodule
worktree 청결 상태만 확인합니다.
DTCore is an external submodule pinned to that commit; this repository must not
stage gitlink changes or files under `Plugins/DTCore` during local decision
cleanup.

## 빌드

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" m7at10_dtEditor Win64 Development "C:\path\to\m7at10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

로컬 실행 프로젝트 예시:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" m7at10_dtEditor Win64 Development "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

## LiDAR 데이터 정책

`UVirtualLidarSensorComp`는 한 번의 scan 또는 replay frame에서 생성된 전체 측정값을 `LastPoints`에 유지합니다.

판단 서버 payload 정책:

- `ServerPayloadStride`
- `MaxServerPayloadPoints`
- `bIncludeMissPointsInServerPayload`
- JSON `schemaVersion = virtual-lidar.v1`
- JSON `payloadPolicy`
- JSON `payloadPolicy.pointSelection = hit_only | hit_and_miss`
- JSON `slabAnalysis`

Editor/UI preview 정책:

- `PreviewPointStride`
- `MaxPreviewPoints`
- `bPointCloudPreviewHitOnly`
- JSON `previewPolicy`

기본 방향은 서버에는 원본에 가까운 hit point를 보내고, Unreal Editor 화면에는 제한된 point만 표시하는 것입니다. 기존 `PayloadPointStride`, `MaxPayloadPoints`, `PointCloudPreviewStride`, `MaxPointCloudPreviewInstances`는 Blueprint 호환을 위해 남아 있지만 새 정책 필드를 우선 사용합니다.

`MaxServerPayloadPoints = 0`은 서버 payload 상한이 없다는 의미이며,
런타임 performance warning에 uncapped 상태가 표시됩니다.

## Camera 데이터 정책

`UVirtualCameraComp`는 SceneCapture 기반 image payload와 monitor 상태 표시를 제공합니다.

- JSON `schemaVersion = virtual-camera.v1`
- resolution, capture mode, JPEG quality, capture interval
- cached payload byte size
- render target preview
- file transport/export 연동

Monitor의 `ExportSelectedSensorServerPayload`는 현재 view에 따라 LiDAR 또는 camera payload를 `Saved/SensorCaptures/<SensorId>/ServerPayload` 아래로 저장합니다.

## File Replay Source

`ULidarCsvReplaySourceComp`와 `ULidarJsonLinesReplaySourceComp`는 저장된 point cloud를 `UVirtualLidarSensorComp::InjectPointCloudFrame`으로 주입합니다. 이 경로를 타면 replay 데이터도 LiDAR JSON payload, recorder, transport, preview, Slab 분석 흐름을 동일하게 통과합니다.

지원 CSV 형식:

```text
row,col,x,y,z
x,y,z
```

지원 JSONL 형식:

```text
{"x":900,"y":-260,"z":0,"distance":936.8,"hit":true,"semanticLabel":"Slab"}
{"worldLocation":[900,-260,0],"localDirection":[1,0,0],"hit":true,"semanticLabel":"Slab"}
```

샘플 파일:

```text
Samples/slab_replay_sample.csv
Samples/slab_replay_sample.jsonl
```

Editor Details 패널 버튼:

```text
PushFrameOnceInEditor
PushFrameOnceNoTransportInEditor
StartReplayInEditor
StopReplayInEditor
```

## 실제 센서 Source Placeholder

현재 준비된 placeholder component:

```text
URos2SensorBridgeSourceComp
ULivoxLidarSourceComp
URealSenseCameraSourceComp
```

아직 SDK/bridge 연결은 수행하지 않고 not-implemented 상태 메시지를 제공합니다. 후속 구현에서도 가능한 한 `InjectPointCloudFrame` 같은 normalized frame 주입 경로를 공유해야 합니다.

## Slab 각도 분석 Workflow

1. Slab actor 또는 component 이름/tag를 `Slab`, `SteelSlab`, `Plate` 중 하나와 매칭되게 설정합니다.
2. `UVirtualLidarSensorComp`에서 `SlabSemanticLabel = Slab`을 유지합니다.
3. 정상 기준 방향에 맞춰 `ReferenceSlabYawDegrees`를 설정합니다.
4. LiDAR scan/replay 후 `FVirtualLidarSlabAnalysisResult` 또는 JSON `slabAnalysis`를 확인합니다.

분석 결과에는 slab hit point count, bounds, center, estimated yaw, reference yaw, angle deviation, confidence, status message가 포함됩니다.

## Widget 조작

권장 Widget parent class:

```text
UVirtualSensorMonitorWidget
```

권장 optional binding 이름:

```text
ViewImage
TitleText
StatusText
ToggleButton
ToggleButtonText
NextCameraButton
NextLidarButton
PointCloudOnlyButton
LidarViewModeButton
LidarViewModeButtonText
LogPointCloudButton
ExportPointCloudButton
LocalSensorCaptureButton
LocalSensorCaptureButtonText
CaptureOnceButton
ExportServerPayloadButton
PreviewMoreButton
PreviewLessButton
PreviewHitOnlyButton
PreviewHitOnlyButtonText
StartRealSensorSourcesButton
StopRealSensorSourcesButton
PushRealSensorSourceButton
```

Status helper:

```text
GetMonitorTitleText
GetMonitorStatusText
GetMonitorDisplayData
GetRealSensorDeploymentSummaryText
GetTransportStatusSummaryText
```

SensorMonitor Blueprint API:

```text
BindRealSensorSource
CaptureSelectedSensorsOnce
StartRealSensorSources
StopRealSensorSources
PushSelectedRealSensorSourceOnce
ExportSelectedSensorServerPayload
ExportSelectedLidarServerPayload
SetLidarPreviewBudget
IncreaseLidarPreviewBudget
DecreaseLidarPreviewBudget
ToggleLidarPreviewHitOnly
```

The native fallback toolbar also exposes Start/Stop/Push controls for the
selected real sensor source, so replay/live-source smoke does not depend on an
accepted production WBP.

SensorManager Blueprint API:

```text
CaptureSelectedOnce
CaptureAllOnce
RegisterRealSensorSource
GetSelectedRealSensorSource
GetRealSensorSourceSummaries
GetHealthSummary
StartAllRealSensorSources
StopAllRealSensorSources
PushSelectedRealSensorSourceOnce
SetSelectedLidarPreviewPolicy
AdjustSelectedLidarPreviewBudget
TogglePointCloudOnlyView
SetPointCloudOnlyMode
```

Level Blueprint 없이 monitor를 자동 생성하려면 map에 `AVirtualSensorMonitorHostActor`를 배치하고 `MonitorWidgetClass = WBP_VirtualSensorMonitor`를 지정합니다.

`MonitorWidgetClass`가 비어 있고 `bUseNativeMonitorWidgetFallback`이 true이면 HostActor가 native `UVirtualSensorMonitorWidget`를 생성합니다. 이 fallback은 smoke test와 기본 조작 확인용이며, 운영 UI는 Designer에서 만든 `WBP_VirtualSensorMonitor` 사용을 권장합니다.

## 자동화 테스트

Payload fixture 검증:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_fixtures.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_fixtures.ps1" -Json
```

Payload mock contract 검증:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_contract.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_contract.ps1" -Json
```

로컬 readiness gate:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
```

이미 빌드된 상태에서 빠르게 확인:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipBuild
```

asset report만 확인하고 smoke test는 생략:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -SkipPayloadContract
```

전체 로컬 smoke gate:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

이미 빌드된 경우:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1" -SkipBuild
```

권장 smoke 설정:

```text
AVirtualSensorManager.bDiscoverOnBeginPlay = true
AVirtualSensorManager.bStartSensorsOnBeginPlay = true
SharedSensorTransport.TransportMode = LogOnly
LiDAR SimulationQuality = RealTimePreview
Camera SimulationQuality = RealTimePreview
LiDAR bUseMultiHit = false
LiDAR ExportOnScan = false
LiDAR bDrawDebugRays = false
```

성능 문제가 있으면 다음 순서로 조정합니다.

```text
1. LiDAR SimulationQuality = Debug
2. PreviewPointStride 증가
3. MaxPreviewPoints 감소
4. Camera CaptureMode = PreviewOnly
5. MultiHit / ExportOnScan / FullSpec 비활성화
```

## 로컬 Asset 상태 점검

커밋 전 진행률/남은 작업/커밋 예정 파일 요약:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_local_decision_precommit_gate_policy.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_goal_progress_blocker_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_goal_progress_blocker_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -ObservedCodexUsagePercent 84
```

`invoke_local_decision_precommit_gate.ps1` is the fastest commit-time local
decision guard. It allows the known local-only untracked files to remain local,
but fails accidental staging or invariant violations for local decision paths,
`Samples/PixelStreaming`, and `Plugins/DTCore`.
`export_goal_progress_blocker_report.ps1` writes a read-only
`Saved/Reports/GoalProgress` snapshot with the current progress percent,
remaining percent, external-evidence blockers, PixelStreaming exclusion, rough
calendar-time expectation, Codex-advanceable vs external-only blockers, and
Codex-thread reset guidance.
Pass `-ObservedCodexUsagePercent` only when the UI shows a usage percentage; the
script records that user-observed value separately from project progress because
it cannot read Codex GPT Plus quota directly or convert quota to calendar weeks.

Monitor WBP manual acceptance package:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\prepare_monitor_wbp_editor_review.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_post_edit_hash_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -EvidencePath "C:\Unreal Projects\m7at10_dt\Saved\Reports\MonitorWbpAcceptance\monitor_wbp_acceptance.evidence.json"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_wbp_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -EvidencePath "C:\Unreal Projects\m7at10_dt\Saved\Reports\MonitorWbpAcceptance\monitor_wbp_acceptance.evidence.json" -Json
```

The package writes local `Saved/Reports/MonitorWbpAcceptance` review files for
Editor-open, optional binding, PIE smoke, exported payload, and owner acceptance
evidence. It does not modify assets, stage files, or accept the binary WBP by
itself.
`prepare_monitor_wbp_editor_review.ps1` additionally creates a timestamped
backup under `Saved/Backups/MonitorWbp`, records the pre-edit SHA256 hash,
generates the WBP acceptance package, and writes a local editor-review checklist
under `Saved/Reports/MonitorWbpEditorReview`. It writes only under `Saved`; it
does not modify or stage `WBP_VirtualSensorMonitor.uasset`.
`export_monitor_wbp_gap_summary.ps1` refreshes the WBP package, TODO, runbook,
and validator output, then writes a compact phase summary and next manual action
under `Saved/Reports/MonitorWbpAcceptance`. It is read-only for assets and git.
The editor-review checklist enumerates the required DisplayData rows, including
`LazExportText`, the `IsShowingLidar`/`HasBoundCamera`/`HasBoundLidar` helper
checks, `GetLastManualExportMessage` export evidence, and the strict
post-edit validation command to run after screenshot/log/export evidence paths
are filled.
After saving the WBP in Unreal Editor, run
`export_monitor_wbp_post_edit_hash_report.ps1`, then run
`update_monitor_wbp_acceptance_hash_evidence.ps1` to copy the reported
`AssetHash`, `PostEditAssetHash`, and `PostEditHashReportPath` values into
`monitor_wbp_acceptance.evidence.json` before strict validation or staging.
The updater edits only the local Saved evidence JSON, creates a timestamped
backup, and never modifies or stages the binary WBP asset.
When the WBP acceptance package is regenerated, an existing
`monitor_wbp_acceptance.evidence.json` is preserved and a fresh comparison
template is written separately as `monitor_wbp_acceptance.template.fresh.json`.
The WBP acceptance template and validator derive optional widget names from the
native `BindWidgetOptional` fields in `VirtualSensorMonitorWidget.h`, then ask
the reviewer to record `IsVariable`, `WidgetClass`, `BoundToExpectedCppName`,
and `MissingOptionalDoesNotCrash` evidence for the actual Designer widget.
The WBP acceptance validator and package also emit `MissingEvidenceActions`,
mapping each failed check to an `EvidenceTarget` and concrete `NextAction`.
The package writes `monitor_wbp_missing_evidence_actions.json` and
`monitor_wbp_missing_evidence_actions.md`, grouped by `EvidencePhase` and
`BlockingStage`, so the manual Editor/PIE pass has a focused checklist before
any attempt to stage `WBP_VirtualSensorMonitor.uasset`.
It also writes `monitor_wbp_evidence_todo.json` and
`monitor_wbp_evidence_todo.md` with checkbox rows grouped by phase for the
manual reviewer.
After collecting Editor/PIE screenshots, logs, or exported payload files, use
`update_monitor_wbp_manual_evidence_paths.ps1` to merge those paths and common
run metadata into the Saved evidence JSON without touching the WBP asset.
Use `update_monitor_wbp_manual_acceptance_sections.ps1` to mark manual
acceptance sections present/accepted from real evidence files. Owner acceptance
requires the explicit `-AcceptOwnerAcceptance` switch.
Use `export_monitor_wbp_acceptance_runbook.ps1` to create
`monitor_wbp_acceptance_runbook.md`, a one-page command sequence for the manual
Editor/PIE acceptance pass.
The evidence template also asks for a `DisplayData visual match`: during PIE,
map each `GetMonitorDisplayData()` row to the visible WBP TextBlock so title,
selected sensor, frame, measurement, server payload, preview, slab, warning,
and view-mode rows are verified against the native status contract. Record
`bShowingLidar`/`SensorMode` and accept the matching
`DisplayDataScreenMatchEvidence` manual section before staging the binary WBP.
The local `WBP_VirtualSensorMonitor.uasset` can exist as an untracked review
candidate while `ReadyToStageMonitorWbpAsset` remains false. Do not stage it
until editor-open, widget-binding, PIE-smoke, no-crash, and owner-acceptance
evidence sections are complete.
Codex can safely update native C++/Blueprint-callable monitor bindings and the
acceptance tooling, but should not byte-patch the binary `.uasset` directly.
Actual WBP layout/Designer changes must be made through Unreal Editor with a
pre-edit hash/backup, compile/save evidence, post-edit hash, PIE smoke, and
owner acceptance before the asset is staged.
For WBP layout work, bind individual TextBlocks to the native summary getters
such as `GetSelectedSensorIdText`, `GetFrameSummaryText`,
`GetServerPayloadSummaryText`, `GetPreviewPolicySummaryText`, and
`GetSlabAnalysisSummaryText`, plus `GetLazExportSummaryText` for the LAZ
placeholder/compressor boundary, instead of parsing the full debug status text.
`GetAcceptanceGateSummaryText` exposes WBP/manual PIE, server-payload, true-LAZ,
and real-sensor deployment evidence state directly from the UE widget.
Use `IsShowingLidar`, `HasBoundCamera`, `HasBoundLidar`, and
`GetLastManualExportMessage` for WBP visibility/enabled-state rules.

Judging-server acceptance package:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_judging_server_acceptance_package.ps1" -ProjectRoot "."
```

The package writes local `Saved/Reports/JudgingServerAcceptance` review files
for payload contract, transport contract, and fillable real-server acceptance
evidence. Endpoint URLs, tokens, passwords, and credential values must stay out
of the repository and out of generated review artifacts.
The fillable evidence draft separates endpoint ownership, authentication policy,
response schema, real endpoint smoke, rate/backpressure, secret redaction, and
owner acceptance sections; these sections are metadata and do not by themselves
claim real judging-server acceptance.

Real sensor adapter deployment package:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_real_sensor_adapter_deployment_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_real_sensor_adapter_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
```

The package writes local `Saved/Reports/RealSensorAdapterDeployment` review
files for adapter readiness, WebSocket sample/registration, broker-smoke draft,
deployment evidence draft/validation, and follow-up evidence. It does not
connect to external brokers or SDKs, does not write endpoint/credential values,
and does not modify assets or stage files.
The evidence draft separates deployment-path sections for replay, HTTP JSON
live, DTCore WebSocket, UDP JSON live, ROS2, Livox, and RealSense. A path section
is review metadata only; local loopback checks and placeholder SDK components
do not prove production real-sensor deployment until the selected path has live
smoke evidence and owner acceptance.
`URealSensorSourceComp::GetDeploymentReadinessSummaryText()` exposes the same
deployment boundary in UE runtime code for Details, Blueprint, or monitor
integration: replay is a schema baseline, while JSON live, ROS2, Livox,
RealSense, and custom sources require external deployment evidence.
Generated deployment reports expose `PreDeploymentEvidenceOnly = true`,
`BrokerlessDispatchIsDeploymentEvidence = false`,
`LoopbackSmokeIsDeploymentBrokerEvidence = false`,
`StaticTransactionRegistrationIsBrokerAcceptance = false`,
`AcceptancePackageIsEvidenceShell = true`, and
`AcceptancePackageIsDeploymentProof = false`. Report generation is not
deployment acceptance, does not modify DTCore, and does not cover
PixelStreaming.
Use `export_real_sensor_adapter_gap_summary.ps1` to refresh the deployment
package and validator output, then write a compact phase summary and next manual
action under `Saved/Reports/RealSensorAdapterDeployment`. It never connects to
brokers or SDKs, writes endpoint/credential values, modifies DTCore/assets, or
stages files.
PixelStreaming is out of scope for the current LiDAR/virtual-sensor work. The
local `Samples/PixelStreaming/` folder should stay untracked, but its
ownership/license decision is not counted as current remaining implementation
work.

LAZ compression acceptance package:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compression_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compression_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
```

The package writes local `Saved/Reports/LazCompressionAcceptance` review files
for the current placeholder, compressor decision, reader/readable-output
evidence, a fillable `LazCompressionAcceptanceEvidenceV1` draft, validation
results, and follow-up commands. It does not install tools, run a compressor,
modify assets, or stage files.
Compressor/reader candidate discovery is readiness metadata, not acceptance
evidence. The package is an evidence shell, not readable LAZ proof, until a
produced `.laz` is validated by a known reader and owner acceptance is recorded.
The evidence draft separates `CompressorSelection`, `ProducedLazEvidence`,
`KnownReaderValidation`, `PlaceholderDistinction`, `RepeatableCommand`, and
`OwnerAcceptance`, so placeholder or copy-surrogate output cannot be mistaken
for accepted LAZ compression.
The validator now checks that produced evidence is a non-empty `.laz`, that its
recorded byte size matches the file, that it differs from the LAS source path,
that a known reader probe exits with code 0 against the same `.laz`, and that
the output is not a `_laz_source_` placeholder or copy-surrogate artifact. The
package summary also reports top missing checks and true-LAZ blockers.
The acceptance package also emits `EvidenceCopyHints` so readiness-report values
such as selected compressor/reader paths, produced `.laz` path, byte size, and
reader probe status can be copied into the fillable evidence JSON without
guessing which section owns each value.
Use `export_laz_compression_acceptance_runbook.ps1` to create
`laz_compression_acceptance_runbook.md`, a command sequence for placeholder
policy validation, compressor/reader readiness, produced `.laz` evidence,
reader probing, evidence fill, and strict validation.
Use `export_laz_compression_gap_summary.ps1` to refresh the LAZ package,
runbook, and validator output, then write a compact phase summary and next
manual action under `Saved/Reports/LazCompressionAcceptance`. It is read-only
for assets, git, tools, and compressor execution.

The LiDAR component also exposes last LAZ export telemetry for widgets and
automation: status text, LAS source path, LAZ output path, placeholder-only,
export attempt/success, external-compressor requested/attempted/succeeded,
produced-output, true-validation, exported point count, return code, output
size, and warning fields. These fields describe the most recent export attempt
only; readable `.laz` acceptance still requires a produced file, known-reader
validation, and owner acceptance.

로컬 프로젝트 상태와 untracked asset decision point 확인:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

구조화된 결과:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -Json
```

Gate 예시:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnCategory LargeContentCandidate,SampleOrThirdParty
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnUnclassifiedUntracked
```

`Windows/`, `Windows.zip`, `launcher.config.json`은 `.gitignore`에 등록되어 git status에서는 숨겨질 수 있지만, 이 스크립트는 파일 시스템을 직접 확인해 존재 여부를 계속 보고합니다.

## 관련 문서

```text
docs/lidar_payload_schema.md
docs/camera_payload_schema.md
docs/widget_designer_setup.md
docs/editor_smoke_test.md
docs/real_sensor_adapter_plan.md
docs/local_asset_report.md
docs/remaining_work.md
```

## 알려진 제한

- Livox SDK, RealSense SDK, ROS2 bridge 직접 입력은 아직 구현되지 않았습니다.
- `ExportLastPointCloudLaz()`는 실제 LAZ 압축이 아닙니다. 현재는 `*_laz_source_*.las` 형식의 LAS 호환 source 파일을 저장하고 warning log를 남깁니다.
- FullSpec, MultiHit, ExportOnScan을 동시에 켜면 editor 성능이 크게 떨어질 수 있습니다.
- 대규모 point cloud rendering은 아직 CPU/instance 기반 preview 한계가 있어 GPU/Niagara 기반 renderer 검토가 필요합니다.
  LiDAR `PreviewBackend`에서 Niagara/custom GPU 후보를 선택할 수 있지만, 현재 후보 값은 CPU fallback 상태 표시용이며 GPU 구현 완료 증거가 아닙니다.
  현재 CPU fallback/GPU smoke evidence boundary는 다음 패키지로 로컬에 export할 수 있습니다:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt"`.
- 실제 map에서의 PIE smoke test와 WBP Designer 배치 검증은 별도 editor 작업이 필요합니다.
- `WBP_VirtualSensorMonitor.uasset`의 실제 레이아웃 수정은 Unreal Editor를
  통해서만 진행합니다. 코드/바인딩/API는 Codex가 수정 가능하지만 `.uasset`
  바이너리 직접 패치는 금지합니다.

남은 구현/에셋 결정/완료 판정 기준은 `docs/remaining_work.md`에서 추적합니다.
