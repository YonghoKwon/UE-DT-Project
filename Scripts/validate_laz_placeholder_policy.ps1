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
    [PSCustomObject]@{ Label = "Monitor widget header"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h" },
    [PSCustomObject]@{ Label = "Monitor widget implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp" },
    [PSCustomObject]@{ Label = "Replay automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "LiDAR payload schema"; Path = "docs\lidar_payload_schema.md" },
    [PSCustomObject]@{ Label = "Remaining work document"; Path = "docs\remaining_work.md" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$lidarHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h"
$lidarCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp"
$monitorHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h"
$monitorCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp"
$replayTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp"
$schemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$decisionReportScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_decision_report.ps1"

Assert-FileExists -Path $decisionReportScript -Label "LAZ compression decision report script"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "ExportLastPointCloudLaz"; Label = "LAZ export API remains explicit" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "LAZ compression is not integrated"; Label = "Runtime warning states LAZ is not integrated" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "_laz_source"; Label = "Placeholder uses laz_source prefix" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "ExportLastPointCloudLas(Prefix)"; Label = "Placeholder writes LAS-compatible source" },
    [PSCustomObject]@{ Path = $monitorHeader; Pattern = "LAS-compatible source file"; Label = "Monitor setting comments clarify LAZ placeholder" },
    [PSCustomObject]@{ Path = $monitorCpp; Pattern = "ExportLastPointCloudLaz"; Label = "Monitor routes LAZ option through placeholder API" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "M7AT10.SensorReplay.LazPlaceholderWritesLasSource"; Label = "LAZ placeholder automation test name" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "does not create compressed .laz files"; Label = "Automation asserts no compressed LAZ output" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "_laz_source_"; Label = "Automation checks laz_source LAS output" },
    [PSCustomObject]@{ Path = $schemaDoc; Pattern = 'does not pretend to create a compressed `.laz` file'; Label = "Schema doc clarifies placeholder" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "True LAZ Compression"; Label = "Remaining work tracks true LAZ" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "intentionally a placeholder"; Label = "Remaining work states placeholder status" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_laz_compression_decision_report.ps1"; Label = "Remaining work documents LAZ decision report" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "Native LAZ library"; Label = "Decision report includes native library path" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "External CLI compressor"; Label = "Decision report includes external CLI path" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "Server/post-processing workflow"; Label = "Decision report includes server post-process path" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "Readable validation from a known point-cloud tool"; Label = "Decision report tracks readable LAZ evidence" }
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
        PlaceholderIsExplicit = $true
        WritesLasSourceOnly = $true
        AutomationCoverageDeclared = $true
        DecisionReportDeclared = $true
        TrueCompressionStillOpen = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "LAZ placeholder policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Placeholder is explicit: $($report.Summary.PlaceholderIsExplicit)"
    Write-Host "Writes LAS source only: $($report.Summary.WritesLasSourceOnly)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
    Write-Host "Decision report declared: $($report.Summary.DecisionReportDeclared)"
    Write-Host "True compression still open: $($report.Summary.TrueCompressionStillOpen)"
}
