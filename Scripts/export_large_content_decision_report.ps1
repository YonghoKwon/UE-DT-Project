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

    if (Test-UnusedLocalContentPath -Path $Point.Path) {
        return [PSCustomObject]@{
            RecommendedDecision = "KeepLocalUnusedCleanup"
            RiskLevel = "High"
            Reason = "User confirmed this large Content asset is unused; keep local or remove manually, do not stage."
        }
    }

    $extensionCounts = @()
    if ($Point.ContentSummary -and $Point.ContentSummary.ExtensionCounts) {
        $extensionCounts = @($Point.ContentSummary.ExtensionCounts)
    }
    $uassetCount = 0
    foreach ($extension in $extensionCounts) {
        if ($extension.Extension -eq ".uasset") {
            $uassetCount = [int]$extension.Count
        }
    }
    $looksLikeBinaryAssetPack = $uassetCount -gt 0 -or [int64]$Point.SizeBytes -ge 100MB

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
    if ($looksLikeBinaryAssetPack) {
        return [PSCustomObject]@{
            RecommendedDecision = "KeepLocalUnlessAccepted"
            RiskLevel = "High"
            Reason = "Binary Unreal asset packs or content over 100 MB need explicit source/license/dependency/storage approval before repository ownership."
        }
    }

    return [PSCustomObject]@{
        RecommendedDecision = "PendingOwnerDecision"
        RiskLevel = "Medium"
        Reason = "Content may be small enough to commit, but still requires source/license/dependency/storage evidence."
    }
}

function Test-UnusedLocalContentPath {
    param([string]$Path)

    $normalizedPath = ($Path -replace "\\", "/").TrimEnd("/").ToLowerInvariant()
    return $normalizedPath -in @(
        "content/chemicalplantenv",
        "content/materials",
        "content/mega_crane",
        "content/meshes",
        "content/textures"
    )
}

function Get-CleanupReason {
    param([string]$Path)

    if (-not (Test-UnusedLocalContentPath -Path $Path)) {
        return ""
    }
    return "User confirmed this large Content asset is unused; keep local or remove manually after reference checks, do not stage."
}

function Get-RequiredAcceptance {
    param([object]$Point)

    $items = @(
        @($Point.EvidenceNeeded) |
            ForEach-Object {
                [PSCustomObject]@{
                    Name = $_
                    Required = $true
                    Status = "MissingOwnerEvidence"
                    SourceRequired = $true
                }
            }
    )

    if ($Point.Category -eq "LargeContentCandidate") {
        if (Test-UnusedLocalContentPath -Path $Point.Path) {
            $items += [PSCustomObject]@{
                Name = "Manual unused-asset cleanup or keep-local decision"
                Required = $true
                Status = "KeepLocalByUserConfirmation"
                SourceRequired = $false
            }
            return $items
        }
        $items += [PSCustomObject]@{
            Name = "Repository storage/versioning approval"
            Required = $true
            Status = "MissingOwnerEvidence"
            SourceRequired = $true
        }
    }
    if ($Point.Category -eq "SampleOrThirdParty") {
        $items += [PSCustomObject]@{
            Name = "Documentation alternative decision"
            Required = $true
            Status = "MissingOwnerEvidence"
            SourceRequired = $true
        }
    }

    return $items
}

function Get-DecisionBlockers {
    param(
        [object]$Point,
        [object[]]$RequiredAcceptance
    )

    if (Test-UnusedLocalContentPath -Path $Point.Path) {
        return @(
            "User confirmed this local Content asset is unused and should not be staged.",
            "Optional cleanup/reference check remains before deleting it from the local project."
        )
    }

    $blockers = @()
    if ($Point.DecisionStatus -ne "AcceptedForRepository") {
        $blockers += "DecisionStatus is $($Point.DecisionStatus), not AcceptedForRepository."
    }
    if (-not $Point.EvidenceSatisfied) {
        $blockers += "Required evidence is not complete and accepted."
    }
    foreach ($item in @($RequiredAcceptance)) {
        $blockers += "$($item.Name): $($item.Status)."
    }
    return $blockers
}

