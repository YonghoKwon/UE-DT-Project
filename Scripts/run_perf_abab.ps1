param(
    [string]$Baseline = 'HEAD^',
    [string]$Candidate = 'HEAD',
    [ValidatePattern('^[12](,[12])?$')][string]$Pairs = '1',
    [ValidateSet('Visible', 'Offscreen', 'Both')][string]$Display = 'Visible',
    [ValidateRange(1, 10)][int]$Blocks = 3,
    [ValidateRange(30, 60)][int]$MeasurementSeconds = 60,  # PERF01 instrument clamps to 30..60
    [double]$ThresholdPercent = 2.0,
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.3',
    [string]$ReportRoot = '',
    [string]$SummarizeOnly = '',
    [switch]$KeepCheckouts,
    [switch]$DryRun
)
# QA-02 standard performance gate: archive checkouts of two revisions, the same committed
# PERF-01 instrument in both, ABAB x Blocks per condition, per-revision run medians.
# FPS decrease, engine p95 increase and game thread increase are judged separately.
$ErrorActionPreference = 'Stop'
$pairList = @($Pairs -split ',' | ForEach-Object { [int]$_ } | Sort-Object -Unique)
$root = Split-Path -Parent $PSScriptRoot
$instrument = 'Source/ma0t10_dtEditor/Private/Perf01AcquisitionTests.cpp'
$map = 'Content/MA0T10/Maps/Tests/SlabScenarioValidationMap.umap'
$editor = "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
$build = "$EngineRoot/Engine/Build/BatchFiles/Build.bat"

function Get-Git([string[]]$Arguments, [string]$Repo = $root) {
    $out = @(& git.exe -C $Repo @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "git failed: $($Arguments -join ' ')" }
    return ($out -join "`n").Trim()
}
function Get-Median([double[]]$Values) {
    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) { return [double]::NaN }
    $mid = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$mid] }
    return ($sorted[$mid - 1] + $sorted[$mid]) / 2
}
function Get-Sha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }

