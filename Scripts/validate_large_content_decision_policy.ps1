param(
    [string]$ProjectRoot = "",
    [string]$LocalProjectRoot = "",
    [switch]$FailIfPresent,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Assert-FileExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found: $Path"
    }
}

function Assert-ContainsText {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet)) {
        throw "$Label missing required text '$Pattern' in $Path"
    }
}

function Invoke-AssetReportJson {
    param([string]$ProjectRoot)

    $scriptPath = Join-Path $PSScriptRoot "report_local_project_status.ps1"
    Assert-FileExists -Path $scriptPath -Label "Local asset report script"
    $jsonText = & $scriptPath -ProjectRoot $ProjectRoot -Json
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "report_local_project_status.ps1 failed with exit code $LASTEXITCODE"
    }
    return ($jsonText | ConvertFrom-Json)
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($LocalProjectRoot)) {
    $LocalProjectRoot = $ProjectRoot
}
$LocalProjectRoot = (Resolve-Path -LiteralPath $LocalProjectRoot).Path

$localAssetDoc = Join-Path $ProjectRoot "docs\local_asset_report.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$evidenceTemplateScript = Join-Path $ProjectRoot "Scripts\export_local_asset_decision_evidence_template.ps1"
$evidenceWorkflowScript = Join-Path $ProjectRoot "Scripts\validate_local_asset_decision_evidence_workflow.ps1"
$assetReportScript = Join-Path $ProjectRoot "Scripts\report_local_project_status.ps1"
$largeContentDecisionReportScript = Join-Path $ProjectRoot "Scripts\export_large_content_decision_report.ps1"
$largeContentCleanupPlanScript = Join-Path $ProjectRoot "Scripts\export_large_content_cleanup_plan.ps1"
$precommitSummaryScript = Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"
$largeContentDecisionPolicyScript = $MyInvocation.MyCommand.Path
Assert-FileExists -Path $localAssetDoc -Label "Local asset report document"
Assert-FileExists -Path $remainingDoc -Label "Remaining work document"
Assert-FileExists -Path $evidenceTemplateScript -Label "Local asset decision evidence template script"
Assert-FileExists -Path $evidenceWorkflowScript -Label "Local asset decision evidence workflow validation script"
Assert-FileExists -Path $largeContentDecisionReportScript -Label "Large content decision report script"
Assert-FileExists -Path $largeContentCleanupPlanScript -Label "Large content cleanup plan script"
Assert-FileExists -Path $precommitSummaryScript -Label "Pre-commit summary script"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $evidenceTemplateScript; Pattern = "Summary"; Label = "Evidence template exports summary object" },
    [PSCustomObject]@{ Path = $evidenceTemplateScript; Pattern = "PendingEvidenceItemCount"; Label = "Evidence template counts pending evidence items" },
    [PSCustomObject]@{ Path = $evidenceTemplateScript; Pattern = "TopBlockingPaths"; Label = "Evidence template summarizes top blocking paths" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "LargeContentCandidate"; Label = "Local asset doc defines large content category" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "SampleOrThirdParty"; Label = "Local asset doc defines sample category" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "extension counts"; Label = "Local asset doc explains extension counts" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "largest files"; Label = "Local asset doc explains largest files" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "GitState"; Label = "Local asset doc explains git state" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "CommitReadiness"; Label = "Local asset doc explains commit readiness" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "ReviewQueue"; Label = "Local asset doc explains review queue" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "ReadyToStage"; Label = "Local asset doc explains ready-to-stage queue" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "NeedsOwnerDecision"; Label = "Local asset doc explains owner-decision queue" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "KeepLocal"; Label = "Local asset doc explains keep-local queue" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "DecisionOwner"; Label = "Local asset doc explains decision owner" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "DecisionStatus"; Label = "Local asset doc explains decision status" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "EvidenceNeeded"; Label = "Local asset doc explains evidence needed" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "DecisionEvidence"; Label = "Local asset doc explains decision evidence" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "EvidenceStatus"; Label = "Local asset doc explains evidence status" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "EvidenceSatisfied"; Label = "Local asset doc explains evidence satisfied" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "PendingOwnerDecision"; Label = "Local asset doc explains pending owner decision" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "EvidencePending"; Label = "Local asset doc explains evidence pending" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "AcceptedForRepository"; Label = "Local asset doc explains accepted repository status" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "AssetOwnerRequired"; Label = "Local asset doc explains asset owner requirement" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "ProjectOwnerRequired"; Label = "Local asset doc explains project owner requirement" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "DecisionOwner does not mean ownership has been accepted"; Label = "Local asset doc warns decision owner is not acceptance" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "EvidenceNeeded must be complete before ReadyToStage"; Label = "Local asset doc gates ready-to-stage on evidence" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "AcceptedForRepository requires complete EvidenceNeeded"; Label = "Local asset doc gates acceptance on evidence" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "ReadyToStage requires AcceptedForRepository"; Label = "Local asset doc gates ready-to-stage on acceptance" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "ReadyToStage requires complete evidence"; Label = "Local asset doc gates ready-to-stage on complete evidence" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "PendingOwnerDecision remains NeedsOwnerDecision"; Label = "Local asset doc keeps pending owner decisions queued" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "EvidencePending remains NeedsOwnerDecision"; Label = "Local asset doc keeps pending evidence queued" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "Recorded evidence must name reviewer, date, and source"; Label = "Local asset doc requires reviewer/date/source" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = 'non-empty `Source`'; Label = "Local asset doc requires per-item source" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "EvidenceSource"; Label = "Local asset doc requires evidence source" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "blank reviewer/date/source"; Label = "Local asset doc documents blank source validation" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "blank item source"; Label = "Local asset doc documents blank item source validation" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "Duplicate normalized evidence paths are invalid"; Label = "Local asset doc rejects duplicate evidence paths" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "Generated output remains KeepLocal"; Label = "Local asset doc keeps generated output local" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "validate_local_asset_decision_evidence_workflow.ps1"; Label = "Local asset doc documents evidence workflow validation" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "export_large_content_decision_report.ps1"; Label = "Local asset doc documents large content decision report" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "export_large_content_cleanup_plan.ps1"; Label = "Local asset doc documents large content cleanup plan" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "LocalProjectRoot"; Label = "Local asset doc documents separate local project root" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "Staged decision gate"; Label = "Local asset doc documents staged decision evidence gate" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "DecisionChecklist"; Label = "Local asset doc explains decision checklist" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "ReviewPriority"; Label = "Local asset doc explains review priority" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "CommitBlocker"; Label = "Local asset doc explains commit blocker" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "BlockingReason"; Label = "Local asset doc explains blocking reason" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "NextReviewAction"; Label = "Local asset doc explains next review action" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "ActionPlan"; Label = "Local asset doc explains action plan" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "asset source, license, production"; Label = "Local asset doc explains source/license/dependency checks" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Content/ChemicalPlantEnv"; Label = "Remaining work tracks ChemicalPlantEnv" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Samples/PixelStreaming"; Label = "Remaining work tracks PixelStreaming" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "map/WBP dependency check"; Label = "Remaining work requires dependency check" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "GitState"; Label = "Remaining work tracks git state reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "CommitReadiness"; Label = "Remaining work tracks commit readiness reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "ReviewQueue"; Label = "Remaining work tracks review queue reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "ReadyToStage"; Label = "Remaining work tracks ready-to-stage queue" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "NeedsOwnerDecision"; Label = "Remaining work tracks owner-decision queue" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "KeepLocal"; Label = "Remaining work tracks keep-local queue" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "DecisionOwner"; Label = "Remaining work tracks decision owner reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "DecisionStatus"; Label = "Remaining work tracks decision status reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "EvidenceNeeded"; Label = "Remaining work tracks evidence-needed reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "DecisionEvidence"; Label = "Remaining work tracks decision evidence reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "EvidenceStatus"; Label = "Remaining work tracks evidence status reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "EvidenceSatisfied"; Label = "Remaining work tracks evidence satisfied reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "PendingOwnerDecision"; Label = "Remaining work tracks pending owner decision" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "EvidencePending"; Label = "Remaining work tracks evidence pending" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "AcceptedForRepository"; Label = "Remaining work tracks accepted repository status" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "AssetOwnerRequired"; Label = "Remaining work tracks asset owner requirement" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "ProjectOwnerRequired"; Label = "Remaining work tracks project owner requirement" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "DecisionOwner is review-routing metadata"; Label = "Remaining work warns decision owner is routing metadata" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "EvidenceNeeded must be complete before ReadyToStage"; Label = "Remaining work gates ready-to-stage on evidence" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "AcceptedForRepository requires complete EvidenceNeeded"; Label = "Remaining work gates acceptance on evidence" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "ReadyToStage requires AcceptedForRepository"; Label = "Remaining work gates ready-to-stage on acceptance" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "ReadyToStage requires complete evidence"; Label = "Remaining work gates ready-to-stage on complete evidence" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Recorded evidence must name reviewer, date, and source"; Label = "Remaining work requires reviewer/date/source" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Each completed evidence item must also include a non-empty source"; Label = "Remaining work requires per-item source" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Duplicate normalized evidence paths are invalid"; Label = "Remaining work rejects duplicate evidence paths" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Generated output remains KeepLocal"; Label = "Remaining work keeps generated output local" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "validate_local_asset_decision_evidence_workflow.ps1"; Label = "Remaining work tracks evidence workflow validation" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_large_content_decision_report.ps1"; Label = "Remaining work tracks large content decision report" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_large_content_cleanup_plan.ps1"; Label = "Remaining work tracks large content cleanup plan" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "LocalProjectRoot"; Label = "Remaining work tracks separate local project root" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Staged decision gate"; Label = "Remaining work tracks staged decision evidence gate" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "owner/source/license"; Label = "Remaining work tracks source/license evidence" }
    ,
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "RequiredAcceptance"; Label = "Large content report exports required acceptance" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "DecisionBlockers"; Label = "Large content report exports decision blockers" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "NextReviewAction"; Label = "Large content report exports next review action" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "TopBlockers"; Label = "Large content report exports top blockers" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "BuiltDataHeavy"; Label = "Large content report exports BuiltData-heavy risk" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "LargestFileRisk"; Label = "Large content report exports largest-file risk" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "StorageRiskReason"; Label = "Large content report exports storage risk reason" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "RedistributionReviewRequired"; Label = "Large content report exports sample redistribution review gate" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "SampleRiskReason"; Label = "Large content report exports sample risk reason" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "UnusedLocalCleanupCandidate"; Label = "Large content report marks unused local cleanup candidates" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "RepositoryAcceptanceRequired"; Label = "Large content report separates repository acceptance requirement" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "CleanupReason"; Label = "Large content report explains unused cleanup reason" },
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "Unused local asset content"; Label = "Asset report routes unused local content to cleanup" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "BuiltData asset over 1 GB"; Label = "Large content report explains BuiltData-heavy blocker" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "Repository storage/versioning approval"; Label = "Large content report requires storage/versioning acceptance" },
    [PSCustomObject]@{ Path = $largeContentDecisionReportScript; Pattern = "Documentation alternative decision"; Label = "Sample report requires documentation alternative decision" }
    ,
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "DryRunOnly"; Label = "Cleanup plan declares dry-run behavior" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "DeletesFiles"; Label = "Cleanup plan declares it does not delete files" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "ModifiesAssets"; Label = "Cleanup plan declares it does not modify assets" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "ManualDeletionOnly"; Label = "Cleanup plan marks manual deletion only" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "SafeToDelete"; Label = "Cleanup plan exports safe-to-delete gate" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "RequiredChecks"; Label = "Cleanup plan exports required checks" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "CheckStatus"; Label = "Cleanup plan exports check status" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "EstimatedRecoverableBytes"; Label = "Cleanup plan exports recoverable size" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "Map reference check"; Label = "Cleanup plan requires map reference check" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "WBP/widget reference check"; Label = "Cleanup plan requires WBP reference check" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "Asset registry/reference viewer check"; Label = "Cleanup plan requires asset reference check" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "Redirector check"; Label = "Cleanup plan requires redirector check" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "Config/startup dependency check"; Label = "Cleanup plan requires startup dependency check" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "Post-move editor smoke"; Label = "Cleanup plan requires post-move smoke" },
    [PSCustomObject]@{ Path = $largeContentCleanupPlanScript; Pattern = "Staging guard"; Label = "Cleanup plan requires staging guard" }
    ,
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "LargeContentDecisionSummary"; Label = "Pre-commit summary exports large-content decision summary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "UnusedCleanupCandidateCount"; Label = "Pre-commit summary reports unused cleanup count" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "UnusedCleanupSize"; Label = "Pre-commit summary reports unused cleanup size" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "RepositoryAcceptanceCandidatePaths"; Label = "Pre-commit summary reports repository acceptance paths" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "SampleCandidateCount"; Label = "Pre-commit summary reports sample candidate count" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "SampleCandidateSize"; Label = "Pre-commit summary reports sample candidate size" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "SampleCandidatePaths"; Label = "Pre-commit summary reports sample candidate paths" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "SampleBoundary"; Label = "Pre-commit summary preserves sample ownership boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "Sample/third-party candidate means keep untracked"; Label = "Pre-commit summary warns sample candidates stay untracked" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "CleanupBoundary"; Label = "Pre-commit summary preserves cleanup boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "map/WBP dependency checks"; Label = "Pre-commit summary preserves map/WBP cleanup boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "not ready to stage"; Label = "Pre-commit summary warns cleanup candidates are not stageable" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "CleanupPlanAvailable"; Label = "Pre-commit summary reports cleanup plan availability" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "CleanupPlanDryRunOnly"; Label = "Pre-commit summary reports cleanup dry-run boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "CleanupPlanDeletesFiles"; Label = "Pre-commit summary reports cleanup deletion boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "CleanupPlanSafeToDeleteCount"; Label = "Pre-commit summary reports cleanup safe-to-delete count" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "CleanupPlanRequiredReferenceCheckCount"; Label = "Pre-commit summary reports required reference checks" }
    ,
    [PSCustomObject]@{ Path = $largeContentDecisionPolicyScript; Pattern = '[string]$LocalProjectRoot'; Label = "Large content policy accepts separate local project root" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$assetReport = Invoke-AssetReportJson -ProjectRoot $LocalProjectRoot
$largeReportJson = & $largeContentDecisionReportScript -ProjectRoot $LocalProjectRoot -Json
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "export_large_content_decision_report.ps1 failed with exit code $LASTEXITCODE"
}
$largeReport = $largeReportJson | ConvertFrom-Json
$cleanupPlanJson = & $largeContentCleanupPlanScript -ProjectRoot $LocalProjectRoot -Json
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "export_large_content_cleanup_plan.ps1 failed with exit code $LASTEXITCODE"
}
$cleanupPlan = $cleanupPlanJson | ConvertFrom-Json
$decisionCandidates = @(
    $assetReport.DecisionPoints |
        Where-Object {
            $_.State -eq "present" -and
            $_.Category -in @("LargeContentCandidate", "SampleOrThirdParty")
        } |
        Sort-Object Category, Path
)

