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

Unreal Engine 5.3 湲곕컲 Digital Twin ?꾨줈?앺듃?낅땲?? 泥좉컯 ?쒖“ ?섍꼍????곸쑝濡?媛???쇱꽌? 媛??移대찓?쇰? 諛곗튂?섍퀬, ?쒕??덉씠???먮뒗 ?ㅼ젣 ?쇱꽌 ?낅젰??Unreal ?섍꼍???ы쁽?????먮떒 ?쒕쾭濡?痢≪젙 ?곗씠?곕? ?꾨떖?섎뒗 寃껋쓣 紐⑺몴濡??⑸땲??

?꾩옱 釉뚮옖移섏쓽 以묒떖 湲곕뒫? 媛??LiDAR/移대찓?? point-cloud-only view, Slab 媛곷룄 遺꾩꽍 v1, CSV/JSONL replay source, sensor monitor widget/host actor, 濡쒖뺄 smoke test 諛?asset ?곹깭 ?먭? ?ㅽ겕由쏀듃?낅땲?? Livox, RealSense, ROS2 吏곸젒 ?낅젰? placeholder component源뚯? 以鍮꾨릺???덇퀬 ?ㅼ젣 SDK/bridge ?곌껐? ?꾩냽 ?묒뾽?낅땲??

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

## 紐⑺몴 湲곕뒫

1. ?쒕??덉씠???곗씠?곕? Unreal???ы쁽?섍퀬 媛??移대찓??LiDAR 痢≪젙媛믪쓣 ?먮떒 ?쒕쾭濡??꾩넚?⑸땲??
2. ?ㅼ젣 ?쇱꽌/移대찓???곗씠?곕? 諛쏆븘 Unreal ?섍꼍???ы쁽?⑸땲??
3. 泥좉컯 怨듭젙??Slab 媛곷룄 ??댁쭚??LiDAR point cloud 湲곕컲?쇰줈 遺꾩꽍?⑸땲??
4. ?먮떒 ?쒕쾭???먮낯/怨좊????곗씠?곗? ?먮뵒??UI??媛꾨왂 preview ?곗씠?곕? 遺꾨━?⑸땲??

## ??μ냼 援ъ꽦

```text
Source/ma0t10_dt/MA0T10/Camera   媛??移대찓??component/actor
Source/ma0t10_dt/MA0T10/Sensor   媛??LiDAR, ?쇱꽌 留ㅻ땲?, replay source, transport/recorder
Source/ma0t10_dt/MA0T10/UI       ?쇱꽌 紐⑤땲??widget 諛?host actor
Source/ma0t10_dt/MA0T10/Crane    ?щ젅???덉젣 援ы쁽
Plugins/DTCore                   怨듯넻 Core ?뚮윭洹몄씤 submodule
Content/MA0T10                   map, widget, Blueprint asset
docs                             payload schema, smoke test, adapter plan, widget setup
Samples                          replay sample data
Scripts                          smoke/status helper scripts
```

## DTCore Submodule

泥섏쓬 諛쏆? ?ㅼ뿉??諛섎뱶??submodule??珥덇린?뷀빀?덈떎.

```powershell
git submodule update --init --recursive
git submodule status
```

湲곗? commit:

```text
2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7 Plugins/DTCore
```

?대쾲 ?묒뾽 踰붿쐞?먯꽌??DTCore ?대? ?뚯뒪瑜?吏곸젒 ?섏젙?섏? ?딆뒿?덈떎. ?꾩옱 DTCore 履쎌뿉??`EnhancedInput` dependency 寃쎄퀬媛 蹂댁씪 ???덉?留? DTCore ?섏젙 ?덉슜 ?꾧퉴吏 DT-Project 履쎌뿉?쒕뒗 ?고쉶?섏? ?딆뒿?덈떎.

