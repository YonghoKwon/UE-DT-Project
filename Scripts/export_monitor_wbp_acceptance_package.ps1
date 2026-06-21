param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$SourceRepoRoot = "",
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

$preflightScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_preflight_report.ps1"
$decisionScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_decision_report.ps1"
$templateScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_acceptance_template.ps1"
$validatorScript = Join-Path $SourceRepoRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"

$preflightJsonPath = Join-Path $OutputRoot "monitor_wbp_preflight.json"
$preflightMarkdownPath = Join-Path $OutputRoot "monitor_wbp_preflight.md"
$decisionJsonPath = Join-Path $OutputRoot "monitor_wbp_decision.json"
$decisionMarkdownPath = Join-Path $OutputRoot "monitor_wbp_decision.md"
$evidenceJsonPath = Join-Path $OutputRoot "monitor_wbp_acceptance.evidence.json"
$evidenceMarkdownPath = Join-Path $OutputRoot "monitor_wbp_acceptance_template.md"
$validationJsonPath = Join-Path $OutputRoot "monitor_wbp_acceptance_validation.json"
$manifestJsonPath = Join-Path $OutputRoot "monitor_wbp_acceptance_package.json"
$manifestMarkdownPath = Join-Path $OutputRoot "README.md"

$preflight = Invoke-JsonScript -ScriptPath $preflightScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    JsonPath = $preflightJsonPath
    MarkdownPath = $preflightMarkdownPath
}
$decision = Invoke-JsonScript -ScriptPath $decisionScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    JsonPath = $decisionJsonPath
    MarkdownPath = $decisionMarkdownPath
}
$template = Invoke-JsonScript -ScriptPath $templateScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    JsonPath = $evidenceJsonPath
    MarkdownPath = $evidenceMarkdownPath
}
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $evidenceJsonPath
}

$manualSteps = @(
    "Open Unreal Editor 5.3 with $ProjectRoot\m7at10_dt.uproject.",
    "Open Content/M7AT10/UI/WBP_VirtualSensorMonitor and verify it loads and compiles without errors.",
    "Compare optional bindings against docs/widget_designer_setup.md and mark missing optional widgets as crash-safe only after testing.",
    "Run PIE in the intended production or smoke-test map with AVirtualSensorMonitorHostActor or Level Blueprint binding.",
    "After any WBP save in Unreal Editor, run export_monitor_wbp_post_edit_hash_report.ps1 and copy AssetHash/PostEditHashReportPath into the evidence file.",
    "Attach log, screenshot, and exported payload evidence to monitor_wbp_acceptance.evidence.json.",
    "Set Production WBP acceptance to AcceptedForRepository only after the project owner accepts the binary asset.",
    "Re-run validate_monitor_wbp_acceptance_evidence.ps1 with -FailOnIncompleteEvidence before staging the WBP."
)

$followUpCommands = @(
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}"' -f (Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_post_edit_hash_report.ps1"), $ProjectRoot, $SourceRepoRoot, $evidenceJsonPath),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -Json' -f $validatorScript, $ProjectRoot, $SourceRepoRoot, $evidenceJsonPath),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -FailOnIncompleteEvidence' -f $validatorScript, $ProjectRoot, $SourceRepoRoot, $evidenceJsonPath),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -RequireAcceptedLocalDecisionEvidence' -f (Join-Path $SourceRepoRoot "Scripts\invoke_local_decision_precommit_gate.ps1"), $ProjectRoot, $SourceRepoRoot)
)

