param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$SourceRepoRoot = "",
    [switch]$IncludeReadiness,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "== $Title =="
}

function Get-GitLines {
    param(
        [string]$WorkingDirectory,
        [string[]]$GitArgs
    )

    Push-Location $WorkingDirectory
    try {
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $output = @(& git.exe @GitArgs 2>&1 | Where-Object { $_ -notmatch "^warning:" })
        $ErrorActionPreference = $previousErrorActionPreference
        if ($LASTEXITCODE -ne 0) {
            throw "git $($GitArgs -join ' ') failed with exit code $LASTEXITCODE"
        }
        return $output
    }
    finally {
        if ($previousErrorActionPreference) {
            $ErrorActionPreference = $previousErrorActionPreference
        }
        Pop-Location
    }
}

function Get-LocalAssetSummary {
    param([string]$ProjectRoot)

    $assetReportScript = Join-Path $script:PSScriptRoot "report_local_project_status.ps1"
    if (-not (Test-Path -LiteralPath $assetReportScript)) {
        return $null
    }

    $jsonText = & powershell -ExecutionPolicy Bypass -File $assetReportScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Local asset report failed with exit code $LASTEXITCODE"
    }

    return $jsonText | ConvertFrom-Json
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

function Get-LargeContentDecisionSummary {
    param([string]$ProjectRoot)

    $largeContentReportScript = Join-Path $script:PSScriptRoot "export_large_content_decision_report.ps1"
    if (-not (Test-Path -LiteralPath $largeContentReportScript -PathType Leaf)) {
        return $null
    }
    $cleanupPlanScript = Join-Path $script:PSScriptRoot "export_large_content_cleanup_plan.ps1"
    $unusedContentArchiveScript = Join-Path $script:PSScriptRoot "invoke_unused_content_archive.ps1"
    $unusedContentArchiveEvidenceScript = Join-Path $script:PSScriptRoot "export_unused_content_archive_evidence.ps1"
    $sampleDecisionReportScript = Join-Path $script:PSScriptRoot "export_sample_content_decision_report.ps1"

    $jsonText = & powershell -ExecutionPolicy Bypass -File $largeContentReportScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Large content decision report failed with exit code $LASTEXITCODE"
    }

    $largeReport = $jsonText | ConvertFrom-Json
    $cleanupPlan = $null
    if (Test-Path -LiteralPath $cleanupPlanScript -PathType Leaf) {
        $cleanupPlanJson = & powershell -ExecutionPolicy Bypass -File $cleanupPlanScript -ProjectRoot $ProjectRoot -Json
        if ($LASTEXITCODE -ne 0) {
            throw "Large content cleanup plan failed with exit code $LASTEXITCODE"
        }
        $cleanupPlan = $cleanupPlanJson | ConvertFrom-Json
    }
    $unusedContentArchiveReport = $null
    if (Test-Path -LiteralPath $unusedContentArchiveScript -PathType Leaf) {
        $archiveJson = & powershell -ExecutionPolicy Bypass -File $unusedContentArchiveScript -ProjectRoot $ProjectRoot -Json
        if ($LASTEXITCODE -ne 0) {
            throw "Unused content archive report failed with exit code $LASTEXITCODE"
        }
        $unusedContentArchiveReport = $archiveJson | ConvertFrom-Json
    }
    $unusedContentArchiveEvidence = $null
    if (Test-Path -LiteralPath $unusedContentArchiveEvidenceScript -PathType Leaf) {
        $archiveEvidenceJson = & powershell -ExecutionPolicy Bypass -File $unusedContentArchiveEvidenceScript -ProjectRoot $ProjectRoot -Json
        if ($LASTEXITCODE -ne 0) {
            throw "Unused content archive evidence failed with exit code $LASTEXITCODE"
        }
        $unusedContentArchiveEvidence = $archiveEvidenceJson | ConvertFrom-Json
    }
    $sampleDecisionReport = $null
    if (Test-Path -LiteralPath $sampleDecisionReportScript -PathType Leaf) {
        $sampleDecisionJson = & powershell -ExecutionPolicy Bypass -File $sampleDecisionReportScript -ProjectRoot $ProjectRoot -Json
        if ($LASTEXITCODE -ne 0) {
            throw "Sample content decision report failed with exit code $LASTEXITCODE"
        }
        $sampleDecisionReport = $sampleDecisionJson | ConvertFrom-Json
    }

    $unusedCleanupCandidates = @($largeReport.Candidates | Where-Object { $_.UnusedLocalCleanupCandidate })
    $repositoryAcceptanceCandidates = @($largeReport.Candidates | Where-Object { $_.RepositoryAcceptanceRequired })
    $sampleCandidates = @($largeReport.Candidates | Where-Object { $_.Category -eq "SampleOrThirdParty" })
    $unusedCleanupBytes = [int64](($unusedCleanupCandidates | Measure-Object -Property SizeBytes -Sum).Sum)
    $sampleBytes = [int64](($sampleCandidates | Measure-Object -Property SizeBytes -Sum).Sum)
    $largestCleanupCandidate = @($unusedCleanupCandidates | Sort-Object SizeBytes -Descending | Select-Object -First 1)
    $largestSampleCandidate = @($sampleCandidates | Sort-Object SizeBytes -Descending | Select-Object -First 1)
    $knownUnusedCleanupPaths = @(
        "Content\ChemicalPlantEnv",
        "Content\Mega_Crane",
        "Content\Materials",
        "Content\Meshes",
        "Content\Textures"
    )

    return [PSCustomObject]@{
        CandidateCount = [int]$largeReport.CandidateCount
        UnusedCleanupCandidateCount = $unusedCleanupCandidates.Count
        KnownUnusedCleanupCandidateMaxCount = $knownUnusedCleanupPaths.Count
        PresentKnownUnusedCleanupCandidateCount = $unusedCleanupCandidates.Count
        KnownUnusedCleanupCandidateAbsentOrArchivedCount = ($knownUnusedCleanupPaths.Count - $unusedCleanupCandidates.Count)
        UnusedCleanupSizeBytes = $unusedCleanupBytes
        UnusedCleanupSize = Convert-ToSizeText -Bytes $unusedCleanupBytes
        RepositoryAcceptanceCandidateCount = $repositoryAcceptanceCandidates.Count
        RepositoryAcceptanceCandidatePaths = @($repositoryAcceptanceCandidates | Select-Object -ExpandProperty Path)
        LargestCleanupCandidatePath = if ($largestCleanupCandidate.Count -gt 0) { [string]$largestCleanupCandidate[0].Path } else { "" }
        LargestCleanupCandidateSize = if ($largestCleanupCandidate.Count -gt 0) { [string]$largestCleanupCandidate[0].Size } else { "" }
        CleanupBoundary = "Cleanup candidate means keep ignored or manually remove after map/WBP dependency checks; it is not ready to stage."
        CleanupPlanAvailable = ($null -ne $cleanupPlan)
        CleanupPlanCandidateCount = if ($cleanupPlan) { [int]$cleanupPlan.Summary.CleanupCandidateCount } else { 0 }
        CleanupPlanRecoverableSize = if ($cleanupPlan) { [string]$cleanupPlan.Summary.TotalRecoverableSize } else { "" }
        CleanupPlanDryRunOnly = if ($cleanupPlan) { [bool]$cleanupPlan.Summary.DryRunOnly } else { $true }
        CleanupPlanDeletesFiles = if ($cleanupPlan) { [bool]$cleanupPlan.Summary.DeletesFiles } else { $false }
        CleanupPlanModifiesAssets = if ($cleanupPlan) { [bool]$cleanupPlan.Summary.ModifiesAssets } else { $false }
        CleanupPlanRequiredReferenceCheckCount = if ($cleanupPlan) { [int]$cleanupPlan.Summary.RequiredReferenceCheckCount } else { 0 }
        CleanupPlanSafeToDeleteCount = if ($cleanupPlan) { [int]$cleanupPlan.Summary.SafeToDeleteCount } else { 0 }
        CleanupPlanReadyForManualDeletionCount = if ($cleanupPlan) { [int]$cleanupPlan.Summary.ReadyForManualDeletionCount } else { 0 }
        CleanupPlanDefaultAction = if ($cleanupPlan) { [string]$cleanupPlan.Summary.DefaultAction } else { "" }
        UnusedContentArchiveAvailable = ($null -ne $unusedContentArchiveReport)
        UnusedContentArchiveCandidateCount = if ($unusedContentArchiveReport) { [int]$unusedContentArchiveReport.Summary.CandidateCount } else { 0 }
        UnusedContentArchivePreviewOnly = if ($unusedContentArchiveReport) { [bool]$unusedContentArchiveReport.Summary.PreviewOnly } else { $true }
        UnusedContentArchiveExecuteRequested = if ($unusedContentArchiveReport) { [bool]$unusedContentArchiveReport.Summary.ExecuteRequested } else { $false }
        UnusedContentArchiveRequiresExplicitArchiveRootForExecute = if ($unusedContentArchiveReport) { [bool]$unusedContentArchiveReport.Summary.RequiresExplicitArchiveRootForExecute } else { $true }
        UnusedContentArchiveDeletesFiles = if ($unusedContentArchiveReport) { [bool]$unusedContentArchiveReport.Summary.DeletesFiles } else { $false }
        UnusedContentArchiveStagesFiles = if ($unusedContentArchiveReport) { [bool]$unusedContentArchiveReport.Summary.StagesFiles } else { $false }
        UnusedContentArchiveModifiesAssets = if ($unusedContentArchiveReport) { [bool]$unusedContentArchiveReport.Summary.ModifiesAssets } else { $false }
        UnusedContentArchiveBoundary = if ($unusedContentArchiveReport) { [string]$unusedContentArchiveReport.Summary.Boundary } else { "" }
        ArchiveEvidenceAvailable = ($null -ne $unusedContentArchiveEvidence)
        ArchiveEvidenceComplete = if ($unusedContentArchiveEvidence) { [bool]$unusedContentArchiveEvidence.Summary.ArchiveEvidenceComplete } else { $false }
        ArchiveEvidenceArchiveRoot = if ($unusedContentArchiveEvidence) { [string]$unusedContentArchiveEvidence.ArchiveRoot } else { "" }
        ArchiveEvidenceLatestRun = if ($unusedContentArchiveEvidence) { [string]$unusedContentArchiveEvidence.Summary.LatestArchiveRun } else { "" }
        ArchiveEvidenceRootOutsideProject = if ($unusedContentArchiveEvidence) { [bool]$unusedContentArchiveEvidence.Summary.ArchiveRootOutsideProject } else { $false }
        ArchiveEvidenceArchivedCount = if ($unusedContentArchiveEvidence) { [int]$unusedContentArchiveEvidence.Summary.ArchivedCount } else { 0 }
        ArchiveEvidencePresentInProjectCount = if ($unusedContentArchiveEvidence) { [int]$unusedContentArchiveEvidence.Summary.PresentInProjectCount } else { 0 }
        ArchiveEvidenceMissingCount = if ($unusedContentArchiveEvidence) { [int]$unusedContentArchiveEvidence.Summary.MissingArchiveEvidenceCount } else { 0 }
        ArchiveEvidenceArchivedSize = if ($unusedContentArchiveEvidence) { [string]$unusedContentArchiveEvidence.Summary.ArchivedSize } else { "" }
        ArchiveEvidenceRootGitState = if ($unusedContentArchiveEvidence) { [string]$unusedContentArchiveEvidence.Summary.ArchiveRootGitState } else { "" }
        ArchiveEvidenceRootStagedFileCount = if ($unusedContentArchiveEvidence) { [int]$unusedContentArchiveEvidence.Summary.ArchiveRootStagedFileCount } else { 0 }
        ArchiveEvidenceDTCoreTouchedFileCount = if ($unusedContentArchiveEvidence) { [int]$unusedContentArchiveEvidence.Summary.DTCoreTouchedFileCount } else { 0 }
        ArchiveEvidenceBoundary = if ($unusedContentArchiveEvidence) { [string]$unusedContentArchiveEvidence.Summary.Boundary } else { "" }
        SampleCandidateCount = $sampleCandidates.Count
        SampleCandidateSizeBytes = $sampleBytes
        SampleCandidateSize = Convert-ToSizeText -Bytes $sampleBytes
        SampleCandidatePaths = @($sampleCandidates | Select-Object -ExpandProperty Path)
        LargestSampleCandidatePath = if ($largestSampleCandidate.Count -gt 0) { [string]$largestSampleCandidate[0].Path } else { "" }
        LargestSampleCandidateSize = if ($largestSampleCandidate.Count -gt 0) { [string]$largestSampleCandidate[0].Size } else { "" }
        SampleBoundary = "Sample/third-party candidate means keep untracked unless project ownership, redistribution approval, and documentation alternative are accepted."
        SampleDecisionReportAvailable = ($null -ne $sampleDecisionReport)
        SampleDecisionReadyToStageCount = if ($sampleDecisionReport) { [int]$sampleDecisionReport.Summary.ReadyToStageCount } else { 0 }
        SampleDecisionDocumentationAlternativePreferredCount = if ($sampleDecisionReport) { [int]$sampleDecisionReport.Summary.DocumentationAlternativePreferredCount } else { 0 }
        SampleDecisionSetupAlternativePreferredCount = if ($sampleDecisionReport) { [int]$sampleDecisionReport.Summary.SetupAlternativePreferredCount } else { 0 }
        SampleDecisionMustRemainUntrackedCount = if ($sampleDecisionReport) { [int]$sampleDecisionReport.Summary.MustRemainUntrackedCount } else { 0 }
        SampleDecisionMissingAcceptanceCount = if ($sampleDecisionReport) { [int]$sampleDecisionReport.Summary.MissingAcceptanceCount } else { 0 }
        SampleDecisionDryRunOnly = if ($sampleDecisionReport) { [bool]$sampleDecisionReport.Summary.DryRunOnly } else { $true }
        SampleDecisionCopiesSampleFiles = if ($sampleDecisionReport) { [bool]$sampleDecisionReport.Summary.CopiesSampleFiles } else { $false }
        SampleDecisionSetupDocumentationPath = if ($sampleDecisionReport) { [string]$sampleDecisionReport.Summary.SetupDocumentationPath } else { "" }
        SampleDecisionUnexpectedSampleStaged = if ($sampleDecisionReport) { [bool]$sampleDecisionReport.Summary.UnexpectedSampleStaged } else { $false }
        SampleDecisionStagedSamplePathCount = if ($sampleDecisionReport) { [int]$sampleDecisionReport.Summary.StagedSamplePathCount } else { 0 }
        SampleDecisionStagedSamplePaths = if ($sampleDecisionReport) { @($sampleDecisionReport.Summary.StagedSamplePaths) } else { @() }
        SampleDecisionDefaultAction = if ($sampleDecisionReport) { [string]$sampleDecisionReport.Summary.DefaultAction } else { "" }
        DefaultAction = [string]$largeReport.Summary.DefaultAction
    }
}

