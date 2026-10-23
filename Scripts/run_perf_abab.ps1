param(
    [string]$Baseline = 'HEAD^',
    [string]$Candidate = 'HEAD',
    [ValidatePattern('^[12](,[12])?$')][string]$Pairs = '1',
    [ValidateSet('Visible', 'Offscreen', 'Both')][string]$Display = 'Visible',
    [ValidateSet('Off', 'ProjectDefault')][string]$RayTracing = 'Off',
    [ValidateRange(1, 10)][int]$Blocks = 3,
    [ValidateRange(30, 120)][int]$MeasurementSeconds = 60,
    [ValidateSet('Perf01', 'ScheduledLidar')][string]$Workload = 'Perf01',
    [switch]$CameraGroundTruth,
    [ValidateSet('MLX80Native', 'Rev7')][string]$LidarProfile = 'MLX80Native',
    [switch]$RequireGpu,
    [ValidateSet('Gpu', 'Cpu')][string]$ScheduledBackend = 'Gpu',
    [ValidateRange(0, 65535)][int]$ScheduledPcdLoopbackPort = 0,
    [ValidateSet(1,2,4)][int]$ScheduledDivisor = 1,
    [string]$NativeLzfValidatorPath = '',
    [double]$ThresholdPercent = 2.0,
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.3',
    [string]$ReportRoot = '',
    [string]$SummarizeOnly = '',
    [switch]$KeepCheckouts,
    [switch]$DryRun
)
# QA-02 standard performance gate: archive checkouts of two revisions, the same committed
# selected instrument in both, ABAB x Blocks per condition, per-revision run medians.
# ScheduledLidar adds candidate per-device Hz ratios; legacy59FPS is informational.
# FPS decrease, engine p95 increase and game thread increase are judged separately.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/PerfRelativeGate.ps1"
if ($Workload -eq 'Perf01' -and $MeasurementSeconds -gt 60) { throw 'PERF01 measurement remains limited to30..60 seconds' }
if ($Workload -ne 'Perf01' -and ($CameraGroundTruth -or $LidarProfile -ne 'MLX80Native')) { throw 'Camera/profile conditions require Perf01 workload' }
if ($Workload -eq 'ScheduledLidar' -and ($Pairs -ne '1' -or $RayTracing -ne 'ProjectDefault')) { throw 'ScheduledLidar requires -Pairs 1 -RayTracing ProjectDefault; its workload is three LiDARs' }
if ($ScheduledPcdLoopbackPort -gt 0 -and $ScheduledPcdLoopbackPort -lt 1024) { throw 'PCD requires an unprivileged credential-free loopback port' }

$pairList = @($Pairs -split ',' | ForEach-Object { [int]$_ } | Sort-Object -Unique)
$root = Split-Path -Parent $PSScriptRoot
$instrument = if ($Workload -eq 'ScheduledLidar') { 'Source/ma0t10_dtEditor/Private/GpuLidarScheduledTests.cpp' } else { 'Source/ma0t10_dtEditor/Private/Perf01AcquisitionTests.cpp' }
$sessionInstrument = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h'
$mapName = if ($Workload -eq 'ScheduledLidar') { 'ShipHatchTestMap' } else { 'SlabScenarioValidationMap' }
$map = "Content/MA0T10/Maps/Tests/$mapName.umap"
$editor = "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
$build = "$EngineRoot/Engine/Build/BatchFiles/Build.bat"

function Get-Git([string[]]$Arguments, [string]$Repo = $root) {
    $out = @(& git.exe -C $Repo @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "git failed: $($Arguments -join ' ')" }
    return ($out -join "`n").Trim()
}
function Get-Sha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }

