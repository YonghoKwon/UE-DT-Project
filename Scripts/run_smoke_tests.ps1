param(
    [string]$ProjectPath = "C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [string[]]$TestGroups = @(
        "MA0T10.EditorSmoke",
        "MA0T10.SensorReplay",
        "MA0T10.SensorRecorder",
        "MA0T10.RealSensorSource",
        "MA0T10.SensorManager",
        "MA0T10.SensorMonitor"
    ),
    [switch]$SkipBuild,
    [switch]$AllowOpenEditor,
    [ValidateSet('NullRHI', 'D3D12')][string]$Rhi = 'NullRHI',
    [string]$ReportRoot = '',
    [string]$SensorSmokeMapDirectory = ''
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
if ($ReportRoot) { $ReportRoot = [IO.Path]::GetFullPath($ReportRoot) }

if (-not $SkipBuild) {
    $OpenEditors = Get-Process UnrealEditor -ErrorAction SilentlyContinue
    if ($OpenEditors -and -not $AllowOpenEditor) {
        $EditorList = ($OpenEditors | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ", "
        throw "UnrealEditor is running ($EditorList). Close the editor before building, use -SkipBuild after a successful build, or pass -AllowOpenEditor if you intentionally want to risk Live Coding/DLL lock failures."
    }

    Invoke-CheckedCommand `
        -Label "Build ma0t10_dtEditor Win64 Development" `
        -FilePath $BuildBat `
        -Arguments @("ma0t10_dtEditor", "Win64", "Development", $ProjectPath, "-WaitMutex", "-NoHotReloadFromIDE")
}

foreach ($TestGroup in $TestGroups) {
    $automationArgs = @(
        $ProjectPath,
        '/Engine/Maps/Entry',
        $(if ($Rhi -eq 'D3D12') { '-d3d12' } else { '-NullRHI' }),
        '-Unattended', '-NoSplash', '-NoSound', '-nop4',
        "-ExecCmds=Automation RunTests $TestGroup; Quit",
        '-TestExit=Automation Test Queue Empty'
    )
    if ($SensorSmokeMapDirectory) { $automationArgs += "-SensorSmokeMapDirectory=$SensorSmokeMapDirectory" }
    if ($ReportRoot) {
        $runDirectory = Join-Path $ReportRoot ($TestGroup -replace '[^a-zA-Z0-9_.-]', '_')
        New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
        # Preserve the exact existing fixtures inside this process's Saved root.
        # Their tests deliberately reject map snapshots outside Saved/Reports.
        $isolatedReports = Join-Path $runDirectory 'user/Saved/Reports'
        if ($SensorSmokeMapDirectory) {
            $sourceMap = Join-Path $SensorSmokeMapDirectory 'SensorTestMap.umap'
            if (-not (Test-Path -LiteralPath $sourceMap -PathType Leaf)) { throw 'Committed SensorTestMap fixture missing' }
            $isolatedMap = Join-Path $isolatedReports 'committed-map'
            New-Item -ItemType Directory -Force -Path $isolatedMap | Out-Null
            $copiedMap = Join-Path $isolatedMap 'SensorTestMap.umap'
            Copy-Item -LiteralPath $sourceMap -Destination $copiedMap
            if ((Get-FileHash -LiteralPath $sourceMap).Hash -ne (Get-FileHash -LiteralPath $copiedMap).Hash) { throw 'Committed map fixture copy mismatch' }
            $automationArgs = @($automationArgs | Where-Object { $_ -notlike '-SensorSmokeMapDirectory=*' })
            $automationArgs += "-SensorSmokeMapDirectory=$isolatedMap"
        }
        $projectDirectory = Split-Path -Parent $ProjectPath
        $coilFixtures = Join-Path $projectDirectory 'Saved/Reports/COIL-CAP-01/fixtures'
        if (Test-Path -LiteralPath $coilFixtures -PathType Container) {
            $isolatedCoil = Join-Path $isolatedReports 'COIL-CAP-01/fixtures'
            New-Item -ItemType Directory -Force -Path $isolatedCoil | Out-Null
            foreach ($fixtureName in @('full','half','list','lowering','mixed','varied')) {
                $sourceFixture = Join-Path $coilFixtures ($fixtureName + '.json')
                if (-not (Test-Path -LiteralPath $sourceFixture -PathType Leaf)) { throw "Coil environment fixture missing: $fixtureName" }
                $copiedFixture = Join-Path $isolatedCoil ($fixtureName + '.json')
                Copy-Item -LiteralPath $sourceFixture -Destination $copiedFixture
                if ((Get-FileHash -LiteralPath $sourceFixture).Hash -ne (Get-FileHash -LiteralPath $copiedFixture).Hash) { throw "Coil fixture copy mismatch: $fixtureName" }
            }
        }
        # A successful Automation Quit can force process exit before module
        # teardown. Give every process its own saved/native safety state.
        $automationArgs += @('-RenderOffscreen', '-NoVSync', "-ReportExportPath=$runDirectory", "-HatchReport=$runDirectory/measurements", "-abslog=$runDirectory/automation.log", "-UserDir=$runDirectory/user")
        $quotedArgs = @($automationArgs | ForEach-Object { '"' + $_ + '"' })
        $process = Start-Process $EditorCmd -ArgumentList $quotedArgs -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput (Join-Path $runDirectory 'console.log') -RedirectStandardError (Join-Path $runDirectory 'stderr.log')
        if ($process.ExitCode -ne 0) { throw "Automation $TestGroup failed with exit code $($process.ExitCode): $runDirectory" }
        if (-not (Test-Path -LiteralPath (Join-Path $runDirectory 'index.json'))) { throw "Automation report missing: $runDirectory" }
        continue
    }
    Invoke-CheckedCommand `
        -Label "Automation $TestGroup" `
        -FilePath $EditorCmd `
        -Arguments $automationArgs
}

Write-Host ""
Write-Host "All smoke checks passed."
