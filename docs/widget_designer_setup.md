# Virtual Sensor Monitor Widget Setup

Parent class:

```text
UVirtualSensorMonitorWidget
```

Recommended Widget Blueprint name:

```text
WBP_VirtualSensorMonitor
```

## Optional Bindings

All names below are optional from the C++ side, but matching them enables automatic button binding.

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

Make sure each named widget has `Is Variable` enabled.

## Button Behavior

```text
CaptureOnceButton
```

Calls `CaptureSelectedSensorsOnce()`. When a SensorManager is bound, this captures the selected camera and selected LiDAR once.

```text
ExportServerPayloadButton
```

Calls `ExportSelectedSensorServerPayload()`. This writes the current monitor view's server JSON payload to `Saved/SensorCaptures/<SensorId>/ServerPayload` so the judging-server contract can be inspected without changing the active transport mode. LiDAR view exports the selected LiDAR payload; camera view exports the bound camera payload.

```text
PreviewMoreButton
```

Increases the selected LiDAR preview budget. Server payload policy is unchanged.

```text
PreviewLessButton
```

Decreases the selected LiDAR preview budget. Server payload policy is unchanged.

```text
PreviewHitOnlyButton
```

Toggles the selected LiDAR preview between hit-only points and all measured points. Server payload policy is unchanged, including whether miss points are sent to the judging server.

## Level Blueprint Binding

Recommended BeginPlay flow:

```text
Create Widget: WBP_VirtualSensorMonitor
Add to Viewport
Get Actor of Class: AVirtualSensorManager
BindSensorManager
```

Prefer `BindSensorManager` over binding camera/LiDAR components manually. The manager keeps selected sensors, point-cloud-only mode, and shared capture behavior aligned.

## Host Actor Binding

To avoid Level Blueprint setup, place this actor in the map:

```text
AVirtualSensorMonitorHostActor
```

Recommended settings:

```text
MonitorWidgetClass = WBP_VirtualSensorMonitor
bAutoCreateOnBeginPlay = true
bAutoFindSensorManager = true
bAddToViewport = true
bUseNativeMonitorWidgetFallback = true
```

Optional settings:

```text
bShowLidarViewOnStart = true
bEnablePointCloudOnlyOnStart = true
ViewportZOrder = 0
```

This actor creates the monitor widget, finds `AVirtualSensorManager`, binds the widget, and adds it to the viewport at BeginPlay.

If `MonitorWidgetClass` is empty and `bUseNativeMonitorWidgetFallback` is true, the host creates the native `UVirtualSensorMonitorWidget`. The native fallback now builds a minimal Slate panel with status text and basic controls for view toggle, sensor selection, point-cloud-only mode, LiDAR view mode, capture once, server payload export, preview hit-only toggle, and preview budget changes.

The native fallback is intended for smoke tests and emergency runtime visibility. A Designer-authored `WBP_VirtualSensorMonitor` is still recommended for the production operator UI because it can include the camera/LiDAR image area, layout polish, and domain-specific slab analysis panels.

## Blueprint-Callable Manager API

The following functions are available on `AVirtualSensorManager`:

```text
CaptureSelectedOnce
CaptureAllOnce
SetSelectedLidarPreviewPolicy
AdjustSelectedLidarPreviewBudget
TogglePointCloudOnlyView
SetPointCloudOnlyMode
```

Use these when a Designer button is easier to wire manually than through automatic C++ binding.

## Status Text Contract

`UVirtualSensorMonitorWidget` exposes these Blueprint-pure helpers so Designer widgets and automation tests can inspect the same text that the native fallback renders:

```text
GetMonitorTitleText
GetMonitorStatusText
GetMonitorDisplayData
GetSelectedSensorIdText
GetFrameSummaryText
GetMeasurementSummaryText
GetServerPayloadSummaryText
GetPreviewPolicySummaryText
GetSlabAnalysisSummaryText
GetLazExportSummaryText
IsShowingLidar
HasBoundCamera
HasBoundLidar
GetLastManualExportMessage
GetTransportStatusSummaryText
GetTransportWarningText
GetViewModeSummaryText
GetAcceptanceGateSummaryText
GetRealSensorDeploymentSummaryText
GetLastRealSensorControlMessage
```

For a production Designer WBP, prefer binding separate TextBlocks to the
smaller summary getters above instead of parsing `GetMonitorStatusText()`.
When a Blueprint graph wants one read node, use `GetMonitorDisplayData()` and
break the returned `FVirtualSensorMonitorDisplayData` struct into title,
selected sensor, frame, measurement, payload, preview, slab, LAZ export,
transport, warning, view mode, acceptance gate, and real-sensor deployment rows.
`GetMonitorStatusText()` remains the native fallback/debug text contract.
When a monitor is bound through `AVirtualSensorManager`, the manager also binds
the selected `URealSensorSourceComp` automatically. It prefers the real source
whose `TargetLidar` matches the selected LiDAR, then falls back to the selected
real-source index.
Optional real-source buttons can call `StartRealSensorSources`,
`StopRealSensorSources`, and `PushSelectedRealSensorSourceOnce`. Missing buttons
do not affect existing WBP assets because every new binding is optional.
The native fallback toolbar exposes the same three controls for pre-WBP smoke
testing.

