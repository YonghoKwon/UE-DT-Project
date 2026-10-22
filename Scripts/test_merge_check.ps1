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
function Case([string]$Name, [int]$Expected, [scriptblock]$Mutate) {
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
    $pass = ($actual -eq $Expected -and $summaryMatches)
    $results.Add([PSCustomObject]@{ Name = $Name; Expected = $Expected; Actual = $actual; Pass = $pass })
    Write-Output "$Name expected=$Expected actual=$actual pass=$pass"
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
Case 'wrong-head' 2 { param($r) $r.commits = @($base) }
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
