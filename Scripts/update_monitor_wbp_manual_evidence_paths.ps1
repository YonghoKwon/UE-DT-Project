param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [string]$EvidenceRunId = "",
    [string]$Operator = "",
    [string]$VerifiedAt = "",
    [string]$MapName = "",
    [string]$PieSession = "",
    [string]$EditorLogPath = "",
    [string]$EditorScreenshotPath = "",
    [string]$PieLogPath = "",
    [string]$PieScreenshotPath = "",
    [string]$ExportedPayloadPath = "",
    [string]$DisplayDataScreenshotPath = "",
    [string]$DisplayDataLogPath = "",
    [string]$HostActorOrLevelBlueprintBinding = "",
    [string]$SensorManagerBinding = "",
    [string]$SelectedCameraId = "",
    [string]$SelectedLidarId = "",
    [switch]$MarkEditorOpened,
    [switch]$MarkEditorCompiled,
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

function Set-StringIfPresent {
    param(
        [object]$Object,
        [string]$Name,
        [string]$Value
    )

    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        Set-JsonProperty -Object $Object -Name $Name -Value $Value
        return $true
    }
    return $false
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

function Resolve-CandidatePath {
    param(
        [string]$Value,
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }
    if ([System.IO.Path]::IsPathRooted($Value)) {
        return $Value
    }

    $projectCandidate = Join-Path $ProjectRoot $Value
    if (Test-Path -LiteralPath $projectCandidate -PathType Leaf) {
        return (Resolve-Path -LiteralPath $projectCandidate).Path
    }

    $sourceCandidate = Join-Path $SourceRepoRoot $Value
    if (Test-Path -LiteralPath $sourceCandidate -PathType Leaf) {
        return (Resolve-Path -LiteralPath $sourceCandidate).Path
    }

    return $projectCandidate
}

function Assert-ExistingFileIfPresent {
    param(
        [string]$Label,
        [string]$Value,
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }

    $resolved = Resolve-CandidatePath -Value $Value -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "$Label not found: $Value (resolved: $resolved)"
    }
    return (Resolve-Path -LiteralPath $resolved).Path
}

