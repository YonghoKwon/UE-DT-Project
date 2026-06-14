param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$ProjectPath = "",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [string[]]$TestGroups = @(
        "M7AT10.EditorSmoke",
        "M7AT10.SensorReplay",
        "M7AT10.SensorRecorder",
        "M7AT10.RealSensorSource",
        "M7AT10.SensorManager",
        "M7AT10.SensorMonitor"
    ),
    [switch]$SkipBuild,
    [switch]$SkipSmoke,
    [switch]$AllowOpenEditor,
    [switch]$FailOnGeneratedOutput,
    [string[]]$FailOnCategory = @()
)

$ErrorActionPreference = "Stop"

function Invoke-ScriptStep {
    param(
        [string]$Label,
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host $ScriptPath

    & $ScriptPath @Parameters
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $ProjectRoot "m7at10_dt.uproject"
}
if (-not (Test-Path -LiteralPath $ProjectPath)) {
    throw "Project file not found: $ProjectPath"
}

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$AssetReportScript = Join-Path $ScriptRoot "report_local_project_status.ps1"
$SmokeScript = Join-Path $ScriptRoot "run_smoke_tests.ps1"

if (-not (Test-Path -LiteralPath $AssetReportScript)) {
    throw "report_local_project_status.ps1 not found: $AssetReportScript"
}
if (-not (Test-Path -LiteralPath $SmokeScript)) {
    throw "run_smoke_tests.ps1 not found: $SmokeScript"
}

$AssetReportParams = @{
    ProjectRoot = $ProjectRoot
    FailOnUnclassifiedUntracked = $true
}
if ($FailOnGeneratedOutput) {
    $AssetReportParams.FailOnGeneratedOutput = $true
}
if ($FailOnCategory.Count -gt 0) {
    $AssetReportParams.FailOnCategory = $FailOnCategory
}

Invoke-ScriptStep `
    -Label "Local asset report" `
    -ScriptPath $AssetReportScript `
    -Parameters $AssetReportParams

if (-not $SkipSmoke) {
    $SmokeParams = @{
        ProjectPath = $ProjectPath
        EngineRoot = $EngineRoot
        TestGroups = $TestGroups
    }
    if ($SkipBuild) {
        $SmokeParams.SkipBuild = $true
    }
    if ($AllowOpenEditor) {
        $SmokeParams.AllowOpenEditor = $true
    }

    Invoke-ScriptStep `
        -Label "Smoke tests" `
        -ScriptPath $SmokeScript `
        -Parameters $SmokeParams
}
else {
    Write-Host ""
    Write-Host "Smoke tests skipped by -SkipSmoke."
}

Write-Host ""
Write-Host "Project readiness checks passed."
