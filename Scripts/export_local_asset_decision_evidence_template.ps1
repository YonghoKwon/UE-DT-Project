param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$OutputPath = "docs\local_asset_decisions.evidence.json",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-OutputPath {
    param(
        [string]$ProjectRoot,
        [string]$OutputPath
    )

    if ([System.IO.Path]::IsPathRooted($OutputPath)) {
        return $OutputPath
    }
    return Join-Path $ProjectRoot $OutputPath
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$assetReportScript = Join-Path $scriptRoot "report_local_project_status.ps1"
if (-not (Test-Path -LiteralPath $assetReportScript)) {
    throw "report_local_project_status.ps1 not found: $assetReportScript"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$resolvedOutputPath = Resolve-OutputPath -ProjectRoot $ProjectRoot -OutputPath $OutputPath

$jsonText = & powershell -ExecutionPolicy Bypass -File $assetReportScript -ProjectRoot $ProjectRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Local asset report failed with exit code $LASTEXITCODE"
}

$report = $jsonText | ConvertFrom-Json
$decisionEntries = @(
    $report.DecisionPoints |
        Where-Object { $_.State -eq "present" -and $_.Category -notlike "Generated*" } |
        ForEach-Object {
            [PSCustomObject]@{
                Path = $_.Path
                DecisionOwner = $_.DecisionOwner
                DecisionStatus = $_.DecisionStatus
                ReviewPriority = $_.ReviewPriority
                ReviewQueue = $_.ReviewQueue
                BlockingReason = $_.BlockingReason
                NextReviewAction = $_.NextReviewAction
                AcceptedBy = ""
                AcceptedAt = ""
                EvidenceSource = ""
                Notes = ""
                Evidence = @(
                    $_.EvidenceNeeded |
                        ForEach-Object {
                            [PSCustomObject]@{
                                Name = $_
                                Status = "Pending"
                                Source = ""
                                Note = ""
                            }
                        }
                )
            }
        }
)
$evidenceItems = @($decisionEntries | ForEach-Object { $_.Evidence })
$blockingDecisions = @($decisionEntries | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_.BlockingReason) })
$templateSummary = [PSCustomObject]@{
    DecisionCount = $decisionEntries.Count
    ReadyToStageCount = @($decisionEntries | Where-Object { $_.ReviewQueue -eq "ReadyToStage" }).Count
    NeedsOwnerDecisionCount = @($decisionEntries | Where-Object { $_.ReviewQueue -eq "NeedsOwnerDecision" }).Count
    KeepLocalCount = @($decisionEntries | Where-Object { $_.ReviewQueue -eq "KeepLocal" }).Count
    BlockingDecisionCount = $blockingDecisions.Count
    EvidenceItemCount = $evidenceItems.Count
    PendingEvidenceItemCount = @($evidenceItems | Where-Object { $_.Status -eq "Pending" }).Count
    TopBlockingPaths = @(
        $blockingDecisions |
            Sort-Object ReviewPriority, Path |
            Select-Object -First 5 |
            ForEach-Object {
                [PSCustomObject]@{
                    Path = $_.Path
                    ReviewPriority = $_.ReviewPriority
                    ReviewQueue = $_.ReviewQueue
                    BlockingReason = $_.BlockingReason
                    NextReviewAction = $_.NextReviewAction
                }
            }
    )
}

$template = [PSCustomObject]@{
    Schema = "LocalAssetDecisionEvidenceV1"
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $report.ProjectRoot
    Summary = $templateSummary
    Instructions = @(
        "DecisionOwner is review-routing metadata, not accepted ownership.",
        "ReadyToStage requires AcceptedForRepository.",
        "AcceptedForRepository requires complete EvidenceNeeded.",
        "ReadyToStage requires complete evidence.",
        "Recorded evidence must name reviewer, date, and source.",
        "PendingOwnerDecision remains NeedsOwnerDecision.",
        "EvidencePending remains NeedsOwnerDecision.",
        "Generated output remains KeepLocal.",
        "ReviewPriority orders the suggested owner-review sequence.",
        "BlockingReason and NextReviewAction are copied from the local asset action plan."
    )
    Decisions = $decisionEntries
}

if ($Json) {
    $template | ConvertTo-Json -Depth 8
    return
}

$parent = Split-Path -Parent $resolvedOutputPath
if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}

$template | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedOutputPath -Encoding UTF8
Write-Host "Wrote local asset decision evidence template: $resolvedOutputPath"