而ㅻ컠 ??DTCore submodule guard:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_dtcore_submodule_guard.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_dtcore_submodule_guard.ps1" -FailOnViolation
```

??寃?щ뒗 read-only?낅땲?? `Plugins/DTCore`瑜??섏젙?섍굅??stage?섏? ?딄퀬,
湲곕? 而ㅻ컠 `2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7` 諛?parent/submodule
worktree 泥?껐 ?곹깭留??뺤씤?⑸땲??
DTCore is an external submodule pinned to that commit; this repository must not
stage gitlink changes or files under `Plugins/DTCore` during local decision
cleanup.

## 鍮뚮뱶

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" ma0t10_dtEditor Win64 Development "C:\path\to\ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

濡쒖뺄 ?ㅽ뻾 ?꾨줈?앺듃 ?덉떆:

```powershell
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" ma0t10_dtEditor Win64 Development "C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
```

## LiDAR ?곗씠???뺤콉

`UVirtualLidarSensorComp`????踰덉쓽 scan ?먮뒗 replay frame?먯꽌 ?앹꽦???꾩껜 痢≪젙媛믪쓣 `LastPoints`???좎??⑸땲??

?먮떒 ?쒕쾭 payload ?뺤콉:

- `ServerPayloadStride`
- `MaxServerPayloadPoints`
- `bIncludeMissPointsInServerPayload`
- JSON `schemaVersion = virtual-lidar.v1`
- JSON `payloadPolicy`
- JSON `payloadPolicy.pointSelection = hit_only | hit_and_miss`
- JSON `slabAnalysis`

Editor/UI preview ?뺤콉:

- `PreviewPointStride`
- `MaxPreviewPoints`
- `bPointCloudPreviewHitOnly`
- JSON `previewPolicy`

湲곕낯 諛⑺뼢? ?쒕쾭?먮뒗 ?먮낯??媛源뚯슫 hit point瑜?蹂대궡怨? Unreal Editor ?붾㈃?먮뒗 ?쒗븳??point留??쒖떆?섎뒗 寃껋엯?덈떎. 湲곗〈 `PayloadPointStride`, `MaxPayloadPoints`, `PointCloudPreviewStride`, `MaxPointCloudPreviewInstances`??Blueprint ?명솚???꾪빐 ?⑥븘 ?덉?留????뺤콉 ?꾨뱶瑜??곗꽑 ?ъ슜?⑸땲??

`MaxServerPayloadPoints = 0`? ?쒕쾭 payload ?곹븳???녿떎???섎??대ŉ,
?고???performance warning??uncapped ?곹깭媛 ?쒖떆?⑸땲??

## Camera ?곗씠???뺤콉

`UVirtualCameraComp`??SceneCapture 湲곕컲 image payload? monitor ?곹깭 ?쒖떆瑜??쒓났?⑸땲??

- JSON `schemaVersion = virtual-camera.v1`
- resolution, capture mode, JPEG quality, capture interval
- cached payload byte size
- render target preview
- file transport/export ?곕룞

Monitor??`ExportSelectedSensorServerPayload`???꾩옱 view???곕씪 LiDAR ?먮뒗 camera payload瑜?`Saved/SensorCaptures/<SensorId>/ServerPayload` ?꾨옒濡???ν빀?덈떎.

## File Replay Source

`ULidarCsvReplaySourceComp`? `ULidarJsonLinesReplaySourceComp`????λ맂 point cloud瑜?`UVirtualLidarSensorComp::InjectPointCloudFrame`?쇰줈 二쇱엯?⑸땲?? ??寃쎈줈瑜??硫?replay ?곗씠?곕룄 LiDAR JSON payload, recorder, transport, preview, Slab 遺꾩꽍 ?먮쫫???숈씪?섍쾶 ?듦낵?⑸땲??

吏??CSV ?뺤떇:

```text
row,col,x,y,z
x,y,z
```

吏??JSONL ?뺤떇:

```text
{"x":900,"y":-260,"z":0,"distance":936.8,"hit":true,"semanticLabel":"Slab"}
{"worldLocation":[900,-260,0],"localDirection":[1,0,0],"hit":true,"semanticLabel":"Slab"}
```

?섑뵆 ?뚯씪:

```text
Samples/slab_replay_sample.csv
Samples/slab_replay_sample.jsonl
```

Editor Details ?⑤꼸 踰꾪듉:

```text
PushFrameOnceInEditor
PushFrameOnceNoTransportInEditor
StartReplayInEditor
StopReplayInEditor
```

## ?ㅼ젣 ?쇱꽌 Source Placeholder

?꾩옱 以鍮꾨맂 placeholder component:

```text
URos2SensorBridgeSourceComp
ULivoxLidarSourceComp
URealSenseCameraSourceComp
```

?꾩쭅 SDK/bridge ?곌껐? ?섑뻾?섏? ?딄퀬 not-implemented ?곹깭 硫붿떆吏瑜??쒓났?⑸땲?? ?꾩냽 援ы쁽?먯꽌??媛?ν븳 ??`InjectPointCloudFrame` 媛숈? normalized frame 二쇱엯 寃쎈줈瑜?怨듭쑀?댁빞 ?⑸땲??

## Slab 媛곷룄 遺꾩꽍 Workflow

1. Slab actor ?먮뒗 component ?대쫫/tag瑜?`Slab`, `SteelSlab`, `Plate` 以??섎굹? 留ㅼ묶?섍쾶 ?ㅼ젙?⑸땲??
2. `UVirtualLidarSensorComp`?먯꽌 `SlabSemanticLabel = Slab`???좎??⑸땲??
3. ?뺤긽 湲곗? 諛⑺뼢??留욎떠 `ReferenceSlabYawDegrees`瑜??ㅼ젙?⑸땲??
4. LiDAR scan/replay ??`FVirtualLidarSlabAnalysisResult` ?먮뒗 JSON `slabAnalysis`瑜??뺤씤?⑸땲??

遺꾩꽍 寃곌낵?먮뒗 slab hit point count, bounds, center, estimated yaw, reference yaw, angle deviation, confidence, status message媛 ?ы븿?⑸땲??

## Widget 議곗옉

沅뚯옣 Widget parent class:

```text
UVirtualSensorMonitorWidget
```

沅뚯옣 optional binding ?대쫫:

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

Level Blueprint ?놁씠 monitor瑜??먮룞 ?앹꽦?섎젮硫?map??`AVirtualSensorMonitorHostActor`瑜?諛곗튂?섍퀬 `MonitorWidgetClass = WBP_VirtualSensorMonitor`瑜?吏?뺥빀?덈떎.

`MonitorWidgetClass`媛 鍮꾩뼱 ?덇퀬 `bUseNativeMonitorWidgetFallback`??true?대㈃ HostActor媛 native `UVirtualSensorMonitorWidget`瑜??앹꽦?⑸땲?? ??fallback? smoke test? 湲곕낯 議곗옉 ?뺤씤?⑹씠硫? ?댁쁺 UI??Designer?먯꽌 留뚮뱺 `WBP_VirtualSensorMonitor` ?ъ슜??沅뚯옣?⑸땲??

## ?먮룞???뚯뒪??

Payload fixture 寃利?

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_fixtures.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_fixtures.ps1" -Json
```