function Get-Verdict([string]$RunFolder, [double]$Threshold) {
    $rows = @(Get-ChildItem -LiteralPath $RunFolder -Filter '*.json' -File | ForEach-Object {
        $data = Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($_.BaseName -notmatch '^(?<cond>.+)-block\d+-\d+-(?<rev>[AB])$') { return }
        [PSCustomObject]@{ Condition = $Matches.cond; Revision = $Matches.rev; Valid = [bool]$data.valid; Fps = [double]$data.average_fps; P95 = [double]$data.engine_frame_p95_ms; GameThread = [double]$data.game_thread_mean_ms; File = $_.Name }
    })
    if ($rows.Count -eq 0) { throw "No ABAB run JSON in $RunFolder" }
    $conditions = foreach ($group in ($rows | Group-Object Condition)) {
        $a = @($group.Group | Where-Object Revision -eq 'A'); $b = @($group.Group | Where-Object Revision -eq 'B')
        $invalid = @($group.Group | Where-Object { -not $_.Valid } | ForEach-Object File)
        $fpsA = Get-Median ($a.Fps); $fpsB = Get-Median ($b.Fps)
        $p95A = Get-Median ($a.P95); $p95B = Get-Median ($b.P95)
        $gtA = Get-Median ($a.GameThread); $gtB = Get-Median ($b.GameThread)
        $fpsDrop = if ($fpsA) { ($fpsA - $fpsB) / $fpsA * 100 } else { [double]::NaN }
        $p95Rise = if ($p95A) { ($p95B - $p95A) / $p95A * 100 } else { [double]::NaN }
        $gtRise = if ($gtA) { ($gtB - $gtA) / $gtA * 100 } else { [double]::NaN }
        $failures = @()
        if ($a.Count -ne $b.Count -or $a.Count -eq 0) { $failures += "unbalanced samples A=$($a.Count) B=$($b.Count)" }
        if ($invalid.Count) { $failures += "invalid runs: $($invalid -join ', ')" }
        if ($fpsDrop -gt $Threshold) { $failures += ('FPS -{0:N3}%' -f $fpsDrop) }
        if ($p95Rise -gt $Threshold) { $failures += ('engine p95 +{0:N3}%' -f $p95Rise) }
        if ($gtRise -gt $Threshold) { $failures += ('game thread +{0:N3}%' -f $gtRise) }
        [PSCustomObject][ordered]@{
            Condition = $group.Name; SamplesPerRevision = $a.Count
            FpsA = $fpsA; FpsB = $fpsB; FpsDropPercent = $fpsDrop
            EngineP95A = $p95A; EngineP95B = $p95B; EngineP95RisePercent = $p95Rise
            GameThreadA = $gtA; GameThreadB = $gtB; GameThreadRisePercent = $gtRise
            Pass = ($failures.Count -eq 0); Failures = $failures
        }
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
    Map = '/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap'; Viewport = '1920x1080'; Rhi = 'D3D12'
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
        $manifest[$label + 'MapSha256'] = Get-Sha256 (Join-Path $target $map)
    }
    # Same fixture workload on both sides, or the gate compares different scenes.
    if ($manifest.AMapSha256 -ne $manifest.BMapSha256) { throw "SlabScenarioValidationMap differs between baseline and candidate (A=$($manifest.AMapSha256) B=$($manifest.BMapSha256)); measurement refused" }
    foreach ($label in 'A', 'B') {
        $target = Join-Path $owned $label
        $log = Join-Path $ReportRoot "$label-editor-build.log"
        & $build ma0t10_dtEditor Win64 Development "-Project=$target/ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE *> $log
        if ($LASTEXITCODE -ne 0) { throw "Editor build $label failed: $log" }
    }
    foreach ($pairCount in $pairList) {
        foreach ($offscreen in $displays) {
            $condition = "${pairCount}pair-" + $(if ($offscreen) { 'offscreen' } else { 'visible' })
            for ($block = 1; $block -le $Blocks; $block++) {
                $index = 0
                foreach ($label in 'A', 'B', 'A', 'B') {
                    $index++
                    $name = "$condition-block$block-$index-$label"
                    $json = Join-Path $runs "$name.json"
                    $arguments = @("`"$owned/$label/ma0t10_dt.uproject`"", '/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap', '-unattended', '-nop4', '-nosplash', '-windowed', '-NoVSync', '-NoSound', '-ResX=1920', '-ResY=1080', '-d3d12', '-Perf01', "-Perf01Pairs=$pairCount", "-Perf01Seconds=$MeasurementSeconds", "`"-Perf01Output=$json`"", "`"-abslog=$runs/$name.log`"", '"-ExecCmds=t.MaxFPS 0,r.VSync 0,r.VSyncEditor 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.PERF01.AcquisitionAndPreview;Quit"', '"-TestExit=Automation Test Queue Empty"')
                    if ($offscreen) { $arguments += '-RenderOffscreen' }
                    $saved = @{}
                    Get-ChildItem Env: | Where-Object Name -like 'MA0T10_*' | ForEach-Object { $saved[$_.Name] = $_.Value; Remove-Item "Env:$($_.Name)" }
                    $env:MA0T10_ACCEPTANCE_RES_X = '1920'; $env:MA0T10_ACCEPTANCE_RES_Y = '1080'
                    try {
                        Write-Host "Run $name"
                        $process = Start-Process -FilePath $editor -ArgumentList $arguments -Wait -PassThru -RedirectStandardOutput "$runs/$name.console.log" -RedirectStandardError "$runs/$name.stderr.log"
                    } finally {
                        Remove-Item Env:MA0T10_ACCEPTANCE_RES_X, Env:MA0T10_ACCEPTANCE_RES_Y -ErrorAction SilentlyContinue
                        foreach ($key in $saved.Keys) { Set-Item "Env:$key" $saved[$key] }
                    }
                    if (-not (Test-Path -LiteralPath $json)) { throw "No measurement for $name (exit $($process.ExitCode))" }
                    $data = Get-Content -LiteralPath $json -Raw -Encoding UTF8 | ConvertFrom-Json
                    Write-Host ('{0}: fps={1:N3} p95={2:N3} gt={3:N3} valid={4}' -f $name, $data.average_fps, $data.engine_frame_p95_ms, $data.game_thread_mean_ms, $data.valid)
                    if ($process.ExitCode -ne 0 -or -not $data.valid) { throw "Invalid run $name" }
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