function Get-DTCoreSubmoduleGuardSummary {
    param([string]$ProjectRoot)

    $dtCoreGuardScript = Join-Path $script:PSScriptRoot "validate_dtcore_submodule_guard.ps1"
    if (-not (Test-Path -LiteralPath $dtCoreGuardScript -PathType Leaf)) {
        return $null
    }

    $jsonText = & powershell -ExecutionPolicy Bypass -File $dtCoreGuardScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "DTCore submodule guard failed with exit code $LASTEXITCODE"
    }

    $guard = $jsonText | ConvertFrom-Json
    return [PSCustomObject]@{
        DTCoreInvariantValid = [bool]$guard.Summary.DTCoreInvariantValid
        ExpectedCommit = [string]$guard.Summary.ExpectedCommit
        ActualCommit = [string]$guard.Summary.ActualCommit
        DTCoreExpectedCommit = [string]$guard.Summary.DTCoreExpectedCommit
        DTCoreActualCommit = [string]$guard.Summary.DTCoreActualCommit
        DTCoreSubmoduleStatus = [string]$guard.Summary.DTCoreSubmoduleStatus
        DTCoreGitlinkStaged = [bool]$guard.Summary.DTCoreGitlinkStaged
        DTCoreGitlinkModified = [bool]$guard.Summary.DTCoreGitlinkModified
        DTCoreWorktreeClean = [bool]$guard.Summary.DTCoreWorktreeClean
        DTCoreStagedPathCount = [int]$guard.Summary.DTCoreStagedPathCount
        DTCoreModifiedPathCount = [int]$guard.Summary.DTCoreModifiedPathCount
        DTCoreUntrackedPathCount = [int]$guard.Summary.DTCoreUntrackedPathCount
        SubmodulePresent = [bool]$guard.Summary.SubmodulePresent
        ExpectedCommitMatches = [bool]$guard.Summary.ExpectedCommitMatches
        SubmoduleStatusClean = [bool]$guard.Summary.SubmoduleStatusClean
        ParentDTCoreStatusLineCount = [int]$guard.Summary.ParentDTCoreStatusLineCount
        StagedDTCorePathCount = [int]$guard.Summary.StagedDTCorePathCount
        SubmoduleWorktreeLineCount = [int]$guard.Summary.SubmoduleWorktreeLineCount
        ViolationCount = [int]$guard.Summary.ViolationCount
        DryRunOnly = [bool]$guard.Summary.DryRunOnly
        ReadOnly = [bool]$guard.Summary.ReadOnly
        ModifiesSubmodule = [bool]$guard.Summary.ModifiesSubmodule
        StagesDTCore = [bool]$guard.Summary.StagesDTCore
        DoesNotUpdateSubmodule = [bool]$guard.Summary.DoesNotUpdateSubmodule
        DoesNotStageFiles = [bool]$guard.Summary.DoesNotStageFiles
        Boundary = [string]$guard.Summary.Boundary
    }
}

