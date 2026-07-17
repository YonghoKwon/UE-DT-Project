param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EditorReviewPath = "",
    [string]$EvidencePath = "",
    [string]$OutputRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Read-JsonFileOrNull {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
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
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpPostEdit"
}
$savedRoot = Join-Path $ProjectRoot "Saved"
New-Item -ItemType Directory -Force -Path $savedRoot | Out-Null
$savedRoot = (Resolve-Path -LiteralPath $savedRoot).Path
$outputRootFullPath = [System.IO.Path]::GetFullPath($OutputRoot)
if (-not $outputRootFullPath.StartsWith($savedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be under ProjectRoot\\Saved because this script declares WritesSavedOnly=true. OutputRoot=$outputRootFullPath SavedRoot=$savedRoot"
}
New-Item -ItemType Directory -Force -Path $outputRootFullPath | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $outputRootFullPath).Path

if ([string]::IsNullOrWhiteSpace($EditorReviewPath)) {
    $EditorReviewPath = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpEditorReview\monitor_wbp_editor_review.json"
}
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpAcceptance\monitor_wbp_acceptance.evidence.json"
}

$wbpRelativePath = "Content\MA0T10\UI\WBP_VirtualSensorMonitorPanel.uasset"
$wbpPath = Join-Path $ProjectRoot $wbpRelativePath
if (-not (Test-Path -LiteralPath $wbpPath -PathType Leaf)) {
    throw "Monitor WBP asset not found: $wbpPath"
}

$currentHash = (Get-FileHash -LiteralPath $wbpPath -Algorithm SHA256).Hash
$editorReview = Read-JsonFileOrNull -Path $EditorReviewPath
$evidence = Read-JsonFileOrNull -Path $EvidencePath

$preEditHash = ""
$backupPath = ""
$backupHash = ""
if ($editorReview) {
    $preEditHash = [string]$editorReview.PreEditAssetHash
    $backupPath = [string]$editorReview.BackupPath
    $backupHash = [string]$editorReview.BackupHash
}

$evidenceAssetHash = ""
if ($evidence) {
    if ($evidence.PSObject.Properties.Name -contains "AssetHash") {
        $evidenceAssetHash = [string]$evidence.AssetHash
    }
    elseif ($evidence.PSObject.Properties.Name -contains "LocalAssetDecisionEvidenceDraft") {
        $evidenceAssetHash = [string]$evidence.LocalAssetDecisionEvidenceDraft.AssetHash
    }
}

$reportJsonPath = Join-Path $OutputRoot "monitor_wbp_post_edit_hash_report.json"
$reportMarkdownPath = Join-Path $OutputRoot "monitor_wbp_post_edit_hash_report.md"
$hashChangedSincePrep = ((-not [string]::IsNullOrWhiteSpace($preEditHash)) -and $preEditHash -ne $currentHash)
$backupExists = ((-not [string]::IsNullOrWhiteSpace($backupPath)) -and (Test-Path -LiteralPath $backupPath -PathType Leaf))
$backupHashStillMatches = $false
if ($backupExists -and -not [string]::IsNullOrWhiteSpace($backupHash)) {
    $backupHashStillMatches = ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -eq $backupHash)
}

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    DryRunOnly = $true
    ModifiesAssets = $false
    StagesFiles = $false
    StagesWbp = $false
    WritesSavedOnly = $true
    WbpRelativePath = $wbpRelativePath
    WbpPath = $wbpPath
    AssetHashAlgorithm = "SHA256"
    CurrentAssetHash = $currentHash
    EditorReviewPath = $EditorReviewPath
    EditorReviewPresent = ($null -ne $editorReview)
    PreEditAssetHash = $preEditHash
    BackupPath = $backupPath
    BackupHash = $backupHash
    BackupExists = $backupExists
    BackupHashStillMatches = $backupHashStillMatches
    EvidencePath = $EvidencePath
    EvidencePresent = ($null -ne $evidence)
    EvidenceAssetHash = $evidenceAssetHash
    ReportJsonPath = $reportJsonPath
    ReportMarkdownPath = $reportMarkdownPath
    ManualEvidenceFieldsToCopy = [PSCustomObject]@{
        AssetHashAlgorithm = "SHA256"
        AssetHash = $currentHash
        PostEditHashReportPath = $reportJsonPath
        PostEditAssetHash = $currentHash
    }
    Summary = [PSCustomObject]@{
        CurrentAssetHash = $currentHash
        PreEditAssetHash = $preEditHash
        HashChangedSinceEditorReviewPrep = $hashChangedSincePrep
        EditorReviewPresent = ($null -ne $editorReview)
        BackupExists = $backupExists
        BackupHashStillMatches = $backupHashStillMatches
        EvidencePresent = ($null -ne $evidence)
        EvidenceAssetHashMatchesCurrent = ((-not [string]::IsNullOrWhiteSpace($evidenceAssetHash)) -and $evidenceAssetHash -eq $currentHash)
        ReadyForEvidenceCopy = (($null -ne $editorReview) -and $backupExists -and $backupHashStillMatches)
        ReadyToStageMonitorWbpAsset = $false
        DryRunOnly = $true
        ModifiesAssets = $false
        StagesFiles = $false
        StagesWbp = $false
        Boundary = "This report records post-edit WBP hash evidence only. It does not edit, accept, or stage WBP_VirtualSensorMonitorPanel.uasset."
        Valid = $true
    }
}

$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportJsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Post-Edit Hash Report") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- WBP path: $($report.WbpPath)") | Out-Null
$lines.Add("- Current SHA256: $($report.CurrentAssetHash)") | Out-Null
$lines.Add("- Pre-edit SHA256: $($report.PreEditAssetHash)") | Out-Null
$lines.Add("- Hash changed since editor review prep: $($report.Summary.HashChangedSinceEditorReviewPrep)") | Out-Null
$lines.Add("- Backup exists: $($report.Summary.BackupExists)") | Out-Null
$lines.Add("- Backup hash still matches: $($report.Summary.BackupHashStillMatches)") | Out-Null
$lines.Add("- Evidence asset hash matches current: $($report.Summary.EvidenceAssetHashMatchesCurrent)") | Out-Null
$lines.Add("- Ready for evidence copy: $($report.Summary.ReadyForEvidenceCopy)") | Out-Null
$lines.Add("- Ready to stage WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Copy Into Acceptance Evidence") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- AssetHashAlgorithm: SHA256") | Out-Null
$lines.Add("- AssetHash: $($report.CurrentAssetHash)") | Out-Null
$lines.Add("- PostEditHashReportPath: $($report.ReportJsonPath)") | Out-Null
$lines.Add("- PostEditAssetHash: $($report.CurrentAssetHash)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null

Write-TextFile -Path $reportMarkdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Monitor WBP post-edit hash report created."
    Write-Host "Report: $reportMarkdownPath"
    Write-Host "Current SHA256: $($report.CurrentAssetHash)"
    Write-Host "Ready for evidence copy: $($report.Summary.ReadyForEvidenceCopy)"
    Write-Host "Ready to stage WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)"
    Write-Host "Modifies assets: $($report.Summary.ModifiesAssets)"
    Write-Host "Stages files: $($report.Summary.StagesFiles)"
}
