param([string]$ReportRoot = '')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/PerfRelativeGate.ps1"
if (!$ReportRoot) { $ReportRoot = Join-Path (Split-Path $PSScriptRoot) ('Saved/Reports/LIDAR-WALLCLOCK-28b/gate-fixtures/' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')) }
New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
$results = @()
function Row([double]$Fps = 58, [double]$P95 = 18, [double]$Gt = 5, [double[]]$Ratios = @(.999, .999, .999)) {
    [PSCustomObject]@{ Valid = $true; Fps = $Fps; P95 = $P95; GameThread = $Gt; Gpu=5; Rss=1024; AcquisitionRatios = $Ratios; File = 'synthetic' }
}
foreach ($case in 'below59-pass', 'fps-regression', 'p95-regression', 'gt-regression', 'gpu-regression', 'rss-regression', 'ratio-boundary', 'ratio-fail', 'zero-hz', 'missing-sensor', 'nan-ratio', 'infinite-metric', 'invalid-run', 'incomplete', 'median-outlier', 'reference-ratio-fail') {
    $a = @(Row; Row; Row; Row; Row; Row)
    $b = @(Row; Row; Row; Row; Row; Row)
    $expected = $case -in @('below59-pass', 'ratio-boundary', 'median-outlier', 'reference-ratio-fail')
    switch ($case) {
        'fps-regression' { $b | ForEach-Object { $_.Fps = 56 } }
        'p95-regression' { $b | ForEach-Object { $_.P95 = 18.4 } }
        'gpu-regression' { $b | ForEach-Object { $_.Gpu=5.11 } }
        'rss-regression' { $b | ForEach-Object { $_.Rss=1045 } }
        'gt-regression' { $b | ForEach-Object { $_.GameThread = 5.11 } }
        'ratio-boundary' { $b[5].AcquisitionRatios = @(.99, 1, 1) }
        'ratio-fail' { $b[5].AcquisitionRatios = @(.989999, 1, 1) }
        'zero-hz' { $b[0].AcquisitionRatios = @(0, 1, 1) }
        'missing-sensor' { $b[0].AcquisitionRatios = @() }
        'nan-ratio' { $b[0].AcquisitionRatios = @([double]::NaN, 1, 1) }
        'infinite-metric' { $b[0].Fps = [double]::PositiveInfinity }
        'invalid-run' { $b[0].Valid = $false }
        'incomplete' { $b = @($b | Select-Object -First 5) }
        'median-outlier' { $b[0].Fps = 20; $b[0].P95 = 99; $b[0].GameThread = 50 }
        'reference-ratio-fail' { $a[0].AcquisitionRatios = @(.90, .90, .90) }
    }
    $verdict = Get-PerfRelativeComparison -A $a -B $b -Expected 6 -RequireAcquisitionRatio
    if ($verdict.Pass -ne $expected) { throw "Fixture $case expected=$expected actual=$($verdict.Pass): $($verdict.Failures -join ', ')" }
    $results += @{ Case = $case; Pass = $true; ExpectedGatePass = $expected; Verdict = $verdict; Synthetic = $true }
}
foreach ($case in 'scheduled-complete', 'scheduled-low-ratio', 'scheduled-short-sensor-list', 'scheduled-regression', 'scheduled-wrong-slot', 'scheduled-missing-run') {
    $folder = Join-Path $ReportRoot $case
    New-Item -ItemType Directory -Path $folder | Out-Null
    for ($block = 1; $block -le 3; $block++) {
        for ($index = 1; $index -le 4; $index++) {
            $label = if ($index % 2) { 'A' } else { 'B' }
            if ($case -eq 'scheduled-missing-run' -and $block -eq 3 -and $index -eq 4) { continue }
            $ratios = if ($label -eq 'A') { @(.90, .90, .90) } else { @(.999, .999, .999) }
            if ($case -eq 'scheduled-low-ratio' -and $label -eq 'B') { $ratios[0] = .94 }
            if ($case -eq 'scheduled-short-sensor-list') { $ratios = @(.999, .999) }
            $data = @{ valid = $true; average_fps = 58; engine_frame_p95_ms = 18; game_thread_mean_ms = 5; gpu_frame_mean_ms=5; rss_bytes=1024; sensors = @($ratios | ForEach-Object { @{ acquisition_ratio = $_ } }) }
            if ($case -eq 'scheduled-regression' -and $label -eq 'B') { $data.game_thread_mean_ms = 5.11 }
            $slot = if ($case -eq 'scheduled-wrong-slot' -and $index -eq 4) { 6 } else { $index }
            $data | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $folder "lidar3-visible-block$block-$slot-$label.json") -Encoding UTF8
        }
    }
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/run_perf_abab.ps1" -Workload ScheduledLidar -RayTracing ProjectDefault -SummarizeOnly $folder *> (Join-Path $ReportRoot "$case.log")
    $expectedExit = if ($case -eq 'scheduled-complete') { 0 } else { 1 }
    if ($LASTEXITCODE -ne $expectedExit) { throw "Cohort fixture $case expected exit$expectedExit actual$LASTEXITCODE" }
    $results += @{ Case = $case; Pass = $true; ExpectedExit = $expectedExit; Synthetic = $true }
}
$results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $ReportRoot 'fixtures.json') -Encoding UTF8
Write-Output "$($results.Count) synthetic relative/ratio fixtures passed; no build or measured performance."