$manifest = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    OutputRoot = $OutputRoot
    DryRunOnly = $true
    ModifiesAssets = $false
    StagesFiles = $false
    StagesWbp = $false
    EvidencePath = $evidenceJsonPath
    Files = [PSCustomObject]@{
        PreflightJson = $preflightJsonPath
        PreflightMarkdown = $preflightMarkdownPath
        DecisionJson = $decisionJsonPath
        DecisionMarkdown = $decisionMarkdownPath
        EvidenceJson = $evidenceJsonPath
        EvidenceMarkdown = $evidenceMarkdownPath
        ValidationJson = $validationJsonPath
        ManifestJson = $manifestJsonPath
        ManifestMarkdown = $manifestMarkdownPath
    }
    PreflightSummary = $preflight.Summary
    DecisionSummary = $decision.Summary
    TemplateSummary = $template.Summary
    ValidationSummary = $validation.Summary
    MissingEvidenceActions = @($validation.MissingEvidenceActions)
    ManualSteps = $manualSteps
    FollowUpCommands = $followUpCommands
    Summary = [PSCustomObject]@{
        PackageCreated = $true
        ReadyForManualEditorReview = [bool]$preflight.Summary.ReadyForManualEditorReview
        WbpAcceptanceEvidenceComplete = [bool]$validation.Summary.Complete
        WbpAcceptanceMissingCount = [int]$validation.Summary.FailedCheckCount
        MissingEvidenceActionCount = [int]$validation.Summary.MissingEvidenceActionCount
        TopMissingEvidenceActions = @($validation.Summary.TopMissingEvidenceActions)
        MonitorWbpAssetPresent = [bool]$validation.Summary.MonitorWbpAssetPresent
        MonitorWbpAssetTracked = [bool]$validation.Summary.MonitorWbpAssetTracked
        MonitorWbpAssetStageAllowed = [bool]$validation.Summary.MonitorWbpAssetStageAllowed
        ReadyToStageMonitorWbpAsset = [bool]$validation.Summary.ReadyToStageMonitorWbpAsset
        EditorManualAcceptancePresent = [bool]$validation.Summary.EditorManualAcceptancePresent
        MonitorWbpManualAcceptanceComplete = [bool]$validation.Summary.MonitorWbpManualAcceptanceComplete
        WbpDirectBinaryPatchSupported = [bool]$preflight.Summary.WbpDirectBinaryPatchSupported
        EditorMediatedAssetEditRequired = [bool]$preflight.Summary.EditorMediatedAssetEditRequired
        CodexCanModifyNativeBindings = [bool]$preflight.Summary.CodexCanModifyNativeBindings
        RequiresBackupBeforeAssetEdit = [bool]$preflight.Summary.RequiresBackupBeforeAssetEdit
        RequiresEditorCompileAndSaveEvidence = [bool]$preflight.Summary.RequiresEditorCompileAndSaveEvidence
        RequiresPieSmokeAfterEdit = [bool]$preflight.Summary.RequiresPieSmokeAfterEdit
        RequiresPostEditHashReport = $true
        RequiresOwnerAcceptanceBeforeStage = [bool]$preflight.Summary.RequiresOwnerAcceptanceBeforeStage
        ManualAcceptanceSectionCount = [int]$validation.Summary.ManualAcceptanceSectionCount
        AcceptedManualAcceptanceSectionCount = [int]$validation.Summary.AcceptedManualAcceptanceSectionCount
        ManualAcceptanceSections = @($validation.Summary.ManualAcceptanceSections)
        EvidenceTemplateCreated = (Test-Path -LiteralPath $evidenceJsonPath -PathType Leaf)
        ValidationReportCreated = $true
        DryRunOnly = $true
        ModifiesAssets = $false
        StagesFiles = $false
        StagesWbp = $false
        Boundary = "This package is a local Saved/Reports artifact for manual Editor/PIE evidence collection. It never modifies assets, never stages files, and does not accept the binary WBP by itself."
        Valid = $true
    }
}

