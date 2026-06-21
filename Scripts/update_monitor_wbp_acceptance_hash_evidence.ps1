param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [string]$PostEditHashReportPath = "",
    [switch]$DryRun,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Set-JsonProperty {
    param(
        [Parameter(Mandatory = $true)][object]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [object]$Value
    )

    if ($Object.PSObject.Properties.Name -contains $Name) {
        $Object.$Name = $Value
    }
    else {
        $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value
    }
}

function Find-RequiredEvidenceItem {
    param(
        [object]$Evidence,
        [string]$Name
    )

    if ($null -eq $Evidence -or -not ($Evidence.PSObject.Properties.Name -contains "RequiredEvidence")) {
        return $null
    }

    return @($Evidence.RequiredEvidence | Where-Object { [string]$_.Name -eq $Name } | Select-Object -First 1)
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

if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpAcceptance\monitor_wbp_acceptance.evidence.json"
}
if ([string]::IsNullOrWhiteSpace($PostEditHashReportPath)) {
    $PostEditHashReportPath = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpPostEdit\monitor_wbp_post_edit_hash_report.json"
}

if (-not (Test-Path -LiteralPath $EvidencePath -PathType Leaf)) {
    throw "EvidencePath not found: $EvidencePath"
}
if (-not (Test-Path -LiteralPath $PostEditHashReportPath -PathType Leaf)) {
    throw "PostEditHashReportPath not found: $PostEditHashReportPath"
}

$EvidencePath = (Resolve-Path -LiteralPath $EvidencePath).Path
$PostEditHashReportPath = (Resolve-Path -LiteralPath $PostEditHashReportPath).Path
$savedRoot = Join-Path $ProjectRoot "Saved"

if (-not $EvidencePath.StartsWith($savedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "EvidencePath must be under ProjectRoot\Saved: $EvidencePath"
}
if (-not $PostEditHashReportPath.StartsWith($savedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "PostEditHashReportPath must be under ProjectRoot\Saved: $PostEditHashReportPath"
}

$evidence = Get-Content -LiteralPath $EvidencePath -Raw | ConvertFrom-Json
$hashReport = Get-Content -LiteralPath $PostEditHashReportPath -Raw | ConvertFrom-Json

if ([int]$hashReport.SchemaVersion -ne 1) {
    throw "Unsupported post-edit hash report schema version: $($hashReport.SchemaVersion)"
}
if ([string]$hashReport.AssetHashAlgorithm -ne "SHA256") {
    throw "Unsupported hash algorithm: $($hashReport.AssetHashAlgorithm)"
}
if (-not [bool]$hashReport.Summary.ReadyForEvidenceCopy) {
    throw "Post-edit hash report is not ready for evidence copy."
}

$currentHash = [string]$hashReport.CurrentAssetHash
if ([string]::IsNullOrWhiteSpace($currentHash)) {
    throw "Post-edit hash report has an empty CurrentAssetHash."
}

$editorEvidence = Find-RequiredEvidenceItem -Evidence $evidence -Name "Editor open verification"
if ($null -eq $editorEvidence) {
    throw "RequiredEvidence item not found: Editor open verification"
}

$backupPath = "$EvidencePath.bak.$((Get-Date).ToString('yyyyMMdd_HHmmss')).json"
Set-JsonProperty -Object $evidence -Name "AssetHashAlgorithm" -Value "SHA256"
Set-JsonProperty -Object $evidence -Name "AssetHash" -Value $currentHash
Set-JsonProperty -Object $evidence -Name "PostEditAssetHash" -Value $currentHash
Set-JsonProperty -Object $evidence -Name "PostEditHashReportPath" -Value $PostEditHashReportPath
Set-JsonProperty -Object $editorEvidence -Name "PostEditAssetHash" -Value $currentHash
Set-JsonProperty -Object $editorEvidence -Name "PostEditHashReportPath" -Value $PostEditHashReportPath

if (-not $DryRun) {
    Copy-Item -LiteralPath $EvidencePath -Destination $backupPath -Force
    $evidence | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $EvidencePath -Encoding UTF8
}

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
    PostEditHashReportPath = $PostEditHashReportPath
    BackupPath = $(if ($DryRun) { "" } else { $backupPath })
    CurrentAssetHash = $currentHash
    DryRun = [bool]$DryRun
    ModifiesAssets = $false
    StagesFiles = $false
    StagesWbp = $false
    Summary = [PSCustomObject]@{
        EvidenceUpdated = -not [bool]$DryRun
        ReadyForEvidenceCopy = [bool]$hashReport.Summary.ReadyForEvidenceCopy
        AssetHashCopied = $currentHash
        EditorEvidenceUpdated = $true
        BackupCreated = (-not [bool]$DryRun)
        Boundary = "This script updates only the Saved acceptance evidence JSON. It does not edit, accept, or stage WBP_VirtualSensorMonitor.uasset."
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP acceptance hash evidence updated."
    Write-Host "Evidence file: $($report.EvidencePath)"
    Write-Host "Post-edit hash report: $($report.PostEditHashReportPath)"
    Write-Host "Current SHA256: $($report.CurrentAssetHash)"
    Write-Host "Dry run: $($report.DryRun)"
    Write-Host "Evidence updated: $($report.Summary.EvidenceUpdated)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