Payload mock contract 寃利?

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_contract.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_payload_contract.ps1" -Json
```

濡쒖뺄 readiness gate:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
```

?대? 鍮뚮뱶???곹깭?먯꽌 鍮좊Ⅴ寃??뺤씤:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipBuild
```

asset report留??뺤씤?섍퀬 smoke test???앸왂:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -SkipPayloadContract
```

?꾩껜 濡쒖뺄 smoke gate:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1"
```

?대? 鍮뚮뱶??寃쎌슦:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\run_smoke_tests.ps1" -SkipBuild
```

沅뚯옣 smoke ?ㅼ젙:

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

?깅뒫 臾몄젣媛 ?덉쑝硫??ㅼ쓬 ?쒖꽌濡?議곗젙?⑸땲??

```text
1. LiDAR SimulationQuality = Debug
2. PreviewPointStride 利앷?
3. MaxPreviewPoints 媛먯냼
4. Camera CaptureMode = PreviewOnly
5. MultiHit / ExportOnScan / FullSpec 鍮꾪솢?깊솕
```

## 濡쒖뺄 Asset ?곹깭 ?먭?

而ㅻ컠 ??吏꾪뻾瑜??⑥? ?묒뾽/而ㅻ컠 ?덉젙 ?뚯씪 ?붿빟:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_local_decision_precommit_gate_policy.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_precommit_summary.ps1" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_goal_progress_blocker_report.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_goal_progress_blocker_report.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "." -ObservedCodexUsagePercent 84
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
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\prepare_monitor_wbp_editor_review.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_post_edit_hash_report.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "." -EvidencePath "C:\Unreal Projects\ma0t10_dt\Saved\Reports\MonitorWbpAcceptance\monitor_wbp_acceptance.evidence.json"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_wbp_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "." -EvidencePath "C:\Unreal Projects\ma0t10_dt\Saved\Reports\MonitorWbpAcceptance\monitor_wbp_acceptance.evidence.json" -Json
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
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_real_sensor_adapter_deployment_package.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_real_sensor_adapter_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt"
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
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compression_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_laz_compression_gap_summary.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt"
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

