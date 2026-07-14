param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Convert-ToMarkdownCell {
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Convert-ToSizeText {
    param([int64]$Bytes)

    if ($Bytes -ge 1GB) {
        return "{0:N1} GB" -f ($Bytes / 1GB)
    }
    if ($Bytes -ge 1MB) {
        return "{0:N1} MB" -f ($Bytes / 1MB)
    }
    if ($Bytes -ge 1KB) {
        return "{0:N1} KB" -f ($Bytes / 1KB)
    }
    return "$Bytes B"
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$largeContentDecisionReportScript = Join-Path $scriptRoot "export_large_content_decision_report.ps1"
if (-not (Test-Path -LiteralPath $largeContentDecisionReportScript -PathType Leaf)) {
    throw "export_large_content_decision_report.ps1 not found: $largeContentDecisionReportScript"
}

$largeReportJson = & powershell -ExecutionPolicy Bypass -File $largeContentDecisionReportScript -ProjectRoot $ProjectRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Large content decision report failed with exit code $LASTEXITCODE"
}

$largeReport = $largeReportJson | ConvertFrom-Json
$cleanupCandidates = @(
    $largeReport.Candidates |
        Where-Object { $_.UnusedLocalCleanupCandidate } |
        Sort-Object SizeBytes -Descending
)
$totalBytes = [int64](($cleanupCandidates | Measure-Object -Property SizeBytes -Sum).Sum)

$preDeleteChecks = @(
    [PSCustomObject]@{
        Name = "Map reference check"
        Status = "NotRun"
        Required = $true
        Description = "Open target production maps and confirm no level, actor, or map build dependency references this folder."
    },
    [PSCustomObject]@{
        Name = "WBP/widget reference check"
        Status = "NotRun"
        Required = $true
        Description = "Confirm monitor widgets and Blueprint UI assets do not reference this folder."
    },
    [PSCustomObject]@{
        Name = "Asset registry/reference viewer check"
        Status = "NotRun"
        Required = $true
        Description = "Use Unreal Reference Viewer or Asset Audit to confirm no map, Blueprint, material, widget, or level sequence references the folder."
    },
    [PSCustomObject]@{
        Name = "Redirector check"
        Status = "NotRun"
        Required = $true
        Description = "Run Fix Up Redirectors for Content before deletion and re-check references."
    },
    [PSCustomObject]@{
        Name = "Config/startup dependency check"
        Status = "NotRun"
        Required = $true
        Description = "Search project Content and Config references for the folder name after redirectors are fixed."
    },
    [PSCustomObject]@{
        Name = "Post-move editor smoke"
        Status = "NotRun"
        Required = $true
        Description = "Move the folder outside the project first or use an ignored backup, then open the editor and run PIE smoke before permanent removal."
    },
    [PSCustomObject]@{
        Name = "Staging guard"
        Status = "NotRun"
        Required = $true
        Description = "Do not stage the folder deletion as repository cleanup unless the folder was previously tracked by Git."
    }
)

$candidates = @(
    $cleanupCandidates |
        ForEach-Object {
            $relativePath = [string]$_.Path
            $fullPath = Join-Path $ProjectRoot $relativePath
            [PSCustomObject]@{
                SchemaVersion = 1
                Path = $relativePath
                NormalizedPath = ($relativePath -replace "\\", "/").TrimEnd("/")
                FullPath = $fullPath
                Exists = [bool](Test-Path -LiteralPath $fullPath)
                Category = [string]$_.Category
                GitState = [string]$_.GitState
                ReviewQueue = [string]$_.ReviewQueue
                CommitReadiness = [string]$_.CommitReadiness
                CommitBlocker = $true
                SizeBytes = [int64]$_.SizeBytes
                Size = [string]$_.Size
                EstimatedRecoverableBytes = [int64]$_.SizeBytes
                FileCount = [int]$_.FileCount
                UnusedLocalCleanupCandidate = [bool]$_.UnusedLocalCleanupCandidate
                CleanupReason = [string]$_.CleanupReason
                NextReviewAction = [string]$_.NextReviewAction
                RepositoryAcceptanceRequired = [bool]$_.RepositoryAcceptanceRequired
                RecommendedDecision = [string]$_.RecommendedDecision
                SafeToStage = $false
                SafeToDelete = $false
                DeletionPerformed = $false
                ManualReferenceCheckRequired = $true
                ManualDeletionOnly = $true
                BlockingReason = "Required reference/dependency checks have not been recorded."
                RecommendedLocalAction = "Optionally move or delete this unused local folder after Unreal reference/dependency checks pass; keep it out of source control."
                RequiredChecks = $preDeleteChecks
                CheckStatus = "NotRun"
                ExtensionCounts = $_.ExtensionCounts
                LargestFiles = $_.LargestFiles
            }
        }
)

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    DryRunOnly = $true
    DeletesFiles = $false
    ModifiesAssets = $false
    CleanupCandidateCount = $candidates.Count
    TotalRecoverableBytes = $totalBytes
    TotalRecoverableSize = Convert-ToSizeText -Bytes $totalBytes
    DeletionPerformed = $false
    SafeToStage = $false
    ManualReferenceCheckRequired = ($candidates.Count -gt 0)
    ManualDeletionOnly = ($candidates.Count -gt 0)
    RequiredReferenceCheckCount = $preDeleteChecks.Count * $candidates.Count
    SafeToDeleteCount = @($candidates | Where-Object { $_.SafeToDelete }).Count
    ReadyForManualDeletionCount = @($candidates | Where-Object { $_.SafeToDelete -and $_.ManualDeletionOnly }).Count
    DefaultAction = "Generate this plan, keep cleanup candidates untracked, then remove locally only after Unreal reference/dependency checks."
    RequiredChecks = $preDeleteChecks
    Candidates = $candidates
    Summary = [PSCustomObject]@{
        CleanupCandidateCount = $candidates.Count
        CleanupCandidateSizeBytes = $totalBytes
        CleanupCandidateSize = Convert-ToSizeText -Bytes $totalBytes
        TotalRecoverableBytes = $totalBytes
        TotalRecoverableSize = Convert-ToSizeText -Bytes $totalBytes
        DryRunOnly = $true
        DeletesFiles = $false
        ModifiesAssets = $false
        DeletionPerformed = $false
        SafeToStage = $false
        SafeToDeleteCount = @($candidates | Where-Object { $_.SafeToDelete }).Count
        ReadyForManualDeletionCount = @($candidates | Where-Object { $_.SafeToDelete -and $_.ManualDeletionOnly }).Count
        RequiredReferenceCheckCount = $preDeleteChecks.Count * $candidates.Count
        ManualReferenceCheckRequired = ($candidates.Count -gt 0)
        LargestCleanupCandidatePath = if ($candidates.Count -gt 0) { [string]$candidates[0].Path } else { "" }
        LargestCleanupCandidateSize = if ($candidates.Count -gt 0) { [string]$candidates[0].Size } else { "" }
        DefaultAction = "Keep untracked or remove manually after dependency checks."
        Valid = ($candidates.Count -eq $largeReport.Summary.UnusedLocalCleanupCandidateCount)
    }
}