function Get-LargestFileBytes {
    param([object]$Point)

    $largest = @()
    if ($Point.ContentSummary -and $Point.ContentSummary.LargestFiles) {
        $largest = @($Point.ContentSummary.LargestFiles)
    }
    if ($largest.Count -eq 0) {
        return 0
    }
    return [int64](($largest | Measure-Object -Property SizeBytes -Maximum).Maximum)
}

function Test-BuiltDataHeavy {
    param([object]$Point)

    $largest = @()
    if ($Point.ContentSummary -and $Point.ContentSummary.LargestFiles) {
        $largest = @($Point.ContentSummary.LargestFiles)
    }
    foreach ($file in $largest) {
        if ([string]$file.Path -match "BuiltData" -and [int64]$file.SizeBytes -ge 1GB) {
            return $true
        }
    }
    return $false
}

function Get-StorageRiskReason {
    param(
        [object]$Point,
        [bool]$BuiltDataHeavy,
        [int64]$LargestFileBytes
    )

    if ($BuiltDataHeavy) {
        return "Contains a BuiltData asset over 1 GB; require map-build ownership and storage/versioning approval."
    }
    if ([int64]$Point.SizeBytes -ge 10GB) {
        return "Folder exceeds 10 GB; require repository storage, clone-time, and artifact strategy approval."
    }
    if ($LargestFileBytes -ge 1GB) {
        return "Contains a single file over 1 GB; require storage/versioning approval before repository ownership."
    }
    if ([int64]$Point.SizeBytes -ge 1GB) {
        return "Folder exceeds 1 GB; require explicit size/storage acceptance."
    }
    if ($Point.Category -eq "SampleOrThirdParty") {
        return "Copied sample/third-party content; require ownership, redistribution, and documentation alternative decision."
    }
    return "No large-file storage risk detected, but owner evidence is still required."
}

function Get-SampleRiskReason {
    param([object]$Point)

    if ($Point.Category -ne "SampleOrThirdParty") {
        return ""
    }
    return "Sample or third-party files should stay local unless project ownership, redistribution approval, and documentation alternative decision are accepted."
}

function Get-NextReviewAction {
    param([object]$Point)

    if (Test-UnusedLocalContentPath -Path $Point.Path) {
        return "Keep this unused local Content path out of source control; optionally remove it manually after Unreal reference/dependency checks."
    }
    if ($Point.Category -eq "SampleOrThirdParty") {
        return "Decide whether the project owns this sample content; if not, keep it local and document setup steps instead."
    }
    return "Collect source/license, dependency, size, and storage/versioning evidence, then record owner acceptance before staging."
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
            $largestFileBytes = Get-LargestFileBytes -Point $_
            $builtDataHeavy = Test-BuiltDataHeavy -Point $_
            [PSCustomObject]@{
                Path = $_.Path
                Category = $_.Category
                GitState = $_.GitState
                ReviewQueue = $_.ReviewQueue
                DecisionStatus = $_.DecisionStatus
                EvidenceSatisfied = $_.EvidenceSatisfied
                SizeBytes = [int64]$_.SizeBytes
                Size = $_.Size
                FileCount = $_.FileCount
                UnusedLocalCleanupCandidate = [bool](Test-UnusedLocalContentPath -Path $_.Path)
                RepositoryAcceptanceRequired = [bool](-not (Test-UnusedLocalContentPath -Path $_.Path))
                CleanupReason = Get-CleanupReason -Path $_.Path
                BuiltDataHeavy = [bool]$builtDataHeavy
                LargestFileRisk = [bool]($largestFileBytes -ge 1GB)
                LargestFileBytes = $largestFileBytes
                StorageRiskReason = Get-StorageRiskReason -Point $_ -BuiltDataHeavy $builtDataHeavy -LargestFileBytes $largestFileBytes
                RedistributionReviewRequired = [bool]($_.Category -eq "SampleOrThirdParty")
                SampleRiskReason = Get-SampleRiskReason -Point $_
                RiskLevel = $decision.RiskLevel
                RecommendedDecision = $decision.RecommendedDecision
                Reason = $decision.Reason
                RequiredAcceptance = Get-RequiredAcceptance -Point $_
                DecisionBlockers = $null
                NextReviewAction = Get-NextReviewAction -Point $_
                ExtensionCounts = if ($_.ContentSummary) { $_.ContentSummary.ExtensionCounts } else { @() }
                LargestFiles = if ($_.ContentSummary) { $_.ContentSummary.LargestFiles } else { @() }
                EvidenceNeeded = $_.EvidenceNeeded
                EvidenceDraft = Convert-ToEvidenceDraft -Point $_ -RecommendedDecision $decision.RecommendedDecision -Reason $decision.Reason
            }
        }
)

