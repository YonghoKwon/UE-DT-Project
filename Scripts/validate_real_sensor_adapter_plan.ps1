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

    $match = Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet
    if (-not $match) {
        throw "$Label missing required text '$Pattern' in $Path"
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$requiredFiles = @(
    [PSCustomObject]@{ Label = "Base source component header"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.h" },
    [PSCustomObject]@{ Label = "Base source component implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.cpp" },
    [PSCustomObject]@{ Label = "Adapter placeholder header"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.h" },
    [PSCustomObject]@{ Label = "Adapter placeholder implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.cpp" },
    [PSCustomObject]@{ Label = "CSV replay header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarCsvReplaySourceComp.h" },
    [PSCustomObject]@{ Label = "CSV replay implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarCsvReplaySourceComp.cpp" },
    [PSCustomObject]@{ Label = "JSONL replay header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLinesReplaySourceComp.h" },
    [PSCustomObject]@{ Label = "JSONL replay implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLinesReplaySourceComp.cpp" },
    [PSCustomObject]@{ Label = "Real sensor automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\RealSensorSourceAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "CSV replay sample"; Path = "Samples\slab_replay_sample.csv" },
    [PSCustomObject]@{ Label = "JSONL replay sample"; Path = "Samples\slab_replay_sample.jsonl" },
    [PSCustomObject]@{ Label = "Adapter plan document"; Path = "docs\real_sensor_adapter_plan.md" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$baseHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.h"
$baseCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.cpp"
$stubsHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.h"
$stubsCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.cpp"
$testsCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\RealSensorSourceAutomationTests.cpp"
$planDoc = Join-Path $ProjectRoot "docs\real_sensor_adapter_plan.md"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $baseHeader; Pattern = "ERealSensorSourceKind"; Label = "Base source kind enum" },
    [PSCustomObject]@{ Path = $baseHeader; Pattern = "PushPointFrameToTarget"; Label = "Normalized LiDAR handoff declaration" },
    [PSCustomObject]@{ Path = $baseCpp; Pattern = "InjectPointCloudFrame"; Label = "Target LiDAR injection" },
    [PSCustomObject]@{ Path = $baseCpp; Pattern = "MarkFramePushed"; Label = "Frame push status accounting" },
    [PSCustomObject]@{ Path = $stubsHeader; Pattern = "URos2SensorBridgeSourceComp"; Label = "ROS2 placeholder class" },
    [PSCustomObject]@{ Path = $stubsHeader; Pattern = "ULivoxLidarSourceComp"; Label = "Livox placeholder class" },
    [PSCustomObject]@{ Path = $stubsHeader; Pattern = "URealSenseCameraSourceComp"; Label = "RealSense placeholder class" },
    [PSCustomObject]@{ Path = $stubsCpp; Pattern = "integration is not implemented yet"; Label = "Placeholder status text" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.BaseState"; Label = "Base state automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.PlaceholderState"; Label = "Placeholder automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.PushFrameToTarget"; Label = "Handoff automation test" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "PushPointFrameToTarget"; Label = "Plan documents normalized handoff" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "Adapter Priority"; Label = "Plan documents adapter priority" }
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
        ReplayAdaptersPresent = $true
        PlaceholderAdaptersPresent = $true
        NormalizedHandoffDocumented = $true
        AutomationCoverageDeclared = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Real sensor adapter plan is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Replay adapters present: $($report.Summary.ReplayAdaptersPresent)"
    Write-Host "Placeholder adapters present: $($report.Summary.PlaceholderAdaptersPresent)"
    Write-Host "Normalized handoff documented: $($report.Summary.NormalizedHandoffDocumented)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
}
