param(
    [string]$ProjectRoot = "",
    [switch]$RequireAssets
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$requiredSource = @(
    "Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h",
    "Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h",
    "Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h",
    "Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorSchedulerSubsystem.h",
    "Source/ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h",
    "Source/ma0t10_dt/MA0T10/Camera/VirtualCameraPayloadCodec.h",
    "Source/ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h",
    "Source/ma0t10_dt/MA0T10/UI/VirtualSensorPanelHostComponent.h",
    "Source/ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
)

$failures = [System.Collections.Generic.List[string]]::new()
foreach ($relativePath in $requiredSource) {
    if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot $relativePath) -PathType Leaf)) {
        $failures.Add("Missing V2 source: $relativePath")
    }
}

$legacyPattern = '\b(AVirtualCameraAct|UVirtualCameraComp|AVirtualSensorAct|UVirtualLidarSensorComp|AVirtualSensorManager|UVirtualSensorPerformanceSubsystem|UVirtualSensorDataTransportComp|UVirtualSensorRecorderComp|URealSensorSourceComp|UVirtualSensorDraggableWidgetBase|UVirtualSensorMonitorWidget|UVirtualSensorSettingsWidget|UVirtualSensorCaptureExportWidget|AVirtualSensorMonitorHostActor)\b'
$scanRoots = @("Source", "Scripts", "docs", "README.md", "AGENTS.md")
$legacyHits = & rg -n --pcre2 $legacyPattern @scanRoots `
    --glob '!Scripts/setup_sensor_test_map.py' `
    --glob '!Scripts/validate_sensor_v2_refactor.ps1' `
    --glob '!docs/sensor_v2_migration.ko.md' 2>$null
if ($LASTEXITCODE -eq 0 -and $legacyHits) {
    $failures.Add("Legacy type references remain outside redirects/migration documentation:`n$($legacyHits -join "`n")")
}

$stagedProtected = & git -C $ProjectRoot diff --cached --name-only -- Plugins/DTCore Config/Game.ini
if ($stagedProtected) {
    $failures.Add("Protected paths are staged: $($stagedProtected -join ', ')")
}

if ($RequireAssets) {
    $requiredAssets = @(
        "Content/MA0T10/UI/WBP_VirtualSensorMonitorPanel.uasset",
        "Content/MA0T10/UI/WBP_VirtualSensorSettingsPanel.uasset",
        "Content/MA0T10/UI/WBP_VirtualSensorCaptureExportPanel.uasset",
        "Content/MA0T10/Maps/Tests/SensorRefactorTestMap.umap",
        "Content/MA0T10/Maps/SensorTestMap.umap"
    )
    foreach ($relativePath in $requiredAssets) {
        if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot $relativePath) -PathType Leaf)) {
            $failures.Add("Missing generated V2 asset: $relativePath")
        }
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Host "Sensor V2 refactor static validation passed."
Write-Host "Required source files: $($requiredSource.Count)"
Write-Host "Asset gate enabled: $RequireAssets"