function Get-Verdict([string]$RunFolder, [double]$Threshold) {
    $rows = @(Get-ChildItem -LiteralPath $RunFolder -Filter '*.json' -File | ForEach-Object {
        $data = Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($_.BaseName -notmatch '^(?<cond>.+)-block(?<block>\d+)-(?<index>\d+)-(?<rev>[AB])$') { return }
        $condition = $Matches.cond; $revision = $Matches.rev
        $block = [int]$Matches.block; $index = [int]$Matches.index
        $slotValid = $block -ge 1 -and $block -le $Blocks -and $index -ge 1 -and $index -le 4 -and $revision -eq $(if ($index % 2) { 'A' } else { 'B' })
        $metrics = @([double]$data.average_fps, [double]$data.engine_frame_p95_ms, [double]$data.game_thread_mean_ms)
        $finitePositive = @($metrics | Where-Object { [double]::IsNaN($_) -or [double]::IsInfinity($_) -or $_ -le 0 }).Count -eq 0
        $ratios = @()
        if ($Workload -eq 'ScheduledLidar') {
            $ratios = @($data.sensors | ForEach-Object { if ($null -eq $_.acquisition_ratio) { [double]::NaN } else { [double]$_.acquisition_ratio } })
            $finitePositive = $finitePositive -and $ratios.Count -eq 3
        }
        $gpuMean = if ($Workload -eq 'ScheduledLidar') { [double]$data.gpu_frame_mean_ms } else { [double]$data.gpu_mean_ms }
        [PSCustomObject]@{ Condition = $condition; Revision = $revision; Valid = [bool]$data.valid -and $finitePositive -and $slotValid; Fps = $metrics[0]; P95 = $metrics[1]; GameThread = $metrics[2]; Gpu = $gpuMean; Rss = [double]$data.rss_bytes; File = $_.Name; AcquisitionRatios = $ratios }
    })
    if ($rows.Count -eq 0) { throw "No ABAB run JSON in $RunFolder" }
    $conditions = foreach ($group in ($rows | Group-Object Condition)) {
        $a = @($group.Group | Where-Object Revision -eq 'A'); $b = @($group.Group | Where-Object Revision -eq 'B')
        $comparison = Get-PerfRelativeComparison -A $a -B $b -Expected (2 * $Blocks) -Threshold $Threshold -RequireAcquisitionRatio:($Workload -eq 'ScheduledLidar') -RequireGpu:$RequireGpu
        $comparison | Add-Member -NotePropertyName Condition -NotePropertyValue $group.Name -PassThru
    }
    return @($conditions)
}

if ($SummarizeOnly) {
    $verdict = Get-Verdict -RunFolder $SummarizeOnly -Threshold $ThresholdPercent
    $verdict | Format-Table Condition, SamplesPerRevision, FpsDropPercent, EngineP95RisePercent, GameThreadRisePercent, Pass -AutoSize | Out-String | Write-Host
    $verdict | ConvertTo-Json -Depth 6
    if (@($verdict | Where-Object { -not $_.Pass }).Count) { exit 1 }
    exit 0
}

$started = [DateTime]::UtcNow
$stamp = $started.ToString('yyyyMMddTHHmmssZ')
if (-not $ReportRoot) { $ReportRoot = Join-Path $root "Saved/Reports/perf/$stamp" }
$ReportRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ReportRoot)
$owned = Join-Path $root "Saved/PerfAbab-$stamp-owned"
$runs = Join-Path $ReportRoot 'runs'
New-Item -ItemType Directory -Force -Path $runs | Out-Null