function Get-WbpDecisionSummary {
    param(
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    $wbpDecisionReportScript = Join-Path $script:PSScriptRoot "export_monitor_wbp_decision_report.ps1"
    if (-not (Test-Path -LiteralPath $wbpDecisionReportScript -PathType Leaf)) {
        return $null
    }

    $params = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $wbpDecisionReportScript,
        "-ProjectRoot", $ProjectRoot,
        "-Json"
    )
    if (-not [string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
        $params += @("-SourceRepoRoot", $SourceRepoRoot)
    }

    $jsonText = & powershell @params
    if ($LASTEXITCODE -ne 0) {
        throw "Monitor WBP decision report failed with exit code $LASTEXITCODE"
    }

    $wbpReport = $jsonText | ConvertFrom-Json
    $missingChecklist = @($wbpReport.ManualAcceptanceChecklist | Where-Object { $_.Status -ne "Recorded" })
    return [PSCustomObject]@{
        WbpPresent = [bool]$wbpReport.WbpPresent
        WbpRelativePath = [string]$wbpReport.WbpRelativePath
        WbpSize = [string]$wbpReport.WbpSize
        GitState = [string]$wbpReport.GitState
        ReviewQueue = [string]$wbpReport.Summary.ReviewQueue
        CommitReadiness = [string]$wbpReport.Summary.CommitReadiness
        EvidenceStatus = [string]$wbpReport.Summary.EvidenceStatus
        EvidenceSatisfied = [bool]$wbpReport.Summary.EvidenceSatisfied
        MissingEvidenceCount = [int]$wbpReport.Summary.MissingEvidenceCount
        ManualAcceptanceMissingCount = [int]$wbpReport.Summary.ManualAcceptanceMissingCount
        AcceptanceTemplateAvailable = [bool]$wbpReport.Summary.AcceptanceTemplateAvailable
        AcceptanceTemplateRequiredEvidenceCount = [int]$wbpReport.Summary.AcceptanceTemplateRequiredEvidenceCount
        MissingAcceptanceItems = @($missingChecklist | Select-Object -ExpandProperty Name)
        RecommendedDecision = [string]$wbpReport.RecommendedDecision
        MonitorPolicyValid = [bool]$wbpReport.Summary.MonitorPolicyValid
        SetupDocContractComplete = [bool]$wbpReport.Summary.SetupDocContractComplete
        ReadyToStage = [bool]$wbpReport.Summary.ReadyToStage
        StagingBlocked = [bool]$wbpReport.Summary.StagingBlocked
        ManualEditorVerificationStillRequired = [bool]$wbpReport.Summary.ManualEditorVerificationStillRequired
        BlockingReason = if ($wbpReport.DecisionPoint) { [string]$wbpReport.DecisionPoint.BlockingReason } else { "" }
        NextReviewAction = if ($wbpReport.DecisionPoint) { [string]$wbpReport.DecisionPoint.NextReviewAction } else { "" }
        UnexpectedWbpStaged = ([string]$wbpReport.GitState -eq "Staged" -and -not [bool]$wbpReport.Summary.ReadyToStage)
        MustRemainUntracked = ([bool]$wbpReport.WbpPresent -and -not [bool]$wbpReport.Summary.ReadyToStage)
        Boundary = "Monitor WBP stays untracked until Unreal Editor open, optional binding check, PIE smoke, and production WBP acceptance evidence are recorded."
    }
}

function Get-WbpPreflightSummary {
    param(
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    $wbpPreflightScript = Join-Path $script:PSScriptRoot "export_monitor_wbp_preflight_report.ps1"
    if (-not (Test-Path -LiteralPath $wbpPreflightScript -PathType Leaf)) {
        return $null
    }

    $params = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $wbpPreflightScript,
        "-ProjectRoot", $ProjectRoot,
        "-Json"
    )
    if (-not [string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
        $params += @("-SourceRepoRoot", $SourceRepoRoot)
    }

    $jsonText = & powershell @params
    if ($LASTEXITCODE -ne 0) {
        throw "Monitor WBP preflight report failed with exit code $LASTEXITCODE"
    }

    $preflight = $jsonText | ConvertFrom-Json
    return [PSCustomObject]@{
        WbpPresent = [bool]$preflight.Summary.WbpPresent
        WbpUntracked = [bool]$preflight.Summary.WbpUntracked
        WbpStaged = [bool]$preflight.Summary.WbpStaged
        WbpWorktreeModified = [bool]$preflight.Summary.WbpWorktreeModified
        AssetHash = [string]$preflight.Summary.AssetHash
        SetupDocContractComplete = [bool]$preflight.Summary.SetupDocContractComplete
        MissingSetupTermCount = [int]$preflight.Summary.MissingSetupTermCount
        AcceptanceTemplateAvailable = [bool]$preflight.Summary.AcceptanceTemplateAvailable
        RequiredEvidenceCount = [int]$preflight.Summary.RequiredEvidenceCount
        PendingEvidenceCount = [int]$preflight.Summary.PendingEvidenceCount
        DecisionReadyToStage = [bool]$preflight.Summary.DecisionReadyToStage
        DecisionMissingEvidenceCount = [int]$preflight.Summary.DecisionMissingEvidenceCount
        PreflightCheckCount = [int]$preflight.Summary.PreflightCheckCount
        BlockedPreflightCheckCount = [int]$preflight.Summary.BlockedPreflightCheckCount
        ReadyForManualEditorReview = [bool]$preflight.Summary.ReadyForManualEditorReview
        ModifiesAssets = [bool]$preflight.Summary.ModifiesAssets
        StagesWbp = [bool]$preflight.Summary.StagesWbp
        Boundary = [string]$preflight.Summary.Boundary
    }
}

function Get-WbpAcceptanceEvidenceSummary {
    param(
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    $wbpEvidenceValidatorScript = Join-Path $script:PSScriptRoot "validate_monitor_wbp_acceptance_evidence.ps1"
    if (-not (Test-Path -LiteralPath $wbpEvidenceValidatorScript -PathType Leaf)) {
        return $null
    }

    $params = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $wbpEvidenceValidatorScript,
        "-ProjectRoot", $ProjectRoot,
        "-Json"
    )
    if (-not [string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
        $params += @("-SourceRepoRoot", $SourceRepoRoot)
    }

    $jsonText = & powershell @params
    if ($LASTEXITCODE -ne 0) {
        throw "Monitor WBP acceptance evidence validator failed with exit code $LASTEXITCODE"
    }

    $evidenceReport = $jsonText | ConvertFrom-Json
    return [PSCustomObject]@{
        EvidenceFilePresent = [bool]$evidenceReport.Summary.EvidenceFilePresent
        EvidenceRecordPresent = [bool]$evidenceReport.Summary.EvidenceRecordPresent
        RequiredEvidenceCount = [int]$evidenceReport.Summary.RequiredEvidenceCount
        PassedCheckCount = [int]$evidenceReport.Summary.PassedCheckCount
        FailedCheckCount = [int]$evidenceReport.Summary.FailedCheckCount
        ReadyToStageCandidate = [bool]$evidenceReport.Summary.ReadyToStageCandidate
        WbpStaged = [bool]$evidenceReport.Summary.WbpStaged
        DecisionReportReadyToStage = [bool]$evidenceReport.Summary.DecisionReportReadyToStage
        DryRunOnly = [bool]$evidenceReport.Summary.DryRunOnly
        ModifiesAssets = [bool]$evidenceReport.Summary.ModifiesAssets
        StagesWbp = [bool]$evidenceReport.Summary.StagesWbp
        Boundary = [string]$evidenceReport.Summary.Boundary
    }
}

function Get-RuntimeConfigDecisionSummary {
    param(
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    $runtimeConfigDecisionReportScript = Join-Path $script:PSScriptRoot "export_runtime_config_decision_report.ps1"
    if (-not (Test-Path -LiteralPath $runtimeConfigDecisionReportScript -PathType Leaf)) {
        return $null
    }

    $params = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $runtimeConfigDecisionReportScript,
        "-ProjectRoot", $ProjectRoot,
        "-Json"
    )
    if (-not [string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
        $params += @("-SourceRepoRoot", $SourceRepoRoot)
    }

    $jsonText = & powershell @params
    if ($LASTEXITCODE -ne 0) {
        throw "Runtime config decision report failed with exit code $LASTEXITCODE"
    }

    $configReport = $jsonText | ConvertFrom-Json
    $missingChecklist = @($configReport.ManualAcceptanceChecklist | Where-Object { $_.Status -ne "Recorded" })
    return [PSCustomObject]@{
        GameIniPresent = [bool]$configReport.GameIniPresent
        GameIniGitState = [string]$configReport.Summary.GameIniGitState
        RuntimeOverridePresent = [bool]$configReport.RuntimeOverridePresent
        RuntimeOverrideKeyCount = @($configReport.RuntimeOverrideKeys).Count
        NonEmptyRuntimeOverrideKeyCount = @($configReport.NonEmptyRuntimeOverrideKeys).Count
        ValuesRedacted = [bool]$configReport.ValuesRedacted
        ReviewQueue = [string]$configReport.Summary.ReviewQueue
        CommitReadiness = [string]$configReport.Summary.CommitReadiness
        EvidenceStatus = [string]$configReport.Summary.EvidenceStatus
        EvidenceSatisfied = [bool]$configReport.Summary.EvidenceSatisfied
        MissingEvidenceCount = [int]$configReport.Summary.MissingEvidenceCount
        ManualAcceptanceMissingCount = [int]$configReport.Summary.ManualAcceptanceMissingCount
        MissingAcceptanceItems = @($missingChecklist | Select-Object -ExpandProperty Name)
        ReadyToStage = [bool]$configReport.Summary.ReadyToStage
        StagingBlocked = [bool]$configReport.Summary.StagingBlocked
        ManualConfigOwnerDecisionStillRequired = [bool]$configReport.Summary.ManualConfigOwnerDecisionStillRequired
        AcceptanceTemplateAvailable = [bool]$configReport.Summary.AcceptanceTemplateAvailable
        AcceptanceTemplateRequiredEvidenceCount = [int]$configReport.Summary.AcceptanceTemplateRequiredEvidenceCount
        UnexpectedGameIniStaged = ([string]$configReport.Summary.GameIniGitState -eq "Staged" -and -not [bool]$configReport.Summary.ReadyToStage)
        MustRemainLocal = ([bool]$configReport.GameIniPresent -and -not [bool]$configReport.Summary.ReadyToStage)
        Recommendation = [string]$configReport.Recommendation
        Boundary = "Config/Game.ini stays local unless config owner accepts shared defaults with redacted diff, no endpoint/credential evidence, and runtime policy pass."
    }
}

function Get-JudgingServerAcceptanceSummary {
    param([string]$ProjectRoot)

    $payloadContractReportScript = Join-Path $script:PSScriptRoot "export_payload_contract_report.ps1"
    $acceptanceTemplateScript = Join-Path $script:PSScriptRoot "export_judging_server_acceptance_template.ps1"
    if ((-not (Test-Path -LiteralPath $payloadContractReportScript -PathType Leaf)) -or
        (-not (Test-Path -LiteralPath $acceptanceTemplateScript -PathType Leaf))) {
        return $null
    }

    $templateJson = & powershell -ExecutionPolicy Bypass -File $acceptanceTemplateScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Judging server acceptance template failed with exit code $LASTEXITCODE"
    }
    $template = $templateJson | ConvertFrom-Json

    $contractJson = & powershell -ExecutionPolicy Bypass -File $payloadContractReportScript -OutputRoot (Join-Path $ProjectRoot "Saved\PayloadContractReports") -NoWrite -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Payload contract report failed with exit code $LASTEXITCODE"
    }
    $contract = $contractJson | ConvertFrom-Json

    return [PSCustomObject]@{
        AcceptanceTemplateAvailable = [bool]$contract.Summary.JudgingServerAcceptanceTemplateAvailable
        RequiredEvidenceCount = [int]$template.Summary.RequiredEvidenceCount
        PendingEvidenceCount = [int]$template.Summary.PendingEvidenceCount
        ValuesRedacted = [bool]$template.Summary.ValuesRedacted
        ModifiesConfig = [bool]$template.Summary.ModifiesConfig
        StagesConfig = [bool]$template.Summary.StagesConfig
        WritesEndpointValues = [bool]$template.Summary.WritesEndpointValues
        WritesCredentialValues = [bool]$template.Summary.WritesCredentialValues
        RealJudgingServerAcceptancePresent = [bool]$contract.Summary.RealJudgingServerAcceptancePresent
        OpenServerAcceptanceDecisionCount = [int]$contract.Summary.OpenServerAcceptanceDecisionCount
        RealServerEvidenceGapCount = [int]$contract.Summary.RealServerEvidenceGapCount
        CurrentReadyToClaimRealServerAcceptance = [bool]$template.Summary.CurrentReadyToClaimRealServerAcceptance
        RecommendedNextAction = [string]$contract.Summary.RecommendedNextAction
        Boundary = "Local fixture/mock/loopback evidence is not real judging-server acceptance. Claim real acceptance only after endpoint/auth/response/rate evidence is recorded with no secrets in repo."
    }
}

function Get-LazExportDecisionSummary {
    param([string]$ProjectRoot)

    $decisionReportScript = Join-Path $script:PSScriptRoot "export_laz_compression_decision_report.ps1"
    $readinessReportScript = Join-Path $script:PSScriptRoot "export_laz_compressor_readiness_report.ps1"
    if ((-not (Test-Path -LiteralPath $decisionReportScript -PathType Leaf)) -or
        (-not (Test-Path -LiteralPath $readinessReportScript -PathType Leaf))) {
        return $null
    }

    $decisionJson = & powershell -ExecutionPolicy Bypass -File $decisionReportScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "LAZ compression decision report failed with exit code $LASTEXITCODE"
    }

    $readinessJson = & powershell -ExecutionPolicy Bypass -File $readinessReportScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "LAZ compressor readiness report failed with exit code $LASTEXITCODE"
    }

    $decisionReport = $decisionJson | ConvertFrom-Json
    $readinessReport = $readinessJson | ConvertFrom-Json
    $acceptanceEvidence = @($decisionReport.AcceptanceEvidenceNeeded)
    $candidatePaths = @($decisionReport.CandidatePaths | Select-Object -ExpandProperty Option)

    return [PSCustomObject]@{
        PlaceholderExplicit = [bool]$decisionReport.Summary.PlaceholderExplicit
        WritesLasSourceOnly = [bool]$decisionReport.Summary.WritesLasSourceOnly
        ExternalCompressorOptInImplemented = [bool]$decisionReport.Summary.ExternalCompressorOptInImplemented
        ExternalCompressorContractHardened = [bool]$decisionReport.Summary.ExternalCompressorContractHardened
        MissingCompressorGuardCovered = [bool]$decisionReport.Summary.MissingCompressorGuardCovered
        ExternalCompressorSuccessCovered = [bool]$decisionReport.Summary.ExternalCompressorSuccessCovered
        CompressorReadinessReportDeclared = [bool]$decisionReport.Summary.CompressorReadinessReportDeclared
        AutomationCoverageDeclared = [bool]$decisionReport.Summary.AutomationCoverageDeclared
        TrueCompressionIntegrated = [bool]$decisionReport.Summary.TrueCompressionIntegrated
        CompressorCandidateFound = [bool]$readinessReport.Summary.CompressorCandidateFound
        ReaderCandidateFound = [bool]$readinessReport.Summary.ReaderCandidateFound
        LazEvidenceFilePresent = [bool]$readinessReport.Summary.LazEvidenceFilePresent
        ReaderProbeRequested = [bool]$readinessReport.Summary.ReaderProbeRequested
        ReaderProbeBlockedReason = [string]$readinessReport.Summary.ReaderProbeBlockedReason
        KnownPointCloudReader = [bool]$readinessReport.Summary.KnownPointCloudReader
        ReaderProbeSucceeded = [bool]$readinessReport.Summary.ReaderProbeSucceeded
        ReadableOutputEvidencePresent = [bool]$readinessReport.Summary.ReadableOutputEvidencePresent
        ReadyForRealLazAutomation = [bool]$readinessReport.Summary.ReadyForRealLazAutomation
        CandidateToolCount = [int]$readinessReport.Summary.CandidateToolCount
        FoundToolCount = [int]$readinessReport.Summary.FoundToolCount
        CandidatePathCount = [int]$decisionReport.Summary.CandidatePathCount
        CandidatePaths = $candidatePaths
        AcceptanceEvidenceCount = [int]$decisionReport.Summary.AcceptanceEvidenceCount
        AcceptanceEvidenceNeeded = $acceptanceEvidence
        RecommendedNextDecision = [string]$decisionReport.Summary.RecommendedNextDecision
        RecommendedNextAction = [string]$readinessReport.Summary.RecommendedNextAction
        ReadyToClaimTrueLaz = ([bool]$decisionReport.Summary.TrueCompressionIntegrated -and [bool]$readinessReport.Summary.ReadyForRealLazAutomation)
        StagingBoundary = "Do not claim true LAZ until a produced .laz file is readable by a known point-cloud reader and the selected compressor/native/server workflow is accepted."
    }
}

