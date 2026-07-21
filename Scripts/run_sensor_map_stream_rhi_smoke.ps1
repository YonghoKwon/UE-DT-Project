param(
    [string]$BrokerUrl = "ws://127.0.0.1:61616",
    [string]$UserName = "artemis",
    [string]$Password = "artemis",
    [int]$TimeoutSeconds = 35,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Build = "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat"
$Editor = "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $ProjectRoot "ma0t10_dt.uproject"
$ReportDir = Join-Path $ProjectRoot "Saved\Reports"
$ProbeReport = Join-Path $ReportDir "sensor_map_stream_rhi_smoke.json"
$ProbeStdOut = Join-Path $ReportDir "sensor_map_stream_probe.stdout.log"
$ProbeStdErr = Join-Path $ReportDir "sensor_map_stream_probe.stderr.log"
$EditorLog = Join-Path $ReportDir "sensor_map_stream_rhi_smoke.log"
$Markdown = Join-Path $ReportDir "sensor_map_stream_rhi_smoke.md"
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

if (-not $SkipBuild) {
    & $Build ma0t10_dtEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Editor build failed with exit code $LASTEXITCODE" }
}

$ProbeScript = Join-Path $ProjectRoot "Tools\Artemis\stomp_probe.mjs"
$ProbeArgs = "`"$ProbeScript`" --url `"$BrokerUrl`" --user `"$UserName`" --password `"$Password`" --count 2 --timeout $TimeoutSeconds --output `"$ProbeReport`""
$Probe = Start-Process -FilePath "node" -ArgumentList $ProbeArgs -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $ProbeStdOut -RedirectStandardError $ProbeStdErr

try {
    $env:MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE = "1"
    $env:MA0T10_ARTEMIS_URL = $BrokerUrl
    $env:MA0T10_ARTEMIS_USER = $UserName
    $env:MA0T10_ARTEMIS_PASSWORD = $Password
    $EditorArgs = @(
        $Project, "-unattended", "-nop4", "-nosplash", "-windowed", "-RenderOffscreen",
        "-ResX=1280", "-ResY=720", "-NoSound",
        "-ExecCmds=Automation RunTests MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke;Quit",
        "-TestExit=Automation Test Queue Empty", "-abslog=$EditorLog"
    )
    & $Editor @EditorArgs
    if ($LASTEXITCODE -ne 0) { throw "Unreal runtime stream automation failed with exit code $LASTEXITCODE" }
    if (-not $Probe.WaitForExit(($TimeoutSeconds + 10) * 1000)) {
        $Probe.Kill()
        throw "STOMP probe timed out"
    }
    $Probe.WaitForExit()
    $Probe.Refresh()
    if ($null -ne $Probe.ExitCode -and $Probe.ExitCode -ne 0) { throw "STOMP probe failed with exit code $($Probe.ExitCode). See $ProbeStdErr" }

    $ProbeResult = Get-Content $ProbeReport -Raw | ConvertFrom-Json
    $EditorText = Get-Content $EditorLog -Raw
    $D3d12 = $EditorText -match "D3D12RHI|DirectX 12|D3D12 Adapter"
    $TestPassed = $EditorText -match "Test Completed\. Result=\{Success\}.*ContinuousThreeStreamSmoke"
    $PerformanceMatch = [regex]::Match($EditorText, '\[SensorStreamRhi\] averageFps=(?<average>[0-9.]+) onePercentLowFps=(?<low>[0-9.]+) p95FrameMs=(?<p95>[0-9.]+) samples=(?<samples>\d+)')
    $AverageFps = if ($PerformanceMatch.Success) { [double]$PerformanceMatch.Groups['average'].Value } else { 0.0 }
    $OnePercentLowFps = if ($PerformanceMatch.Success) { [double]$PerformanceMatch.Groups['low'].Value } else { 0.0 }
    $P95FrameMs = if ($PerformanceMatch.Success) { [double]$PerformanceMatch.Groups['p95'].Value } else { 0.0 }
    $FrameSamples = if ($PerformanceMatch.Success) { [int]$PerformanceMatch.Groups['samples'].Value } else { 0 }
    $ProbeResult | Add-Member -NotePropertyName d3d12Detected -NotePropertyValue $D3d12 -Force
    $ProbeResult | Add-Member -NotePropertyName unrealAutomationPassed -NotePropertyValue $TestPassed -Force
    $ProbeResult | Add-Member -NotePropertyName averageFps -NotePropertyValue $AverageFps -Force
    $ProbeResult | Add-Member -NotePropertyName onePercentLowFps -NotePropertyValue $OnePercentLowFps -Force
    $ProbeResult | Add-Member -NotePropertyName p95FrameMs -NotePropertyValue $P95FrameMs -Force
    $ProbeResult | Add-Member -NotePropertyName frameSamples -NotePropertyValue $FrameSamples -Force
    $ProbeResult | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ProbeReport -Encoding UTF8
    $Passed = $ProbeResult.success -and $D3d12 -and $TestPassed -and $PerformanceMatch.Success
    $MarkdownText = @"
# SensorRefactorTestMap continuous stream RHI smoke

- Result: **$(if ($Passed) { "PASS" } else { "FAIL" })**
- D3D12 detected: $D3d12
- Unreal automation passed: $TestPassed
- Messages per topic: $($ProbeResult.expectedPerTopic)
- LiDAR received: $($ProbeResult.counts.'topic.virtual.sensor.lidar.0')
- Camera received: $($ProbeResult.counts.'topic.virtual.sensor.camera.0')
- Point Cloud received: $($ProbeResult.counts.'topic.virtual.sensor.export.0')
- Average FPS: $AverageFps
- 1% low FPS: $OnePercentLowFps
- p95 frame time: $P95FrameMs ms
- Frame samples: $FrameSamples
- Probe report: $ProbeReport
- Editor log: $EditorLog
"@
    $MarkdownText | Set-Content -LiteralPath $Markdown -Encoding UTF8
    if (-not $Passed) { throw "Continuous SensorRefactorTestMap stream RHI smoke failed. See $Markdown" }
    Write-Host "Continuous SensorRefactorTestMap stream RHI smoke passed."
    Write-Host $Markdown
}
finally {
    Remove-Item Env:MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_ARTEMIS_URL -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_ARTEMIS_USER -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_ARTEMIS_PASSWORD -ErrorAction SilentlyContinue
    if (-not $Probe.HasExited) { $Probe.Kill() }
}