$validation | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $validationJsonPath -Encoding UTF8
$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestJsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Acceptance Package") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($manifest.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($manifest.ProjectRoot)") | Out-Null
$lines.Add("- Source repo: $($manifest.SourceRepoRoot)") | Out-Null
$lines.Add("- Output root: $($manifest.OutputRoot)") | Out-Null
$lines.Add("- Evidence file: $($manifest.EvidencePath)") | Out-Null
$lines.Add("- Ready for manual editor review: $($manifest.Summary.ReadyForManualEditorReview)") | Out-Null
$lines.Add("- WBP acceptance evidence complete: $($manifest.Summary.WbpAcceptanceEvidenceComplete)") | Out-Null
$lines.Add("- Missing acceptance check count: $($manifest.Summary.WbpAcceptanceMissingCount)") | Out-Null
$lines.Add("- Missing evidence action count: $($manifest.Summary.MissingEvidenceActionCount)") | Out-Null
$lines.Add("- Monitor WBP asset present: $($manifest.Summary.MonitorWbpAssetPresent)") | Out-Null
$lines.Add("- Monitor WBP asset tracked: $($manifest.Summary.MonitorWbpAssetTracked)") | Out-Null
$lines.Add("- Monitor WBP asset stage allowed: $($manifest.Summary.MonitorWbpAssetStageAllowed)") | Out-Null
$lines.Add("- Ready to stage monitor WBP asset: $($manifest.Summary.ReadyToStageMonitorWbpAsset)") | Out-Null
$lines.Add("- Editor manual acceptance present: $($manifest.Summary.EditorManualAcceptancePresent)") | Out-Null
$lines.Add("- Monitor WBP manual acceptance complete: $($manifest.Summary.MonitorWbpManualAcceptanceComplete)") | Out-Null
$lines.Add("- Direct binary patch supported: $($manifest.Summary.WbpDirectBinaryPatchSupported)") | Out-Null
$lines.Add("- Editor-mediated asset edit required: $($manifest.Summary.EditorMediatedAssetEditRequired)") | Out-Null
$lines.Add("- Codex can modify native bindings: $($manifest.Summary.CodexCanModifyNativeBindings)") | Out-Null
$lines.Add("- Requires backup before asset edit: $($manifest.Summary.RequiresBackupBeforeAssetEdit)") | Out-Null
$lines.Add("- Manual acceptance sections: $($manifest.Summary.ManualAcceptanceSections -join ', ')") | Out-Null
$lines.Add("- Accepted manual acceptance sections: $($manifest.Summary.AcceptedManualAcceptanceSectionCount)/$($manifest.Summary.ManualAcceptanceSectionCount)") | Out-Null
$lines.Add("- Dry run only: $($manifest.DryRunOnly)") | Out-Null
$lines.Add("- Modifies assets: $($manifest.ModifiesAssets)") | Out-Null
$lines.Add("- Stages files: $($manifest.StagesFiles)") | Out-Null
$lines.Add("- Stages WBP: $($manifest.StagesWbp)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Files") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Name | Path |") | Out-Null
$lines.Add("| --- | --- |") | Out-Null
foreach ($property in $manifest.Files.PSObject.Properties) {
    $lines.Add(("| {0} | {1} |" -f (Convert-ToMarkdownCell $property.Name), (Convert-ToMarkdownCell $property.Value))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Missing Evidence Actions") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Check | Evidence target | Next action |") | Out-Null
$lines.Add("| --- | --- | --- |") | Out-Null
foreach ($action in @($manifest.MissingEvidenceActions | Select-Object -First 12)) {
    $lines.Add(("| {0} | {1} | {2} |" -f (Convert-ToMarkdownCell $action.Check), (Convert-ToMarkdownCell $action.EvidenceTarget), (Convert-ToMarkdownCell $action.NextAction))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Manual Steps") | Out-Null
$lines.Add("") | Out-Null
for ($index = 0; $index -lt $manualSteps.Count; $index++) {
    $lines.Add(("{0}. {1}" -f ($index + 1), $manualSteps[$index])) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Follow-up Commands") | Out-Null
$lines.Add("") | Out-Null
foreach ($command in $followUpCommands) {
    $lines.Add('```powershell') | Out-Null
    $lines.Add($command) | Out-Null
    $lines.Add('```') | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($manifest.Summary.Boundary)") | Out-Null

Write-TextFile -Path $manifestMarkdownPath -Lines $lines

if ($Json) {
    $manifest | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP acceptance package created."
    Write-Host "Output root: $($manifest.OutputRoot)"
    Write-Host "Evidence file: $($manifest.EvidencePath)"
    Write-Host "Ready for manual editor review: $($manifest.Summary.ReadyForManualEditorReview)"
    Write-Host "WBP acceptance evidence complete: $($manifest.Summary.WbpAcceptanceEvidenceComplete)"
    Write-Host "Missing acceptance check count: $($manifest.Summary.WbpAcceptanceMissingCount)"
    Write-Host "Missing evidence action count: $($manifest.Summary.MissingEvidenceActionCount)"
    Write-Host "Monitor WBP asset present: $($manifest.Summary.MonitorWbpAssetPresent)"
    Write-Host "Monitor WBP asset tracked: $($manifest.Summary.MonitorWbpAssetTracked)"
    Write-Host "Monitor WBP asset stage allowed: $($manifest.Summary.MonitorWbpAssetStageAllowed)"
    Write-Host "Ready to stage monitor WBP asset: $($manifest.Summary.ReadyToStageMonitorWbpAsset)"
    Write-Host "Editor manual acceptance present: $($manifest.Summary.EditorManualAcceptancePresent)"
    Write-Host "Monitor WBP manual acceptance complete: $($manifest.Summary.MonitorWbpManualAcceptanceComplete)"
    Write-Host "Dry run only: $($manifest.DryRunOnly)"
    Write-Host "Modifies assets: $($manifest.ModifiesAssets)"
    Write-Host "Stages files: $($manifest.StagesFiles)"
}
