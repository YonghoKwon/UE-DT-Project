param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$OutputRoot = "",
    [string]$BackupRoot = "",
    [switch]$SkipBackup,
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
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpEditorReview"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

if ([string]::IsNullOrWhiteSpace($BackupRoot)) {
    $BackupRoot = Join-Path $ProjectRoot "Saved\Backups\MonitorWbp"
}
New-Item -ItemType Directory -Force -Path $BackupRoot | Out-Null
$BackupRoot = (Resolve-Path -LiteralPath $BackupRoot).Path

$wbpRelativePath = "Content\MA0T10\UI\WBP_VirtualSensorMonitor.uasset"
$wbpPath = Join-Path $ProjectRoot $wbpRelativePath
$preflightScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_preflight_report.ps1"
$acceptancePackageScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_acceptance_package.ps1"

$preflight = Invoke-JsonScript -ScriptPath $preflightScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}

$acceptancePackage = Invoke-JsonScript -ScriptPath $acceptancePackageScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}

$assetPresent = Test-Path -LiteralPath $wbpPath -PathType Leaf
if (-not $assetPresent) {
    throw "Monitor WBP asset not found: $wbpPath"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$assetHash = (Get-FileHash -LiteralPath $wbpPath -Algorithm SHA256).Hash
$backupPath = Join-Path $BackupRoot ("WBP_VirtualSensorMonitor.{0}.{1}.uasset" -f $timestamp, $assetHash.Substring(0, 12))
$backupHash = ""
$backupCreated = $false

if (-not $SkipBackup) {
    Copy-Item -LiteralPath $wbpPath -Destination $backupPath -Force
    $backupHash = (Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash
    $backupCreated = $true
}

$reviewJsonPath = Join-Path $OutputRoot "monitor_wbp_editor_review.json"
$reviewMarkdownPath = Join-Path $OutputRoot "monitor_wbp_editor_review.md"
$editorCommand = ('& "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor.exe" "{0}"' -f (Join-Path $ProjectRoot "ma0t10_dt.uproject"))
$acceptanceEvidencePath = [string]$acceptancePackage.EvidencePath

$manualChecklist = @(
    [PSCustomObject]@{ Step = "OpenEditor"; Required = $true; Evidence = "Editor opens project and WBP without load errors." },
    [PSCustomObject]@{ Step = "CompileWidget"; Required = $true; Evidence = "WBP compiles without errors after binding changes." },
    [PSCustomObject]@{ Step = "BindDisplayRows"; Required = $true; Evidence = "TextBlocks bind to GetMonitorDisplayData or individual summary getters, including LazExportText/GetLazExportSummaryText." },
    [PSCustomObject]@{ Step = "BindStateHelpers"; Required = $true; Evidence = "Visibility/enabled rules use IsShowingLidar, HasBoundCamera, HasBoundLidar, and avoid parsing full debug text." },
    [PSCustomObject]@{ Step = "PieSmoke"; Required = $true; Evidence = "PIE confirms camera/LiDAR switching, helper state values, preview policy, payload export, manual export message, slab row, LAZ boundary row, and no crash." },
    [PSCustomObject]@{ Step = "FillEvidenceFile"; Required = $true; Evidence = "Fill monitor_wbp_acceptance.evidence.json sections for DisplayDataScreenMatchEvidence, PieSmokeEvidence, and OwnerAcceptance." },
    [PSCustomObject]@{ Step = "PostEditHash"; Required = $true; Evidence = "Record post-edit SHA256 hash in acceptance evidence." },
    [PSCustomObject]@{ Step = "PostEditValidation"; Required = $true; Evidence = "Run validate_monitor_wbp_acceptance_evidence.ps1 with -FailOnIncompleteEvidence after screenshot/log/export evidence paths are filled." },
    [PSCustomObject]@{ Step = "OwnerAcceptance"; Required = $true; Evidence = "Project owner accepts the binary WBP before staging." }
)

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    DryRunOnly = $false
    ModifiesAssets = $false
    StagesFiles = $false
    StagesWbp = $false
    WritesSavedOnly = $true
    WbpRelativePath = $wbpRelativePath
    WbpPath = $wbpPath
    AssetHashAlgorithm = "SHA256"
    PreEditAssetHash = $assetHash
    BackupRoot = $BackupRoot
    BackupPath = $(if ($backupCreated) { $backupPath } else { "" })
    BackupCreated = $backupCreated
    BackupHash = $backupHash
    BackupHashMatchesPreEditHash = ($backupCreated -and $backupHash -eq $assetHash)
    OutputRoot = $OutputRoot
    ReviewJsonPath = $reviewJsonPath
    ReviewMarkdownPath = $reviewMarkdownPath
    AcceptancePackageRoot = [string]$acceptancePackage.OutputRoot
    AcceptanceEvidencePath = $acceptanceEvidencePath
    EditorCommand = $editorCommand
    PreflightSummary = $preflight.Summary
    AcceptancePackageSummary = $acceptancePackage.Summary
    ManualChecklist = $manualChecklist
    RequiredDisplayDataRows = @("TitleText", "SelectedSensorText", "FrameText", "MeasurementText", "ServerPayloadText", "PreviewText", "SlabText", "LazExportText", "WarningText", "ViewModeText")
    RequiredStateHelperChecks = @("IsShowingLidar", "HasBoundCamera", "HasBoundLidar", "GetLastManualExportMessage")
    FollowUpCommands = @(
        $editorCommand,
        ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EditorReviewPath "{3}" -EvidencePath "{4}"' -f (Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_post_edit_hash_report.ps1"), $ProjectRoot, $SourceRepoRoot, $reviewJsonPath, $acceptanceEvidencePath),
        ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -Json' -f (Join-Path $SourceRepoRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"), $ProjectRoot, $SourceRepoRoot, $acceptanceEvidencePath),
        ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -FailOnIncompleteEvidence' -f (Join-Path $SourceRepoRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"), $ProjectRoot, $SourceRepoRoot, $acceptanceEvidencePath)
    )
    Summary = [PSCustomObject]@{
        ReadyForManualEditorReview = [bool]$preflight.Summary.ReadyForManualEditorReview
        AssetBackupCreated = $backupCreated
        BackupHashMatchesPreEditHash = ($backupCreated -and $backupHash -eq $assetHash)
        AcceptancePackageCreated = [bool]$acceptancePackage.Summary.PackageCreated
        AcceptanceEvidencePath = $acceptanceEvidencePath
        WbpAcceptanceEvidenceComplete = [bool]$acceptancePackage.Summary.WbpAcceptanceEvidenceComplete
        ReadyToStageMonitorWbpAsset = [bool]$acceptancePackage.Summary.ReadyToStageMonitorWbpAsset
        MonitorWbpAssetStageAllowed = [bool]$acceptancePackage.Summary.MonitorWbpAssetStageAllowed
        ManualChecklistCount = $manualChecklist.Count
        RequiredDisplayDataRowCount = 10
        RequiresLazExportTextEvidence = $true
        RequiresStateHelperSmokeEvidence = $true
        RequiresManualExportMessageEvidence = $true
        RequiresEvidenceFileCompletion = $true
        RequiresPostEditStrictValidation = $true
        RequiresPostEditHashReport = $true
        RequiresEditorMediatedAssetEdit = [bool]$preflight.Summary.EditorMediatedAssetEditRequired
        DirectBinaryPatchSupported = [bool]$preflight.Summary.WbpDirectBinaryPatchSupported
        ModifiesAssets = $false
        StagesFiles = $false
        StagesWbp = $false
        WritesSavedOnly = $true
        Boundary = "This script prepares backup and review evidence under Saved only. It does not edit, accept, or stage WBP_VirtualSensorMonitor.uasset."
        Valid = ($backupCreated -and $backupHash -eq $assetHash -and [bool]$acceptancePackage.Summary.PackageCreated)
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reviewJsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Editor Review Prep") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($report.ProjectRoot)") | Out-Null
$lines.Add("- WBP path: $($report.WbpPath)") | Out-Null
$lines.Add("- Pre-edit SHA256: $($report.PreEditAssetHash)") | Out-Null
$lines.Add("- Backup created: $($report.BackupCreated)") | Out-Null
$lines.Add("- Backup path: $($report.BackupPath)") | Out-Null
$lines.Add("- Backup hash matches: $($report.Summary.BackupHashMatchesPreEditHash)") | Out-Null
$lines.Add("- Acceptance evidence path: $($report.AcceptanceEvidencePath)") | Out-Null
$lines.Add("- Ready for manual editor review: $($report.Summary.ReadyForManualEditorReview)") | Out-Null
$lines.Add("- Ready to stage WBP: $($report.Summary.ReadyToStageMonitorWbpAsset)") | Out-Null
$lines.Add("- Required DisplayData rows: $($report.RequiredDisplayDataRows -join ', ')") | Out-Null
$lines.Add("- Required state helper checks: $($report.RequiredStateHelperChecks -join ', ')") | Out-Null
$lines.Add("- Modifies assets: $($report.Summary.ModifiesAssets)") | Out-Null
$lines.Add("- Stages files: $($report.Summary.StagesFiles)") | Out-Null
$lines.Add("- Writes Saved only: $($report.Summary.WritesSavedOnly)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Editor Command") | Out-Null
$lines.Add("") | Out-Null
$lines.Add('```powershell') | Out-Null
$lines.Add($report.EditorCommand) | Out-Null
$lines.Add('```') | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Manual Checklist") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Step | Required | Evidence |") | Out-Null
$lines.Add("| --- | --- | --- |") | Out-Null
foreach ($item in $manualChecklist) {
    $lines.Add(("| {0} | {1} | {2} |" -f (Convert-ToMarkdownCell $item.Step), $item.Required, (Convert-ToMarkdownCell $item.Evidence))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Follow-up Commands") | Out-Null
$lines.Add("") | Out-Null
foreach ($command in $report.FollowUpCommands) {
    $lines.Add('```powershell') | Out-Null
    $lines.Add($command) | Out-Null
    $lines.Add('```') | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null

Write-TextFile -Path $reviewMarkdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP editor review prep created."
    Write-Host "Review report: $reviewMarkdownPath"
    Write-Host "Backup created: $($report.BackupCreated)"
    Write-Host "Backup path: $($report.BackupPath)"
    Write-Host "Backup hash matches: $($report.Summary.BackupHashMatchesPreEditHash)"
    Write-Host "Acceptance evidence: $($report.AcceptanceEvidencePath)"
    Write-Host "Ready for manual editor review: $($report.Summary.ReadyForManualEditorReview)"
    Write-Host "Ready to stage WBP: $($report.Summary.ReadyToStageMonitorWbpAsset)"
    Write-Host "Modifies assets: $($report.Summary.ModifiesAssets)"
    Write-Host "Stages files: $($report.Summary.StagesFiles)"
}
