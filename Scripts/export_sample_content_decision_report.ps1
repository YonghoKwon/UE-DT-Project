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
$sampleCandidates = @(
    $largeReport.Candidates |
        Where-Object { $_.Category -eq "SampleOrThirdParty" } |
        Sort-Object SizeBytes -Descending
)
$totalBytes = [int64](($sampleCandidates | Measure-Object -Property SizeBytes -Sum).Sum)

$stagedSamplePaths = @()
Push-Location $ProjectRoot
try {
    $stagedNames = @(git diff --cached --name-only -- Samples/PixelStreaming 2>$null)
    if ($LASTEXITCODE -eq 0) {
        $stagedSamplePaths = @(
            $stagedNames |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { ([string]$_).Replace("/", "\") }
        )
    }
}
finally {
    Pop-Location
}

$requiredAcceptance = @(
    [PSCustomObject]@{
        Name = "Project ownership decision"
        Status = "MissingOwnerEvidence"
        Required = $true
        Description = "Record whether DT-Project intentionally owns and redistributes this copied sample."
    },
    [PSCustomObject]@{
        Name = "License/redistribution approval"
        Status = "MissingOwnerEvidence"
        Required = $true
        Description = "Record source, license, redistribution terms, and reviewer approval before repository ownership."
    },
    [PSCustomObject]@{
        Name = "Setup documentation alternative"
        Status = "MissingOwnerEvidence"
        Required = $true
        Description = "Prefer documenting how to obtain or generate the sample unless the project must own the files."
    }
)

$setupPlanSteps = @(
    "Keep Samples/PixelStreaming out of source control for the current work scope.",
    "Do not count Pixel Streaming sample ownership as remaining DT-Project LiDAR/virtual-sensor implementation work.",
    "Use docs/pixel_streaming_setup.md only as a setup note if Pixel Streaming becomes a future project requirement.",
    "If ownership is accepted in a future scope, re-run local asset decision gates and stage only the reviewed sample paths."
)

$candidates = @(
    $sampleCandidates |
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
                FileCount = [int]$_.FileCount
                RedistributionReviewRequired = [bool]$_.RedistributionReviewRequired
                RepositoryAcceptanceRequired = [bool]$_.RepositoryAcceptanceRequired
                RecommendedDecision = "KeepLocalUnlessOwned"
                SourceReport = "Scripts/export_large_content_decision_report.ps1"
                SetupDocumentationPath = "docs\pixel_streaming_setup.md"
                ProjectOwnershipAccepted = $false
                LicenseRedistributionAccepted = $false
                DocumentationAlternativeAccepted = $false
                DocumentationAlternativePreferred = $true
                SetupAlternativePreferred = $true
                ExcludedFromCurrentScope = $true
                CountsTowardRemainingWork = $false
                CurrentScopeDecision = "IgnoreForCurrentLiDARVirtualSensorWork"
                SetupPlanSteps = $setupPlanSteps
                ReadyToStage = $false
                SafeToStage = $false
                MustRemainUntracked = $true
                StagedSamplePathCount = @($stagedSamplePaths | Where-Object { $_.StartsWith(($relativePath -replace "/", "\"), [System.StringComparison]::OrdinalIgnoreCase) }).Count
                UnexpectedSampleStaged = (@($stagedSamplePaths | Where-Object { $_.StartsWith(($relativePath -replace "/", "\"), [System.StringComparison]::OrdinalIgnoreCase) }).Count -gt 0)
                MissingAcceptanceCount = $requiredAcceptance.Count
                RequiredAcceptance = $requiredAcceptance
                DecisionBlockers = @(
                    "Project ownership decision is not recorded.",
                    "License/redistribution approval is not recorded.",
                    "Documentation alternative decision is not recorded."
                )
                BlockingReason = "Out of current scope; keep local and untracked unless Pixel Streaming ownership is explicitly reopened."
                NextReviewAction = [string]$_.NextReviewAction
                Boundary = "Current LiDAR/virtual-sensor work ignores Pixel Streaming. Keep untracked; only a future Pixel Streaming scope needs ownership and redistribution acceptance."
                SampleRiskReason = [string]$_.SampleRiskReason
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
    ModifiesAssets = $false
    CopiesSampleFiles = $false
    SourceReport = "Scripts/export_large_content_decision_report.ps1"
    SetupDocumentationPath = "docs\pixel_streaming_setup.md"
    StagedSamplePathCount = $stagedSamplePaths.Count
    StagedSamplePaths = $stagedSamplePaths
    UnexpectedSampleStaged = ($stagedSamplePaths.Count -gt 0)
    SampleCandidateCount = $candidates.Count
    SampleCandidateSizeBytes = $totalBytes
    SampleCandidateSize = Convert-ToSizeText -Bytes $totalBytes
    RepositoryAcceptanceRequiredCount = @($candidates | Where-Object { $_.RepositoryAcceptanceRequired }).Count
    DocumentationAlternativePreferredCount = @($candidates | Where-Object { $_.DocumentationAlternativePreferred }).Count
    ExcludedFromCurrentScopeCount = @($candidates | Where-Object { $_.ExcludedFromCurrentScope }).Count
    CountsTowardRemainingWorkCount = @($candidates | Where-Object { $_.CountsTowardRemainingWork }).Count
    MustRemainUntrackedCount = @($candidates | Where-Object { $_.MustRemainUntracked }).Count
    ReadyToStageCount = @($candidates | Where-Object { $_.ReadyToStage }).Count
    SetupPlanSteps = $setupPlanSteps
    DefaultAction = "Ignore copied Pixel Streaming sample content for the current LiDAR/virtual-sensor scope; keep it untracked."
    Candidates = $candidates
    Summary = [PSCustomObject]@{
        SampleCandidateCount = $candidates.Count
        SampleCandidateSizeBytes = $totalBytes
        SampleCandidateSize = Convert-ToSizeText -Bytes $totalBytes
        DryRunOnly = $true
        ModifiesAssets = $false
        CopiesSampleFiles = $false
        SetupDocumentationPath = "docs\pixel_streaming_setup.md"
        StagedSamplePathCount = $stagedSamplePaths.Count
        StagedSamplePaths = $stagedSamplePaths
        UnexpectedSampleStaged = ($stagedSamplePaths.Count -gt 0)
        RepositoryAcceptanceRequiredCount = @($candidates | Where-Object { $_.RepositoryAcceptanceRequired }).Count
        DocumentationAlternativePreferredCount = @($candidates | Where-Object { $_.DocumentationAlternativePreferred }).Count
        SetupAlternativePreferredCount = @($candidates | Where-Object { $_.SetupAlternativePreferred }).Count
        ExcludedFromCurrentScopeCount = @($candidates | Where-Object { $_.ExcludedFromCurrentScope }).Count
        CountsTowardRemainingWorkCount = @($candidates | Where-Object { $_.CountsTowardRemainingWork }).Count
        MustRemainUntrackedCount = @($candidates | Where-Object { $_.MustRemainUntracked }).Count
        ReadyToStageCount = @($candidates | Where-Object { $_.ReadyToStage }).Count
        MissingAcceptanceCount = [int](($candidates | Measure-Object -Property MissingAcceptanceCount -Sum).Sum)
        LargestSampleCandidatePath = if ($candidates.Count -gt 0) { [string]$candidates[0].Path } else { "" }
        LargestSampleCandidateSize = if ($candidates.Count -gt 0) { [string]$candidates[0].Size } else { "" }
        DefaultAction = "Ignore for current scope, keep untracked, and reopen only if Pixel Streaming becomes a project requirement."
        Valid = ($candidates.Count -eq $largeReport.Summary.RedistributionReviewRequiredCount -and $stagedSamplePaths.Count -eq 0)
    }
}

if (-not $report.Summary.Valid) {
    throw "Sample decision report is invalid. CandidateCount=$($candidates.Count) RedistributionReviewRequiredCount=$($largeReport.Summary.RedistributionReviewRequiredCount) StagedSamplePathCount=$($stagedSamplePaths.Count)"
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Sample Content Decision Report") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($report.ProjectRoot)") | Out-Null
$lines.Add("- Dry run only: $($report.DryRunOnly)") | Out-Null
$lines.Add("- Modifies assets: $($report.ModifiesAssets)") | Out-Null
$lines.Add("- Copies sample files: $($report.CopiesSampleFiles)") | Out-Null
$lines.Add("- Setup documentation: $($report.SetupDocumentationPath)") | Out-Null
$lines.Add("- Unexpected sample staged: $($report.UnexpectedSampleStaged)") | Out-Null
$lines.Add("- Staged sample path count: $($report.StagedSamplePathCount)") | Out-Null
$lines.Add("- Sample candidates: $($report.SampleCandidateCount)") | Out-Null
$lines.Add("- Sample candidate size: $($report.SampleCandidateSize)") | Out-Null
$lines.Add("- Must remain untracked: $($report.MustRemainUntrackedCount)") | Out-Null
$lines.Add("- Excluded from current scope: $($report.ExcludedFromCurrentScopeCount)") | Out-Null
$lines.Add("- Counts toward remaining work: $($report.CountsTowardRemainingWorkCount)") | Out-Null
$lines.Add("- Ready to stage: $($report.ReadyToStageCount)") | Out-Null
$lines.Add("- Default action: $($report.DefaultAction)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Setup Plan Steps") | Out-Null
$lines.Add("") | Out-Null
foreach ($step in $report.SetupPlanSteps) {
    $lines.Add("- $step") | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("| Path | Size | Files | Git state | Current scope decision | Excluded from current scope | Counts toward remaining work | Must remain untracked | Ready to stage | Required acceptance |") | Out-Null
$lines.Add("| --- | ---: | ---: | --- | --- | --- | --- | --- | --- | --- |") | Out-Null
foreach ($candidate in $report.Candidates) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} |" -f `
        (Convert-ToMarkdownCell $candidate.Path),
        (Convert-ToMarkdownCell $candidate.Size),
        (Convert-ToMarkdownCell $candidate.FileCount),
        (Convert-ToMarkdownCell $candidate.GitState),
        (Convert-ToMarkdownCell $candidate.CurrentScopeDecision),
        (Convert-ToMarkdownCell $candidate.ExcludedFromCurrentScope),
        (Convert-ToMarkdownCell $candidate.CountsTowardRemainingWork),
        (Convert-ToMarkdownCell $candidate.MustRemainUntracked),
        (Convert-ToMarkdownCell $candidate.ReadyToStage),
        (Convert-ToMarkdownCell (@($candidate.RequiredAcceptance | ForEach-Object { $_.Name }) -join "; ")))) | Out-Null
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
