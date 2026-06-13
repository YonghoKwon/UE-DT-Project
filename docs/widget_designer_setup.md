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
PreviewMoreButton
PreviewLessButton
```

Make sure each named widget has `Is Variable` enabled.

## Button Behavior

```text
CaptureOnceButton
```

Calls `CaptureSelectedSensorsOnce()`. When a SensorManager is bound, this captures the selected camera and selected LiDAR once.

```text
PreviewMoreButton
```

Increases the selected LiDAR preview budget. Server payload policy is unchanged.

```text
PreviewLessButton
```

Decreases the selected LiDAR preview budget. Server payload policy is unchanged.

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

If `MonitorWidgetClass` is empty and `bUseNativeMonitorWidgetFallback` is true, the host creates the native `UVirtualSensorMonitorWidget`. The native fallback now builds a minimal Slate panel with status text and basic controls for view toggle, sensor selection, point-cloud-only mode, LiDAR view mode, capture once, and preview budget changes.

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
