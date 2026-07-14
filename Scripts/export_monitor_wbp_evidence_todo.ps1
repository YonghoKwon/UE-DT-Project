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
if (-not (Test-Path -LiteralPath $EvidencePath -PathType Leaf)) {
    throw "EvidencePath not found: $EvidencePath"
}
$EvidencePath = (Resolve-Path -LiteralPath $EvidencePath).Path

$validatorScript = Join-Path $SourceRepoRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
}

$jsonPath = Join-Path $OutputRoot "monitor_wbp_evidence_todo.json"
$markdownPath = Join-Path $OutputRoot "monitor_wbp_evidence_todo.md"
$actions = @($validation.MissingEvidenceActions)
$phaseGroups = @(
    $actions |
        Group-Object -Property EvidencePhase |
        Sort-Object Name |
        ForEach-Object {
            [PSCustomObject]@{
                EvidencePhase = $_.Name
                Count = $_.Count
                BlockingStages = @($_.Group | Group-Object -Property BlockingStage | Sort-Object Name | ForEach-Object {
                    [PSCustomObject]@{
                        BlockingStage = $_.Name
                        Count = $_.Count
                    }
                })
                Actions = @($_.Group)
            }
        }
)

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
    ActionsByPhase = $phaseGroups
    Actions = $actions
    Summary = [PSCustomObject]@{
        TodoCreated = $true
        MissingEvidenceActionCount = $actions.Count
        FailedCheckCount = [int]$validation.Summary.FailedCheckCount
        ReadyToStageMonitorWbpAsset = [bool]$validation.Summary.ReadyToStageMonitorWbpAsset
        MonitorWbpManualAcceptanceComplete = [bool]$validation.Summary.MonitorWbpManualAcceptanceComplete
        WbpAcceptanceEvidenceComplete = [bool]$validation.Summary.Complete
        PhaseCount = $phaseGroups.Count
        Boundary = "This TODO report is generated from read-only WBP acceptance validation. It does not modify assets, stage files, or accept the binary WBP."
        Valid = $true
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Evidence TODO") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Evidence file: $($report.EvidencePath)") | Out-Null
$lines.Add("- Missing evidence action count: $($report.Summary.MissingEvidenceActionCount)") | Out-Null
$lines.Add("- Ready to stage monitor WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)") | Out-Null
$lines.Add("- Manual acceptance complete: $($report.Summary.MonitorWbpManualAcceptanceComplete)") | Out-Null
$lines.Add("") | Out-Null
foreach ($group in $report.ActionsByPhase) {
    $lines.Add("## $($group.EvidencePhase)") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("| Done | Check | Blocking stage | Evidence target | Next action |") | Out-Null
    $lines.Add("| --- | --- | --- | --- | --- |") | Out-Null
    foreach ($action in @($group.Actions)) {
        $lines.Add(("| [ ] | {0} | {1} | {2} | {3} |" -f (Convert-ToMarkdownCell $action.Check), (Convert-ToMarkdownCell $action.BlockingStage), (Convert-ToMarkdownCell $action.EvidenceTarget), (Convert-ToMarkdownCell $action.NextAction))) | Out-Null
    }
    $lines.Add("") | Out-Null
}
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null
Write-TextFile -Path $markdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP evidence TODO exported."
    Write-Host "JSON: $jsonPath"
    Write-Host "Markdown: $markdownPath"
    Write-Host "Missing evidence action count: $($report.Summary.MissingEvidenceActionCount)"
    Write-Host "Ready to stage monitor WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