foreach ($candidate in $candidates) {
    $candidate.DecisionBlockers = Get-DecisionBlockers -Point $candidate -RequiredAcceptance $candidate.RequiredAcceptance
}

$totalBytes = [int64](($candidatePoints | Measure-Object -Property SizeBytes -Sum).Sum)
$topBlockers = @(
    $candidates |
        Sort-Object @{ Expression = { if ($_.RiskLevel -eq "High") { 0 } else { 1 } } }, @{ Expression = "SizeBytes"; Descending = $true }, Path |
        Select-Object -First 3 |
        ForEach-Object {
            [PSCustomObject]@{
                Path = $_.Path
                RiskLevel = $_.RiskLevel
                Size = $_.Size
                RecommendedDecision = $_.RecommendedDecision
                NextReviewAction = $_.NextReviewAction
            }
        }
)
$builtDataHeavyCandidates = @($candidates | Where-Object { $_.BuiltDataHeavy })
$largestFileRiskCandidates = @($candidates | Where-Object { $_.LargestFileRisk })
$redistributionReviewCandidates = @($candidates | Where-Object { $_.RedistributionReviewRequired })
$unusedCleanupCandidates = @($candidates | Where-Object { $_.UnusedLocalCleanupCandidate })
$report = [PSCustomObject]@{
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    CandidateCount = $candidates.Count
    TotalSizeBytes = $totalBytes
    TotalSize = if ($totalBytes -ge 1GB) { "{0:N1} GB" -f ($totalBytes / 1GB) } elseif ($totalBytes -ge 1MB) { "{0:N1} MB" -f ($totalBytes / 1MB) } else { "$totalBytes B" }
    KeepLocalByDefaultCount = @($candidates | Where-Object { $_.RecommendedDecision -like "KeepLocal*" }).Count
    PendingOwnerDecisionCount = @($candidates | Where-Object { $_.RecommendedDecision -eq "PendingOwnerDecision" }).Count
    HighRiskCount = @($candidates | Where-Object { $_.RiskLevel -eq "High" }).Count
    BuiltDataHeavyCount = $builtDataHeavyCandidates.Count
    LargestFileRiskCount = $largestFileRiskCandidates.Count
    RedistributionReviewRequiredCount = $redistributionReviewCandidates.Count
    UnusedLocalCleanupCandidateCount = $unusedCleanupCandidates.Count
    TopBlockers = $topBlockers
    Candidates = $candidates
    Summary = [PSCustomObject]@{
        HasLargeContentDecisionCandidates = ($candidates.Count -gt 0)
        RequiresOwnerSourceLicenseDecision = ($candidates.Count -gt 0)
        RequiredAcceptanceDeclared = ($candidates.Count -eq 0 -or @($candidates | Where-Object { @($_.RequiredAcceptance).Count -eq 0 }).Count -eq 0)
        DecisionBlockersDeclared = ($candidates.Count -eq 0 -or @($candidates | Where-Object { @($_.DecisionBlockers).Count -eq 0 }).Count -eq 0)
        StorageRiskReasonDeclared = ($candidates.Count -eq 0 -or @($candidates | Where-Object { [string]::IsNullOrWhiteSpace([string]$_.StorageRiskReason) }).Count -eq 0)
        BuiltDataHeavyCandidateCount = $builtDataHeavyCandidates.Count
        LargestFileRiskCandidateCount = $largestFileRiskCandidates.Count
        RedistributionReviewRequiredCount = $redistributionReviewCandidates.Count
        UnusedLocalCleanupCandidateCount = $unusedCleanupCandidates.Count
        TopBlockerCount = $topBlockers.Count
        DefaultAction = "Keep unused large Content assets and copied samples out of source control; remove unused local assets manually only after reference checks."
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
$lines.Add("- High risk candidates: $($report.HighRiskCount)") | Out-Null
$lines.Add("- BuiltData-heavy candidates: $($report.BuiltDataHeavyCount)") | Out-Null
$lines.Add("- Largest-file risk candidates: $($report.LargestFileRiskCount)") | Out-Null
$lines.Add("- Redistribution review required: $($report.RedistributionReviewRequiredCount)") | Out-Null
$lines.Add("- Unused local cleanup candidates: $($report.UnusedLocalCleanupCandidateCount)") | Out-Null
$lines.Add("- Default action: $($report.Summary.DefaultAction)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Top Blockers") | Out-Null
$lines.Add("") | Out-Null
if ($report.TopBlockers.Count -eq 0) {
    $lines.Add("- None.") | Out-Null
}
else {
    $lines.Add("| Path | Risk | Size | Recommended decision | Next action |") | Out-Null
    $lines.Add("| --- | --- | ---: | --- | --- |") | Out-Null
    foreach ($blocker in $report.TopBlockers) {
        $lines.Add(("| {0} | {1} | {2} | {3} | {4} |" -f `
            (Convert-ToMarkdownCell $blocker.Path),
            (Convert-ToMarkdownCell $blocker.RiskLevel),
            (Convert-ToMarkdownCell $blocker.Size),
            (Convert-ToMarkdownCell $blocker.RecommendedDecision),
            (Convert-ToMarkdownCell $blocker.NextReviewAction))) | Out-Null
    }
}
$lines.Add("") | Out-Null
$lines.Add("| Path | Category | Size | Files | Git state | Risk | Unused cleanup | Repo acceptance required | BuiltData-heavy | Largest-file risk | Recommended decision | Next action | Storage risk |") | Out-Null
$lines.Add("| --- | --- | ---: | ---: | --- | --- | --- | --- | --- | --- | --- | --- | --- |") | Out-Null
foreach ($candidate in $report.Candidates) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} | {10} | {11} | {12} |" -f `
        (Convert-ToMarkdownCell $candidate.Path),
        (Convert-ToMarkdownCell $candidate.Category),
        (Convert-ToMarkdownCell $candidate.Size),
        (Convert-ToMarkdownCell $candidate.FileCount),
        (Convert-ToMarkdownCell $candidate.GitState),
        (Convert-ToMarkdownCell $candidate.RiskLevel),
        (Convert-ToMarkdownCell $candidate.UnusedLocalCleanupCandidate),
        (Convert-ToMarkdownCell $candidate.RepositoryAcceptanceRequired),
        (Convert-ToMarkdownCell $candidate.BuiltDataHeavy),
        (Convert-ToMarkdownCell $candidate.LargestFileRisk),
        (Convert-ToMarkdownCell $candidate.RecommendedDecision),
        (Convert-ToMarkdownCell $candidate.NextReviewAction),
        (Convert-ToMarkdownCell $candidate.StorageRiskReason))) | Out-Null
}
$lines.Add("") | Out-Null
foreach ($candidate in $report.Candidates) {
    $lines.Add("## $($candidate.Path)") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("- Evidence needed: $(@($candidate.EvidenceNeeded) -join ', ')") | Out-Null
    $lines.Add("- Next review action: $($candidate.NextReviewAction)") | Out-Null
    if (-not [string]::IsNullOrWhiteSpace($candidate.CleanupReason)) {
        $lines.Add("- Cleanup reason: $($candidate.CleanupReason)") | Out-Null
    }
    $lines.Add("- Storage risk: $($candidate.StorageRiskReason)") | Out-Null
    if (-not [string]::IsNullOrWhiteSpace($candidate.SampleRiskReason)) {
        $lines.Add("- Sample risk: $($candidate.SampleRiskReason)") | Out-Null
    }
    if (@($candidate.RequiredAcceptance).Count -gt 0) {
        $lines.Add("- Required acceptance:") | Out-Null
        foreach ($item in @($candidate.RequiredAcceptance)) {
            $lines.Add("  - $($item.Name): $($item.Status)") | Out-Null
        }
    }
    if (@($candidate.DecisionBlockers).Count -gt 0) {
        $lines.Add("- Decision blockers:") | Out-Null
        foreach ($blocker in @($candidate.DecisionBlockers)) {
            $lines.Add("  - $blocker") | Out-Null
        }
    }
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