濡쒖뺄 ?꾨줈?앺듃 ?곹깭? untracked asset decision point ?뺤씤:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

援ъ“?붾맂 寃곌낵:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -Json
```

Gate ?덉떆:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnCategory LargeContentCandidate,SampleOrThirdParty
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnUnclassifiedUntracked
```

`Windows/`, `Windows.zip`, `launcher.config.json`? `.gitignore`???깅줉?섏뼱 git status?먯꽌???④꺼吏????덉?留? ???ㅽ겕由쏀듃???뚯씪 ?쒖뒪?쒖쓣 吏곸젒 ?뺤씤??議댁옱 ?щ?瑜?怨꾩냽 蹂닿퀬?⑸땲??

## 愿??臾몄꽌

```text
docs/lidar_payload_schema.md
docs/camera_payload_schema.md
docs/widget_designer_setup.md
docs/editor_smoke_test.md
docs/real_sensor_adapter_plan.md
docs/local_asset_report.md
docs/remaining_work.md
```

## ?뚮젮吏??쒗븳

- Livox SDK, RealSense SDK, ROS2 bridge 吏곸젒 ?낅젰? ?꾩쭅 援ы쁽?섏? ?딆븯?듬땲??
- `ExportLastPointCloudLaz()`???ㅼ젣 LAZ ?뺤텞???꾨떃?덈떎. ?꾩옱??`*_laz_source_*.las` ?뺤떇??LAS ?명솚 source ?뚯씪????ν븯怨?warning log瑜??④퉩?덈떎.
- FullSpec, MultiHit, ExportOnScan???숈떆??耳쒕㈃ editor ?깅뒫???ш쾶 ?⑥뼱吏????덉뒿?덈떎.
- ?洹쒕え point cloud rendering? ?꾩쭅 CPU/instance 湲곕컲 preview ?쒓퀎媛 ?덉뼱 GPU/Niagara 湲곕컲 renderer 寃?좉? ?꾩슂?⑸땲??
  LiDAR `PreviewBackend`?먯꽌 Niagara/custom GPU ?꾨낫瑜??좏깮?????덉?留? ?꾩옱 ?꾨낫 媛믪? CPU fallback ?곹깭 ?쒖떆?⑹씠硫?GPU 援ы쁽 ?꾨즺 利앷굅媛 ?꾨떃?덈떎.
  ?꾩옱 CPU fallback/GPU smoke evidence boundary???ㅼ쓬 ?⑦궎吏濡?濡쒖뺄??export?????덉뒿?덈떎:
  `powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -LocalProjectRoot "C:\Unreal Projects\ma0t10_dt"`.
- ?ㅼ젣 map?먯꽌??PIE smoke test? WBP Designer 諛곗튂 寃利앹? 蹂꾨룄 editor ?묒뾽???꾩슂?⑸땲??
- `WBP_VirtualSensorMonitor.uasset`???ㅼ젣 ?덉씠?꾩썐 ?섏젙? Unreal Editor瑜?
  ?듯빐?쒕쭔 吏꾪뻾?⑸땲?? 肄붾뱶/諛붿씤??API??Codex媛 ?섏젙 媛?ν븯吏留?`.uasset`
  諛붿씠?덈━ 吏곸젒 ?⑥튂??湲덉??⑸땲??

?⑥? 援ы쁽/?먯뀑 寃곗젙/?꾨즺 ?먯젙 湲곗?? `docs/remaining_work.md`?먯꽌 異붿쟻?⑸땲??