$refs = [ordered]@{ A = (Get-Git @('rev-parse', $Baseline)); B = (Get-Git @('rev-parse', $Candidate)) }
$dtcore = Get-Git @('-C', 'Plugins/DTCore', 'rev-parse', 'HEAD')
$instrumentSource = Join-Path $root $instrument
if (-not (Test-Path -LiteralPath $instrumentSource)) { throw "Missing instrument $instrument" }
$gpu = @(Get-CimInstance Win32_VideoController | ForEach-Object { [ordered]@{ Name = $_.Name; Driver = $_.DriverVersion; Mode = "$($_.CurrentHorizontalResolution)x$($_.CurrentVerticalResolution)" } })
$displays = switch ($Display) { 'Visible' { @($false) } 'Offscreen' { @($true) } default { @($false, $true) } }
$manifest = [ordered]@{
    SchemaVersion = 1; Gate = 'QA-02 perf ABAB'; StartedUtc = $started.ToString('o')
    Baseline = [ordered]@{ Ref = $Baseline; Commit = $refs.A }; Candidate = [ordered]@{ Ref = $Candidate; Commit = $refs.B }
    DTCore = $dtcore; Instrument = [ordered]@{ Path = $instrument; Sha256 = (Get-Sha256 $instrumentSource) }
    SessionInstrument = [ordered]@{ Path = $sessionInstrument; Sha256 = (Get-Sha256 (Join-Path $root $sessionInstrument)) }
    Workload = $Workload; ScheduledBackend = $ScheduledBackend; ScheduledPcdLoopbackPort = $ScheduledPcdLoopbackPort; ScheduledDivisor = $ScheduledDivisor
    CameraGroundTruth = [bool]$CameraGroundTruth; LidarProfile = $LidarProfile; RequireGpu = [bool]$RequireGpu
    RelativeGateSha256 = (Get-Sha256 "$PSScriptRoot/PerfRelativeGate.ps1")
    ReceiverSha256 = (Get-Sha256 "$root/Tools/Artemis/pcd_loopback_probe.py")
    NativeLzfValidatorSha256 = $(if ($NativeLzfValidatorPath) { Get-Sha256 $NativeLzfValidatorPath } else { '' })
    BackgroundCpuSnapshots = 'runs/<run>.background-cpu.json; informational, not used in verdict'
    Map = "/Game/MA0T10/Maps/Tests/$mapName"; Viewport = '1920x1080'; Rhi = 'D3D12'; RayTracing = $RayTracing
    Pairs = $pairList; Display = $Display; Blocks = $Blocks; WarmupSeconds = 10; MeasurementSeconds = $MeasurementSeconds
    ThresholdPercent = $ThresholdPercent; Gpu = $gpu; Os = [Environment]::OSVersion.ToString()
    CheckoutRoot = $owned; KeptCheckouts = [bool]$KeepCheckouts; Worktree = (Get-Git @('status', '--porcelain=v1'))
}
$manifestPath = Join-Path $ReportRoot 'manifest.json'
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
if ($DryRun) { Write-Host "Dry run manifest: $manifestPath"; exit 0 }

