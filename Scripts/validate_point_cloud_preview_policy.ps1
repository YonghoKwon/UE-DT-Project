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
    [PSCustomObject]@{ Label = "LiDAR component header"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h" },
    [PSCustomObject]@{ Label = "LiDAR component implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp" },
    [PSCustomObject]@{ Label = "CSV point cloud preview actor header"; Path = "Source\m7at10_dt\M7AT10\Sensor\CsvPointCloudPreviewActor.h" },
    [PSCustomObject]@{ Label = "Sensor manager implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualSensorManager.cpp" },
    [PSCustomObject]@{ Label = "Monitor widget implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp" },
    [PSCustomObject]@{ Label = "Replay automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "Sensor manager automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\SensorManagerAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "CSV point cloud preview automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\CsvPointCloudPreviewAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "Monitor automation tests"; Path = "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "LiDAR payload schema"; Path = "docs\lidar_payload_schema.md" },
    [PSCustomObject]@{ Label = "Editor smoke test document"; Path = "docs\editor_smoke_test.md" },
    [PSCustomObject]@{ Label = "Remaining work document"; Path = "docs\remaining_work.md" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$lidarHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h"
$lidarCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp"
$csvPreviewHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\CsvPointCloudPreviewActor.h"
$managerCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualSensorManager.cpp"
$monitorCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp"
$replayTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp"
$managerTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\SensorManagerAutomationTests.cpp"
$csvPreviewTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\CsvPointCloudPreviewAutomationTests.cpp"
$monitorTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp"
$schemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$rendererDecisionReportScript = Join-Path $ProjectRoot "Scripts\export_point_cloud_renderer_decision_report.ps1"

Assert-FileExists -Path $rendererDecisionReportScript -Label "Point cloud renderer decision report script"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "ServerPayloadStride"; Label = "Server payload stride policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "MaxServerPayloadPoints"; Label = "Server payload max-point policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "PreviewPointStride"; Label = "Preview stride policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "MaxPreviewPoints"; Label = "Preview max-point policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "GetPerformanceWarning"; Label = "Performance warning API" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "Preview is uncapped"; Label = "Uncapped preview warning" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "FullSpec+MultiHit"; Label = "FullSpec multihit warning" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "FullSpec export-on-scan"; Label = "Export-on-scan warning" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "AddInstances(InstanceTransforms, false, true)"; Label = "Live preview uses batched ISM instance upload" },
    [PSCustomObject]@{ Path = $csvPreviewHeader; Pattern = "ClampMax = `"100000`""; Label = "Procedural CSV batch metadata allows high-density sections" },
    [PSCustomObject]@{ Path = $managerCpp; Pattern = "FMath::Min(LidarComp->MaxPreviewPoints, 3000)"; Label = "PointCloudOnly preview max clamp" },
    [PSCustomObject]@{ Path = $managerCpp; Pattern = "FMath::Max(2, LidarComp->PreviewPointStride)"; Label = "PointCloudOnly preview stride clamp" },
    [PSCustomObject]@{ Path = $monitorCpp; Pattern = "PreviewPoints"; Label = "Monitor exposes preview point count" },
    [PSCustomObject]@{ Path = $monitorCpp; Pattern = "Transport/Warning"; Label = "Monitor exposes warning row" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "M7AT10.SensorReplay.PerformanceWarningStatus"; Label = "Replay warning automation test" },
    [PSCustomObject]@{ Path = $managerTests; Pattern = "M7AT10.SensorManager.PointCloudOnlyPreservesPayloadPolicy"; Label = "PointCloudOnly policy automation test" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "M7AT10.Sensor.CsvPointCloudPreview.ProceduralHighDensityLoad"; Label = "CSV procedural high-density automation test" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "M7AT10.Sensor.CsvPointCloudPreview.InstancedBatchLoad"; Label = "CSV instanced batch automation test" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "120000"; Label = "CSV procedural automation uses high-density sample" },
    [PSCustomObject]@{ Path = $monitorTests; Pattern = "M7AT10.SensorMonitor.PerformanceWarningStatusText"; Label = "Monitor warning automation test" },
    [PSCustomObject]@{ Path = $schemaDoc; Pattern = 'Server-side judgment should not use `previewPolicy` as measurement truth.'; Label = "Schema documents preview/server split" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "PreviewPointStride = 2"; Label = "Smoke doc preview stride recommendation" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "MaxPreviewPoints = 5000"; Label = "Smoke doc preview max recommendation" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Large Point Cloud Renderer"; Label = "Remaining work tracks high-density renderer" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_point_cloud_renderer_decision_report.ps1"; Label = "Remaining work documents renderer decision report" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "Niagara point renderer"; Label = "Renderer report includes Niagara path" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "Custom GPU buffer renderer"; Label = "Renderer report includes custom GPU path" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "External viewer workflow"; Label = "Renderer report includes external viewer path" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "Keep CPU ISM fallback"; Label = "Renderer report keeps CPU fallback" }
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
        ServerPreviewSplitDocumented = $true
        RuntimeWarningsDeclared = $true
        PointCloudOnlyClampDeclared = $true
        BatchedInstanceUploadDeclared = $true
        RendererDecisionReportDeclared = $true
        ProceduralCsvPreviewCoverageDeclared = $true
        AutomationCoverageDeclared = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Point cloud preview policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Server/preview split documented: $($report.Summary.ServerPreviewSplitDocumented)"
    Write-Host "Runtime warnings declared: $($report.Summary.RuntimeWarningsDeclared)"
    Write-Host "PointCloudOnly clamps declared: $($report.Summary.PointCloudOnlyClampDeclared)"
    Write-Host "Batched instance upload declared: $($report.Summary.BatchedInstanceUploadDeclared)"
    Write-Host "Renderer decision report declared: $($report.Summary.RendererDecisionReportDeclared)"
    Write-Host "Procedural CSV preview coverage declared: $($report.Summary.ProceduralCsvPreviewCoverageDeclared)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
}
