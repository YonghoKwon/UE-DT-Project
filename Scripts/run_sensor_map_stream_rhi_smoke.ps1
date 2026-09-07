param(
    [string]$BrokerUrl = "ws://127.0.0.1:61616",
    [string]$UserName = "artemis",
    [string]$Password = "artemis",
    [ValidateRange(5, 3600)][int]$WarmupSeconds = 10,
    [ValidateRange(10, 3600)][int]$MeasurementSeconds = 60,
    [int]$TimeoutSeconds = 100,
    [ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$ReportLabel = "sensor_map_stream_rhi_smoke",
    [switch]$SkipBuild,
    [switch]$SlabSessions,
    [switch]$PointCloudOnly
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Build = "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat"
$Editor = "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $ProjectRoot "ma0t10_dt.uproject"
$ReportDir = Join-Path $ProjectRoot "Saved\Reports"
$ProbeReport = Join-Path $ReportDir "$ReportLabel.json"
$ProbeStdOut = Join-Path $ReportDir "$ReportLabel.probe.stdout.log"
$ProbeStdErr = Join-Path $ReportDir "$ReportLabel.probe.stderr.log"
$EditorLog = Join-Path $ReportDir "$ReportLabel.editor.log"
$Markdown = Join-Path $ReportDir "$ReportLabel.md"
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

if (-not $SkipBuild) {
    & $Build ma0t10_dtEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Editor build failed with exit code $LASTEXITCODE" }
}

$ProbeScript = Join-Path $ProjectRoot "Tools\Artemis\stomp_probe.mjs"
$MinimumTimeout = $WarmupSeconds + $MeasurementSeconds + 25
if ($TimeoutSeconds -lt $MinimumTimeout) { $TimeoutSeconds = $MinimumTimeout }
$ProbeArgs = "`"$ProbeScript`" --url `"$BrokerUrl`" --user `"$UserName`" --password `"$Password`" --warmup $WarmupSeconds --duration $MeasurementSeconds --require-contiguous-pcd true --timeout $TimeoutSeconds --output `"$ProbeReport`""
if ($SlabSessions) { $ProbeArgs = "`"$ProbeScript`" --url `"$BrokerUrl`" --user `"$UserName`" --password `"$Password`" --warmup 0 --duration 63 --slab-runs 2 --require-contiguous-pcd true --timeout $TimeoutSeconds --output `"$ProbeReport`"" }
if ($PointCloudOnly) { $ProbeArgs += ' --topics topic.virtual.sensor.export.0'; $ProbeArgs += " --save-first-pcd `"$(Join-Path $ReportDir "$ReportLabel.pcd")`"" }
$Probe = Start-Process -FilePath "node" -ArgumentList $ProbeArgs -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $ProbeStdOut -RedirectStandardError $ProbeStdErr

try {
    $env:MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE = "1"
    $env:MA0T10_PCD_ONLY = if ($PointCloudOnly) { "1" } else { "0" }
    $env:MA0T10_RUN_SLAB_SCENARIO_SMOKE = if ($SlabSessions) { '1' } else { '0' }
    $env:MA0T10_ARTEMIS_URL = $BrokerUrl
    $env:MA0T10_ARTEMIS_USER = $UserName
    $env:MA0T10_ARTEMIS_PASSWORD = $Password
    # Keep PIE alive long enough for broker connection/setup plus the probe's
    # independent warmup and measurement windows.
	$env:MA0T10_STREAM_WARMUP_SECONDS = [string]$WarmupSeconds
	$env:MA0T10_STREAM_MEASURE_SECONDS = [string]$MeasurementSeconds
    $EditorArgs = @(
        $Project, "-unattended", "-nop4", "-nosplash", "-windowed", "-RenderOffscreen", "-NoVSync",
        "-ResX=1920", "-ResY=1080", "-NoSound",
        "-ExecCmds=t.MaxFPS 0,r.VSync 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke;Quit",
        "-TestExit=Automation Test Queue Empty", "-abslog=$EditorLog"
    )
    & $Editor @EditorArgs
    $EditorExitCode = $LASTEXITCODE
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
    $TestPassed = $EditorExitCode -eq 0 -and $EditorText -match "Test Completed\. Result=\{Success\}.*ContinuousThreeStreamSmoke"
    $PerformanceMatch = [regex]::Match($EditorText, '\[SensorStreamRhi\] averageFps=(?<average>[0-9.]+) onePercentLowFps=(?<low>[0-9.]+) p95FrameMs=(?<p95>[0-9.]+) samples=(?<samples>\d+)')
	$HighThroughputMatch = [regex]::Match($EditorText, '\[SensorHighThroughputRhi\] cameraSubmitted=(?<cameraSubmitted>\d+) cameraReceipt=(?<cameraReceipt>\d+) cameraConsumer=(?<cameraConsumer>\d+) cameraHz=(?<cameraHz>[0-9.]+) lidarSubmitted=(?<lidarSubmitted>\d+) lidarReceipt=(?<lidarReceipt>\d+) lidarConsumer=(?<lidarConsumer>\d+) lidarHz=(?<lidarHz>[0-9.]+) pcdSubmitted=(?<pcdSubmitted>\d+) pcdReceipt=(?<pcdReceipt>\d+) pcdConsumer=(?<pcdConsumer>\d+) pcdHz=(?<pcdHz>[0-9.]+)')
    $PcdMatch = [regex]::Match($EditorText, '\[SensorPcdNoLossRhi\] input=(?<input>\d+) serialized=(?<serialized>\d+) submitted=(?<submitted>\d+) receipts=(?<receipts>\d+) serializeHz=(?<hz>[0-9.]+) serializeP95Ms=(?<p95>[0-9.]+) inputQueue=(?<inputQueue>\d+) preparedQueue=(?<preparedQueue>\d+) receiptQueue=(?<receiptQueue>\d+) gaps=(?<gaps>\d+) retries=(?<retries>\d+) overload=(?<overload>\d+)')
    $AverageFps = if ($PerformanceMatch.Success) { [double]$PerformanceMatch.Groups['average'].Value } else { 0.0 }
    $OnePercentLowFps = if ($PerformanceMatch.Success) { [double]$PerformanceMatch.Groups['low'].Value } else { 0.0 }
    $P95FrameMs = if ($PerformanceMatch.Success) { [double]$PerformanceMatch.Groups['p95'].Value } else { 0.0 }
    $WallMatch = [regex]::Match($EditorText, '\[SensorStreamWallPacing\] averageFps=(?<fps>[0-9.]+) p95CallbackMs=(?<p95>[0-9.]+)')
    $WallP95 = if ($WallMatch.Success) { [double]$WallMatch.Groups['p95'].Value } else { 0.0 }
    $FrameSamples = if ($PerformanceMatch.Success) { [int]$PerformanceMatch.Groups['samples'].Value } else { 0 }
	$ReceiverLidar = if ($HighThroughputMatch.Success) { [int64]$HighThroughputMatch.Groups['lidarConsumer'].Value } else { 0 }
	$ReceiverCamera = if ($HighThroughputMatch.Success) { [int64]$HighThroughputMatch.Groups['cameraConsumer'].Value } else { 0 }
	$ReceiverPointCloud = if ($HighThroughputMatch.Success) { [int64]$HighThroughputMatch.Groups['pcdConsumer'].Value } else { 0 }
	$UiMatch = [regex]::Match($EditorText, '\[SensorUiReceiverRhi\] pcdReceived=(?<received>\d+) pcdValidated=(?<valid>\d+) failures=(?<failed>\d+) gaps=(?<gaps>\d+)')
    $UiPcdValidated = if ($UiMatch.Success) { [int64]$UiMatch.Groups['valid'].Value } else { -1 }
    $ReceiverFailures = if ($UiMatch.Success) { [int64]$UiMatch.Groups['failed'].Value } else { -1 }
	$CameraHz = if ($HighThroughputMatch.Success) { [double]$HighThroughputMatch.Groups['cameraHz'].Value } else { 0.0 }
	$LidarHz = if ($HighThroughputMatch.Success) { [double]$HighThroughputMatch.Groups['lidarHz'].Value } else { 0.0 }
	$PcdHz = if ($HighThroughputMatch.Success) { [double]$HighThroughputMatch.Groups['pcdHz'].Value } else { 0.0 }
    $PcdInput = if ($PcdMatch.Success) { [int64]$PcdMatch.Groups['input'].Value } else { -1 }
    $PcdSerialized = if ($PcdMatch.Success) { [int64]$PcdMatch.Groups['serialized'].Value } else { -1 }
    $PcdSubmitted = if ($PcdMatch.Success) { [int64]$PcdMatch.Groups['submitted'].Value } else { -1 }
    $PcdReceipts = if ($PcdMatch.Success) { [int64]$PcdMatch.Groups['receipts'].Value } else { -1 }
    $PcdSerializationHz = if ($PcdMatch.Success) { [double]$PcdMatch.Groups['hz'].Value } else { 0.0 }
    $PcdSerializationP95 = if ($PcdMatch.Success) { [double]$PcdMatch.Groups['p95'].Value } else { 0.0 }
    $PointCloudMetrics = $ProbeResult.metrics.'topic.virtual.sensor.export.0'
    $ProbeResult | Add-Member -NotePropertyName d3d12Detected -NotePropertyValue $D3d12 -Force
    $ProbeResult | Add-Member -NotePropertyName unrealAutomationPassed -NotePropertyValue $TestPassed -Force
    $ProbeResult | Add-Member -NotePropertyName averageFps -NotePropertyValue $AverageFps -Force
    $ProbeResult | Add-Member -NotePropertyName onePercentLowFps -NotePropertyValue $OnePercentLowFps -Force
    $ProbeResult | Add-Member -NotePropertyName p95FrameMs -NotePropertyValue $P95FrameMs -Force
    $ProbeResult | Add-Member -NotePropertyName frameSamples -NotePropertyValue $FrameSamples -Force
	$ProbeResult | Add-Member -NotePropertyName internalReceiverLidar -NotePropertyValue $ReceiverLidar -Force
	$ProbeResult | Add-Member -NotePropertyName internalReceiverCamera -NotePropertyValue $ReceiverCamera -Force
	$ProbeResult | Add-Member -NotePropertyName internalReceiverPointCloud -NotePropertyValue $ReceiverPointCloud -Force
	$ProbeResult | Add-Member -NotePropertyName internalReceiverFailures -NotePropertyValue $ReceiverFailures -Force
    $ProbeResult | Add-Member -NotePropertyName publisherPcdInput -NotePropertyValue $PcdInput -Force
    $ProbeResult | Add-Member -NotePropertyName publisherPcdSerialized -NotePropertyValue $PcdSerialized -Force
    $ProbeResult | Add-Member -NotePropertyName publisherPcdSubmitted -NotePropertyValue $PcdSubmitted -Force
    $ProbeResult | Add-Member -NotePropertyName publisherPcdReceipts -NotePropertyValue $PcdReceipts -Force
    $ProbeResult | Add-Member -NotePropertyName publisherPcdSerializationHz -NotePropertyValue $PcdSerializationHz -Force
    $ProbeResult | Add-Member -NotePropertyName publisherPcdSerializationP95Ms -NotePropertyValue $PcdSerializationP95 -Force
    $ProbeResult | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ProbeReport -Encoding UTF8
	$Passed = $ProbeResult.success -and $D3d12 -and $TestPassed -and $PerformanceMatch.Success -and $HighThroughputMatch.Success -and $PcdMatch.Success -and
        ($PointCloudOnly -or ($ReceiverLidar -ge 2 -and $ReceiverCamera -ge 2)) -and $ReceiverPointCloud -ge 2 -and $ReceiverFailures -eq 0 -and
        $PcdInput -gt 0 -and $PcdInput -eq $PcdSerialized -and $PcdInput -eq $PcdSubmitted -and $PcdInput -eq $PcdReceipts -and $PcdInput -eq $ReceiverPointCloud -and $UiPcdValidated -eq $PcdInput -and
		($PointCloudOnly -or ($CameraHz -ge 29.0 -and $LidarHz -ge 19.0)) -and $PcdHz -ge 19.0 -and
		$AverageFps -ge 55.0 -and $OnePercentLowFps -ge 45.0 -and $P95FrameMs -le 20.0 -and
		$PcdSerializationHz -ge 19.0 -and $PcdSerializationP95 -le 20.0 -and
        $null -ne $PointCloudMetrics -and ($SlabSessions -or $PointCloudMetrics.receiveHz -ge 19.0) -and
        $PointCloudMetrics.frameGaps -eq 0 -and $PointCloudMetrics.duplicates -eq 0 -and $PointCloudMetrics.invalidCount -eq 0
    $MarkdownText = @"
# SensorRefactorTestMap continuous stream RHI smoke

- Result: **$(if ($Passed) { "PASS" } else { "FAIL" })**
- D3D12 detected: $D3d12
- Unreal automation passed: $TestPassed
- Warmup: $WarmupSeconds s
- Measurement: $MeasurementSeconds s
- Slab sessions: $SlabSessions (two 30-second runs when enabled; external rates validated per run)
- LiDAR received: $($ProbeResult.counts.'topic.virtual.sensor.lidar.0')
- Camera received: $($ProbeResult.counts.'topic.virtual.sensor.camera.0')
- Point Cloud received: $($ProbeResult.counts.'topic.virtual.sensor.export.0')
- Point Cloud receive Hz: $($PointCloudMetrics.receiveHz)
- Point Cloud throughput: $($PointCloudMetrics.megabytesPerSecond) MiB/s
- Point Cloud FrameId gaps: $($PointCloudMetrics.frameGaps)
- Point Cloud duplicates: $($PointCloudMetrics.duplicates)
- Point Cloud invalid frames: $($PointCloudMetrics.invalidCount)
- Publisher PCD input/serialized/submitted/receipt: $PcdInput / $PcdSerialized / $PcdSubmitted / $PcdReceipts
- Publisher PCD serialization: $PcdSerializationHz Hz, p95 $PcdSerializationP95 ms
- Average FPS: $AverageFps
- 1% low FPS: $OnePercentLowFps
- p95 engine frame time (FApp delta, fixed timestep rejected): $P95FrameMs ms
- p95 automation callback wall-clock pacing: $WallP95 ms
- Frame samples: $FrameSamples
- Internal LiDAR receiver validated: $ReceiverLidar
- Internal Camera receiver validated: $ReceiverCamera
- Internal Point Cloud receiver validated: $ReceiverPointCloud
- Internal receiver failures: $ReceiverFailures
- UI Host PCD validated: $UiPcdValidated
- Point Cloud only: $PointCloudOnly
- Internal Camera/LiDAR/PCD submit Hz: $CameraHz / $LidarHz / $PcdHz
- Probe report: $ProbeReport
- Editor log: $EditorLog
"@
    $MarkdownText | Set-Content -LiteralPath $Markdown -Encoding UTF8
    if (-not $Passed) { throw "Continuous SensorRefactorTestMap stream RHI smoke failed. See $Markdown" }
    Write-Host "Continuous SensorRefactorTestMap stream RHI smoke passed."
    Write-Host $Markdown
}
finally {
    Remove-Item Env:MA0T10_RUN_SLAB_SCENARIO_SMOKE -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_ARTEMIS_URL -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_ARTEMIS_USER -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_ARTEMIS_PASSWORD -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_STREAM_MEASURE_SECONDS -ErrorAction SilentlyContinue
	Remove-Item Env:MA0T10_STREAM_WARMUP_SECONDS -ErrorAction SilentlyContinue
    if (-not $Probe.HasExited) { $Probe.Kill() }
}
