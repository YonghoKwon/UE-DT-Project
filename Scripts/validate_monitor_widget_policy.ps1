param(
    [string]$ProjectRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Assert-FileExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found: $Path"
    }
}

function Assert-ContainsText {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet)) {
        throw "$Label missing required text '$Pattern' in $Path"
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$requiredFiles = @(
    [PSCustomObject]@{ Label = "Monitor widget header"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h" },
    [PSCustomObject]@{ Label = "Monitor widget implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp" },
    [PSCustomObject]@{ Label = "Monitor host header"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.h" },
    [PSCustomObject]@{ Label = "Monitor host implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.cpp" },
    [PSCustomObject]@{ Label = "Monitor automation tests"; Path = "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "Widget setup document"; Path = "docs\widget_designer_setup.md" },
    [PSCustomObject]@{ Label = "Local asset report document"; Path = "docs\local_asset_report.md" },
    [PSCustomObject]@{ Label = "Remaining work document"; Path = "docs\remaining_work.md" },
    [PSCustomObject]@{ Label = "Local project status report"; Path = "Scripts\report_local_project_status.ps1" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$widgetHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h"
$widgetCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp"
$hostHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.h"
$hostCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.cpp"
$testsCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp"
$setupDoc = Join-Path $ProjectRoot "docs\widget_designer_setup.md"
$localAssetDoc = Join-Path $ProjectRoot "docs\local_asset_report.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$assetReportScript = Join-Path $ProjectRoot "Scripts\report_local_project_status.ps1"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "BindWidgetOptional"; Label = "Widget uses optional bindings" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetMonitorTitleText"; Label = "Title text inspection API" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetMonitorStatusText"; Label = "Status text inspection API" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "ShouldUseNativeFallbackWidget"; Label = "Native fallback detection" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "RebuildWidget"; Label = "Native fallback Slate builder" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "ExportSelectedSensorServerPayload"; Label = "Server payload export control" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "PreviewHitOnlyButton"; Label = "Preview hit-only control binding" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "RefreshLocalCaptureCameraPendingState"; Label = "Camera local capture pending-state helper" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "LocalCaptureCameraAsyncWriteCount"; Label = "Camera async write count is tracked" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "if (LockedData)"; Label = "GPU readback unlock is guarded" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "RowPitchInPixels < Pending.Width"; Label = "GPU readback row pitch is validated" },
    [PSCustomObject]@{ Path = $hostHeader; Pattern = "bUseNativeMonitorWidgetFallback"; Label = "Host fallback setting" },
    [PSCustomObject]@{ Path = $hostCpp; Pattern = "GetEffectiveMonitorWidgetClass"; Label = "Host resolves fallback widget class" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.SensorMonitor.HostNativeFallback"; Label = "Host fallback automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.SensorMonitor.LidarStatusTextContract"; Label = "LiDAR status automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.SensorMonitor.ServerPayloadExport"; Label = "Server payload export automation test" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "WBP_VirtualSensorMonitor"; Label = "Setup doc names production WBP" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "Optional Bindings"; Label = "Setup doc lists optional bindings" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "native fallback"; Label = "Setup doc explains native fallback" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "binary Designer widget"; Label = "Local asset doc guards WBP binary" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Production Monitor WBP"; Label = "Remaining work tracks WBP decision" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Commit the WBP only after manual editor verification."; Label = "Remaining work requires manual WBP verification" },
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "WBP_VirtualSensorMonitor.uasset"; Label = "Asset report tracks local WBP path" },
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "Detected binary monitor WBP asset"; Label = "Asset report emits WBP decision note" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    CheckedFiles = @($requiredFiles | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Path = Join-Path $ProjectRoot $_.Path
        }
    })
    CheckedContracts = @($requiredTexts | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Pattern = $_.Pattern
            Path = $_.Path
        }
    })
    Summary = [PSCustomObject]@{
        RequiredFileCount = $requiredFiles.Count
        RequiredContractCount = $requiredTexts.Count
        OptionalBindingsDeclared = $true
        NativeFallbackDeclared = $true
        CameraCapturePendingStateGuarded = $true
        LocalWbpDecisionGuardDeclared = $true
        AutomationCoverageDeclared = $true
        ManualEditorVerificationStillRequired = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Monitor widget policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Optional bindings declared: $($report.Summary.OptionalBindingsDeclared)"
    Write-Host "Native fallback declared: $($report.Summary.NativeFallbackDeclared)"
    Write-Host "Camera capture pending state guarded: $($report.Summary.CameraCapturePendingStateGuarded)"
    Write-Host "Local WBP decision guard declared: $($report.Summary.LocalWbpDecisionGuardDeclared)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
    Write-Host "Manual editor verification still required: $($report.Summary.ManualEditorVerificationStillRequired)"
}