foreach ($candidate in @($largeReport.Candidates)) {
    if (@($candidate.RequiredAcceptance).Count -eq 0) {
        throw "Large content decision candidate is missing RequiredAcceptance: $($candidate.Path)"
    }
    if (@($candidate.DecisionBlockers).Count -eq 0) {
        throw "Large content decision candidate is missing DecisionBlockers: $($candidate.Path)"
    }
    if ([string]::IsNullOrWhiteSpace([string]$candidate.NextReviewAction)) {
        throw "Large content decision candidate is missing NextReviewAction: $($candidate.Path)"
    }
    if ([string]::IsNullOrWhiteSpace([string]$candidate.StorageRiskReason)) {
        throw "Large content decision candidate is missing StorageRiskReason: $($candidate.Path)"
    }
    $acceptanceNames = @($candidate.RequiredAcceptance | ForEach-Object { [string]$_.Name })
    if ($candidate.Category -eq "LargeContentCandidate") {
        if ([bool]$candidate.UnusedLocalCleanupCandidate) {
            if (-not [bool]$candidate.RepositoryAcceptanceRequired) {
                if (-not ($acceptanceNames -contains "Manual unused-asset cleanup or keep-local decision")) {
                    throw "Unused large content candidate $($candidate.Path) is missing cleanup/keep-local acceptance item."
                }
            }
            else {
                throw "Unused large content candidate $($candidate.Path) must not require repository acceptance."
            }
        }
        else {
            foreach ($required in @("Asset source", "License/redistribution approval", "Size/storage acceptance", "Repository storage/versioning approval")) {
                if (-not ($acceptanceNames -contains $required)) {
                    throw "Large content candidate $($candidate.Path) is missing required acceptance item: $required"
                }
            }
        }
    }
    elseif ($candidate.Category -eq "SampleOrThirdParty") {
        if (-not [bool]$candidate.RedistributionReviewRequired) {
            throw "Sample candidate $($candidate.Path) must require redistribution review."
        }
        if ([string]::IsNullOrWhiteSpace([string]$candidate.SampleRiskReason)) {
            throw "Sample candidate $($candidate.Path) is missing sample risk reason."
        }
        foreach ($required in @("Project ownership decision", "License/redistribution approval", "Documentation alternative decision")) {
            if (-not ($acceptanceNames -contains $required)) {
                throw "Sample candidate $($candidate.Path) is missing required acceptance item: $required"
            }
        }
    }
}

