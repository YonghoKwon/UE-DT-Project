# Isolated synthetic git repository under Saved; never builds or touches product protected paths.
[CmdletBinding()]
param([string]$ReportRoot = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = Split-Path -Parent $PSScriptRoot
if (-not $ReportRoot) { $ReportRoot = Join-Path $root ('Saved/Reports/AGENT-HARNESS-01/self-test-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')) }
$ReportRoot = [IO.Path]::GetFullPath($ReportRoot)
$fixture = Join-Path $ReportRoot 'fixture'
if (Test-Path -LiteralPath $fixture) { throw 'Fixture already exists; use a fresh report directory' }
New-Item -ItemType Directory -Path "$fixture/Scripts", "$fixture/Tools/agent/schema", "$fixture/cases" -Force | Out-Null
foreach ($name in @('merge_check.ps1','validate_agent_result.ps1')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination "$fixture/Scripts/$name" }
Copy-Item -LiteralPath "$root/Tools/agent/schema/result.schema.json" -Destination "$fixture/Tools/agent/schema/result.schema.json"
function Git([string[]]$GitArgs) {
    $lines = @(& git.exe -C $fixture @GitArgs)
    if ($LASTEXITCODE -ne 0) { throw "Fixture git failed: $($GitArgs -join ' ')" }
    return $lines
}
Git @('init','--initial-branch=fixture') | Out-Null
Git @('config','user.name','Agent Harness Fixture') | Out-Null
Git @('config','user.email','fixture@example.invalid') | Out-Null
Git @('config','commit.gpgsign','false') | Out-Null
Git @('config','core.autocrlf','false') | Out-Null
Git @('config','core.hooksPath',"$fixture/no-hooks") | Out-Null
Git @('add','--','Scripts/merge_check.ps1','Scripts/validate_agent_result.ps1','Tools/agent/schema/result.schema.json') | Out-Null
Git @('commit','-m','fixture baseline') | Out-Null
$base = (Git @('rev-parse','HEAD')) -join ''
'Harmless fixture content' | Set-Content -LiteralPath "$fixture/note.txt"
Git @('add','--','note.txt') | Out-Null
Git @('commit','-m','fixture candidate') | Out-Null
function New-Result {
    $gates = [ordered]@{}
    foreach ($name in @('quick','feature','release','perf','layer','dtcore_guard','protected_paths')) {
        $gates[$name] = [ordered]@{ status = 'PASS'; manifest = ''; note = 'Synthetic fixture only' }
        if ($name -eq 'perf') { $gates[$name].rt_mode = 'ProjectDefault' }
    }
    return [ordered]@{
        task_id = 'FIXTURE'; branch = 'fixture'; commits = @((Git @('rev-parse','HEAD')) -join ''); base_commit = $base
        gates = $gates; measurements = @(); not_done = @(); known_limits = @('Synthetic; not product acceptance')
        decisions_needed = @(); artifacts = @(); personal_settings_restored = $true; secrets_exposed = $false; secrets_note = 'No credentials used'
    }
}
$results = New-Object 'Collections.Generic.List[object]'
function Case([string]$Name, [int]$Expected, [scriptblock]$Mutate, [scriptblock]$Inspect = {}) {
    $r = New-Result
    & $Mutate $r
    $directory = Join-Path $fixture "cases/$Name"
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $path = Join-Path $directory 'result.json'
    $r | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $path -Encoding UTF8
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$fixture/Scripts/merge_check.ps1" -ResultJson $path -BaseCommit $base *> "$directory/output.txt"
    $actual = $LASTEXITCODE
    $summaryPath = Join-Path $directory 'merge-check.json'
    $summaryMatches = $false
    if (Test-Path -LiteralPath $summaryPath) { $summaryMatches = (Get-Content -LiteralPath $summaryPath -Raw -Encoding UTF8 | ConvertFrom-Json).ExitCode -eq $Expected }
    $detail = ''
    if ($summaryMatches) {
        try { & $Inspect (Get-Content -LiteralPath $summaryPath -Raw -Encoding UTF8 | ConvertFrom-Json) }
        catch { $detail = $_.Exception.Message; $summaryMatches = $false }
    }
    $pass = ($actual -eq $Expected -and $summaryMatches)
    $results.Add([PSCustomObject]@{ Name = $Name; Expected = $Expected; Actual = $actual; Pass = $pass; Detail = $detail })
    Write-Output "$Name expected=$Expected actual=$actual pass=$pass $detail"
}
Case 'pass' 0 { param($r) }
Case 'failed-gate' 2 { param($r) $r.gates.feature.status = 'FAIL' }
Case 'not-run' 1 { param($r) $r.gates.feature.status = 'NOT_RUN' }
Case 'skipped' 1 { param($r) $r.gates.feature.status = 'SKIPPED' }
Case 'secrets' 2 { param($r) $r.secrets_exposed = $true }
Case 'settings' 1 { param($r) $r.personal_settings_restored = $false }
Case 'decision' 1 { param($r) $r.decisions_needed = @('Review synthetic decision') }
Case 'rt-off' 1 { param($r) $r.gates.perf.rt_mode = 'Off' }
Case 'invalid-schema' 2 { param($r) $r.gates.quick.status = 'Success' }
Case 'missing-manifest' 2 { param($r) $r.gates.quick.manifest = 'absent.json' }
'{"Failed":0}' | Set-Content -LiteralPath "$fixture/good.json" -Encoding UTF8
Case 'valid-manifest' 0 { param($r) $r.gates.quick.manifest = 'good.json' }
'{"Valid":true,"FailedNames":[],"Steps":[{"State":"Success"}],"Tests":[]}' | Set-Content -LiteralPath "$fixture/verify.json" -Encoding UTF8
Case 'verify-manifest' 0 { param($r) $r.gates.quick.manifest = 'verify.json' }
'{"Failed":1}' | Set-Content -LiteralPath "$fixture/failed.json" -Encoding UTF8
Case 'failed-manifest' 2 { param($r) $r.gates.quick.manifest = 'failed.json' }
'{}' | Set-Content -LiteralPath "$fixture/empty.json" -Encoding UTF8
Case 'empty-manifest' 2 { param($r) $r.gates.quick.manifest = 'empty.json' }
'{"Valid":true,"FailedNames":[],"Steps":[],"Tests":[{"State":"NotRun"}]}' | Set-Content -LiteralPath "$fixture/notrun.json" -Encoding UTF8
Case 'manifest-notrun-test' 2 { param($r) $r.gates.quick.manifest = 'notrun.json' }
'{"Valid":true,"Tests":[{"State":"Skip"}]}' | Set-Content -LiteralPath "$fixture/skip.json" -Encoding UTF8
Case 'manifest-skip-test' 1 { param($r) $r.gates.feature.manifest = 'skip.json' }
Case 'threshold-contradiction' 2 {
    param($r)
    $r.measurements = @(@{ name = 'synthetic Hz'; condition = @{ map='fixture'; resolution='none'; rhi='none'; rt='none'; transport='none'; sensors='none' }; value=9; unit='Hz'; threshold=@{operator='>=';value=10};status='PASS' })
}
Case 'wrong-base' 2 { param($r) $r.base_commit = (Git @('rev-parse','HEAD')) -join '' }
Case 'reported-ancestor' 1 { param($r) $r.commits = @($base) } {
    param($summary)
    if (@($summary.Review | Where-Object { $_ -eq 'HEAD has 1 commits after reported commits: fixture candidate' }).Count -ne 1) { throw 'Missing ancestor count/subject review' }
}
$divergent = (Git @('commit-tree',"$base^{tree}",'-p',$base,'-m','fixture divergent report')) -join ''
Case 'reported-nonancestor' 2 { param($r) $r.commits = @($divergent) } {
    param($summary)
    if ($summary.Blocked -notcontains 'Last reported commit is not an ancestor of requested head') { throw 'Missing nonancestor block' }
}
Git @('update-ref','refs/remotes/origin/remote-only','HEAD') | Out-Null
Case 'remote-only-branch' 0 { param($r) $r.branch = 'remote-only' }
Case 'missing-branch' 1 { param($r) $r.branch = 'missing-local-and-origin' } {
    param($summary)
    if ($summary.Review -notcontains 'branch ref not found locally') { throw 'Missing branch review' }
}
Git @('update-ref','refs/remotes/origin/wrong-remote',$base) | Out-Null
Case 'wrong-remote-head' 2 { param($r) $r.branch = 'wrong-remote' }
# Diverged base has its own protected-path change; only branch-side note.txt belongs to the diff.
$originalBase = $base
Git @('switch','-c','aheadbase',$base) | Out-Null
New-Item -ItemType Directory -Path "$fixture/Config" -Force | Out-Null
'; synthetic base-only change' | Set-Content -LiteralPath "$fixture/Config/Game.ini"
Git @('add','--','Config/Game.ini') | Out-Null
Git @('commit','-m','fixture master advances independently') | Out-Null
$base = (Git @('rev-parse','HEAD')) -join ''
Git @('switch','fixture') | Out-Null
Case 'merge-base-diff' 0 { param($r) } {
    param($summary)
    if (@($summary.ChangedPaths).Count -ne 1 -or $summary.ChangedPaths -notcontains 'note.txt') { throw 'Diff includes changes from base-side history' }
}
$base = $originalBase
# INFO/unfinished measurements have distinct requirements; gate status still rejects INFO.
function Measurement([string]$Status) {
    return @{ name='synthetic measurement';condition=@{map='fixture';resolution='none';rhi='none';rt='none';transport='none';sensors='none'};unit='Hz';status=$Status }
}
Case 'measurement-not-run-omitted' 1 { param($r) $r.measurements = @((Measurement 'NOT_RUN')) }
Case 'measurement-skipped-omitted' 1 { param($r) $r.measurements = @((Measurement 'SKIPPED')) }
Case 'measurement-info' 0 { param($r) $m=Measurement 'INFO';$m.value=123;$r.measurements=@($m) }
Case 'measurement-info-threshold-rejected' 2 { param($r) $m=Measurement 'INFO';$m.value=123;$m.threshold=@{operator='>=';value=10};$r.measurements=@($m) }
Case 'measurement-info-value-required' 2 { param($r) $r.measurements=@((Measurement 'INFO')) }
Case 'measurement-pass-fields-required' 2 { param($r) $r.measurements=@((Measurement 'PASS')) }
Case 'measurement-fail-fields-required' 2 { param($r) $r.measurements=@((Measurement 'FAIL')) }
Case 'measurement-pending-value-type' 2 { param($r) $m=Measurement 'NOT_RUN';$m.value='unknown';$r.measurements=@($m) }
Case 'gate-info-rejected' 2 { param($r) $r.gates.quick.status='INFO' }
'{"Valid":true,"Tests":[{"State":"Skip"},{"State":"Success"},{"State":"Skip"}],"SkipNames":["a","b"]}' | Set-Content -LiteralPath "$fixture/multi-skip.json" -Encoding UTF8
Case 'manifest-skips-counted-once' 1 { param($r) $r.gates.feature.manifest='multi-skip.json' } {
    param($summary)
    $lines=@($summary.Review | Where-Object { $_ -match 'skipped tests|SkipNames' })
    if ($lines.Count -ne 1 -or $lines[0] -ne 'feature manifest contains 2 skipped tests; confirm opt-in scope') { throw 'Skip review duplicated or count incorrect' }
}
'{"Valid":true,"SkipNames":["a","b","c"]}' | Set-Content -LiteralPath "$fixture/skipnames-only.json" -Encoding UTF8
Case 'manifest-skipnames-only' 1 { param($r) $r.gates.release.manifest='skipnames-only.json' } {
    param($summary)
    if ($summary.Review -notcontains 'release manifest contains 3 skipped tests; confirm opt-in scope') { throw 'SkipNames-only count lost' }
}
Case 'tools-only-no-perf-review' 1 { param($r) $r.gates.perf.status='NOT_RUN' } {
    param($summary)
    if ($summary.Review -contains 'runtime change without RT ProjectDefault perf PASS') { throw 'Tools-only change incorrectly needs runtime perf review' }
}
# Rule text in AGENTS must agree with the prohibition on Codex submodule updates.
$agents = Get-Content -LiteralPath "$root/AGENTS.md" -Raw -Encoding UTF8
$policyOK = $agents.Contains('병렬 worktree는 Claude가 생성 시 Plugins/DTCore를 고정 gitlink 커밋으로 초기화한다.') -and $agents.Contains('Codex는 submodule을 update/변경하지 않는다.') -and $agents.Contains('미초기화 상태면 quick 실패를 한계로 기록.') -and $agents.Contains('4. push·PR·merge·worktree 생성·submodule update는 하지 않는다.')
$results.Add([PSCustomObject]@{Name='parallel-dtcore-policy';Expected=$true;Actual=$policyOK;Pass=$policyOK;Detail=''})
Write-Output "parallel-dtcore-policy pass=$policyOK"
# All runtime path classes need an independent perf review unless RT ProjectDefault passed.
foreach ($path in @('Source/Fixture.cpp','Config/Fixture.ini','Shaders/Fixture.usf','Content/Review/Fixture.uasset','Content/Review/Fixture.umap')) {
    New-Item -ItemType Directory -Path (Split-Path -Parent "$fixture/$path") -Force | Out-Null
    'synthetic content' | Set-Content -LiteralPath "$fixture/$path"
    Git @('add','--',$path) | Out-Null
    Git @('commit','-m',"fixture runtime $path") | Out-Null
    $caseName = 'runtime-' + [IO.Path]::GetFileName($path)
    Case $caseName 1 { param($r) $r.gates.perf.status='NOT_RUN' } {
        param($summary)
        if ($summary.Review -notcontains 'runtime change without RT ProjectDefault perf PASS') { throw 'Runtime perf review missing' }
        if ($path -match '\.(uasset|umap)$' -and $summary.Review -notcontains "asset change requires review: $path") { throw 'Asset incorrectly blocked or not reviewed' }
    }
    # Isolate the next path class from earlier runtime changes.
    $base = (Git @('rev-parse','HEAD')) -join ''
}
# Reuse just the latest asset change for review cases.
$base = (Git @('rev-parse','HEAD^')) -join ''
Case 'asset-review-with-perf-pass' 1 { param($r) } {
    param($summary)
    if ($summary.Review -notcontains 'asset change requires review: Content/Review/Fixture.umap') { throw 'Asset review missing' }
    if ($summary.Review -contains 'runtime change without RT ProjectDefault perf PASS') { throw 'RT ProjectDefault PASS incorrectly reviewed' }
}
Case 'runtime-rt-off-review' 1 { param($r) $r.gates.perf.rt_mode='Off' } {
    param($summary)
    if ($summary.Review -notcontains 'runtime change without RT ProjectDefault perf PASS') { throw 'RT off accepted for runtime gate' }
}
$base = (Git @('rev-parse','HEAD')) -join ''
New-Item -ItemType Directory -Path "$fixture/Content/MA0T10/Maps" -Force | Out-Null
'synthetic protected map' | Set-Content -LiteralPath "$fixture/Content/MA0T10/Maps/SensorTestMap.umap"
Git @('add','--','Content/MA0T10/Maps/SensorTestMap.umap') | Out-Null
Git @('commit','-m','fixture protected sensor map') | Out-Null
Case 'sensor-map-block' 2 { param($r) } {
    param($summary)
    if ($summary.ProtectedPaths -notcontains 'Content/MA0T10/Maps/SensorTestMap.umap') { throw 'SensorTestMap not protected' }
}
$base = (Git @('rev-parse','HEAD')) -join ''
'# synthetic gate change' | Set-Content -LiteralPath "$fixture/Scripts/run_perf_abab.ps1"
Git @('add','--','Scripts/run_perf_abab.ps1') | Out-Null
Git @('commit','-m','fixture gate modification') | Out-Null
Case 'gate-review' 1 { param($r) }
New-Item -ItemType Directory -Path "$fixture/Config" -Force | Out-Null
'; synthetic fixture, no credential' | Set-Content -LiteralPath "$fixture/Config/Game.ini"
Git @('add','--','Config/Game.ini') | Out-Null
Git @('commit','-m','fixture protected path') | Out-Null
Case 'protected-path' 2 { param($r) }
$failed = @($results | Where-Object { -not $_.Pass }).Count
[ordered]@{ Failed=$failed; Passed=$results.Count-$failed; Tests=@($results.ToArray()); Synthetic=$true } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $ReportRoot 'manifest.json') -Encoding UTF8
Write-Output "Self-test: $($results.Count-$failed) passed / $failed failed. Report: $ReportRoot"
if ($failed) { exit 1 }
exit 0