function Get-PointCloudRendererDecisionSummary {
    param([string]$ProjectRoot)

    $rendererDecisionReportScript = Join-Path $script:PSScriptRoot "export_point_cloud_renderer_decision_report.ps1"
    if (-not (Test-Path -LiteralPath $rendererDecisionReportScript -PathType Leaf)) {
        return $null
    }

    $jsonText = & powershell -ExecutionPolicy Bypass -File $rendererDecisionReportScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Point cloud renderer decision report failed with exit code $LASTEXITCODE"
    }

    $rendererReport = $jsonText | ConvertFrom-Json
    $candidateRenderers = @($rendererReport.CandidateRenderers | Select-Object -ExpandProperty Option)
    $gpuMissingFields = @($rendererReport.Summary.GpuViewportSmokeMissingEvidenceFields)

    return [PSCustomObject]@{
        RendererPhase = [string]$rendererReport.Summary.RendererPhase
        GpuRendererIntegrated = [bool]$rendererReport.Summary.GpuRendererIntegrated
        RecommendedFirstGpuCandidate = [string]$rendererReport.Summary.RecommendedFirstGpuCandidate
        CpuPreviewFallbackEvidencePresent = [bool]$rendererReport.Summary.CpuPreviewFallbackEvidencePresent
        CpuFallbackPerformanceEvidencePresent = [bool]$rendererReport.Summary.CpuFallbackPerformanceEvidencePresent
        CpuIsmFallbackSmokePresent = [bool]$rendererReport.Summary.CpuIsmFallbackSmokePresent
        CpuProceduralDenseEvidencePresent = [bool]$rendererReport.Summary.CpuProceduralDenseEvidencePresent
        CsvPerformanceEvidencePresent = [bool]$rendererReport.Summary.CsvPerformanceEvidencePresent
        CsvEvidenceLinesWithinRun = [bool]$rendererReport.Summary.CsvEvidenceLinesWithinRun
        CsvFailureEvidencePresent = [bool]$rendererReport.Summary.CsvFailureEvidencePresent
        CsvPreviewMaxAcceptedPoints = [int64]$rendererReport.Summary.CsvPreviewMaxAcceptedPoints
        GpuViewportSmokeEvidencePresent = [bool]$rendererReport.Summary.GpuViewportSmokeEvidencePresent
        GpuViewportSmokeMissingEvidenceFieldCount = [int]$rendererReport.Summary.GpuViewportSmokeMissingEvidenceFieldCount
        GpuViewportSmokeMissingEvidenceFields = $gpuMissingFields
        GpuFallbackPreservationEvidencePresent = [bool]$rendererReport.Summary.GpuFallbackPreservationEvidencePresent
        GpuDenseFrameEvidencePresent = [bool]$rendererReport.Summary.GpuDenseFrameEvidencePresent
        GpuSpikeActionPlanDeclared = [bool]$rendererReport.Summary.GpuSpikeActionPlanDeclared
        GpuSpikeActionPlanItemCount = [int]$rendererReport.Summary.GpuSpikeActionPlanItemCount
        CandidateRendererCount = [int]$rendererReport.Summary.CandidateRendererCount
        CandidateRenderers = $candidateRenderers
        DecisionGateCount = [int]$rendererReport.Summary.DecisionGateCount
        AcceptanceEvidenceCount = [int]$rendererReport.Summary.AcceptanceEvidenceCount
        RecommendedNextDecision = [string]$rendererReport.Summary.RecommendedNextDecision
        ReadyToClaimGpuDensePreview = ([bool]$rendererReport.Summary.GpuRendererIntegrated -and [bool]$rendererReport.Summary.GpuViewportSmokeEvidencePresent -and [bool]$rendererReport.Summary.GpuFallbackPreservationEvidencePresent -and [bool]$rendererReport.Summary.GpuDenseFrameEvidencePresent)
        Boundary = "Do not claim dense GPU/Niagara preview readiness until the GPU path exists and viewport, fallback-preservation, and dense-frame evidence are recorded."
    }
}

function Get-ReadinessSummary {
    param(
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    $readinessScript = Join-Path $script:PSScriptRoot "check_project_readiness.ps1"
    if (-not (Test-Path -LiteralPath $readinessScript -PathType Leaf)) {
        return $null
    }

    $params = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $readinessScript,
        "-ProjectRoot", $ProjectRoot,
        "-SkipSmoke",
        "-SkipWebSocketLidarSmokeEvidence",
        "-SkipWebSocketBrokerSmokeReport",
        "-SkipLazPlaceholderPolicy",
        "-Json"
    )
    if (-not [string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
        $params += @("-SourceRepoRoot", $SourceRepoRoot)
    }

    $jsonText = & powershell @params
    if ($LASTEXITCODE -ne 0) {
        throw "Project readiness summary failed with exit code $LASTEXITCODE"
    }

    $readiness = $jsonText | ConvertFrom-Json
    $skippedSteps = @($readiness.Steps | Where-Object { $_.Status -eq "Skipped" })
    return [PSCustomObject]@{
        Passed = [bool]$readiness.Passed
        FastReadinessPassed = [bool]$readiness.Passed
        Mode = "FastStaticPrecommit"
        ReadinessMode = "FastStaticPrecommit"
        ManualEvidenceStillRequired = ($skippedSteps.Count -gt 0)
        ReadinessCaveat = "Passed means fast/static pre-commit gates passed, not full PIE, broker, LAZ, editor, or deployment evidence complete."
        Boundary = "Skipped readiness steps are intentional omissions from this fast pre-commit check, not proof that manual/editor/deployment evidence exists."
        ProjectRoot = [string]$readiness.ProjectRoot
        SourceRepoRoot = [string]$readiness.SourceRepoRoot
        StepCount = [int]$readiness.StepCount
        PassedStepCount = [int]$readiness.PassedStepCount
        SkippedStepCount = [int]$readiness.SkippedStepCount
        SkippedSteps = @($skippedSteps | Select-Object -ExpandProperty Label)
        SkippedByPrecommitPolicy = @($skippedSteps | Select-Object -ExpandProperty Label)
        SkippedStepDetails = @(
            $skippedSteps |
                ForEach-Object {
                    [PSCustomObject]@{
                        Label = $_.Label
                        Reason = $_.Message
                        EvidenceBoundary = "Not covered by fast readiness."
                    }
                }
        )
    }
}

function Get-RepoChangeSummary {
    param([string]$RepoRoot)

    $statusLines = Get-GitLines -WorkingDirectory $RepoRoot -GitArgs @("status", "--porcelain=v1")
    $stagedChanges = @()
    $unstagedChanges = @()
    $untrackedChanges = @()

    foreach ($line in $statusLines) {
        if ([string]::IsNullOrWhiteSpace($line) -or $line.Length -lt 3) {
            continue
        }

        $indexStatus = $line.Substring(0, 1)
        $worktreeStatus = $line.Substring(1, 1)
        $path = $line.Substring(3)

        if ($indexStatus -eq "?" -and $worktreeStatus -eq "?") {
            $untrackedChanges += $path
            continue
        }

        if ($indexStatus -ne " ") {
            $stagedChanges += "$indexStatus`t$path"
        }
        if ($worktreeStatus -ne " ") {
            $unstagedChanges += "$worktreeStatus`t$path"
        }
    }

    return [PSCustomObject]@{
        Staged = $stagedChanges
        Unstaged = $unstagedChanges
        Untracked = $untrackedChanges
    }
}

function New-WorkArea {
    param(
        [string]$Name,
        [int]$Percent,
        [string]$Done,
        [string]$Remaining
    )

    return [PSCustomObject]@{
        Name = $Name
        Percent = $Percent
        Done = $Done
        Remaining = $Remaining
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Get-Location).Path).Path
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
    $SourceRepoRoot = $repoRoot
}
$SourceRepoRoot = (Resolve-Path -LiteralPath $SourceRepoRoot).Path

