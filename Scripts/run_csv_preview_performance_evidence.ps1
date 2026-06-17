param(
    [string]$ProjectRoot = "",
    [string]$LocalProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [switch]$SkipAutomation,
    [switch]$SkipBuild,
    [switch]$AllowOpenEditor,
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

function Invoke-CheckedScript {
    param(
        [string]$Label,
        [string]$ScriptPath,
        [string[]]$Arguments
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host "powershell -ExecutionPolicy Bypass -File `"$ScriptPath`" $($Arguments -join ' ')"

    & powershell -ExecutionPolicy Bypass -File $ScriptPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$LocalProjectRoot = (Resolve-Path -LiteralPath $LocalProjectRoot).Path

$runSmokeScript = Join-Path $ProjectRoot "Scripts\run_smoke_tests.ps1"
$csvReportScript = Join-Path $ProjectRoot "Scripts\export_csv_preview_performance_report.ps1"
$rendererReportScript = Join-Path $ProjectRoot "Scripts\export_point_cloud_renderer_decision_report.ps1"
$projectPath = Join-Path $LocalProjectRoot "m7at10_dt.uproject"
$reportDir = Join-Path $LocalProjectRoot "Saved\Reports"
$csvMarkdownPath = Join-Path $reportDir "csv_preview_performance.md"
$csvJsonPath = Join-Path $reportDir "csv_preview_performance.json"
$rendererMarkdownPath = Join-Path $reportDir "point_cloud_renderer_decision.md"
$rendererJsonPath = Join-Path $reportDir "point_cloud_renderer_decision.json"

foreach ($file in @(
    [PSCustomObject]@{ Path = $runSmokeScript; Label = "Smoke test runner" },
    [PSCustomObject]@{ Path = $csvReportScript; Label = "CSV preview performance report script" },
    [PSCustomObject]@{ Path = $rendererReportScript; Label = "Point cloud renderer decision report script" },
    [PSCustomObject]@{ Path = $projectPath; Label = "Unreal project" }
)) {
    Assert-FileExists -Path $file.Path -Label $file.Label
}

if (-not (Test-Path -LiteralPath $reportDir -PathType Container)) {
    New-Item -ItemType Directory -Path $reportDir | Out-Null
}

if (-not $SkipAutomation) {
    $smokeArgs = @(
        "-ProjectPath", $projectPath,
        "-EngineRoot", $EngineRoot,
        "-TestGroups", "M7AT10.Sensor.CsvPointCloudPreview"
    )
    if ($SkipBuild) {
        $smokeArgs += "-SkipBuild"
    }
    if ($AllowOpenEditor) {
        $smokeArgs += "-AllowOpenEditor"
    }

    Invoke-CheckedScript -Label "CSV preview automation" -ScriptPath $runSmokeScript -Arguments $smokeArgs
}

Invoke-CheckedScript `
    -Label "CSV preview performance report" `
    -ScriptPath $csvReportScript `
    -Arguments @(
        "-ProjectRoot", $ProjectRoot,
        "-LocalProjectRoot", $LocalProjectRoot,
        "-RequireAutomationSuccess",
        "-MarkdownPath", $csvMarkdownPath,
        "-JsonPath", $csvJsonPath
    )

Invoke-CheckedScript `
    -Label "Point cloud renderer decision report with required CSV evidence" `
    -ScriptPath $rendererReportScript `
    -Arguments @(
        "-ProjectRoot", $ProjectRoot,
        "-LocalProjectRoot", $LocalProjectRoot,
        "-RequireCsvPerformanceEvidence",
        "-MarkdownPath", $rendererMarkdownPath,
        "-JsonPath", $rendererJsonPath
    )

$csvReport = Get-Content -LiteralPath $csvJsonPath -Raw | ConvertFrom-Json
$rendererReport = Get-Content -LiteralPath $rendererJsonPath -Raw | ConvertFrom-Json

$summary = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    LocalProjectRoot = $LocalProjectRoot
    AutomationRun = (-not $SkipAutomation)
    CsvReportMarkdownPath = $csvMarkdownPath
    CsvReportJsonPath = $csvJsonPath
    RendererReportMarkdownPath = $rendererMarkdownPath
    RendererReportJsonPath = $rendererJsonPath
    CsvPreview = $csvReport.Summary
    RendererDecision = $rendererReport.Summary
    Valid = ([bool]$csvReport.Summary.Valid -and [bool]$rendererReport.Summary.Valid -and [bool]$rendererReport.Summary.CpuFallbackPerformanceEvidencePresent)
}

if (-not $summary.Valid) {
    throw "CSV preview performance evidence workflow did not produce valid CPU fallback evidence."
}

if ($Json) {
    $summary | ConvertTo-Json -Depth 6
}
else {
    Write-Host ""
    Write-Host "CSV preview performance evidence workflow passed."
    Write-Host "Max accepted points: $($summary.CsvPreview.MaxAcceptedPoints)"
    Write-Host "Max total load ms: $($summary.CsvPreview.MaxTotalLoadMs)"
    Write-Host "CPU fallback performance evidence present: $($summary.RendererDecision.CpuFallbackPerformanceEvidencePresent)"
    Write-Host "CSV report: $csvMarkdownPath"
    Write-Host "Renderer report: $rendererMarkdownPath"
}
