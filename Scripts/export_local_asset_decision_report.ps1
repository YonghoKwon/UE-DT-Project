param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Format-Bool {
    param([bool]$Value)

    if ($Value) {
        return "yes"
    }
    return "no"
}

function Convert-ToMarkdownTableCell {
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }

    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Add-MarkdownLine {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Value = ""
    )

    $Lines.Add($Value) | Out-Null
}

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

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$assetReportScript = Join-Path $scriptRoot "report_local_project_status.ps1"
if (-not (Test-Path -LiteralPath $assetReportScript)) {
    throw "report_local_project_status.ps1 not found: $assetReportScript"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$jsonText = & powershell -ExecutionPolicy Bypass -File $assetReportScript -ProjectRoot $ProjectRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Local asset report failed with exit code $LASTEXITCODE"
}

$report = $jsonText | ConvertFrom-Json
$presentDecisionPoints = @($report.DecisionPoints | Where-Object { $_.State -eq "present" })
$reviewCandidates = @($presentDecisionPoints | Where-Object { $_.Category -eq "ReviewCandidate" })
$largeContentCandidates = @($presentDecisionPoints | Where-Object { $_.Category -eq "LargeContentCandidate" })
$sampleCandidates = @($presentDecisionPoints | Where-Object { $_.Category -eq "SampleOrThirdParty" })
$generatedCandidates = @($presentDecisionPoints | Where-Object { $_.Category -like "Generated*" })
$readyToStagePoints = @($presentDecisionPoints | Where-Object { $_.ReviewQueue -eq "ReadyToStage" })
$needsOwnerDecisionPoints = @($presentDecisionPoints | Where-Object { $_.ReviewQueue -eq "NeedsOwnerDecision" })
$keepLocalPoints = @($presentDecisionPoints | Where-Object { $_.ReviewQueue -eq "KeepLocal" })

$decisionReport = [PSCustomObject]@{
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $report.ProjectRoot
    UProject = $report.UProject
    GitBranch = $report.GitBranch
    RecentCommits = $report.RecentCommits
    Summary = $report.Summary
    ReviewCandidateCount = $reviewCandidates.Count
    LargeContentCandidateCount = $largeContentCandidates.Count
    SampleOrThirdPartyCount = $sampleCandidates.Count
    GeneratedOrLocalOutputCount = $generatedCandidates.Count
    ReadyToStageCount = $readyToStagePoints.Count
    NeedsOwnerDecisionCount = $needsOwnerDecisionPoints.Count
    KeepLocalCount = $keepLocalPoints.Count
    DecisionPoints = $report.DecisionPoints
    RecommendedReviewOrder = @(
        "Keep generated package outputs untracked or remove them before code review.",
        "Open the monitor WBP in Unreal Editor, verify optional bindings, and run PIE smoke before staging.",
        "Inspect Config/Game.ini and share it only if project-wide defaults are intentional.",
        "Decide ownership, source, versioning, and storage strategy for large Content folders.",
        "Keep PixelStreaming samples untracked unless the project needs to own those sample files."
    )
}

