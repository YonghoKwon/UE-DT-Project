param(
    [string]$ProjectRoot = "",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [int]$CameraCount = 2,
    [int]$LidarCount = 2,
    [int]$WarmupSeconds = 10,
    [int]$SampleSeconds = 60,
    [ValidateSet("Niagara", "Cpu", "Off")]
    [string]$LidarRenderer = "Niagara",
    [string]$LogPath = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$SkipBuild,
    [switch]$SkipLaunch,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$projectPath = Join-Path $ProjectRoot "ma0t10_dt.uproject"
$editorPath = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$buildPath = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
$reportRoot = Join-Path $ProjectRoot "Saved\Reports"
$rendererSlug = $LidarRenderer.ToLowerInvariant()
if ([string]::IsNullOrWhiteSpace($LogPath)) { $LogPath = Join-Path $reportRoot "fullspec_${CameraCount}c_${LidarCount}l_${rendererSlug}.log" }
if ([string]::IsNullOrWhiteSpace($MarkdownPath)) { $MarkdownPath = Join-Path $reportRoot "fullspec_${CameraCount}c_${LidarCount}l_${rendererSlug}.md" }
if ([string]::IsNullOrWhiteSpace($JsonPath)) { $JsonPath = Join-Path $reportRoot "fullspec_${CameraCount}c_${LidarCount}l_${rendererSlug}.json" }

foreach ($path in @($projectPath, $editorPath, $buildPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file not found: $path" }
}
if ($CameraCount -lt 0 -or $CameraCount -gt 16 -or $LidarCount -lt 0 -or $LidarCount -gt 16) {
    throw "CameraCount and LidarCount must be between 0 and 16."
}
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null

if (-not $SkipBuild) {
    & $buildPath ma0t10_dtEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE" }
}

if (-not $SkipLaunch) {
    Remove-Item -LiteralPath $LogPath -Force -ErrorAction SilentlyContinue
    $arguments = @(
        "`"$projectPath`"",
        "/Game/MA0T10/Maps/SensorTestMap",
        "-game", "-windowed", "-ResX=1920", "-ResY=1080",
        "-NoSplash", "-NoSound", "-NoVSync", "-Unattended", "-RenderOffscreen",
        "-ExecCmds=`"t.IdleWhenNotForeground 0`"",
        "-VirtualSensorPerfCameras=$CameraCount",
        "-VirtualSensorPerfLidars=$LidarCount",
        "-VirtualSensorPerfLidarRenderer=$LidarRenderer",
        "-VirtualSensorPerfWarmupSeconds=$WarmupSeconds",
        "-abslog=`"$LogPath`""
    )
    $process = Start-Process -FilePath $editorPath -ArgumentList $arguments -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddSeconds($WarmupSeconds + $SampleSeconds + 5)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 1
        $process.Refresh()
    }
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(5000)) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
    }
}

if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) { throw "Performance log not found: $LogPath" }
$lines = @(Get-Content -LiteralPath $LogPath)
$aggregatePattern = '\[VirtualSensorPerf\] targetFps=(?<target>\d+) camera=(?<camera>\d+) lidar=(?<lidar>\d+) averageFps=(?<average>[0-9.]+) onePercentLowFps=(?<low>[0-9.]+) p95FrameMs=(?<p95>[0-9.]+) schedulerMs=(?<scheduler>[0-9.]+) pendingAcquisition=(?<pendingA>\d+) pendingDerived=(?<pendingD>\d+) droppedAcquisition=(?<droppedA>\d+) droppedDerived=(?<droppedD>\d+) bestEffort=(?<best>\d+)'
$sensorPattern = '\[VirtualSensorPerfSensor\] kind=(?<kind>Camera|Lidar) sensorId=(?<id>\S+).*?rateHz=(?<rate>[0-9.]+) acquisitionMs=(?<acquisition>[0-9.]+) postMs=(?<post>[0-9.]+) pendingAcquisition=(?<pendingA>\d+) pendingDerived=(?<pendingD>\d+) droppedAcquisition=(?<droppedA>\d+) droppedDerived=(?<droppedD>\d+)'

$aggregate = @()
$sensor = @()
for ($index = 0; $index -lt $lines.Count; ++$index) {
    $line = $lines[$index]
    $match = [regex]::Match($line, $aggregatePattern)
    if ($match.Success -and [int]$match.Groups['camera'].Value -eq $CameraCount -and [int]$match.Groups['lidar'].Value -eq $LidarCount) {
        $aggregate += [PSCustomObject]@{
            Line = $index + 1; TargetFps = [int]$match.Groups['target'].Value
            AverageFps = [double]$match.Groups['average'].Value; OnePercentLowFps = [double]$match.Groups['low'].Value
            P95FrameMs = [double]$match.Groups['p95'].Value; SchedulerMs = [double]$match.Groups['scheduler'].Value
            PendingAcquisition = [int]$match.Groups['pendingA'].Value; PendingDerived = [int]$match.Groups['pendingD'].Value
            DroppedAcquisition = [int]$match.Groups['droppedA'].Value; DroppedDerived = [int]$match.Groups['droppedD'].Value
            BestEffort = ([int]$match.Groups['best'].Value -ne 0)
        }
    }
}

if ($aggregate.Count -le $WarmupSeconds) {
    throw "Not enough matching telemetry samples. Found $($aggregate.Count), need more than warmup $WarmupSeconds."
}
$samples = @($aggregate | Select-Object -Skip $WarmupSeconds -First $SampleSeconds)
$firstLine = $samples[0].Line
$lastLine = $samples[-1].Line
for ($index = $firstLine - 1; $index -lt [Math]::Min($lines.Count, $lastLine + (($CameraCount + $LidarCount) * 2)); ++$index) {
    $match = [regex]::Match($lines[$index], $sensorPattern)
    if (-not $match.Success) { continue }
    $line = $lines[$index]
    $width = if ($line -match 'width=(\d+)') { [int]$Matches[1] } else { 0 }
    $height = if ($line -match 'height=(\d+)') { [int]$Matches[1] } else { 0 }
    $rays = if ($line -match 'rays=(\d+)') { [int]$Matches[1] } else { 0 }
    $budgetSkipped = if ($line -match 'budgetSkipped=(\d+)') { [int]$Matches[1] } else { 0 }
    $failedAcquisition = if ($line -match 'failedAcquisition=(\d+)') { [int]$Matches[1] } else { 0 }
    $queueOverflow = if ($line -match 'queueOverflow=(\d+)') { [int]$Matches[1] } else { 0 }
    $sensor += [PSCustomObject]@{
        Line = $index + 1; Kind = $match.Groups['kind'].Value; SensorId = $match.Groups['id'].Value
        Width = $width; Height = $height; Rays = $rays; RateHz = [double]$match.Groups['rate'].Value
        AcquisitionMs = [double]$match.Groups['acquisition'].Value; PostMs = [double]$match.Groups['post'].Value
        PendingAcquisition = [int]$match.Groups['pendingA'].Value; PendingDerived = [int]$match.Groups['pendingD'].Value
        DroppedAcquisition = [int]$match.Groups['droppedA'].Value; DroppedDerived = [int]$match.Groups['droppedD'].Value
        BudgetSkipped = $budgetSkipped; FailedAcquisition = $failedAcquisition; QueueOverflow = $queueOverflow
    }
}

$averageFps = ($samples | Measure-Object AverageFps -Average).Average
$onePercentLow = ($samples | Measure-Object OnePercentLowFps -Minimum).Minimum
$p95FrameMs = ($samples | Measure-Object P95FrameMs -Maximum).Maximum
$maxPendingPerSensor = if ($sensor.Count -gt 0) { (($sensor | ForEach-Object { [Math]::Max($_.PendingAcquisition, $_.PendingDerived) }) | Measure-Object -Maximum).Maximum } else { 999 }
$cameraShapeValid = @($sensor | Where-Object Kind -eq 'Camera' | Where-Object { $_.Width -ne 1280 -or $_.Height -ne 720 }).Count -eq 0
$lidarShapeValid = @($sensor | Where-Object Kind -eq 'Lidar' | Where-Object { $_.Rays -ne 21600 }).Count -eq 0
$enoughSamples = $samples.Count -ge [Math]::Max(1, [Math]::Floor($SampleSeconds * 0.8))

$threshold = if ($CameraCount -le 2 -and $LidarCount -le 2) {
    [PSCustomObject]@{ AverageFps = 55.0; OnePercentLowFps = 45.0; P95FrameMs = 20.0; CameraHz = 5.0; LidarHz = 2.0 }
} elseif ($CameraCount -le 4 -and $LidarCount -le 4) {
    [PSCustomObject]@{ AverageFps = 28.0; OnePercentLowFps = 24.0; P95FrameMs = 36.0; CameraHz = 2.5; LidarHz = 2.0 }
} else {
    $null
}
$thresholdPass = $null -eq $threshold -or ($averageFps -ge $threshold.AverageFps -and $onePercentLow -ge $threshold.OnePercentLowFps -and $p95FrameMs -le $threshold.P95FrameMs)
$sensorSummary = @($sensor | Group-Object Kind, SensorId | ForEach-Object {
    $group = $_.Group
    [PSCustomObject]@{
        Kind = $group[0].Kind; SensorId = $group[0].SensorId
        AverageRateHz = [Math]::Round(($group | Measure-Object RateHz -Average).Average, 3)
        MaxAcquisitionMs = ($group | Measure-Object AcquisitionMs -Maximum).Maximum
        MaxPostMs = ($group | Measure-Object PostMs -Maximum).Maximum
        FinalDroppedAcquisition = $group[-1].DroppedAcquisition
        FinalDroppedDerived = $group[-1].DroppedDerived
        FinalBudgetSkipped = $group[-1].BudgetSkipped
        FinalFailedAcquisition = $group[-1].FailedAcquisition
        FinalQueueOverflow = $group[-1].QueueOverflow
    }
})
$getFairnessRatio = {
    param([object[]]$Rows)
    if ($Rows.Count -le 1) { return 1.0 }
    $rates = @($Rows | ForEach-Object AverageRateHz | Where-Object { $_ -gt 0.0 })
    if ($rates.Count -ne $Rows.Count) { return [double]::PositiveInfinity }
    return (($rates | Measure-Object -Maximum).Maximum / ($rates | Measure-Object -Minimum).Minimum)
}
$cameraFairnessRatio = & $getFairnessRatio @($sensorSummary | Where-Object Kind -eq 'Camera')
$lidarFairnessRatio = & $getFairnessRatio @($sensorSummary | Where-Object Kind -eq 'Lidar')
$fairnessPass = $cameraFairnessRatio -le 1.2 -and $lidarFairnessRatio -le 1.2
$cameraRows = @($sensorSummary | Where-Object Kind -eq 'Camera')
$lidarRows = @($sensorSummary | Where-Object Kind -eq 'Lidar')
$minimumCameraHz = if ($cameraRows.Count -gt 0) { ($cameraRows | Measure-Object AverageRateHz -Minimum).Minimum } else { 0.0 }
$minimumLidarHz = if ($lidarRows.Count -gt 0) { ($lidarRows | Measure-Object AverageRateHz -Minimum).Minimum } else { 0.0 }
# A finite sample window can exclude one completion at either boundary. Keep a
# narrow 0.5% measurement tolerance so 4.98 Hz represents a 5 Hz schedule while
# genuine starvation (for example 4.8 Hz) still fails.
$rateMeasurementTolerance = 0.995
$completionRatePass = $null -eq $threshold -or (($CameraCount -eq 0 -or $minimumCameraHz -ge $threshold.CameraHz * $rateMeasurementTolerance) -and ($LidarCount -eq 0 -or $minimumLidarHz -ge $threshold.LidarHz * $rateMeasurementTolerance))
$queueHealthPass = @($sensorSummary | Where-Object { $_.FinalFailedAcquisition -gt 0 -or $_.FinalQueueOverflow -gt 0 }).Count -eq 0
$valid = $enoughSamples -and $cameraShapeValid -and $lidarShapeValid -and $maxPendingPerSensor -le 1 -and $thresholdPass -and $completionRatePass -and $fairnessPass -and $queueHealthPass
$lidarVisiblePointLimit = switch ($LidarRenderer) {
    "Niagara" { 21600 }
    "Cpu" { 5000 }
    default { 0 }
}

$report = [PSCustomObject]@{
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString('o')
    ProjectRoot = $ProjectRoot; LogPath = $LogPath; CameraCount = $CameraCount; LidarCount = $LidarCount
    LidarRenderer = $LidarRenderer; LidarVisiblePointLimit = $lidarVisiblePointLimit
    WarmupSeconds = $WarmupSeconds; RequestedSampleSeconds = $SampleSeconds; Samples = $samples.Count
    Summary = [PSCustomObject]@{
        AverageFps = [Math]::Round($averageFps, 3); OnePercentLowFps = [Math]::Round($onePercentLow, 3)
        P95FrameTimeMs = [Math]::Round($p95FrameMs, 3); MaxPendingPerSensor = $maxPendingPerSensor
        CameraFrameShapeValid = $cameraShapeValid; LidarFrameShapeValid = $lidarShapeValid
        CameraFairnessRatio = [Math]::Round($cameraFairnessRatio, 3); LidarFairnessRatio = [Math]::Round($lidarFairnessRatio, 3)
        MinimumCameraCompletionHz = [Math]::Round($minimumCameraHz, 3); MinimumLidarCompletionHz = [Math]::Round($minimumLidarHz, 3)
        FairnessPass = $fairnessPass; CompletionRatePass = $completionRatePass; QueueHealthPass = $queueHealthPass
        CompletionRateMeasurementTolerance = $rateMeasurementTolerance
        Threshold = $threshold; ThresholdPass = $thresholdPass; Valid = $valid
    }
    Sensors = $sensorSummary
}

$report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
$markdown = @(
    '# FullSpec Virtual Sensor Performance Report', '',
    "Generated UTC: $($report.GeneratedUtc)", '',
    "- Scenario: Camera $CameraCount + LiDAR $LidarCount",
    "- LiDAR renderer: $LidarRenderer",
    "- Selected LiDAR 3D visible-point limit: $lidarVisiblePointLimit",
    "- Samples: $($samples.Count) after ${WarmupSeconds}s warmup",
    "- Average FPS: $($report.Summary.AverageFps)",
    "- Conservative 1% low FPS: $($report.Summary.OnePercentLowFps)",
    "- Worst rolling p95 frame time: $($report.Summary.P95FrameTimeMs) ms",
    "- Max pending jobs per sensor: $($report.Summary.MaxPendingPerSensor)",
    "- Camera frames are 1280x720: $cameraShapeValid",
    "- LiDAR scans are 21,600 rays: $lidarShapeValid",
    "- Camera max/min completion ratio: $($report.Summary.CameraFairnessRatio)",
    "- LiDAR max/min completion ratio: $($report.Summary.LidarFairnessRatio)",
    "- Minimum Camera completion Hz: $($report.Summary.MinimumCameraCompletionHz)",
    "- Minimum LiDAR completion Hz: $($report.Summary.MinimumLidarCompletionHz)",
    "- Completion-rate threshold pass: $completionRatePass",
    "- Completion-rate measurement tolerance: $([Math]::Round((1.0 - $rateMeasurementTolerance) * 100.0, 2))%",
    "- Queue/failure health pass: $queueHealthPass",
    "- Fairness ratio <= 1.2: $fairnessPass",
    "- Threshold pass: $thresholdPass",
    "- Valid: $valid", '',
    '## Per-sensor completion telemetry', '',
    '| Kind | SensorId | Average Hz | Max acquisition ms | Max post ms | Budget skips | Failed acquisition | Queue overflow | Dropped derived |',
    '| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |'
)
foreach ($item in $sensorSummary) {
    $markdown += "| $($item.Kind) | $($item.SensorId) | $($item.AverageRateHz) | $($item.MaxAcquisitionMs) | $($item.MaxPostMs) | $($item.FinalBudgetSkipped) | $($item.FinalFailedAcquisition) | $($item.FinalQueueOverflow) | $($item.FinalDroppedDerived) |"
}
$markdown += @('', "This run uses the normal renderer with the requested LiDAR backend '$LidarRenderer'. It does not pass ``-NullRHI``. MultiHit and automatic per-scan exports are disabled by the benchmark command-line setup.")
$markdown | Set-Content -LiteralPath $MarkdownPath -Encoding UTF8

if ($Json) { $report | ConvertTo-Json -Depth 7 } else {
    Write-Host "FullSpec report: $MarkdownPath"
    Write-Host "JSON report: $JsonPath"
    Write-Host "Valid: $valid"
}
if (-not $valid) { exit 1 }