$workAreas = @(
    (New-WorkArea `
        -Name "Virtual sensor baseline" `
        -Percent 88 `
        -Done "LiDAR/camera payload, replay, preserved LiDAR grid coords, slab analysis, monitor fallback, monitor camera capture pending-state guards, monitor policy validation, smoke/readiness scripts are implemented." `
        -Remaining "Full editor PIE validation and production map/WBP verification remain."),
    (New-WorkArea `
        -Name "Server payload contract" `
        -Percent 89 `
        -Done "LiDAR/camera schema docs, compatibility notes, fixtures, fixture validator with camera base64/JPEG/byteSize and simulationQuality enum checks, preserved row/col/returnIndex contract, local mock contract validator with camera image invariant checks, schema review policy, server transport contract notes, weak HTTP callback handling, 2xx acceptance tracking, outbound HTTP POST loopback acceptance automation, and exportable contract review report are in place. The payload contract report now includes a server acceptance readiness matrix for endpoint ownership, authentication, retry/timeout, batching/backpressure, response schema, and real judging-server acceptance evidence. A read-only judging-server acceptance template now records the evidence gates needed to claim real server acceptance without writing endpoint or credential values. A judging-server acceptance package exporter now writes a local Saved/Reports bundle with payload contract, transport contract, fillable real-server evidence, manual steps, follow-up commands, and generated-artifact sensitive-pattern scanning without writing endpoint or credential values." `
        -Remaining "Judging server approval, completed acceptance template evidence, real server accepted/rejected response evidence, final endpoint/auth/retry/batching owner decisions, and server-owned response schema tests remain."),
    (New-WorkArea `
        -Name "Local project asset decisions" `
        -Percent 97 `
        -Done "Decision points are reported, unclassified untracked files and staged decision paths are gated, large/sample folders include content summaries, per-decision GitState/CommitReadiness/ReviewQueue/DecisionOwner/DecisionStatus/EvidenceNeeded/EvidenceStatus/EvidenceSatisfied/DecisionChecklist fields are exported, review queues separate ReadyToStage/NeedsOwnerDecision/KeepLocal paths, unresolved owner/evidence metadata is documented and validated, ReadyToStage now requires AcceptedForRepository with complete evidence plus reviewer/date/source evidence, duplicate normalized evidence paths are rejected, an evidence template exporter is available, the evidence workflow and staged decision gate are covered by temp-project automation, runtime config validation inspects the real local project and emits a Game.ini RecommendedDecision, WBP metadata/Git/setup-contract decision reporting is available, and empty DTCore runtime override Game.ini files are now classified as KeepLocal local placeholders instead of owner-acceptance candidates. Local asset reports now include ReviewPriority, CommitBlocker, BlockingReason, NextReviewAction, ActionPlan, large-content RequiredAcceptance, DecisionBlockers, and TopBlockers. The evidence template now exports Summary, pending evidence counts, and TopBlockingPaths for owner review. The currently unused large Content folders are now classified as KeepLocal unused cleanup candidates instead of repository-acceptance candidates. A read-only large content cleanup plan exporter now summarizes cleanup candidates, recoverable local disk size, required pre-delete checks, and deletion/staging safety boundaries without deleting files or modifying assets. A separate unused-content archive tool now previews optional local folder moves to an explicit archive root outside the project, requires reference-check confirmation for execution, and never deletes files, stages git changes, or modifies Unreal assets. Pixel Streaming copied sample files now have a repository-side setup alternative in docs/pixel_streaming_setup.md, and the sample decision report/pre-commit summary point to that documentation path while keeping Samples/PixelStreaming untracked until ownership and redistribution evidence are accepted. A read-only DTCore submodule guard now checks that Plugins/DTCore stays pinned to 2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7 with no parent, staged, or submodule worktree changes. A consolidated local decision pre-commit gate now runs the DTCore guard, staged local-decision gate, sample staged-path gate, large-content policy, runtime-config policy, monitor widget policy, WBP preflight, WBP evidence report, and pre-commit summary in one read-only command. A dedicated local decision pre-commit gate policy validator now proves the default gate passes for the current known-local state while strict evidence mode fails until WBP acceptance evidence is complete. A monitor WBP acceptance package exporter now writes a local Saved/Reports bundle with preflight, decision, fillable evidence draft, validation summary, manual steps, and strict follow-up commands without modifying assets or staging files. The pre-commit summary now consumes the large-content cleanup plan, archive tool preview, large-content decision report, sample content decision report, DTCore submodule guard, monitor-WBP decision report, monitor-WBP preflight report, and monitor-WBP acceptance evidence validator, surfacing unused cleanup candidate count/size, dry-run cleanup plan status, archive safety status, sample/third-party candidate count/size, sample setup documentation path, DTCore invariant status, WBP Git/evidence state, missing WBP acceptance items, WBP preflight readiness, WBP evidence validation completeness, the largest cleanup/sample blockers, and the remaining repository-acceptance candidate paths. The focused monitor WBP decision report and runtime config decision report now reuse the local asset decision engine, accept EvidencePath, expose ReviewQueue/CommitReadiness/EvidenceStatus/MissingEvidenceCount/ReadyToStage, export manual acceptance checklists, and can fail on incomplete evidence as opt-in pre-commit gates. The dedicated WBP acceptance evidence validator checks the accepted evidence file, SHA256 asset hash, editor-open evidence, optional binding crash safety, PIE smoke evidence, and production owner acceptance without modifying assets or staging the binary WBP. The large content decision report now flags BuiltDataHeavy, LargestFileRisk, StorageRiskReason, RedistributionReviewRequired, SampleRiskReason, UnusedLocalCleanupCandidate, RepositoryAcceptanceRequired, and CleanupReason for owner review. The project readiness wrapper now accepts SourceRepoRoot so source docs/policies can be checked while local Unreal asset/config decisions are scanned from the real project root, and the pre-commit summary can include the fast readiness JSON result with skipped-step evidence boundaries." `
        -Remaining "Manual WBP editor-open/binding/PIE acceptance evidence file completion, optional execution of unused-content archive after map/WBP/reference checks with an explicit outside-project archive root, PixelStreaming project ownership/license/documentation-alternative acceptance, non-empty Game.ini endpoint/credential review if values are added later, and any final AcceptedForRepository evidence remain."),
    (New-WorkArea `
        -Name "Real sensor adapters" `
        -Percent 82 `
        -Done "ROS2/Livox/RealSense placeholders, normalized frame handoff path, replay samples, LiDAR JSON live bridge component, camera JSON live bridge component, generic live JSON payload bridge helper, optional HTTP and UDP JSON live bridge components, HTTP payload automation, local HTTP loopback POST smoke automation, local UDP datagram smoke automation, DTCore WebSocket transaction handler, safe routing guards, WebSocket live sample payload, editor sample/push helpers, transaction routing automation, static adapter-plan validation, exportable WebSocket transaction registration checklist, optional data-table registration evidence automation, editor-only row repair commandlet, DT_TransactionCode row pass evidence, row-based handler parse/process evidence, broker smoke evidence report tooling, WebSocket LiDAR smoke evidence workflow wrapper, brokerless DTCore dispatch automation, opt-in brokerless workflow execution, hardened camera JSON live payload validation with transport/recorder evidence, expanded malformed camera schema rejection coverage, and a consolidated real-sensor adapter readiness report are in place. The readiness report ranks deployment paths and exports RequiredEvidence, DeploymentBlockers, NextAction, and DeploymentActionPlan entries for replay, HTTP, WebSocket, UDP, and SDK routes. A real-sensor adapter deployment package exporter now writes a local Saved/Reports bundle with readiness, adapter-plan validation, WebSocket sample validation, transaction registration, broker-smoke draft, manual steps, and follow-up commands while explicitly avoiding external broker/SDK connections, asset edits, git staging, endpoint values, and credential values. Broker smoke reporting now requires concrete evidence fields such as run id, map/PIE session, log path, before/after frame counts, target point count, cached payload bytes or hash, broker client command, operator, and notes before marking the smoke complete." `
        -Remaining "Actual SDK/ROS2/Livox/RealSense connections, completed deployment STOMP/WebSocket broker PIE smoke evidence using the required evidence schema, HTTP/UDP deployment exposure and credential decisions, final production adapter owner approval, and successful real-frame smoke tests remain."),
    (New-WorkArea `
        -Name "Large point cloud rendering" `
        -Percent 71 `
        -Done "Server payload and preview policies are separated with preview caps, runtime warnings, point-cloud-only clamps, batched ISM AddInstances live preview uploads, procedural CSV high-density preview automation, instanced fallback automation, CSV preview parse/build/load telemetry, generous headless procedural performance-budget automation, exportable CSV preview performance evidence from Unreal automation logs, static preview-policy validation, and a high-density renderer decision report covering CPU fallback, Niagara, custom GPU buffers, and external viewer workflows. The renderer decision report consumes local CSV preview performance evidence, separates ISM smoke from procedural dense evidence, rejects failure lines inside the selected automation run block, records decision gates, recommends a Niagara point-renderer spike while preserving CPU preview fallback, exports GPU spike action-plan, renderer phase, viewport smoke, fallback-preservation, and dense-frame evidence gates, and is now surfaced in the pre-commit summary as a renderer decision/readiness boundary. A point cloud renderer acceptance package exporter now writes a local Saved/Reports bundle with the decision report, preview-policy validation, optional CSV performance evidence, GPU smoke follow-up commands, and explicit no-GPU-implementation/no-asset-edit/no-staging boundaries." `
        -Remaining "Niagara spike implementation, actual viewport screenshot/nonblank pixel evidence, renderer-specific dense-frame performance validation, fallback-preservation verification after GPU integration, and final GPU asset/module ownership remain."),
    (New-WorkArea `
        -Name "LAZ export" `
        -Percent 56 `
        -Done "Placeholder behavior is explicit, tested as LAS-compatible source export, covered by static placeholder-policy validation, supported by a compression-path decision report, and now has an opt-in external compressor path with missing-compressor guard automation plus a positive external process-contract automation that verifies `{input}`/`{output}` handling and non-empty `.laz` output creation. A local compressor readiness report scans compressor/reader candidates, records that tool readiness is not readable-output evidence, is included in the project readiness gate, accepts explicit `.laz` evidence with an optional known-reader probe plus blocked-probe reporting, and is now surfaced in the pre-commit summary as a LAZ decision/readiness boundary. A LAZ compression acceptance package exporter now writes a local Saved/Reports bundle with the decision report, compressor readiness report, placeholder policy validation, manual acceptance steps, and follow-up commands while explicitly avoiding tool installation, compressor execution, asset edits, and git staging." `
        -Remaining "Accepted compressor/tool selection, actual readable compressed `.laz` output evidence, native/server workflow decision if external CLI is not enough, and true compressed-output automation remain.")
)