if (-not $report.Summary.Valid) {
    throw "Cleanup plan candidate count does not match the large content decision report."
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Unused Content Cleanup Plan") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($report.ProjectRoot)") | Out-Null
$lines.Add("- Dry run only: $($report.DryRunOnly)") | Out-Null
$lines.Add("- Deletes files: $($report.DeletesFiles)") | Out-Null
$lines.Add("- Modifies assets: $($report.ModifiesAssets)") | Out-Null
$lines.Add("- Cleanup candidates: $($report.CleanupCandidateCount)") | Out-Null
$lines.Add("- Potential local disk recovery: $($report.TotalRecoverableSize)") | Out-Null
$lines.Add("- Deletion performed: $($report.DeletionPerformed)") | Out-Null
$lines.Add("- Safe to stage: $($report.SafeToStage)") | Out-Null
$lines.Add("- Manual reference check required: $($report.ManualReferenceCheckRequired)") | Out-Null
$lines.Add("- Default action: $($report.DefaultAction)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Pre-delete Checks") | Out-Null
$lines.Add("") | Out-Null
foreach ($check in $report.RequiredChecks) {
    $lines.Add("- $($check.Name): $($check.Status) - $($check.Description)") | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("| Path | Size | Files | Exists | Safe to stage | Safe to delete | Manual deletion only | Reference check required | Recommended local action |") | Out-Null
$lines.Add("| --- | ---: | ---: | --- | --- | --- | --- | --- | --- |") | Out-Null
foreach ($candidate in $report.Candidates) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} |" -f `
        (Convert-ToMarkdownCell $candidate.Path),
        (Convert-ToMarkdownCell $candidate.Size),
        (Convert-ToMarkdownCell $candidate.FileCount),
        (Convert-ToMarkdownCell $candidate.Exists),
        (Convert-ToMarkdownCell $candidate.SafeToStage),
        (Convert-ToMarkdownCell $candidate.SafeToDelete),
        (Convert-ToMarkdownCell $candidate.ManualDeletionOnly),
        (Convert-ToMarkdownCell $candidate.ManualReferenceCheckRequired),
        (Convert-ToMarkdownCell $candidate.RecommendedLocalAction))) | Out-Null
}

if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-TextFile -Path $MarkdownPath -Lines $lines
}
if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    $lines | ForEach-Object { Write-Host $_ }
}
