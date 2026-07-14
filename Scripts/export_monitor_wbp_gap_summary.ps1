param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [string]$OutputRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "Required script not found: $ScriptPath"
    }

    $output = @(& powershell -ExecutionPolicy Bypass -File $ScriptPath @Parameters -Json)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Convert-ToMarkdownCell {
    param([object]$Value)
    if ($null -eq $Value) { return "" }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "ProjectRoot not found: $ProjectRoot"
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ([string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
    $SourceRepoRoot = Split-Path -Parent $PSScriptRoot
}
if (-not (Test-Path -LiteralPath $SourceRepoRoot -PathType Container)) {
    throw "SourceRepoRoot not found: $SourceRepoRoot"
}
$SourceRepoRoot = (Resolve-Path -LiteralPath $SourceRepoRoot).Path

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpAcceptance"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
if (-not $OutputRoot.StartsWith((Join-Path $ProjectRoot "Saved"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be under ProjectRoot\Saved: $OutputRoot"
}

if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $OutputRoot "monitor_wbp_acceptance.evidence.json"
}

$packageScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_acceptance_package.ps1"
$todoScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_evidence_todo.ps1"
$runbookScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_acceptance_runbook.ps1"
$validatorScript = Join-Path $SourceRepoRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"

$package = Invoke-JsonScript -ScriptPath $packageScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    OutputRoot = $OutputRoot
}
$todo = Invoke-JsonScript -ScriptPath $todoScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
    OutputRoot = $OutputRoot
}
$runbook = Invoke-JsonScript -ScriptPath $runbookScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    OutputRoot = $OutputRoot
}
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
}

$actions = @($validation.MissingEvidenceActions)
$phaseSummaries = @(
    $actions |
        Group-Object -Property EvidencePhase |
        Sort-Object -Property @{ Expression = "Count"; Descending = $true }, @{ Expression = "Name"; Ascending = $true } |
        ForEach-Object {
            $first = @($_.Group | Select-Object -First 1)
            [PSCustomObject]@{
                EvidencePhase = [string]$_.Name
                MissingActionCount = [int]$_.Count
                FirstBlockingStage = if ($first.Count -gt 0) { [string]$first[0].BlockingStage } else { "" }
                FirstEvidenceTarget = if ($first.Count -gt 0) { [string]$first[0].EvidenceTarget } else { "" }
                FirstNextAction = if ($first.Count -gt 0) { [string]$first[0].NextAction } else { "" }
            }
        }
)
$nextAction = if ($phaseSummaries.Count -gt 0) { [string]$phaseSummaries[0].FirstNextAction } else { "WBP acceptance evidence is complete; run the strict validator before staging." }

$jsonPath = Join-Path $OutputRoot "monitor_wbp_gap_summary.json"
$markdownPath = Join-Path $OutputRoot "monitor_wbp_gap_summary.md"
$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
    OutputRoot = $OutputRoot
    JsonPath = $jsonPath
    MarkdownPath = $markdownPath
    DryRunOnly = $true
    ModifiesAssets = $false
    StagesFiles = $false
    StagesWbp = $false
    PackageSummary = $package.Summary
    TodoSummary = $todo.Summary
    RunbookSummary = $runbook.Summary
    ValidationSummary = $validation.Summary
    PhaseSummaries = $phaseSummaries
    Summary = [PSCustomObject]@{
        GapSummaryCreated = $true
        MissingEvidenceActionCount = $actions.Count
        MissingPhaseCount = $phaseSummaries.Count
        ReadyToStageMonitorWbpAsset = [bool]$validation.Summary.ReadyToStageMonitorWbpAsset
        MonitorWbpManualAcceptanceComplete = [bool]$validation.Summary.MonitorWbpManualAcceptanceComplete
        NextManualAction = $nextAction
        PackagePath = [string]$package.MarkdownPath
        TodoPath = [string]$todo.MarkdownPath
        RunbookPath = [string]$runbook.MarkdownPath
        Boundary = "This summary only refreshes Saved/Reports WBP acceptance artifacts. It does not modify assets, stage files, or accept the binary WBP."
        Valid = $true
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Gap Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Evidence file: $($report.EvidencePath)") | Out-Null
$lines.Add("- Missing evidence actions: $($report.Summary.MissingEvidenceActionCount)") | Out-Null
$lines.Add("- Missing evidence phases: $($report.Summary.MissingPhaseCount)") | Out-Null
$lines.Add("- Ready to stage WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)") | Out-Null
$lines.Add("- Manual acceptance complete: $($report.Summary.MonitorWbpManualAcceptanceComplete)") | Out-Null
$lines.Add("- Next manual action: $($report.Summary.NextManualAction)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Phase Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Phase | Missing actions | First blocking stage | First evidence target | First next action |") | Out-Null
$lines.Add("| --- | --- | --- | --- | --- |") | Out-Null
foreach ($phase in $phaseSummaries) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} |" -f `
        (Convert-ToMarkdownCell $phase.EvidencePhase),
        (Convert-ToMarkdownCell $phase.MissingActionCount),
        (Convert-ToMarkdownCell $phase.FirstBlockingStage),
        (Convert-ToMarkdownCell $phase.FirstEvidenceTarget),
        (Convert-ToMarkdownCell $phase.FirstNextAction))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Generated Artifacts") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Package: $($report.Summary.PackagePath)") | Out-Null
$lines.Add("- TODO: $($report.Summary.TodoPath)") | Out-Null
$lines.Add("- Runbook: $($report.Summary.RunbookPath)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null
Write-TextFile -Path $markdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP gap summary exported."
    Write-Host "JSON: $jsonPath"
    Write-Host "Markdown: $markdownPath"
    Write-Host "Missing evidence actions: $($report.Summary.MissingEvidenceActionCount)"
    Write-Host "Next manual action: $($report.Summary.NextManualAction)"
    Write-Host "Ready to stage WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
