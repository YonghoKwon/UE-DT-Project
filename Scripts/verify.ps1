param(
    [ValidateSet('quick', 'feature', 'release', 'perf')][string]$Level = 'quick',
    [string[]]$Tests = @('MA0T10.SensorReceiver'),
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.3',
    [string]$BaseCommit = 'HEAD^',
    [ValidatePattern('^[12](,[12])?$')][string]$PerfPairs = '1',
    [ValidateSet('Visible', 'Offscreen', 'Both')][string]$PerfDisplay = 'Visible',
    [ValidateRange(30, 60)][int]$PerfMeasurementSeconds = 60
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$started = [DateTime]::UtcNow
$report = Join-Path $root ('Saved/Reports/verify/' + $started.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Force -Path $report | Out-Null
$steps = [Collections.Generic.List[object]]::new()
$testResults = [Collections.Generic.List[object]]::new()
$protectedPaths = @('Config/Game.ini', 'Samples/PixelStreaming', 'Content/MA0T10/Maps/SensorTestMap.umap', 'Plugins/DTCore')
function Get-GitLines([string[]]$Arguments) {
    $lines = @(& git.exe -C $root @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "git failed: $($Arguments -join ' ')" }
    return $lines
}
function Invoke-Step([string]$Name, [scriptblock]$Action) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $state = 'Success'; $reason = ''
    try { & $Action *> (Join-Path $report ($Name + '.log')) }
    catch { $state = 'Failed'; $reason = $_.Exception.Message; throw }
    finally {
        $steps.Add([PSCustomObject]@{ Name = $Name; State = $state; Seconds = $watch.Elapsed.TotalSeconds; Reason = $reason })
        Write-Host "$Name $state ($([Math]::Round($watch.Elapsed.TotalSeconds, 2))s)"
    }
}
function Protected-Hashes {
    $hashes = @{}
    foreach ($path in $protectedPaths | Where-Object { $_ -ne 'Plugins/DTCore' }) {
        $absolute = Join-Path $root $path
        if (Test-Path -LiteralPath $absolute) {
            Get-ChildItem -LiteralPath $absolute -Recurse -File | ForEach-Object { $hashes[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        }
    }
    return $hashes
}
function Read-Tests {
    foreach ($index in Get-ChildItem -LiteralPath $report -Filter index.json -Recurse -File) {
        $data = Get-Content -LiteralPath $index.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        if (@($data.tests).Count -eq 0) { throw "Empty test report: $($index.FullName)" }
        foreach ($test in $data.tests) {
            $reasons = @($test.entries | ForEach-Object { $_.event.message } | Where-Object { $_ -match '\b(skip|skipped|skipping)\b|\[OPT-IN SKIP\]' })
            $state = if ($test.state -eq 'Success' -and $reasons.Count -gt 0) { 'Skip' } else { $test.state }
            $variables = @($reasons | ForEach-Object { [regex]::Matches($_, 'MA0T10_[A-Z0-9_]+') } | ForEach-Object { $_.Value } | Sort-Object -Unique)
            $testResults.Add([PSCustomObject]@{ Name = $test.fullTestPath; State = $state; Reasons = $reasons; Variables = $variables; Report = $index.FullName })
        }
    }
}
$before = Protected-Hashes
$saveRoot = Join-Path $root 'Saved/SaveGames'
$saveBackup = Join-Path $report 'personal-save-backup'
$originalSaves = @()
$sourceFingerprint = @{}
$failure = ''
$head = (Get-GitLines -Arguments @('rev-parse', 'HEAD')) -join ''
$submodule = (Get-GitLines -Arguments @('-C', 'Plugins/DTCore', 'rev-parse', 'HEAD')) -join ''
try {
    Invoke-Step 'diff-check' { & git -C $root diff --check; if ($LASTEXITCODE -ne 0) { throw 'Worktree whitespace check failed' }; & git -C $root diff --cached --check; if ($LASTEXITCODE -ne 0) { throw 'Index whitespace check failed' } }
    Invoke-Step 'dtcore-guard' { & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/validate_dtcore_submodule_guard.ps1" -FailOnViolation -Json; if ($LASTEXITCODE -ne 0) { throw 'DTCore guard failed' } }
    Invoke-Step 'layer-gate' { & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/validate_architecture_layers.ps1" -Json; if ($LASTEXITCODE -ne 0) { throw 'Layer gate failed' } }
    Invoke-Step 'protected-paths' {
        $staged = @(Get-GitLines -Arguments (@('diff', '--cached', '--name-only', '--') + $protectedPaths))
        $committed = @(Get-GitLines -Arguments (@('diff', '--name-only', $BaseCommit, 'HEAD', '--') + $protectedPaths))
        if ($staged.Count -or $committed.Count) { throw "Protected paths staged/committed: $(@($staged + $committed) -join ', ')" }
        Write-Output 'Existing unstaged user changes are preserved; protected index/commit paths are absent.'
    }
    foreach ($script in @('validate_payload_contract', 'validate_payload_fixtures', 'validate_opt_in_inventory', 'validate_sensor_ui_vocabulary')) {
        Invoke-Step $script { & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/$script.ps1" -Json; if ($LASTEXITCODE -ne 0) { throw "$script failed" } }
    }
    Invoke-Step 'perf-cohort-fixtures' {
        & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/validate_perf_cohort.ps1" -ReportRoot "$report/perf-cohort-fixtures"
        if ($LASTEXITCODE -ne 0) { throw 'Synthetic perf cohort gate regression failed' }
    }
    if ($Level -eq 'perf') {
        Invoke-Step 'perf-abab' {
            & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/run_perf_abab.ps1" -Baseline $BaseCommit -Candidate HEAD -Pairs $PerfPairs -Display $PerfDisplay -MeasurementSeconds $PerfMeasurementSeconds -EngineRoot $EngineRoot -ReportRoot "$report/perf"
            if ($LASTEXITCODE -ne 0) { throw 'Perf ABAB gate failed' }
        }
    }
    if ($Level -in @('feature', 'release')) {
        if (Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue) { throw 'Close Editor and Live Coding before feature/release' }
        if (Test-Path -LiteralPath $saveRoot) {
            $originalSaves = @(Get-ChildItem -LiteralPath $saveRoot -Recurse -File)
            Copy-Item -LiteralPath $saveRoot -Destination $saveBackup -Recurse
        }
        Get-ChildItem -LiteralPath (Join-Path $root 'Source') -Recurse -File | ForEach-Object { $sourceFingerprint[$_.FullName.Substring($root.Length + 1)] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        $fixture = Join-Path $report 'committed-map'
        Invoke-Step 'committed-map-fixture' {
            & git -C $root archive --format=zip "--output=$report/map.zip" HEAD Content/MA0T10/Maps/SensorTestMap.umap
            if ($LASTEXITCODE -ne 0) { throw 'Committed map extraction failed' }
            Expand-Archive -LiteralPath (Join-Path $report 'map.zip') -DestinationPath $fixture
        }
        Invoke-Step 'editor-build' {
            & "$EngineRoot/Engine/Build/BatchFiles/Build.bat" ma0t10_dtEditor Win64 Development "-Project=$root/ma0t10_dt.uproject" -WaitMutex -NoHotReloadFromIDE
            if ($LASTEXITCODE -ne 0) { throw 'Editor build failed' }
        }
        Invoke-Step 'feature-tests' {
            & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/run_smoke_tests.ps1" -ProjectPath "$root/ma0t10_dt.uproject" -EngineRoot $EngineRoot -SkipBuild -Rhi D3D12 -ReportRoot "$report/feature" -TestGroups ($Tests -join '+') -SensorSmokeMapDirectory "$fixture/Content/MA0T10/Maps"
            if ($LASTEXITCODE -ne 0) { throw 'Feature tests failed' }
        }
        if ($Level -eq 'release') {
            Invoke-Step 'full-d3d12' {
                & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/run_smoke_tests.ps1" -ProjectPath "$root/ma0t10_dt.uproject" -EngineRoot $EngineRoot -SkipBuild -Rhi D3D12 -ReportRoot "$report/release" -TestGroups MA0T10 -SensorSmokeMapDirectory "$fixture/Content/MA0T10/Maps"
                if ($LASTEXITCODE -ne 0) { throw 'Full D3D12 tests failed' }
            }
            Invoke-Step 'wbp-memory-compile' {
                $widgetArgs = @("`"$root/ma0t10_dt.uproject`"", '/Engine/Maps/Entry', '-run=pythonscript', "`"-script=$PSScriptRoot/validate_sensor_tool_widgets.py`"", '-NullRHI', '-unattended', '-nosplash', '-nop4', "`"-abslog=$report/widgets.log`"")
                $process = Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -ArgumentList $widgetArgs -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput "$report/widgets-console.log" -RedirectStandardError "$report/widgets-stderr.log"
                if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath "$report/widgets.log" -SimpleMatch 'all seven compiled; assets were not saved' -Quiet)) { throw 'WBP memory compilation failed or did not finish' }
            }
            Invoke-Step 'ui-vocabulary-rendered' {
                $inventory = Join-Path $root 'Saved/Reports/UXR/UXR-07/visible-text.json'
                if (-not (Test-Path -LiteralPath $inventory) -or (Get-Item -LiteralPath $inventory).LastWriteTimeUtc -lt $started) { throw 'Current release did not produce the owned UI text inventory' }
                & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/validate_sensor_ui_vocabulary.ps1" -RenderedTextReport $inventory -RequireRenderedText -Json
                if ($LASTEXITCODE -ne 0) { throw 'Owned UI vocabulary failed' }
                Copy-Item -LiteralPath $inventory -Destination (Join-Path $report 'ui-visible-text.json')
            }
        }
    }
} catch { $failure = $_.Exception.Message }
finally {
    try { Read-Tests; if (@($testResults | Where-Object { $_.State -notin @('Success', 'Skip') }).Count) { throw 'Automation contains Failed/NotRun tests' } }
    catch { if (-not $failure) { $failure = $_.Exception.Message } }
    try {
        if (Test-Path -LiteralPath $saveBackup) {
            Get-ChildItem -LiteralPath $saveBackup -Recurse -File | ForEach-Object {
                $destination = Join-Path $saveRoot $_.FullName.Substring($saveBackup.Length + 1)
                Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
                if ((Get-FileHash -LiteralPath $_.FullName).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) { throw 'SaveGame restore mismatch' }
            }
        }
        $after = Protected-Hashes
        if ($before.Count -ne $after.Count -or @($before.Keys | Where-Object { $after[$_] -ne $before[$_] }).Count) { throw 'Protected worktree hashes changed' }
    } catch { if (-not $failure) { $failure = $_.Exception.Message } }
    $inventoryLog = Join-Path $report 'validate_opt_in_inventory.log'
    $envNames = if (Test-Path -LiteralPath $inventoryLog) { try { (Get-Content $inventoryLog -Raw -Encoding UTF8 | ConvertFrom-Json).Names } catch { @() } } else { @() }
    $manifest = [ordered]@{
        SchemaVersion = 1; Level = $Level; StartedUtc = $started.ToString('o'); Seconds = ([DateTime]::UtcNow - $started).TotalSeconds
        Commit = $head; Branch = ((Get-GitLines -Arguments @('branch', '--show-current')) -join ''); SubmoduleCommit = $submodule; ProtectedCommitBase = $BaseCommit
        Worktree = @(Get-GitLines -Arguments @('status', '--porcelain=v1')); SourceHashes = $sourceFingerprint; TestsRequested = $Tests
        Environment = @{ PowerShell = $PSVersionTable.PSVersion.ToString(); OS = [Environment]::OSVersion.ToString(); EngineRoot = $EngineRoot; Engine = (Get-Content "$EngineRoot/Engine/Build/Build.version" -Raw | ConvertFrom-Json); Rhi = $(if ($Level -eq 'quick') { 'None' } else { 'D3D12' }); RenderOffscreen = $(if ($Level -eq 'perf') { $PerfDisplay } else { $Level -ne 'quick' }); OptIn = @($envNames | ForEach-Object { @{ Name = $_; Set = (-not [string]::IsNullOrEmpty([Environment]::GetEnvironmentVariable($_))) } }) }
        Steps = @($steps.ToArray()); Tests = @($testResults.ToArray()); ExecutedSuccessNames = @($testResults | Where-Object State -eq Success | ForEach-Object Name); FailedNames = @($testResults | Where-Object { $_.State -notin @('Success','Skip') } | ForEach-Object Name); SkipNames = @($testResults | Where-Object State -eq Skip | ForEach-Object Name)
        ProtectedHashCount = $before.Count; PersonalSavesRestored = $originalSaves.Count; Failure = $failure; Valid = (-not $failure)
    }
    $manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $report 'manifest.json') -Encoding UTF8
    Write-Host "Manifest: $report/manifest.json"
}
if ($failure) { throw $failure }
