param(
    [string]$ProjectPath = "C:\Unreal Projects\m7at10_dt\m7at10_dt.uproject",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [string[]]$TestGroups = @(
        "M7AT10.EditorSmoke",
        "M7AT10.SensorReplay",
        "M7AT10.SensorRecorder",
        "M7AT10.RealSensorSource",
        "M7AT10.SensorManager",
        "M7AT10.SensorMonitor"
    ),
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedCommand {
    param(
        [string]$Label,
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host "$FilePath $($Arguments -join ' ')"

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $ProjectPath)) {
    throw "Project file not found: $ProjectPath"
}

$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if (-not (Test-Path -LiteralPath $BuildBat)) {
    throw "Build.bat not found: $BuildBat"
}
if (-not (Test-Path -LiteralPath $EditorCmd)) {
    throw "UnrealEditor-Cmd.exe not found: $EditorCmd"
}

if (-not $SkipBuild) {
    Invoke-CheckedCommand `
        -Label "Build m7at10_dtEditor Win64 Development" `
        -FilePath $BuildBat `
        -Arguments @("m7at10_dtEditor", "Win64", "Development", $ProjectPath, "-WaitMutex")
}

foreach ($TestGroup in $TestGroups) {
    Invoke-CheckedCommand `
        -Label "Automation $TestGroup" `
        -FilePath $EditorCmd `
        -Arguments @(
            $ProjectPath,
            "-NullRHI",
            "-Unattended",
            "-NoSplash",
            "-NoSound",
            "-ExecCmds=Automation RunTests $TestGroup; Quit",
            "-TestExit=Automation Test Queue Empty"
        )
}

Write-Host ""
Write-Host "All smoke checks passed."