function Set-CommonMetadata {
    param(
        [object]$Item,
        [string]$EvidenceRunId,
        [string]$Operator,
        [string]$VerifiedAt
    )

    $changed = 0
    if (Set-StringIfPresent -Object $Item -Name "EvidenceRunId" -Value $EvidenceRunId) { $changed++ }
    if (Set-StringIfPresent -Object $Item -Name "Operator" -Value $Operator) { $changed++ }
    if (Set-StringIfPresent -Object $Item -Name "VerifiedAt" -Value $VerifiedAt) { $changed++ }
    return $changed
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
if (-not (Test-Path -LiteralPath $EvidencePath -PathType Leaf)) {
    throw "EvidencePath not found: $EvidencePath"
}
$EvidencePath = (Resolve-Path -LiteralPath $EvidencePath).Path

if (-not $EvidencePath.StartsWith((Join-Path $ProjectRoot "Saved"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "EvidencePath must be under ProjectRoot\Saved: $EvidencePath"
}

$resolvedPaths = [ordered]@{
    EditorLogPath = Assert-ExistingFileIfPresent -Label "EditorLogPath" -Value $EditorLogPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    EditorScreenshotPath = Assert-ExistingFileIfPresent -Label "EditorScreenshotPath" -Value $EditorScreenshotPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    PieLogPath = Assert-ExistingFileIfPresent -Label "PieLogPath" -Value $PieLogPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    PieScreenshotPath = Assert-ExistingFileIfPresent -Label "PieScreenshotPath" -Value $PieScreenshotPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    ExportedPayloadPath = Assert-ExistingFileIfPresent -Label "ExportedPayloadPath" -Value $ExportedPayloadPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    DisplayDataScreenshotPath = Assert-ExistingFileIfPresent -Label "DisplayDataScreenshotPath" -Value $DisplayDataScreenshotPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    DisplayDataLogPath = Assert-ExistingFileIfPresent -Label "DisplayDataLogPath" -Value $DisplayDataLogPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
}

$evidence = Get-Content -LiteralPath $EvidencePath -Raw | ConvertFrom-Json
$editor = Find-RequiredEvidenceItem -Evidence $evidence -Name "Editor open verification"
$pie = Find-RequiredEvidenceItem -Evidence $evidence -Name "PIE smoke result"
$display = Find-RequiredEvidenceItem -Evidence $evidence -Name "DisplayData visual match"
if ($null -eq $editor -or $null -eq $pie -or $null -eq $display) {
    throw "Required WBP evidence sections are missing. Expected Editor open verification, PIE smoke result, and DisplayData visual match."
}

$updatedFields = [System.Collections.Generic.List[string]]::new()
foreach ($item in @($editor, $pie, $display)) {
    $count = Set-CommonMetadata -Item $item -EvidenceRunId $EvidenceRunId -Operator $Operator -VerifiedAt $VerifiedAt
    if ($count -gt 0) {
        $updatedFields.Add("$($item.Name).CommonMetadata") | Out-Null
    }
}

if (Set-StringIfPresent -Object $editor -Name "MapName" -Value $MapName) { $updatedFields.Add("Editor.MapName") | Out-Null }
if (Set-StringIfPresent -Object $editor -Name "EditorLogPath" -Value $resolvedPaths.EditorLogPath) { $updatedFields.Add("Editor.EditorLogPath") | Out-Null }
if (Set-StringIfPresent -Object $editor -Name "ScreenshotPath" -Value $resolvedPaths.EditorScreenshotPath) { $updatedFields.Add("Editor.ScreenshotPath") | Out-Null }
if ($MarkEditorOpened) {
    Set-JsonProperty -Object $editor -Name "OpenedInEditor" -Value $true
    $updatedFields.Add("Editor.OpenedInEditor") | Out-Null
}
if ($MarkEditorCompiled) {
    Set-JsonProperty -Object $editor -Name "CompiledWithoutErrors" -Value $true
    $updatedFields.Add("Editor.CompiledWithoutErrors") | Out-Null
}

if (Set-StringIfPresent -Object $pie -Name "MapName" -Value $MapName) { $updatedFields.Add("PIE.MapName") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "PieSession" -Value $PieSession) { $updatedFields.Add("PIE.PieSession") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "HostActorOrLevelBlueprintBinding" -Value $HostActorOrLevelBlueprintBinding) { $updatedFields.Add("PIE.HostActorOrLevelBlueprintBinding") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "SensorManagerBinding" -Value $SensorManagerBinding) { $updatedFields.Add("PIE.SensorManagerBinding") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "SelectedCameraId" -Value $SelectedCameraId) { $updatedFields.Add("PIE.SelectedCameraId") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "SelectedLidarId" -Value $SelectedLidarId) { $updatedFields.Add("PIE.SelectedLidarId") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "LogPath" -Value $resolvedPaths.PieLogPath) { $updatedFields.Add("PIE.LogPath") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "ScreenshotPath" -Value $resolvedPaths.PieScreenshotPath) { $updatedFields.Add("PIE.ScreenshotPath") | Out-Null }
if (Set-StringIfPresent -Object $pie -Name "ExportedPayloadPath" -Value $resolvedPaths.ExportedPayloadPath) { $updatedFields.Add("PIE.ExportedPayloadPath") | Out-Null }

if (Set-StringIfPresent -Object $display -Name "MapName" -Value $MapName) { $updatedFields.Add("DisplayData.MapName") | Out-Null }
if (Set-StringIfPresent -Object $display -Name "PieSession" -Value $PieSession) { $updatedFields.Add("DisplayData.PieSession") | Out-Null }
if (Set-StringIfPresent -Object $display -Name "ScreenshotPath" -Value $resolvedPaths.DisplayDataScreenshotPath) { $updatedFields.Add("DisplayData.ScreenshotPath") | Out-Null }
if (Set-StringIfPresent -Object $display -Name "EditorLogPath" -Value $resolvedPaths.DisplayDataLogPath) { $updatedFields.Add("DisplayData.EditorLogPath") | Out-Null }

$backupPath = "$EvidencePath.bak.$((Get-Date).ToString('yyyyMMdd_HHmmss')).json"
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
    BackupPath = $(if ($DryRun) { "" } else { $backupPath })
    DryRun = [bool]$DryRun
    ModifiesAssets = $false
    StagesFiles = $false
    StagesWbp = $false
    ResolvedPaths = [PSCustomObject]$resolvedPaths
    UpdatedFields = @($updatedFields)
    Summary = [PSCustomObject]@{
        EvidenceUpdated = -not [bool]$DryRun
        UpdatedFieldCount = $updatedFields.Count
        BackupCreated = (-not [bool]$DryRun)
        EditorEvidenceTouched = ($updatedFields | Where-Object { $_ -match "^Editor\." }).Count -gt 0
        PieEvidenceTouched = ($updatedFields | Where-Object { $_ -match "^PIE\." }).Count -gt 0
        DisplayDataEvidenceTouched = ($updatedFields | Where-Object { $_ -match "^DisplayData\." }).Count -gt 0
        Boundary = "This script updates only the Saved acceptance evidence JSON. It validates referenced evidence files and never edits, accepts, or stages WBP_VirtualSensorMonitorPanel.uasset."
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP manual evidence paths updated."
    Write-Host "Evidence file: $($report.EvidencePath)"
    Write-Host "Updated field count: $($report.Summary.UpdatedFieldCount)"
    Write-Host "Dry run: $($report.DryRun)"
    Write-Host "Evidence updated: $($report.Summary.EvidenceUpdated)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
