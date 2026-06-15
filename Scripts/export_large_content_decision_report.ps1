param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
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

function Get-DefaultDecision {
    param([object]$Point)

    if ($Point.Category -eq "SampleOrThirdParty") {
        return [PSCustomObject]@{
            RecommendedDecision = "KeepLocalUnlessOwned"
            RiskLevel = "Medium"
            Reason = "Copied sample or third-party content should stay local unless the project intentionally owns and redistributes it."
        }
    }

    if ([int64]$Point.SizeBytes -ge 1GB) {
        return [PSCustomObject]@{
            RecommendedDecision = "KeepLocalUnlessAccepted"
            RiskLevel = "High"
            Reason = "Large content over 1 GB needs explicit source/license/dependency/storage approval before repository ownership."
        }
    }

    return [PSCustomObject]@{
        RecommendedDecision = "PendingOwnerDecision"
        RiskLevel = "Medium"
        Reason = "Content may be small enough to commit, but still requires source/license/dependency/storage evidence."
    }
}

function Convert-ToEvidenceDraft {
    param(
        [object]$Point,
        [string]$RecommendedDecision,
        [string]$Reason
    )

    return [PSCustomObject]@{
        Path = $Point.Path
        DecisionOwner = $Point.DecisionOwner
        DecisionStatus = $RecommendedDecision
        AcceptedBy = ""
        AcceptedAt = ""
        EvidenceSource = "Scripts/export_large_content_decision_report.ps1"
        Notes = $Reason
        Evidence = @(
            @($Point.EvidenceNeeded) |
                ForEach-Object {
                    [PSCustomObject]@{
                        Name = $_
                        Status = "Pending"
                        Source = ""
                        Note = "Owner-provided evidence required before AcceptedForRepository."
                    }
                }
        )
    }
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$assetReportScript = Join-Path $scriptRoot "report_local_project_status.ps1"
if (-not (Test-Path -LiteralPath $assetReportScript)) {
    throw "report_local_project_status.ps1 not found: $assetReportScript"
}

$jsonText = & powershell -ExecutionPolicy Bypass -File $assetReportScript -ProjectRoot $ProjectRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Local asset report failed with exit code $LASTEXITCODE"
}

$assetReport = $jsonText | ConvertFrom-Json
$candidatePoints = @(
    $assetReport.DecisionPoints |
        Where-Object { $_.State -eq "present" -and $_.Category -in @("LargeContentCandidate", "SampleOrThirdParty") } |
        Sort-Object SizeBytes -Descending
)

$candidates = @(
    $candidatePoints |
        ForEach-Object {
            $decision = Get-DefaultDecision -Point $_
            [PSCustomObject]@{
                Path = $_.Path
                Category = $_.Category
                GitState = $_.GitState
                ReviewQueue = $_.ReviewQueue
                SizeBytes = [int64]$_.SizeBytes
                Size = $_.Size
                FileCount = $_.FileCount
                RiskLevel = $decision.RiskLevel
                RecommendedDecision = $decision.RecommendedDecision
                Reason = $decision.Reason
                ExtensionCounts = if ($_.ContentSummary) { $_.ContentSummary.ExtensionCounts } else { @() }
                LargestFiles = if ($_.ContentSummary) { $_.ContentSummary.LargestFiles } else { @() }
                EvidenceNeeded = $_.EvidenceNeeded
                EvidenceDraft = Convert-ToEvidenceDraft -Point $_ -RecommendedDecision $decision.RecommendedDecision -Reason $decision.Reason
            }
        }
)

$totalBytes = [int64](($candidatePoints | Measure-Object -Property SizeBytes -Sum).Sum)
$report = [PSCustomObject]@{
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    CandidateCount = $candidates.Count
    TotalSizeBytes = $totalBytes
    TotalSize = if ($totalBytes -ge 1GB) { "{0:N1} GB" -f ($totalBytes / 1GB) } elseif ($totalBytes -ge 1MB) { "{0:N1} MB" -f ($totalBytes / 1MB) } else { "$totalBytes B" }
    KeepLocalByDefaultCount = @($candidates | Where-Object { $_.RecommendedDecision -like "KeepLocal*" }).Count
    PendingOwnerDecisionCount = @($candidates | Where-Object { $_.RecommendedDecision -eq "PendingOwnerDecision" }).Count
    Candidates = $candidates
    Summary = [PSCustomObject]@{
        HasLargeContentDecisionCandidates = ($candidates.Count -gt 0)
        RequiresOwnerSourceLicenseDecision = ($candidates.Count -gt 0)
        DefaultAction = "Keep large/sample content local unless source/license/dependency/storage evidence is complete and accepted."
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Large Content Decision Report") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($report.ProjectRoot)") | Out-Null
$lines.Add("- Candidate count: $($report.CandidateCount)") | Out-Null
$lines.Add("- Total size: $($report.TotalSize)") | Out-Null
$lines.Add("- Keep local by default: $($report.KeepLocalByDefaultCount)") | Out-Null
$lines.Add("- Pending owner decision: $($report.PendingOwnerDecisionCount)") | Out-Null
$lines.Add("- Default action: $($report.Summary.DefaultAction)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Path | Category | Size | Files | Git state | Risk | Recommended decision | Reason |") | Out-Null
$lines.Add("| --- | --- | ---: | ---: | --- | --- | --- | --- |") | Out-Null
foreach ($candidate in $report.Candidates) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} |" -f `
        (Convert-ToMarkdownCell $candidate.Path),
        (Convert-ToMarkdownCell $candidate.Category),
        (Convert-ToMarkdownCell $candidate.Size),
        (Convert-ToMarkdownCell $candidate.FileCount),
        (Convert-ToMarkdownCell $candidate.GitState),
        (Convert-ToMarkdownCell $candidate.RiskLevel),
        (Convert-ToMarkdownCell $candidate.RecommendedDecision),
        (Convert-ToMarkdownCell $candidate.Reason))) | Out-Null
}
$lines.Add("") | Out-Null
foreach ($candidate in $report.Candidates) {
    $lines.Add("## $($candidate.Path)") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("- Evidence needed: $(@($candidate.EvidenceNeeded) -join ', ')") | Out-Null
    $extensions = @($candidate.ExtensionCounts | ForEach-Object { "$($_.Extension)=$($_.Count)" }) -join ", "
    if (-not [string]::IsNullOrWhiteSpace($extensions)) {
        $lines.Add("- Extensions: $extensions") | Out-Null
    }
    if (@($candidate.LargestFiles).Count -gt 0) {
        $lines.Add("- Largest files:") | Out-Null
        foreach ($largestFile in @($candidate.LargestFiles)) {
            $lines.Add("  - $($largestFile.Size) $($largestFile.Path)") | Out-Null
        }
    }
    $lines.Add("") | Out-Null
}

if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-TextFile -Path $MarkdownPath -Lines $lines
}
if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $parent = Split-Path -Parent $JsonPath
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    $lines | ForEach-Object { Write-Host $_ }
}