if (-not [bool]$cleanupPlan.DryRunOnly) {
    throw "Cleanup plan must be dry-run only."
}
if ([bool]$cleanupPlan.DeletesFiles) {
    throw "Cleanup plan must not delete files."
}
if ([bool]$cleanupPlan.ModifiesAssets) {
    throw "Cleanup plan must not modify assets."
}
if ([bool]$cleanupPlan.DeletionPerformed) {
    throw "Cleanup plan must not perform deletion."
}
if ([bool]$cleanupPlan.SafeToStage) {
    throw "Cleanup plan must not mark cleanup candidates as safe to stage."
}
if ([int]$cleanupPlan.Summary.CleanupCandidateCount -ne [int]$largeReport.Summary.UnusedLocalCleanupCandidateCount) {
    throw "Cleanup plan candidate count must match unused cleanup candidate count."
}
if ([int]$cleanupPlan.Summary.CleanupCandidateCount -ne 5) {
    throw "Expected exactly 5 unused local cleanup candidates for the current local project state."
}
$expectedCleanupPaths = @(
    "Content\ChemicalPlantEnv",
    "Content\Mega_Crane",
    "Content\Materials",
    "Content\Meshes",
    "Content\Textures"
)
foreach ($expectedPath in $expectedCleanupPaths) {
    $matches = @($cleanupPlan.Candidates | Where-Object { [string]$_.Path -eq $expectedPath })
    if ($matches.Count -ne 1) {
        throw "Cleanup plan is missing expected cleanup candidate: $expectedPath"
    }
}
foreach ($candidate in @($cleanupPlan.Candidates)) {
    if (-not [bool]$candidate.UnusedLocalCleanupCandidate) {
        throw "Cleanup candidate must be marked UnusedLocalCleanupCandidate: $($candidate.Path)"
    }
    if ([bool]$candidate.RepositoryAcceptanceRequired) {
        throw "Cleanup candidate must not require repository acceptance: $($candidate.Path)"
    }
    if (-not [bool]$candidate.ManualDeletionOnly) {
        throw "Cleanup candidate must be manual-deletion-only: $($candidate.Path)"
    }
    if ([bool]$candidate.SafeToDelete) {
        throw "Cleanup candidate must default SafeToDelete=false until checks are recorded: $($candidate.Path)"
    }
    if (@($candidate.RequiredChecks).Count -eq 0) {
        throw "Cleanup candidate is missing RequiredChecks: $($candidate.Path)"
    }
    if ([string]::IsNullOrWhiteSpace([string]$candidate.CleanupReason)) {
        throw "Cleanup candidate is missing CleanupReason: $($candidate.Path)"
    }
    if ([string]::IsNullOrWhiteSpace([string]$candidate.NextReviewAction)) {
        throw "Cleanup candidate is missing NextReviewAction: $($candidate.Path)"
    }
    if ([int64]$candidate.SizeBytes -le 0 -or [int]$candidate.FileCount -le 0) {
        throw "Cleanup candidate is missing size/file metadata: $($candidate.Path)"
    }
}