$lines = [System.Collections.Generic.List[string]]::new()
Add-MarkdownLine -Lines $lines -Value "# Local Asset Decision Report"
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "- Generated: $($decisionReport.GeneratedAt)"
Add-MarkdownLine -Lines $lines -Value "- Project: $($decisionReport.ProjectRoot)"
Add-MarkdownLine -Lines $lines -Value "- UProject: $($decisionReport.UProject)"
Add-MarkdownLine -Lines $lines -Value "- Branch: $($decisionReport.GitBranch)"
Add-MarkdownLine -Lines $lines -Value "- Present decision points: $($report.Summary.PresentDecisionPoints)"
Add-MarkdownLine -Lines $lines -Value "- Generated/local output present: $(Format-Bool $report.Summary.HasGeneratedOutput)"
Add-MarkdownLine -Lines $lines -Value "- Unclassified untracked paths: $($report.Summary.UnclassifiedUntrackedCount)"
Add-MarkdownLine -Lines $lines -Value "- Staged decision paths: $($report.Summary.StagedDecisionPointCount)"
Add-MarkdownLine -Lines $lines -Value "- Ready to stage: $($decisionReport.ReadyToStageCount)"
Add-MarkdownLine -Lines $lines -Value "- Needs owner decision: $($decisionReport.NeedsOwnerDecisionCount)"
Add-MarkdownLine -Lines $lines -Value "- Keep local: $($decisionReport.KeepLocalCount)"
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "## Recommended Review Order"
Add-MarkdownLine -Lines $lines
foreach ($item in $decisionReport.RecommendedReviewOrder) {
    Add-MarkdownLine -Lines $lines -Value "- $item"
}
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "## Review Queues"
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "### Ready To Stage"
Add-MarkdownLine -Lines $lines
if ($readyToStagePoints.Count -eq 0) {
    Add-MarkdownLine -Lines $lines -Value "- None. Known local asset decision points require manual evidence before staging."
}
else {
    foreach ($point in $readyToStagePoints) {
        Add-MarkdownLine -Lines $lines -Value "- $($point.Path)"
    }
}
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "### Needs Owner Decision"
Add-MarkdownLine -Lines $lines
if ($needsOwnerDecisionPoints.Count -eq 0) {
    Add-MarkdownLine -Lines $lines -Value "- None."
}
else {
    foreach ($point in $needsOwnerDecisionPoints) {
        Add-MarkdownLine -Lines $lines -Value "- $($point.Path) ($($point.Category), $($point.Size))"
    }
}
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "### Keep Local"
Add-MarkdownLine -Lines $lines
if ($keepLocalPoints.Count -eq 0) {
    Add-MarkdownLine -Lines $lines -Value "- None."
}
else {
    foreach ($point in $keepLocalPoints) {
        Add-MarkdownLine -Lines $lines -Value "- $($point.Path) ($($point.Category), $($point.Size))"
    }
}
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "## Decision Point Summary"
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "| Path | Category | State | Git state | Commit readiness | Review queue | Decision owner | Decision status | Files | Size | Recommendation |"
Add-MarkdownLine -Lines $lines -Value "| --- | --- | --- | --- | --- | --- | --- | --- | ---: | ---: | --- |"
foreach ($point in $report.DecisionPoints) {
    Add-MarkdownLine -Lines $lines -Value ("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} | {10} |" -f `
        (Convert-ToMarkdownTableCell $point.Path),
        (Convert-ToMarkdownTableCell $point.Category),
        (Convert-ToMarkdownTableCell $point.State),
        (Convert-ToMarkdownTableCell $point.GitState),
        (Convert-ToMarkdownTableCell $point.CommitReadiness),
        (Convert-ToMarkdownTableCell $point.ReviewQueue),
        (Convert-ToMarkdownTableCell $point.DecisionOwner),
        (Convert-ToMarkdownTableCell $point.DecisionStatus),
        (Convert-ToMarkdownTableCell $point.FileCount),
        (Convert-ToMarkdownTableCell $point.Size),
        (Convert-ToMarkdownTableCell $point.Recommendation))
}
Add-MarkdownLine -Lines $lines
Add-MarkdownLine -Lines $lines -Value "## Present Details"
Add-MarkdownLine -Lines $lines
foreach ($point in $presentDecisionPoints) {
    Add-MarkdownLine -Lines $lines -Value "### $($point.Path)"
    Add-MarkdownLine -Lines $lines
    Add-MarkdownLine -Lines $lines -Value "- Category: $($point.Category)"
    Add-MarkdownLine -Lines $lines -Value "- Git state: $($point.GitState)"
    Add-MarkdownLine -Lines $lines -Value "- Commit readiness: $($point.CommitReadiness)"
    Add-MarkdownLine -Lines $lines -Value "- Review queue: $($point.ReviewQueue)"
    Add-MarkdownLine -Lines $lines -Value "- Decision owner: $($point.DecisionOwner)"
    Add-MarkdownLine -Lines $lines -Value "- Decision status: $($point.DecisionStatus)"
    Add-MarkdownLine -Lines $lines -Value "- Kind: $($point.Kind)"
    Add-MarkdownLine -Lines $lines -Value "- Files: $($point.FileCount)"
    Add-MarkdownLine -Lines $lines -Value "- Size: $($point.Size)"
    if ($point.LastWriteTime) {
        Add-MarkdownLine -Lines $lines -Value "- Last write: $($point.LastWriteTime)"
    }
    Add-MarkdownLine -Lines $lines -Value "- Recommendation: $($point.Recommendation)"
    if (-not [string]::IsNullOrWhiteSpace($point.DetectedNote)) {
        Add-MarkdownLine -Lines $lines -Value "- Detected: $($point.DetectedNote)"
    }
    if ($point.DecisionChecklist -and $point.DecisionChecklist.Count -gt 0) {
        Add-MarkdownLine -Lines $lines -Value "- Decision checklist:"
        foreach ($check in $point.DecisionChecklist) {
            Add-MarkdownLine -Lines $lines -Value "  - $check"
        }
    }
    if ($point.EvidenceNeeded -and $point.EvidenceNeeded.Count -gt 0) {
        Add-MarkdownLine -Lines $lines -Value "- Evidence needed:"
        foreach ($evidence in $point.EvidenceNeeded) {
            Add-MarkdownLine -Lines $lines -Value "  - $evidence"
        }
    }
    if ($point.ContentSummary -and $point.ContentSummary.ExtensionCounts.Count -gt 0) {
        $extensions = @($point.ContentSummary.ExtensionCounts | ForEach-Object { "$($_.Extension)=$($_.Count)" }) -join ", "
        Add-MarkdownLine -Lines $lines -Value "- Extensions: $extensions"
    }
    if ($point.ContentSummary -and $point.ContentSummary.LargestFiles.Count -gt 0) {
        Add-MarkdownLine -Lines $lines -Value "- Largest files:"
        foreach ($largestFile in $point.ContentSummary.LargestFiles) {
            Add-MarkdownLine -Lines $lines -Value "  - $($largestFile.Size) $($largestFile.Path)"
        }
    }
    Add-MarkdownLine -Lines $lines
}

if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-TextFile -Path $MarkdownPath -Lines $lines
}
if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $parent = Split-Path -Parent $JsonPath
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    $decisionReport | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}

if ($Json) {
    $decisionReport | ConvertTo-Json -Depth 8
}
else {
    $lines | ForEach-Object { Write-Host $_ }
    if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
        Write-Host ""
        Write-Host "Markdown written: $MarkdownPath"
    }
    if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
        Write-Host "JSON written: $JsonPath"
    }
}