$overallPercent = [int][Math]::Round((($workAreas | Measure-Object -Property Percent -Average).Average), 0)
$changeSummary = Get-RepoChangeSummary -RepoRoot $repoRoot
$staged = $changeSummary.Staged
$unstaged = $changeSummary.Unstaged
$untracked = $changeSummary.Untracked
$branch = (Get-GitLines -WorkingDirectory $repoRoot -GitArgs @("branch", "--show-current") | Select-Object -First 1)
$recentCommit = (Get-GitLines -WorkingDirectory $repoRoot -GitArgs @("log", "--oneline", "-1") | Select-Object -First 1)
$localAssetReport = Get-LocalAssetSummary -ProjectRoot $ProjectRoot
$largeContentDecisionSummary = Get-LargeContentDecisionSummary -ProjectRoot $ProjectRoot
$dtCoreSubmoduleGuardSummary = Get-DTCoreSubmoduleGuardSummary -ProjectRoot $ProjectRoot
$wbpDecisionSummary = Get-WbpDecisionSummary -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
$wbpPreflightSummary = Get-WbpPreflightSummary -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
$wbpAcceptanceEvidenceSummary = Get-WbpAcceptanceEvidenceSummary -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
$runtimeConfigDecisionSummary = Get-RuntimeConfigDecisionSummary -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
$judgingServerAcceptanceSummary = Get-JudgingServerAcceptanceSummary -ProjectRoot $SourceRepoRoot
$lazExportDecisionSummary = Get-LazExportDecisionSummary -ProjectRoot $SourceRepoRoot
$pointCloudRendererDecisionSummary = Get-PointCloudRendererDecisionSummary -ProjectRoot $SourceRepoRoot
$readinessSummary = if ($IncludeReadiness) { Get-ReadinessSummary -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot } else { $null }

$report = [PSCustomObject]@{
    RepositoryRoot = $repoRoot
    LocalProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    Branch = $branch
    RecentCommit = $recentCommit
    OverallPercent = $overallPercent
    WorkAreas = $workAreas
    StagedChanges = $staged
    UnstagedChanges = $unstaged
    UntrackedChanges = $untracked
    LocalAssetSummary = if ($localAssetReport) { $localAssetReport.Summary } else { $null }
    LargeContentDecisionSummary = $largeContentDecisionSummary
    DTCoreSubmoduleGuardSummary = $dtCoreSubmoduleGuardSummary
    WbpDecisionSummary = $wbpDecisionSummary
    WbpPreflightSummary = $wbpPreflightSummary
    WbpAcceptanceEvidenceSummary = $wbpAcceptanceEvidenceSummary
    RuntimeConfigDecisionSummary = $runtimeConfigDecisionSummary
    JudgingServerAcceptanceSummary = $judgingServerAcceptanceSummary
    LazExportDecisionSummary = $lazExportDecisionSummary
    PointCloudRendererDecisionSummary = $pointCloudRendererDecisionSummary
    ReadinessSummary = $readinessSummary
}

if ($Json) {
    $report | ConvertTo-Json -Depth 6
    exit 0
}

Write-Section "Progress"
Write-Host "Overall progress: $overallPercent%"
foreach ($area in $workAreas) {
    Write-Host "$($area.Percent)% - $($area.Name)"
    Write-Host "  done: $($area.Done)"
    Write-Host "  remaining: $($area.Remaining)"
}

Write-Section "Git"
Write-Host "Branch: $branch"
Write-Host "Latest commit: $recentCommit"

Write-Section "Commit candidates"
if ($staged.Count -gt 0) {
    Write-Host "Staged:"
    $staged | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host "Staged: none"
}

if ($unstaged.Count -gt 0) {
    Write-Host "Unstaged:"
    $unstaged | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host "Unstaged: none"
}

if ($untracked.Count -gt 0) {
    Write-Host "Untracked:"
    $untracked | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host "Untracked: none"
}

if ($localAssetReport) {
    Write-Section "Local asset decisions"
    Write-Host "Present decision points: $($localAssetReport.Summary.PresentDecisionPoints)"
    Write-Host "Generated/local-output items present: $($localAssetReport.Summary.GeneratedOrLocalOutputItemsPresent)"
    Write-Host "Unclassified untracked paths: $($localAssetReport.Summary.UnclassifiedUntrackedCount)"
}

if ($largeContentDecisionSummary) {
    Write-Section "Large content cleanup"
    Write-Host "Unused cleanup candidates: $($largeContentDecisionSummary.UnusedCleanupCandidateCount)"
    Write-Host "Known unused cleanup candidate max count: $($largeContentDecisionSummary.KnownUnusedCleanupCandidateMaxCount)"
    Write-Host "Present known unused cleanup candidate count: $($largeContentDecisionSummary.PresentKnownUnusedCleanupCandidateCount)"
    Write-Host "Known unused cleanup candidate absent or archived count: $($largeContentDecisionSummary.KnownUnusedCleanupCandidateAbsentOrArchivedCount)"
    Write-Host "Unused cleanup size: $($largeContentDecisionSummary.UnusedCleanupSize)"
    Write-Host "Largest cleanup candidate: $($largeContentDecisionSummary.LargestCleanupCandidatePath) ($($largeContentDecisionSummary.LargestCleanupCandidateSize))"
    Write-Host "Repository-acceptance candidates: $($largeContentDecisionSummary.RepositoryAcceptanceCandidateCount)"
    if ($largeContentDecisionSummary.RepositoryAcceptanceCandidatePaths.Count -gt 0) {
        Write-Host "Repository-acceptance paths: $(@($largeContentDecisionSummary.RepositoryAcceptanceCandidatePaths) -join ', ')"
    }
    Write-Host "Boundary: $($largeContentDecisionSummary.CleanupBoundary)"
    Write-Host "Cleanup plan available: $($largeContentDecisionSummary.CleanupPlanAvailable)"
    Write-Host "Cleanup plan candidates: $($largeContentDecisionSummary.CleanupPlanCandidateCount)"
    Write-Host "Cleanup plan recoverable size: $($largeContentDecisionSummary.CleanupPlanRecoverableSize)"
    Write-Host "Cleanup plan dry run only: $($largeContentDecisionSummary.CleanupPlanDryRunOnly)"
    Write-Host "Cleanup plan deletes files: $($largeContentDecisionSummary.CleanupPlanDeletesFiles)"
    Write-Host "Cleanup plan modifies assets: $($largeContentDecisionSummary.CleanupPlanModifiesAssets)"
    Write-Host "Cleanup plan required reference checks: $($largeContentDecisionSummary.CleanupPlanRequiredReferenceCheckCount)"
    Write-Host "Cleanup plan safe-to-delete count: $($largeContentDecisionSummary.CleanupPlanSafeToDeleteCount)"
    Write-Host "Cleanup plan ready-for-manual-deletion count: $($largeContentDecisionSummary.CleanupPlanReadyForManualDeletionCount)"
    Write-Host "Cleanup plan default action: $($largeContentDecisionSummary.CleanupPlanDefaultAction)"
    Write-Host "Unused content archive available: $($largeContentDecisionSummary.UnusedContentArchiveAvailable)"
    Write-Host "Unused content archive candidates: $($largeContentDecisionSummary.UnusedContentArchiveCandidateCount)"
    Write-Host "Unused content archive preview only: $($largeContentDecisionSummary.UnusedContentArchivePreviewOnly)"
    Write-Host "Unused content archive execute requested: $($largeContentDecisionSummary.UnusedContentArchiveExecuteRequested)"
    Write-Host "Unused content archive requires explicit archive root: $($largeContentDecisionSummary.UnusedContentArchiveRequiresExplicitArchiveRootForExecute)"
    Write-Host "Unused content archive deletes files: $($largeContentDecisionSummary.UnusedContentArchiveDeletesFiles)"
    Write-Host "Unused content archive stages files: $($largeContentDecisionSummary.UnusedContentArchiveStagesFiles)"
    Write-Host "Unused content archive modifies assets: $($largeContentDecisionSummary.UnusedContentArchiveModifiesAssets)"
    Write-Host "Unused content archive boundary: $($largeContentDecisionSummary.UnusedContentArchiveBoundary)"
    Write-Host "Archive evidence available: $($largeContentDecisionSummary.ArchiveEvidenceAvailable)"
    Write-Host "Archive evidence complete: $($largeContentDecisionSummary.ArchiveEvidenceComplete)"
    Write-Host "Archive evidence root: $($largeContentDecisionSummary.ArchiveEvidenceArchiveRoot)"
    Write-Host "Archive evidence latest run: $($largeContentDecisionSummary.ArchiveEvidenceLatestRun)"
    Write-Host "Archive evidence root outside project: $($largeContentDecisionSummary.ArchiveEvidenceRootOutsideProject)"
    Write-Host "Archive evidence archived count: $($largeContentDecisionSummary.ArchiveEvidenceArchivedCount)"
    Write-Host "Archive evidence present in project count: $($largeContentDecisionSummary.ArchiveEvidencePresentInProjectCount)"
    Write-Host "Archive evidence missing count: $($largeContentDecisionSummary.ArchiveEvidenceMissingCount)"
    Write-Host "Archive evidence archived size: $($largeContentDecisionSummary.ArchiveEvidenceArchivedSize)"
    Write-Host "Archive evidence root git state: $($largeContentDecisionSummary.ArchiveEvidenceRootGitState)"
    Write-Host "Archive evidence root staged file count: $($largeContentDecisionSummary.ArchiveEvidenceRootStagedFileCount)"
    Write-Host "Archive evidence DTCore touched file count: $($largeContentDecisionSummary.ArchiveEvidenceDTCoreTouchedFileCount)"
    Write-Host "Archive evidence boundary: $($largeContentDecisionSummary.ArchiveEvidenceBoundary)"

    Write-Section "Sample and third-party decisions"
    Write-Host "Sample/third-party candidates: $($largeContentDecisionSummary.SampleCandidateCount)"
    Write-Host "Sample/third-party size: $($largeContentDecisionSummary.SampleCandidateSize)"
    Write-Host "Largest sample candidate: $($largeContentDecisionSummary.LargestSampleCandidatePath) ($($largeContentDecisionSummary.LargestSampleCandidateSize))"
    if ($largeContentDecisionSummary.SampleCandidatePaths.Count -gt 0) {
        Write-Host "Sample/third-party paths: $(@($largeContentDecisionSummary.SampleCandidatePaths) -join ', ')"
    }
    Write-Host "Boundary: $($largeContentDecisionSummary.SampleBoundary)"
    Write-Host "Sample decision report available: $($largeContentDecisionSummary.SampleDecisionReportAvailable)"
    Write-Host "Sample decision ready-to-stage count: $($largeContentDecisionSummary.SampleDecisionReadyToStageCount)"
    Write-Host "Sample decision documentation alternative preferred: $($largeContentDecisionSummary.SampleDecisionDocumentationAlternativePreferredCount)"
    Write-Host "Sample decision setup alternative preferred: $($largeContentDecisionSummary.SampleDecisionSetupAlternativePreferredCount)"
    Write-Host "Sample decision must remain untracked: $($largeContentDecisionSummary.SampleDecisionMustRemainUntrackedCount)"
    Write-Host "Sample decision missing acceptance count: $($largeContentDecisionSummary.SampleDecisionMissingAcceptanceCount)"
    Write-Host "Sample decision dry run only: $($largeContentDecisionSummary.SampleDecisionDryRunOnly)"
    Write-Host "Sample decision copies sample files: $($largeContentDecisionSummary.SampleDecisionCopiesSampleFiles)"
    Write-Host "Sample decision setup documentation: $($largeContentDecisionSummary.SampleDecisionSetupDocumentationPath)"
    Write-Host "Sample decision unexpected sample staged: $($largeContentDecisionSummary.SampleDecisionUnexpectedSampleStaged)"
    Write-Host "Sample decision staged sample path count: $($largeContentDecisionSummary.SampleDecisionStagedSamplePathCount)"
    if ($largeContentDecisionSummary.SampleDecisionStagedSamplePaths.Count -gt 0) {
        Write-Host "Sample decision staged sample paths: $(@($largeContentDecisionSummary.SampleDecisionStagedSamplePaths) -join ', ')"
    }
    Write-Host "Sample decision default action: $($largeContentDecisionSummary.SampleDecisionDefaultAction)"
}