$totalBytes = [int64](($decisionCandidates | Measure-Object -Property SizeBytes -Sum).Sum)
$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    LocalProjectRoot = $LocalProjectRoot
    Candidates = @($decisionCandidates | ForEach-Object {
        [PSCustomObject]@{
            Path = $_.Path
            Category = $_.Category
            FileCount = $_.FileCount
            SizeBytes = [int64]$_.SizeBytes
            Size = $_.Size
            Recommendation = $_.Recommendation
            ExtensionCounts = $_.ContentSummary.ExtensionCounts
            LargestFiles = $_.ContentSummary.LargestFiles
        }
    })
    CheckedContracts = @($requiredTexts | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Pattern = $_.Pattern
            Path = $_.Path
        }
    })
    Summary = [PSCustomObject]@{
        CandidateCount = $decisionCandidates.Count
        TotalSizeBytes = $totalBytes
        TotalSize = if ($totalBytes -ge 1GB) { "{0:N1} GB" -f ($totalBytes / 1GB) } elseif ($totalBytes -ge 1MB) { "{0:N1} MB" -f ($totalBytes / 1MB) } else { "$totalBytes B" }
        LargeDecisionReportCandidateCount = $largeReport.CandidateCount
        LargeDecisionReportHighRiskCount = $largeReport.HighRiskCount
        RequiredAcceptanceDeclared = $largeReport.Summary.RequiredAcceptanceDeclared
        DecisionBlockersDeclared = $largeReport.Summary.DecisionBlockersDeclared
        StorageRiskReasonDeclared = $largeReport.Summary.StorageRiskReasonDeclared
        BuiltDataHeavyCandidateCount = $largeReport.Summary.BuiltDataHeavyCandidateCount
        LargestFileRiskCandidateCount = $largeReport.Summary.LargestFileRiskCandidateCount
        RedistributionReviewRequiredCount = $largeReport.Summary.RedistributionReviewRequiredCount
        TopBlockerCount = $largeReport.Summary.TopBlockerCount
        CleanupPlanCandidateCount = $cleanupPlan.Summary.CleanupCandidateCount
        CleanupPlanRecoverableSize = $cleanupPlan.Summary.TotalRecoverableSize
        CleanupPlanDryRunOnly = $cleanupPlan.Summary.DryRunOnly
        CleanupPlanDeletesFiles = $cleanupPlan.Summary.DeletesFiles
        CleanupPlanSafeToDeleteCount = $cleanupPlan.Summary.SafeToDeleteCount
        CleanupPlanReadyForManualDeletionCount = $cleanupPlan.Summary.ReadyForManualDeletionCount
        CleanupPlanRequiredReferenceCheckCount = $cleanupPlan.Summary.RequiredReferenceCheckCount
        StrictFailureRequested = [bool]$FailIfPresent
        ExplicitOwnershipDecisionRequired = ($decisionCandidates.Count -gt 0)
        Valid = $true
    }
}

