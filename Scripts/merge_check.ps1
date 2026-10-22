# Read-only merge assessment. Writes only merge-check.json beside ResultJson.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ResultJson,
    [Parameter(Mandatory = $true)][string]$BaseCommit,
    [string]$Head = 'HEAD'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = Split-Path -Parent $PSScriptRoot
$blocked = New-Object 'Collections.Generic.List[string]'
$review = New-Object 'Collections.Generic.List[string]'
$gateSummary = New-Object 'Collections.Generic.List[object]'
$manifestSummary = New-Object 'Collections.Generic.List[object]'
$protected = @(); $testChanges = @(); $paths = @(); $baseSha = ''; $headSha = ''
function Has($Value, [string]$Name) { return ($null -ne $Value -and $null -ne $Value.PSObject.Properties[$Name]) }
function Repo-Path([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $root $Path))
}
function Git([string[]]$GitArgs) {
    $lines = @(& git.exe -C $root -c core.quotepath=false @GitArgs)
    if ($LASTEXITCODE -ne 0) { throw "git failed: $($GitArgs -join ' ')" }
    return $lines
}
function Check-Manifest([string]$Gate, [string]$Path) {
    $absolute = Repo-Path $Path
    if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) { $blocked.Add("$Gate manifest missing: $Path"); return }
    try {
        $m = Get-Content -LiteralPath $absolute -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($null -eq $m -or $m -isnot [PSCustomObject]) { throw 'Manifest must be an object' }
        $recognized = $false; $bad = New-Object 'Collections.Generic.List[string]'
        if (Has $m 'Failed') {
            $recognized = $true
            if (($m.Failed -isnot [int] -and $m.Failed -isnot [long]) -or $m.Failed -ne 0) { $bad.Add('Failed must be numeric zero') }
        }
        if (Has $m 'Valid') {
            $recognized = $true
            if ($m.Valid -isnot [bool] -or -not $m.Valid) { $bad.Add('Valid is not true') }
        }
        if (Has $m 'Pass') {
            $recognized = $true
            if ($m.Pass -isnot [bool] -or -not $m.Pass) { $bad.Add('Pass is not true') }
        }
        if (-not $recognized) { $bad.Add('No recognized Failed/Valid/Pass verdict') }
        if ((Has $m 'Failure') -and -not [string]::IsNullOrEmpty([string]$m.Failure)) { $bad.Add('Failure is nonempty') }
        if (Has $m 'FailedNames') {
            if ($m.FailedNames -isnot [Array] -or @($m.FailedNames).Count -ne 0) { $bad.Add('FailedNames is nonempty or malformed') }
        }
        $skippedTests = 0
        foreach ($collection in @('Steps', 'Tests')) {
            if (Has $m $collection) {
                if ($m.$collection -isnot [Array]) { $bad.Add("$collection must be an array"); continue }
                foreach ($entry in @($m.$collection)) {
                    $allowed = if ($collection -eq 'Steps') { @('Success') } else { @('Success','Skip') }
                    if (-not (Has $entry 'State') -or $entry.State -cnotin $allowed) { $bad.Add("$collection contains failed/not-run/unknown State") }
                    elseif ($entry.State -eq 'Skip') { $skippedTests++ }
                }
            }
        }
        if (Has $m 'SkipNames') {
            if ($m.SkipNames -isnot [Array]) { $bad.Add('SkipNames must be an array') }
            else { $skippedTests = [Math]::Max($skippedTests, @($m.SkipNames).Count) }
        }
        if ($skippedTests -gt 0) { $review.Add("$Gate manifest contains $skippedTests skipped tests; confirm opt-in scope") }
        if ((Has $m 'Commit') -and $m.Commit -ne $headSha) { $review.Add("$Gate manifest Commit differs from requested head") }
        foreach ($reason in $bad) { $blocked.Add("$Gate manifest: $reason ($Path)") }
        $manifestSummary.Add([PSCustomObject]@{ Gate = $Gate; Path = $absolute; Valid = ($bad.Count -eq 0) })
    } catch { $blocked.Add("$Gate manifest invalid JSON/structure: $Path") }
}
try {
    $ResultJson = Repo-Path $ResultJson
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'validate_agent_result.ps1') -ResultJson $ResultJson
    if ($LASTEXITCODE -ne 0) { throw 'result.json schema validation failed' }
    $r = Get-Content -LiteralPath $ResultJson -Raw -Encoding UTF8 | ConvertFrom-Json
    $baseSha = (Git @('rev-parse','--verify',"$BaseCommit^{commit}")) -join ''
    $headSha = (Git @('rev-parse','--verify',"$Head^{commit}")) -join ''
    $declaredBase = (Git @('rev-parse','--verify',"$($r.base_commit)^{commit}")) -join ''
    if ($declaredBase -ne $baseSha) { $blocked.Add('result base_commit differs from -BaseCommit') }
    if (@($r.commits).Count -gt 0) {
        $last = (Git @('rev-parse','--verify',"$($r.commits[-1])^{commit}")) -join ''
        if ($last -ne $headSha) {
            & git.exe -C $root merge-base --is-ancestor $last $headSha
            $ancestorExit = $LASTEXITCODE
            if ($ancestorExit -eq 0) {
                $count = (Git @('rev-list','--count',"$last..$headSha")) -join ''
                $subjects = (Git @('log','--reverse','--format=%s',"$last..$headSha")) -join '; '
                $review.Add("HEAD has $count commits after reported commits: $subjects")
            } elseif ($ancestorExit -eq 1) { $blocked.Add('Last reported commit is not an ancestor of requested head') }
            else { throw 'Cannot check reported commit ancestry' }
        }
    }
    $branchHead = @(& git.exe -C $root rev-parse --verify --quiet "refs/heads/$($r.branch)^{commit}") -join ''
    if ($LASTEXITCODE -ne 0) {
        $branchHead = @(& git.exe -C $root rev-parse --verify --quiet "refs/remotes/origin/$($r.branch)^{commit}") -join ''
        if ($LASTEXITCODE -ne 0) { $branchHead = ''; $review.Add('branch ref not found locally') }
    }
    if ($branchHead -and $branchHead -ne $headSha) { $blocked.Add('Reported branch does not resolve to requested head') }
    foreach ($property in $r.gates.PSObject.Properties) {
        $name = $property.Name; $gate = $property.Value
        $gateSummary.Add([PSCustomObject]@{ Gate = $name; Status = $gate.status })
        switch ($gate.status) {
            'FAIL' { $blocked.Add("Gate FAIL: $name") }
            'NOT_RUN' { $review.Add("Gate NOT_RUN: $name - $($gate.note)") }
            'SKIPPED' { $review.Add("Gate SKIPPED: $name - $($gate.note)") }
        }
        if ($gate.manifest) { Check-Manifest $name $gate.manifest }
    }
    if ($r.gates.perf.status -eq 'PASS' -and $r.gates.perf.rt_mode -ne 'ProjectDefault') { $review.Add('Perf PASS does not establish RT ProjectDefault gate') }
    foreach ($m in @($r.measurements)) {
        if ($m.status -eq 'FAIL') { $blocked.Add("Measurement FAIL: $($m.name)") }
        elseif ($m.status -in @('SKIPPED','NOT_RUN')) { $review.Add("Measurement $($m.status): $($m.name)") }
        if ($m.status -eq 'PASS') {
            $meets = switch ($m.threshold.operator) {
                '>=' { $m.value -ge $m.threshold.value }
                '<=' { $m.value -le $m.threshold.value }
                '==' { $m.value -eq $m.threshold.value }
            }
            if (-not $meets) { $blocked.Add("Measurement PASS contradicts threshold: $($m.name)") }
        }
    }
    $paths = @(Git @('diff','--no-renames','--name-only',"$baseSha...$headSha",'--'))
    $protected = @($paths | Where-Object { $_ -match '^(Plugins/DTCore(?:/|$)|Config/Game\.ini$|Samples/PixelStreaming(?:/|$)|CLAUDE\.md$|docs/IMPROVEMENTS_CLAUDE\.md$|Saved/Reports/Inspection(?:/|$)|Content/MA0T10/Maps/SensorTestMap\.umap$)' })
    foreach ($path in $protected) { $blocked.Add("Protected path changed: $path") }
    foreach ($path in @($paths | Where-Object { $_ -match '(?i)\.(umap|uasset)$' -and $_ -notin $protected })) {
        $review.Add("asset change requires review: $path")
    }
    if (@($paths | Where-Object { $_ -match '^(Source|Config|Content|Shaders)/' }).Count -gt 0 -and
        -not ($r.gates.perf.status -eq 'PASS' -and $r.gates.perf.rt_mode -eq 'ProjectDefault')) {
        $review.Add('runtime change without RT ProjectDefault perf PASS')
    }
    $testChanges = @($paths | Where-Object { $_ -match '^Scripts/(verify|run_perf_abab|merge_check|test_merge_check|validate_[^/]+)\.ps1$' -or $_ -match '(?i)(Tests\.cpp$|/Tests/|fixture|Tools/agent/schema/)' })
    foreach ($path in $testChanges) { $review.Add("Test/gate/expectation change requires independent review: $path") }
    if ($r.secrets_exposed) { $blocked.Add('secrets_exposed=true; inspect secrets_note without reproducing credentials') }
    if (-not $r.personal_settings_restored) { $review.Add('Personal settings not confirmed restored') }
    foreach ($decision in @($r.decisions_needed)) { $review.Add("Decision needed: $decision") }
    foreach ($pending in @($r.not_done)) { $review.Add("Not done: $pending") }
} catch { $blocked.Add($_.Exception.Message) }
$code = if ($blocked.Count) { 2 } elseif ($review.Count) { 1 } else { 0 }
$verdict = @('MERGE_CANDIDATE','USER_REVIEW_REQUIRED','BLOCKED')[$code]
$summary = [ordered]@{
    TaskId = $(if (Get-Variable r -ErrorAction SilentlyContinue) { $r.task_id } else { '' })
    Verdict = $verdict; ExitCode = $code; BaseCommit = $baseSha; Head = $headSha
    Gates = @($gateSummary.ToArray()); Manifests = @($manifestSummary.ToArray())
    ChangedPaths = @($paths); ProtectedPaths = @($protected); ReviewPaths = @($testChanges)
    Blocked = @($blocked.ToArray()); Review = @($review.ToArray())
    Note = 'Committed diff and reported evidence only; independent human review is required. No build/Editor/merge executed.'
}
try {
    $output = Join-Path (Split-Path -Parent $ResultJson) 'merge-check.json'
    $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $output -Encoding UTF8
} catch { Write-Output 'BLOCKED: cannot write merge-check.json'; exit 2 }
Write-Output "Merge check: $verdict (exit $code)"
foreach ($gate in $gateSummary) { Write-Output "Gate $($gate.Gate): $($gate.Status)" }
foreach ($reason in $blocked) { Write-Output "BLOCK: $reason" }
foreach ($reason in ($review | Select-Object -Unique)) { Write-Output "REVIEW: $reason" }
Write-Output "Summary: $output"
exit $code
