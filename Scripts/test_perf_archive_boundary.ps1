param([string]$Baseline = 'origin/master', [string]$ReportRoot = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $ReportRoot) { $ReportRoot = Join-Path $root ('Saved/Reports/perf-archive-boundary/' + [Guid]::NewGuid().ToString('N')) }
$owned = [IO.Path]::GetFullPath($ReportRoot)
$allowed = [IO.Path]::GetFullPath((Join-Path $root 'Saved/Reports')).TrimEnd('\') + '\'
if (-not $owned.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $owned)) { throw 'Use a fresh owned Saved/Reports directory.' }
New-Item -ItemType Directory -Path $owned | Out-Null
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run_perf_abab.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Performance script parse failed.' }
foreach ($name in @('Get-Git', 'Prepare-DevicePerfArchive')) {
    $node = @($ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name }, $true))
    if ($node.Count -ne 1) { throw "Expected one function $name" }
    . ([scriptblock]::Create($node[0].Extent.Text))
}
$prepared = Prepare-DevicePerfArchive -Target (Join-Path $owned 'current') -Revision $Baseline -Label 'current'
if ($prepared.DTDevice -notmatch '^[0-9a-f]{40}$') { throw 'Current plugin archive was not prepared.' }
$rejected = $false
try { Prepare-DevicePerfArchive -Target (Join-Path $owned 'legacy') -Revision '6a313ea' -Label 'legacy' | Out-Null }
catch { if ($_.Exception.Message -notlike 'DTDevice gitlink required*') { throw }; $rejected = $true }
if (-not $rejected) { throw 'Legacy baseline was accepted.' }
@{ Pass = $true; CurrentPlugin = $prepared.DTDevice; LegacyRejected = $rejected; NoUnrealExecution = $true } |
    ConvertTo-Json | Set-Content (Join-Path $owned 'manifest.json') -Encoding UTF8
Write-Output 'Perf archive boundary PASS (no Unreal execution).'