if ($dtCoreSubmoduleGuardSummary) {
    Write-Section "DTCore submodule guard"
    Write-Host "Invariant valid: $($dtCoreSubmoduleGuardSummary.DTCoreInvariantValid)"
    Write-Host "Expected commit: $($dtCoreSubmoduleGuardSummary.ExpectedCommit)"
    Write-Host "Actual commit: $($dtCoreSubmoduleGuardSummary.ActualCommit)"
    Write-Host "DTCore submodule status: $($dtCoreSubmoduleGuardSummary.DTCoreSubmoduleStatus)"
    Write-Host "DTCore gitlink staged: $($dtCoreSubmoduleGuardSummary.DTCoreGitlinkStaged)"
    Write-Host "DTCore gitlink modified: $($dtCoreSubmoduleGuardSummary.DTCoreGitlinkModified)"
    Write-Host "DTCore worktree clean: $($dtCoreSubmoduleGuardSummary.DTCoreWorktreeClean)"
    Write-Host "Submodule present: $($dtCoreSubmoduleGuardSummary.SubmodulePresent)"
    Write-Host "Expected commit matches: $($dtCoreSubmoduleGuardSummary.ExpectedCommitMatches)"
    Write-Host "Submodule status clean: $($dtCoreSubmoduleGuardSummary.SubmoduleStatusClean)"
    Write-Host "Parent DTCore status lines: $($dtCoreSubmoduleGuardSummary.ParentDTCoreStatusLineCount)"
    Write-Host "Staged DTCore paths: $($dtCoreSubmoduleGuardSummary.StagedDTCorePathCount)"
    Write-Host "Submodule worktree lines: $($dtCoreSubmoduleGuardSummary.SubmoduleWorktreeLineCount)"
    Write-Host "Violation count: $($dtCoreSubmoduleGuardSummary.ViolationCount)"
    Write-Host "Dry run only: $($dtCoreSubmoduleGuardSummary.DryRunOnly)"
    Write-Host "Read only: $($dtCoreSubmoduleGuardSummary.ReadOnly)"
    Write-Host "Modifies submodule: $($dtCoreSubmoduleGuardSummary.ModifiesSubmodule)"
    Write-Host "Stages DTCore: $($dtCoreSubmoduleGuardSummary.StagesDTCore)"
    Write-Host "Does not update submodule: $($dtCoreSubmoduleGuardSummary.DoesNotUpdateSubmodule)"
    Write-Host "Does not stage files: $($dtCoreSubmoduleGuardSummary.DoesNotStageFiles)"
    Write-Host "Boundary: $($dtCoreSubmoduleGuardSummary.Boundary)"
}

if ($wbpDecisionSummary) {
    Write-Section "Monitor WBP decision"
    Write-Host "WBP present: $($wbpDecisionSummary.WbpPresent)"
    Write-Host "WBP path: $($wbpDecisionSummary.WbpRelativePath)"
    Write-Host "WBP size: $($wbpDecisionSummary.WbpSize)"
    Write-Host "Git state: $($wbpDecisionSummary.GitState)"
    Write-Host "Review queue: $($wbpDecisionSummary.ReviewQueue)"
    Write-Host "Commit readiness: $($wbpDecisionSummary.CommitReadiness)"
    Write-Host "Evidence status: $($wbpDecisionSummary.EvidenceStatus)"
    Write-Host "Evidence satisfied: $($wbpDecisionSummary.EvidenceSatisfied)"
    Write-Host "Missing evidence count: $($wbpDecisionSummary.MissingEvidenceCount)"
    Write-Host "Missing acceptance items: $(@($wbpDecisionSummary.MissingAcceptanceItems) -join ', ')"
    Write-Host "Recommended decision: $($wbpDecisionSummary.RecommendedDecision)"
    Write-Host "Monitor policy valid: $($wbpDecisionSummary.MonitorPolicyValid)"
    Write-Host "Setup doc contract complete: $($wbpDecisionSummary.SetupDocContractComplete)"
    Write-Host "Ready to stage: $($wbpDecisionSummary.ReadyToStage)"
    Write-Host "Staging blocked: $($wbpDecisionSummary.StagingBlocked)"
    Write-Host "Manual editor verification still required: $($wbpDecisionSummary.ManualEditorVerificationStillRequired)"
    Write-Host "Acceptance template available: $($wbpDecisionSummary.AcceptanceTemplateAvailable)"
    Write-Host "Acceptance template required evidence count: $($wbpDecisionSummary.AcceptanceTemplateRequiredEvidenceCount)"
    Write-Host "Unexpected WBP staged: $($wbpDecisionSummary.UnexpectedWbpStaged)"
    Write-Host "Must remain untracked: $($wbpDecisionSummary.MustRemainUntracked)"
    Write-Host "Blocking reason: $($wbpDecisionSummary.BlockingReason)"
    Write-Host "Next review action: $($wbpDecisionSummary.NextReviewAction)"
    Write-Host "Boundary: $($wbpDecisionSummary.Boundary)"
}

if ($wbpPreflightSummary) {
    Write-Section "Monitor WBP preflight"
    Write-Host "WBP present: $($wbpPreflightSummary.WbpPresent)"
    Write-Host "WBP untracked: $($wbpPreflightSummary.WbpUntracked)"
    Write-Host "WBP staged: $($wbpPreflightSummary.WbpStaged)"
    Write-Host "WBP worktree modified: $($wbpPreflightSummary.WbpWorktreeModified)"
    Write-Host "Asset hash: $($wbpPreflightSummary.AssetHash)"
    Write-Host "Setup doc contract complete: $($wbpPreflightSummary.SetupDocContractComplete)"
    Write-Host "Missing setup term count: $($wbpPreflightSummary.MissingSetupTermCount)"
    Write-Host "Acceptance template available: $($wbpPreflightSummary.AcceptanceTemplateAvailable)"
    Write-Host "Required evidence count: $($wbpPreflightSummary.RequiredEvidenceCount)"
    Write-Host "Pending evidence count: $($wbpPreflightSummary.PendingEvidenceCount)"
    Write-Host "Decision ready to stage: $($wbpPreflightSummary.DecisionReadyToStage)"
    Write-Host "Decision missing evidence count: $($wbpPreflightSummary.DecisionMissingEvidenceCount)"
    Write-Host "Preflight checks: $($wbpPreflightSummary.PreflightCheckCount)"
    Write-Host "Blocked preflight checks: $($wbpPreflightSummary.BlockedPreflightCheckCount)"
    Write-Host "Ready for manual editor review: $($wbpPreflightSummary.ReadyForManualEditorReview)"
    Write-Host "Modifies assets: $($wbpPreflightSummary.ModifiesAssets)"
    Write-Host "Stages WBP: $($wbpPreflightSummary.StagesWbp)"
    Write-Host "Boundary: $($wbpPreflightSummary.Boundary)"
}

if ($wbpAcceptanceEvidenceSummary) {
    Write-Section "Monitor WBP acceptance evidence"
    Write-Host "Evidence file present: $($wbpAcceptanceEvidenceSummary.EvidenceFilePresent)"
    Write-Host "Evidence record present: $($wbpAcceptanceEvidenceSummary.EvidenceRecordPresent)"
    Write-Host "Required evidence count: $($wbpAcceptanceEvidenceSummary.RequiredEvidenceCount)"
    Write-Host "Passed checks: $($wbpAcceptanceEvidenceSummary.PassedCheckCount)"
    Write-Host "Failed checks: $($wbpAcceptanceEvidenceSummary.FailedCheckCount)"
    Write-Host "Ready to stage candidate: $($wbpAcceptanceEvidenceSummary.ReadyToStageCandidate)"
    Write-Host "WBP staged: $($wbpAcceptanceEvidenceSummary.WbpStaged)"
    Write-Host "Decision report ready to stage: $($wbpAcceptanceEvidenceSummary.DecisionReportReadyToStage)"
    Write-Host "Dry run only: $($wbpAcceptanceEvidenceSummary.DryRunOnly)"
    Write-Host "Modifies assets: $($wbpAcceptanceEvidenceSummary.ModifiesAssets)"
    Write-Host "Stages WBP: $($wbpAcceptanceEvidenceSummary.StagesWbp)"
    Write-Host "Boundary: $($wbpAcceptanceEvidenceSummary.Boundary)"
}