if ($FailIfPresent -and $decisionCandidates.Count -gt 0) {
    $paths = @($decisionCandidates | ForEach-Object { "$($_.Path)=$($_.Size)" }) -join ", "
    throw "Large content or sample/third-party candidates are present: $paths. Keep them untracked until explicit ownership, dependency, and size decisions are complete."
}

if ($Json) {
    $report | ConvertTo-Json -Depth 7
}
else {
    Write-Host "Large content decision policy is internally consistent."
    Write-Host "ProjectRoot: $($report.ProjectRoot)"
    Write-Host "LocalProjectRoot: $($report.LocalProjectRoot)"
    Write-Host "Candidate count: $($report.Summary.CandidateCount)"
    Write-Host "Total candidate size: $($report.Summary.TotalSize)"
    Write-Host "Large decision report high-risk count: $($report.Summary.LargeDecisionReportHighRiskCount)"
    Write-Host "Required acceptance declared: $($report.Summary.RequiredAcceptanceDeclared)"
    Write-Host "Decision blockers declared: $($report.Summary.DecisionBlockersDeclared)"
    Write-Host "Storage risk reason declared: $($report.Summary.StorageRiskReasonDeclared)"
    Write-Host "BuiltData-heavy candidates: $($report.Summary.BuiltDataHeavyCandidateCount)"
    Write-Host "Largest-file risk candidates: $($report.Summary.LargestFileRiskCandidateCount)"
    Write-Host "Redistribution review required: $($report.Summary.RedistributionReviewRequiredCount)"
    Write-Host "Top blocker count: $($report.Summary.TopBlockerCount)"
    Write-Host "Cleanup plan candidates: $($report.Summary.CleanupPlanCandidateCount)"
    Write-Host "Cleanup plan recoverable size: $($report.Summary.CleanupPlanRecoverableSize)"
    Write-Host "Cleanup plan dry run only: $($report.Summary.CleanupPlanDryRunOnly)"
    Write-Host "Cleanup plan deletes files: $($report.Summary.CleanupPlanDeletesFiles)"
    Write-Host "Cleanup plan safe-to-delete count: $($report.Summary.CleanupPlanSafeToDeleteCount)"
    Write-Host "Cleanup plan ready-for-manual-deletion count: $($report.Summary.CleanupPlanReadyForManualDeletionCount)"
    Write-Host "Cleanup plan required reference checks: $($report.Summary.CleanupPlanRequiredReferenceCheckCount)"
    Write-Host "Explicit ownership decision required: $($report.Summary.ExplicitOwnershipDecisionRequired)"
    foreach ($candidate in $report.Candidates) {
        Write-Host "$($candidate.Category): $($candidate.Path) files=$($candidate.FileCount) size=$($candidate.Size)"
    }
}
