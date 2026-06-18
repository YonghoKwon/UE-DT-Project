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
```

The LiDAR status text is expected to include:

```text
Sensor id
Frame id
Scan interval and ray count
Measured point and hit counts
Server payload point count, JSON byte length, and policy
Preview point count and policy
Slab angle/deviation/confidence
Transport/performance warning
LiDAR view mode
CSV export row/return contract
```

`M7AT10.SensorMonitor.LidarStatusTextContract` verifies this contract against the replay sample data.
`M7AT10.SensorMonitor.PerformanceWarningStatusText` verifies that LiDAR performance warnings are surfaced in the same monitor status text.

Static monitor-policy readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_widget_policy.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
```

This check keeps optional C++ bindings, native fallback behavior, local binary
WBP decision guards, setup documentation, and automation test names aligned
before `WBP_VirtualSensorMonitor.uasset` is committed.
The focused WBP decision report shows the current `ReviewQueue`,
`CommitReadiness`, `EvidenceStatus`, `MissingEvidenceCount`, and manual
acceptance checklist. Use `-EvidencePath` and `-FailOnIncompleteEvidence` only
after a project owner has filled the accepted WBP evidence record.

Local camera capture notes:

- GPU readback failures remove the failed readback without calling `Unlock()`
  on a null lock result.
- GPU readback row pitch is checked before copying rows into the JPEG buffer.
- Camera write pending state is derived from both queued GPU readbacks and
  active async JPEG writes, so a failed readback does not leave timed capture
  permanently blocked.