The LiDAR status text is expected to include:

```text
Sensor id
Frame id
Scan interval and ray count
Measured point and hit counts
Server payload point count, JSON byte length, and policy
Preview point count and policy
Slab angle/deviation/confidence
LAZ placeholder/compressor/true-validation state
Transport/performance warning
LiDAR view mode
Acceptance gate status
Real sensor deployment readiness
CSV export row/return contract
```

`MA0T10.SensorMonitor.LidarStatusTextContract` verifies this contract against the replay sample data.
The same automation verifies that the smaller Designer-facing getters expose
sensor id, frame/scan interval, measured ray/hit counts, server payload policy,
preview policy, slab analysis, LAZ export state, warning, and view mode values.
`MA0T10.SensorMonitor.PerformanceWarningStatusText` verifies that LiDAR performance warnings are surfaced in the same monitor status text.

Static monitor-policy readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_widget_policy.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_template.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_wbp_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\prepare_monitor_wbp_editor_review.ps1" -ProjectRoot "C:\Unreal Projects\ma0t10_dt" -SourceRepoRoot "."
```

This check keeps optional C++ bindings, native fallback behavior, local binary
WBP decision guards, setup documentation, and automation test names aligned
before `WBP_VirtualSensorMonitor.uasset` is committed.
Codex should not directly patch the binary WBP asset. Add reusable monitor
behavior through native C++ or Blueprint-callable bindings first, then use
Unreal Editor for Designer/layout edits with backup, hash, compile/save, PIE,
and owner-acceptance evidence.
The focused WBP decision report shows the current `ReviewQueue`,
`CommitReadiness`, `EvidenceStatus`, `MissingEvidenceCount`, and manual
acceptance checklist. Use `-EvidencePath` and `-FailOnIncompleteEvidence` only
after a project owner has filled the accepted WBP evidence record.
The WBP acceptance template is read-only and does not stage or modify the
`.uasset`; it structures the editor-open, optional-binding, PIE-smoke, and
production-owner evidence fields that must be recorded before the binary WBP can
move out of `MustRemainUntracked`.
The optional-binding evidence list is generated from
`VirtualSensorMonitorWidget.h` `BindWidgetOptional` fields, so the checklist
uses the same C++ names as automatic binding. For each present widget, record
`IsVariable`, `WidgetClass`, and `BoundToExpectedCppName`; for each missing
optional widget, record `MissingOptionalDoesNotCrash`.
The acceptance template also includes a `DisplayData visual match` evidence
section. During PIE, record each `GetMonitorDisplayData()` row
(`TitleText`, `SelectedSensorText`, `FrameText`, `MeasurementText`,
`ServerPayloadText`, `PreviewText`, `SlabText`, `WarningText`, and
`ViewModeText`) and match it to the visible WBP TextBlock. Record
`bShowingLidar`/`SensorMode` so camera and LiDAR evidence are not mixed.
`WarningText` should record the visible no-warning row when no warning is
produced, rather than being left blank. The same manual acceptance also includes
`DisplayDataScreenMatchEvidence`, backed by a screenshot or editor/PIE log.
The acceptance evidence validator is also read-only. It checks the filled
evidence file against the current SHA256 asset hash, optional binding safety,
Editor/PIE evidence paths, and owner acceptance, and it only throws for
incomplete evidence when `-FailOnIncompleteEvidence` is passed.
Before opening the binary WBP in Unreal Editor, run
`prepare_monitor_wbp_editor_review.ps1`. It copies the current
`WBP_VirtualSensorMonitor.uasset` to `Saved/Backups/MonitorWbp`, records the
pre-edit SHA256 hash, generates the acceptance package under `Saved/Reports`,
and writes an editor-review checklist without modifying or staging the asset.
The checklist calls out the required DisplayData rows, including
`LazExportText`, state helper checks for `IsShowingLidar`, `HasBoundCamera`,
and `HasBoundLidar`, manual export message evidence from
`GetLastManualExportMessage`, evidence-file completion, and post-edit strict
validation.
After saving the WBP through Unreal Editor, run
`export_monitor_wbp_post_edit_hash_report.ps1`. It writes a
`Saved/Reports/MonitorWbpPostEdit` report with the current SHA256 hash,
pre-edit hash comparison, backup status, and the exact evidence fields to copy
into `monitor_wbp_acceptance.evidence.json`. This report is evidence only; it
does not accept or stage the binary asset.

Local camera capture notes:

- GPU readback failures remove the failed readback without calling `Unlock()`
  on a null lock result.
- GPU readback row pitch is checked before copying rows into the JPEG buffer.
- Camera write pending state is derived from both queued GPU readbacks and
  active async JPEG writes, so a failed readback does not leave timed capture
  permanently blocked.