if ($runtimeConfigDecisionSummary) {
    Write-Section "Runtime config decision"
    Write-Host "Game.ini present: $($runtimeConfigDecisionSummary.GameIniPresent)"
    Write-Host "Git state: $($runtimeConfigDecisionSummary.GameIniGitState)"
    Write-Host "Runtime override present: $($runtimeConfigDecisionSummary.RuntimeOverridePresent)"
    Write-Host "Runtime override key count: $($runtimeConfigDecisionSummary.RuntimeOverrideKeyCount)"
    Write-Host "Non-empty runtime override key count: $($runtimeConfigDecisionSummary.NonEmptyRuntimeOverrideKeyCount)"
    Write-Host "Values redacted: $($runtimeConfigDecisionSummary.ValuesRedacted)"
    Write-Host "Review queue: $($runtimeConfigDecisionSummary.ReviewQueue)"
    Write-Host "Commit readiness: $($runtimeConfigDecisionSummary.CommitReadiness)"
    Write-Host "Evidence status: $($runtimeConfigDecisionSummary.EvidenceStatus)"
    Write-Host "Missing evidence count: $($runtimeConfigDecisionSummary.MissingEvidenceCount)"
    Write-Host "Missing acceptance items: $(@($runtimeConfigDecisionSummary.MissingAcceptanceItems) -join ', ')"
    Write-Host "Ready to stage: $($runtimeConfigDecisionSummary.ReadyToStage)"
    Write-Host "Staging blocked: $($runtimeConfigDecisionSummary.StagingBlocked)"
    Write-Host "Manual config owner decision still required: $($runtimeConfigDecisionSummary.ManualConfigOwnerDecisionStillRequired)"
    Write-Host "Acceptance template available: $($runtimeConfigDecisionSummary.AcceptanceTemplateAvailable)"
    Write-Host "Acceptance template required evidence count: $($runtimeConfigDecisionSummary.AcceptanceTemplateRequiredEvidenceCount)"
    Write-Host "Unexpected Game.ini staged: $($runtimeConfigDecisionSummary.UnexpectedGameIniStaged)"
    Write-Host "Must remain local: $($runtimeConfigDecisionSummary.MustRemainLocal)"
    Write-Host "Recommendation: $($runtimeConfigDecisionSummary.Recommendation)"
    Write-Host "Boundary: $($runtimeConfigDecisionSummary.Boundary)"
}

if ($judgingServerAcceptanceSummary) {
    Write-Section "Judging server acceptance"
    Write-Host "Acceptance template available: $($judgingServerAcceptanceSummary.AcceptanceTemplateAvailable)"
    Write-Host "Required evidence count: $($judgingServerAcceptanceSummary.RequiredEvidenceCount)"
    Write-Host "Pending evidence count: $($judgingServerAcceptanceSummary.PendingEvidenceCount)"
    Write-Host "Values redacted: $($judgingServerAcceptanceSummary.ValuesRedacted)"
    Write-Host "Modifies config: $($judgingServerAcceptanceSummary.ModifiesConfig)"
    Write-Host "Stages config: $($judgingServerAcceptanceSummary.StagesConfig)"
    Write-Host "Writes endpoint values: $($judgingServerAcceptanceSummary.WritesEndpointValues)"
    Write-Host "Writes credential values: $($judgingServerAcceptanceSummary.WritesCredentialValues)"
    Write-Host "Real judging-server acceptance present: $($judgingServerAcceptanceSummary.RealJudgingServerAcceptancePresent)"
    Write-Host "Open server acceptance decisions: $($judgingServerAcceptanceSummary.OpenServerAcceptanceDecisionCount)"
    Write-Host "Real server evidence gaps: $($judgingServerAcceptanceSummary.RealServerEvidenceGapCount)"
    Write-Host "Ready to claim real server acceptance: $($judgingServerAcceptanceSummary.CurrentReadyToClaimRealServerAcceptance)"
    Write-Host "Recommended next action: $($judgingServerAcceptanceSummary.RecommendedNextAction)"
    Write-Host "Boundary: $($judgingServerAcceptanceSummary.Boundary)"
}

if ($lazExportDecisionSummary) {
    Write-Section "LAZ export decision"
    Write-Host "Placeholder explicit: $($lazExportDecisionSummary.PlaceholderExplicit)"
    Write-Host "Writes LAS source only: $($lazExportDecisionSummary.WritesLasSourceOnly)"
    Write-Host "External compressor opt-in implemented: $($lazExportDecisionSummary.ExternalCompressorOptInImplemented)"
    Write-Host "External compressor contract hardened: $($lazExportDecisionSummary.ExternalCompressorContractHardened)"
    Write-Host "Missing-compressor guard covered: $($lazExportDecisionSummary.MissingCompressorGuardCovered)"
    Write-Host "External compressor success covered: $($lazExportDecisionSummary.ExternalCompressorSuccessCovered)"
    Write-Host "Compressor candidate found: $($lazExportDecisionSummary.CompressorCandidateFound)"
    Write-Host "Reader candidate found: $($lazExportDecisionSummary.ReaderCandidateFound)"
    Write-Host "Candidate tools: $($lazExportDecisionSummary.FoundToolCount)/$($lazExportDecisionSummary.CandidateToolCount) found"
    Write-Host "LAZ evidence file present: $($lazExportDecisionSummary.LazEvidenceFilePresent)"
    Write-Host "Reader probe requested: $($lazExportDecisionSummary.ReaderProbeRequested)"
    Write-Host "Reader probe blocked reason: $($lazExportDecisionSummary.ReaderProbeBlockedReason)"
    Write-Host "Readable output evidence present: $($lazExportDecisionSummary.ReadableOutputEvidencePresent)"
    Write-Host "Ready for real LAZ automation: $($lazExportDecisionSummary.ReadyForRealLazAutomation)"
    Write-Host "True compression integrated: $($lazExportDecisionSummary.TrueCompressionIntegrated)"
    Write-Host "Ready to claim true LAZ: $($lazExportDecisionSummary.ReadyToClaimTrueLaz)"
    Write-Host "Candidate paths: $(@($lazExportDecisionSummary.CandidatePaths) -join ', ')"
    Write-Host "Acceptance evidence needed: $(@($lazExportDecisionSummary.AcceptanceEvidenceNeeded) -join '; ')"
    Write-Host "Recommended next decision: $($lazExportDecisionSummary.RecommendedNextDecision)"
    Write-Host "Recommended next action: $($lazExportDecisionSummary.RecommendedNextAction)"
    Write-Host "Boundary: $($lazExportDecisionSummary.StagingBoundary)"
}

if ($pointCloudRendererDecisionSummary) {
    Write-Section "Point cloud renderer decision"
    Write-Host "Renderer phase: $($pointCloudRendererDecisionSummary.RendererPhase)"
    Write-Host "GPU renderer integrated: $($pointCloudRendererDecisionSummary.GpuRendererIntegrated)"
    Write-Host "Recommended first GPU candidate: $($pointCloudRendererDecisionSummary.RecommendedFirstGpuCandidate)"
    Write-Host "CPU preview fallback evidence present: $($pointCloudRendererDecisionSummary.CpuPreviewFallbackEvidencePresent)"
    Write-Host "CPU fallback performance evidence present: $($pointCloudRendererDecisionSummary.CpuFallbackPerformanceEvidencePresent)"
    Write-Host "CPU ISM fallback smoke present: $($pointCloudRendererDecisionSummary.CpuIsmFallbackSmokePresent)"
    Write-Host "CPU procedural dense evidence present: $($pointCloudRendererDecisionSummary.CpuProceduralDenseEvidencePresent)"
    Write-Host "CSV performance evidence present: $($pointCloudRendererDecisionSummary.CsvPerformanceEvidencePresent)"
    Write-Host "CSV evidence lines within run: $($pointCloudRendererDecisionSummary.CsvEvidenceLinesWithinRun)"
    Write-Host "CSV failure evidence present: $($pointCloudRendererDecisionSummary.CsvFailureEvidencePresent)"
    Write-Host "CSV preview max accepted points: $($pointCloudRendererDecisionSummary.CsvPreviewMaxAcceptedPoints)"
    Write-Host "GPU viewport smoke evidence present: $($pointCloudRendererDecisionSummary.GpuViewportSmokeEvidencePresent)"
    Write-Host "GPU viewport smoke missing fields: $($pointCloudRendererDecisionSummary.GpuViewportSmokeMissingEvidenceFieldCount)"
    Write-Host "GPU fallback preservation evidence present: $($pointCloudRendererDecisionSummary.GpuFallbackPreservationEvidencePresent)"
    Write-Host "GPU dense-frame evidence present: $($pointCloudRendererDecisionSummary.GpuDenseFrameEvidencePresent)"
    Write-Host "Ready to claim GPU dense preview: $($pointCloudRendererDecisionSummary.ReadyToClaimGpuDensePreview)"
    Write-Host "Candidate renderers: $(@($pointCloudRendererDecisionSummary.CandidateRenderers) -join ', ')"
    Write-Host "Recommended next decision: $($pointCloudRendererDecisionSummary.RecommendedNextDecision)"
    Write-Host "Boundary: $($pointCloudRendererDecisionSummary.Boundary)"
}

if ($readinessSummary) {
    Write-Section "Fast readiness"
    Write-Host "Passed: $($readinessSummary.Passed)"
    Write-Host "Fast readiness passed: $($readinessSummary.FastReadinessPassed)"
    Write-Host "Mode: $($readinessSummary.ReadinessMode)"
    Write-Host "Manual evidence still required: $($readinessSummary.ManualEvidenceStillRequired)"
    Write-Host "Caveat: $($readinessSummary.ReadinessCaveat)"
    Write-Host "Boundary: $($readinessSummary.Boundary)"
    Write-Host "Steps: $($readinessSummary.PassedStepCount)/$($readinessSummary.StepCount) passed, $($readinessSummary.SkippedStepCount) skipped"
    Write-Host "Source repo root: $($readinessSummary.SourceRepoRoot)"
    Write-Host "Local project root: $($readinessSummary.ProjectRoot)"
    if (@($readinessSummary.SkippedStepDetails).Count -gt 0) {
        Write-Host "Skipped (not covered by fast readiness):"
        $readinessSummary.SkippedStepDetails | ForEach-Object {
            Write-Host "  $($_.Label) - $($_.Reason)"
        }
    }
}
