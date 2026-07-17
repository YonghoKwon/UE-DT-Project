param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [string]$EditorOpenEvidencePath = "",
    [string]$WidgetBindingEvidencePath = "",
    [string]$PieSmokeEvidencePath = "",
    [string]$SensorSelectionEvidencePath = "",
    [string]$LidarStatusPanelEvidencePath = "",
    [string]$SlabAnalysisPanelEvidencePath = "",
    [string]$DisplayDataScreenMatchEvidencePath = "",
    [string]$NoCrashEvidencePath = "",
    [string]$OwnerAcceptanceEvidencePath = "",
    [switch]$AcceptOwnerAcceptance,
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

function Resolve-EvidenceFile {
    param(
        [string]$Value,
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }

    $candidate = $Value
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $projectCandidate = Join-Path $ProjectRoot $candidate
        $sourceCandidate = Join-Path $SourceRepoRoot $candidate
        if (Test-Path -LiteralPath $projectCandidate -PathType Leaf) {
            $candidate = $projectCandidate
        }
        elseif (Test-Path -LiteralPath $sourceCandidate -PathType Leaf) {
            $candidate = $sourceCandidate
        }
        else {
            $candidate = $projectCandidate
        }
    }

    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Evidence file not found: $Value (resolved: $candidate)"
    }

    return (Resolve-Path -LiteralPath $candidate).Path
}

function Set-ManualSectionAccepted {
    param(
        [object]$ManualAcceptanceSections,
        [string]$SectionName,
        [string]$EvidencePath,
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
        return $null
    }
    if ($null -eq $ManualAcceptanceSections -or -not ($ManualAcceptanceSections.PSObject.Properties.Name -contains $SectionName)) {
        throw "Manual acceptance section not found: $SectionName"
    }

    $resolved = Resolve-EvidenceFile -Value $EvidencePath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    $section = $ManualAcceptanceSections.$SectionName
    Set-JsonProperty -Object $section -Name "EvidencePath" -Value $resolved
    Set-JsonProperty -Object $section -Name "Present" -Value $true
    Set-JsonProperty -Object $section -Name "Accepted" -Value $true

    return [PSCustomObject]@{
        Section = $SectionName
        EvidencePath = $resolved
        Present = $true
        Accepted = $true
    }
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

if (-not [string]::IsNullOrWhiteSpace($OwnerAcceptanceEvidencePath) -and -not $AcceptOwnerAcceptance) {
    throw "OwnerAcceptanceEvidencePath requires -AcceptOwnerAcceptance so owner acceptance cannot be marked accidentally."
}

$evidence = Get-Content -LiteralPath $EvidencePath -Raw | ConvertFrom-Json
if (-not ($evidence.PSObject.Properties.Name -contains "ManualAcceptanceSections")) {
    throw "ManualAcceptanceSections not found in evidence file: $EvidencePath"
}

$updates = [System.Collections.Generic.List[object]]::new()
$sectionInputs = [ordered]@{
    EditorOpenEvidence = $EditorOpenEvidencePath
    WidgetBindingEvidence = $WidgetBindingEvidencePath
    PieSmokeEvidence = $PieSmokeEvidencePath
    SensorSelectionEvidence = $SensorSelectionEvidencePath
    LidarStatusPanelEvidence = $LidarStatusPanelEvidencePath
    SlabAnalysisPanelEvidence = $SlabAnalysisPanelEvidencePath
    DisplayDataScreenMatchEvidence = $DisplayDataScreenMatchEvidencePath
    NoCrashEvidence = $NoCrashEvidencePath
}

foreach ($pair in $sectionInputs.GetEnumerator()) {
    $update = Set-ManualSectionAccepted -ManualAcceptanceSections $evidence.ManualAcceptanceSections -SectionName $pair.Key -EvidencePath $pair.Value -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    if ($null -ne $update) {
        $updates.Add($update) | Out-Null
    }
}

if ($AcceptOwnerAcceptance) {
    $update = Set-ManualSectionAccepted -ManualAcceptanceSections $evidence.ManualAcceptanceSections -SectionName "OwnerAcceptance" -EvidencePath $OwnerAcceptanceEvidencePath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    if ($null -ne $update) {
        $updates.Add($update) | Out-Null
    }
}

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
    UpdatedSections = @($updates)
    Summary = [PSCustomObject]@{
        EvidenceUpdated = -not [bool]$DryRun
        UpdatedSectionCount = $updates.Count
        OwnerAcceptanceTouched = (@($updates | Where-Object { [string]$_.Section -eq "OwnerAcceptance" }).Count -gt 0)
        BackupCreated = (-not [bool]$DryRun)
        Boundary = "This script updates only ManualAcceptanceSections in the Saved evidence JSON. OwnerAcceptance requires -AcceptOwnerAcceptance and it never edits, accepts, or stages WBP_VirtualSensorMonitorPanel.uasset by itself."
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP manual acceptance sections updated."
    Write-Host "Evidence file: $($report.EvidencePath)"
    Write-Host "Updated section count: $($report.Summary.UpdatedSectionCount)"
    Write-Host "Owner acceptance touched: $($report.Summary.OwnerAcceptanceTouched)"
    Write-Host "Dry run: $($report.DryRun)"
    Write-Host "Evidence updated: $($report.Summary.EvidenceUpdated)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