if (Get-Process UnrealEditor, UnrealEditor-Cmd, LiveCodingConsole -ErrorAction SilentlyContinue) { throw 'Close every Editor, PIE and Live Coding before the perf gate (mouse checks, builds and traces must not overlap).' }
if (Test-Path -LiteralPath $owned) { throw "Owned checkout folder already exists: $owned" }
New-Item -ItemType Directory -Path $owned | Out-Null
@{ CreatedBy = 'Scripts/run_perf_abab.ps1'; Root = $root; StartedUtc = $started.ToString('o') } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $owned 'ownership.json') -Encoding UTF8
$failure = ''
try {
    foreach ($label in 'A', 'B') {
        $target = Join-Path $owned $label
        $zip = Join-Path $owned "$label.zip"
        & git.exe -C $root archive --format=zip "--output=$zip" $refs[$label]; if ($LASTEXITCODE -ne 0) { throw "archive $label failed" }
        Expand-Archive -LiteralPath $zip -DestinationPath $target
        $pluginZip = Join-Path $owned "$label-DTCore.zip"
        & git.exe -C (Join-Path $root 'Plugins/DTCore') archive --format=zip "--output=$pluginZip" $dtcore; if ($LASTEXITCODE -ne 0) { throw "DTCore archive $label failed" }
        Expand-Archive -LiteralPath $pluginZip -DestinationPath (Join-Path $target 'Plugins/DTCore')
        Copy-Item -LiteralPath $instrumentSource -Destination (Join-Path $target $instrument) -Force
        Copy-Item -LiteralPath (Join-Path $root $sessionInstrument) -Destination (Join-Path $target $sessionInstrument) -Force
        if ((Get-Sha256 (Join-Path $target $instrument)) -ne $manifest.Instrument.Sha256 -or (Get-Sha256 (Join-Path $target $sessionInstrument)) -ne $manifest.SessionInstrument.Sha256) { throw "Instrument overlay hash mismatch for $label" }
        $manifest[$label + 'MapSha256'] = Get-Sha256 (Join-Path $target $map)
    }
    # Same fixture workload on both sides, or the gate compares different scenes.
    if ($manifest.AMapSha256 -ne $manifest.BMapSha256) { throw "$mapName differs between baseline and candidate (A=$($manifest.AMapSha256) B=$($manifest.BMapSha256)); measurement refused" }
    foreach ($label in 'A', 'B') {
        $target = Join-Path $owned $label
        $log = Join-Path $ReportRoot "$label-editor-build.log"
        & $build ma0t10_dtEditor Win64 Development "-Project=$target/ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE *> $log
        if ($LASTEXITCODE -ne 0) { throw "Editor build $label failed: $log" }
    }
    foreach ($pairCount in $pairList) {
        foreach ($offscreen in $displays) {
            $condition = $(if ($Workload -eq 'ScheduledLidar') { "lidar3-$ScheduledBackend-div$ScheduledDivisor-pcd$ScheduledPcdLoopbackPort-" } else { "${pairCount}pair-" }) + $(if ($offscreen) { 'offscreen' } else { 'visible' })
            if ($Workload -eq 'Perf01' -and ($CameraGroundTruth -or $LidarProfile -ne 'MLX80Native')) { $condition += "-gt$([int][bool]$CameraGroundTruth)-$LidarProfile" }
            for ($block = 1; $block -le $Blocks; $block++) {
                $index = 0
                foreach ($label in 'A', 'B', 'A', 'B') {
                    $index++
                    $name = "$condition-block$block-$index-$label"
                    $json = Join-Path $runs "$name.json"
                    if (Test-Path -LiteralPath $json) { throw "Refuse to overwrite run evidence: $json" }
                    $arguments = @("`"$owned/$label/ma0t10_dt.uproject`"", "/Game/MA0T10/Maps/Tests/$mapName", '-unattended', '-nop4', '-nosplash', '-windowed', '-NoVSync', '-NoSound', '-ResX=1920', '-ResY=1080', '-d3d12', '-Perf01', "-Perf01Pairs=$pairCount", "-Perf01Seconds=$MeasurementSeconds", "`"-Perf01Output=$json`"", "`"-abslog=$runs/$name.log`"", '"-ExecCmds=t.MaxFPS 0,r.VSync 0,r.VSyncEditor 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.PERF01.AcquisitionAndPreview;Quit"', '"-TestExit=Automation Test Queue Empty"')
                    if ($Workload -eq 'ScheduledLidar') {
                        $arguments = @("`"$owned/$label/ma0t10_dt.uproject`"", "/Game/MA0T10/Maps/Tests/$mapName", '-unattended', '-nop4', '-nosplash', '-windowed', '-NoVSync', '-NoSound', '-ResX=1920', '-ResY=1080', '-d3d12', "-GpuLidarMeasurementSeconds=$MeasurementSeconds", "`"-GpuLidarOutput=$json`"", "`"-abslog=$runs/$name.log`"", '"-ExecCmds=t.MaxFPS 60,r.VSync 0,r.VSyncEditor 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.GpuLidar.ScheduledThree;Quit"', '"-TestExit=Automation Test Queue Empty"')
                        $arguments += @('-PcdBandwidthDefaults', "-PcdBandwidthDivisor=$ScheduledDivisor")
                        if ($label -eq 'A') { $arguments += '-GpuLidarReferenceRun' }
                        if ($ScheduledBackend -eq 'Cpu') { $arguments += '-GpuLidarCpu' }
                        if ($ScheduledPcdLoopbackPort) { $arguments += "-GpuLidarPcdLoopbackPort=$ScheduledPcdLoopbackPort" }
                    }
                    if ($offscreen) { $arguments += '-RenderOffscreen' }
                    if ($Workload -eq 'Perf01') {
                        $arguments += "-Perf01LidarProfile=$LidarProfile"
                        if ($CameraGroundTruth) { $arguments += '-CameraTruthPerformance' }
                    }
                    if ($RayTracing -eq 'Off') { $arguments += '-noraytracing' }
                    $arguments += @("-Perf01SourceCommit=$($refs[$label])", "-Perf01RunId=$name", "-Perf01InstrumentSha=$($manifest.Instrument.Sha256)", "-Perf01SessionSha=$($manifest.SessionInstrument.Sha256)", "`"-ReportExportPath=$runs/$name.automation`"")
                    $saved = @{}
                    Get-ChildItem Env: | Where-Object Name -like 'MA0T10_*' | ForEach-Object { $saved[$_.Name] = $_.Value; Remove-Item "Env:$($_.Name)" }
                    $env:MA0T10_ACCEPTANCE_RES_X = '1920'; $env:MA0T10_ACCEPTANCE_RES_Y = '1080'
                    $probe = $null
                    try {
                        if ($Workload -eq 'ScheduledLidar' -and $ScheduledPcdLoopbackPort) {
                            # Own one credential-free receiver per run. Frame IDs restart
                            # with each Editor; separate reports avoid artificial gaps/duplicates.
                            $probeArgs = @("`"$root/Tools/Artemis/pcd_loopback_probe.py`"", '--port', $ScheduledPcdLoopbackPort, '--report', "`"$runs/$name.receiver.json`"", '--stop-file', "`"$runs/$name.receiver.stop`"")
                            if ($NativeLzfValidatorPath) { $probeArgs += @('--native-validator', "`"$NativeLzfValidatorPath`"") }
                            $probe = Start-Process -FilePath (Get-Command python.exe -ErrorAction Stop).Source -ArgumentList $probeArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$runs/$name.receiver.log" -RedirectStandardError "$runs/$name.receiver.stderr.log"
                            # Windows PowerShell needs the native handle retained before
                            # exit; otherwise WaitForExit can leave ExitCode null (even0).
                            $probeHandle = $probe.Handle
                            $ready = $false
                            for ($attempt = 0; $attempt -lt 50; $attempt++) {
                                if (Test-Path -LiteralPath "$runs/$name.receiver.log") { $ready = [bool](Select-String -LiteralPath "$runs/$name.receiver.log" -SimpleMatch 'PCD loopback ready' -Quiet) }
                                if ($ready -or $probe.HasExited) { break }
                                Start-Sleep -Milliseconds 100
                            }
                            if (!$ready) { throw "Local PCD receiver not ready; inspect $name.receiver.stderr.log" }
                        }
                        $snapshot = [ordered]@{ CapturedUtc = [DateTime]::UtcNow.ToString('o'); Scope = 'before launch; measurement follows10s warmup'; UsedForVerdict = $false; LogicalProcessors = [Environment]::ProcessorCount; ProcessPercentBasis = 'raw multi-core percent; can exceed100'; Error = '' }
                        try {
                            $snapshot.TotalCpuPercent = (Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor -Filter "Name='_Total'").PercentProcessorTime
                            $snapshot.Processes = @(Get-CimInstance Win32_PerfFormattedData_PerfProc_Process | Where-Object { $_.Name -notin @('_Total', 'Idle') } | Sort-Object PercentProcessorTime -Descending | Select-Object -First 10 Name, IDProcess, PercentProcessorTime)
                        } catch { $snapshot.Error = $_.Exception.Message }
                        $snapshot | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$runs/$name.background-cpu.json" -Encoding UTF8
                        Write-Host "Run $name"
                        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput "$runs/$name.console.log" -RedirectStandardError "$runs/$name.stderr.log"
                    } finally {
                        Remove-Item Env:MA0T10_ACCEPTANCE_RES_X, Env:MA0T10_ACCEPTANCE_RES_Y -ErrorAction SilentlyContinue
                        foreach ($key in $saved.Keys) { Set-Item "Env:$key" $saved[$key] }
                        if ($null -ne $probe) {
                            [IO.File]::WriteAllText("$runs/$name.receiver.stop", '')
                            if (!$probe.WaitForExit(10000)) { throw "Owned receiver did not stop; PID=$($probe.Id)" }
                            if ($null -eq $probe.ExitCode -or $probe.ExitCode -ne 0) { throw "Owned receiver failed or exit code unavailable; inspect $name.receiver.stderr.log" }
                        }
                    }
                    if (-not (Test-Path -LiteralPath $json)) { throw "No measurement for $name (exit $($process.ExitCode))" }
                    $data = Get-Content -LiteralPath $json -Raw -Encoding UTF8 | ConvertFrom-Json
                    Write-Host ('{0}: fps={1:N3} p95={2:N3} gt={3:N3} valid={4}' -f $name, $data.average_fps, $data.engine_frame_p95_ms, $data.game_thread_mean_ms, $data.valid)
                    if ($process.ExitCode -ne 0 -or -not $data.valid) { throw "Invalid run $name" }
                    if ($data.Perf01SourceCommit -ne $refs[$label] -or $data.Perf01RunId -ne $name -or $data.Perf01InstrumentSha -ne $manifest.Instrument.Sha256 -or $data.Perf01SessionSha -ne $manifest.SessionInstrument.Sha256) { throw "Run provenance mismatch: $name" }
                    if ($data.measurement_seconds -ne $MeasurementSeconds -or $data.warmup_seconds -ne 10 -or $data.actual_client -ne '1920x1080' -or [bool]$data.render_offscreen -ne $offscreen) { throw "Run workload mismatch: $name" }
                    if ($Workload -eq 'Perf01' -and $data.pairs -ne $pairCount) { throw "Run pair count mismatch: $name" }
                    if ($Workload -eq 'Perf01') {
                        if ([bool]$data.camera_ground_truth_enabled -ne [bool]$CameraGroundTruth -or $data.lidar_profile -ne $LidarProfile) { throw "GT/profile condition mismatch: $name" }
                        if ($LidarProfile -eq 'Rev7') {
                            $lidars = @($data.sensors | Where-Object kind -eq 'Lidar')
                            if ($lidars.Count -ne $pairCount) { throw "Missing Rev7 devices: $name" }
                            foreach ($sensor in $lidars) {
                                if ($sensor.device_profile -ne 'OusterOS1_64_Rev7' -or $sensor.backend -ne 'hardware_ray_tracing' -or !$sensor.semantic_classification_enabled) { throw "Rev7 backend/semantics mismatch: $name" }
                            }
                        }
                    }
                    if ($Workload -eq 'ScheduledLidar') {
                        if ($data.sensor_count -ne 3 -or @($data.sensors).Count -ne 3 -or [bool]$data.reference_run -ne ($label -eq 'A')) { throw "Scheduled workload mismatch: $name" }
                        foreach ($sensor in $data.sensors) {
                            if ([Math]::Abs($sensor.requested_hz - 20) -gt .001 -or $sensor.horizontal_samples -ne 1024 -or $sensor.vertical_channels -ne 64) { throw "Scheduled density/request mismatch: $name" }
                        }
                        if ($ScheduledPcdLoopbackPort) {
                            $receiver = Get-Content -LiteralPath "$runs/$name.receiver.json" -Raw -Encoding UTF8 | ConvertFrom-Json
                            if ($receiver.invalid -ne 0 -or @($receiver.errors).Count -ne 0) { throw "External PCD schema/checksum receiver failed: $name" }
                            $windowStart = [DateTimeOffset]::Parse($data.measurement_started_utc).ToUnixTimeMilliseconds()/1000.0
                            $windowEnd = [DateTimeOffset]::Parse($data.measurement_ended_utc).ToUnixTimeMilliseconds()/1000.0
                            $windowRows = @()
                            foreach ($sensor in $data.sensors) {
                                $external = $receiver.sensors.PSObject.Properties[$sensor.sensor]
                                if ($null -eq $external -or $external.Value.frames -le 0) { throw "External PCD receive check failed: $name" }
                                # Same10s warmup exclusion as acquisition/output metrics. Keep
                                # raw warmup-inclusive totals; never reinterpret their gaps as0.
                                $samples = @($external.Value.samples | Where-Object { $_[0] -ge $windowStart -and $_[0] -le $windowEnd })
                                $gaps=0; $duplicates=0
                                for($i=1;$i -lt $samples.Count;$i++) {
                                    $delta=[long]$samples[$i][1]-[long]$samples[$i-1][1]
                                    if($delta -le 0){$duplicates++}elseif($delta -ne $ScheduledDivisor){$gaps++}
                                }
                                $windowRows += @{Sensor=$sensor.sensor;Frames=$samples.Count;Gaps=$gaps;Duplicates=$duplicates;ExpectedFrameIdStep=$ScheduledDivisor;WarmupInclusiveGaps=$external.Value.gaps;WarmupInclusiveDuplicates=$external.Value.duplicates}
                                if (!$samples.Count -or $gaps -ne 0 -or $duplicates -ne 0) { throw "External measured-window PCD gap/duplicate check failed: $name" }
                            }
                            $windowRows | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$runs/$name.receiver-window.json" -Encoding UTF8
                        }
                    }
                    $csv = "$json.frames.csv"
                    if (-not (Test-Path -LiteralPath $csv) -or ([IO.File]::ReadAllLines($csv).Count - 1) -ne $data.samples) { throw "Raw frame sample count mismatch: $name" }
                    $automationIndex = "$runs/$name.automation/index.json"
                    if (-not (Test-Path -LiteralPath $automationIndex)) { throw "Automation completion record missing: $name" }
                    $automation = Get-Content -LiteralPath $automationIndex -Raw -Encoding UTF8 | ConvertFrom-Json
                    if (@($automation.tests).Count -ne 1 -or $automation.tests[0].state -ne 'Success') { throw "Automation did not complete successfully: $name" }
                }
            }
        }
    }
    $manifest.Verdict = Get-Verdict -RunFolder $runs -Threshold $ThresholdPercent
} catch { $failure = $_.Exception.Message }
finally {
    if (-not $KeepCheckouts -and (Test-Path -LiteralPath (Join-Path $owned 'ownership.json'))) {
        $marker = Get-Content -LiteralPath (Join-Path $owned 'ownership.json') -Raw | ConvertFrom-Json
        if ($marker.CreatedBy -eq 'Scripts/run_perf_abab.ps1' -and (Resolve-Path $owned).Path.StartsWith((Join-Path $root 'Saved'))) { Remove-Item -LiteralPath $owned -Recurse -Force }
    }
    $manifest.Failure = $failure
    $manifest.Pass = (-not $failure) -and -not @($manifest.Verdict | Where-Object { -not $_.Pass }).Count
    $manifest.Seconds = ([DateTime]::UtcNow - $started).TotalSeconds
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Write-Host "Perf manifest: $manifestPath"
}
if ($failure) { throw $failure }
if (-not $manifest.Pass) { exit 1 }
