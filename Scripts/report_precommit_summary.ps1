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

    $jsonText = & powershell -ExecutionPolicy Bypass -File $largeContentReportScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Large content decision report failed with exit code $LASTEXITCODE"
    }

    $largeReport = $jsonText | ConvertFrom-Json
    $unusedCleanupCandidates = @($largeReport.Candidates | Where-Object { $_.UnusedLocalCleanupCandidate })
    $repositoryAcceptanceCandidates = @($largeReport.Candidates | Where-Object { $_.RepositoryAcceptanceRequired })
    $sampleCandidates = @($largeReport.Candidates | Where-Object { $_.Category -eq "SampleOrThirdParty" })
    $unusedCleanupBytes = [int64](($unusedCleanupCandidates | Measure-Object -Property SizeBytes -Sum).Sum)
    $sampleBytes = [int64](($sampleCandidates | Measure-Object -Property SizeBytes -Sum).Sum)
    $largestCleanupCandidate = @($unusedCleanupCandidates | Sort-Object SizeBytes -Descending | Select-Object -First 1)
    $largestSampleCandidate = @($sampleCandidates | Sort-Object SizeBytes -Descending | Select-Object -First 1)

    return [PSCustomObject]@{
        CandidateCount = [int]$largeReport.CandidateCount
        UnusedCleanupCandidateCount = $unusedCleanupCandidates.Count
        UnusedCleanupSizeBytes = $unusedCleanupBytes
        UnusedCleanupSize = Convert-ToSizeText -Bytes $unusedCleanupBytes
        RepositoryAcceptanceCandidateCount = $repositoryAcceptanceCandidates.Count
        RepositoryAcceptanceCandidatePaths = @($repositoryAcceptanceCandidates | Select-Object -ExpandProperty Path)
        LargestCleanupCandidatePath = if ($largestCleanupCandidate.Count -gt 0) { [string]$largestCleanupCandidate[0].Path } else { "" }
        LargestCleanupCandidateSize = if ($largestCleanupCandidate.Count -gt 0) { [string]$largestCleanupCandidate[0].Size } else { "" }
        CleanupBoundary = "Cleanup candidate means keep ignored or manually remove after map/WBP dependency checks; it is not ready to stage."
        SampleCandidateCount = $sampleCandidates.Count
        SampleCandidateSizeBytes = $sampleBytes
        SampleCandidateSize = Convert-ToSizeText -Bytes $sampleBytes
        SampleCandidatePaths = @($sampleCandidates | Select-Object -ExpandProperty Path)
        LargestSampleCandidatePath = if ($largestSampleCandidate.Count -gt 0) { [string]$largestSampleCandidate[0].Path } else { "" }
        LargestSampleCandidateSize = if ($largestSampleCandidate.Count -gt 0) { [string]$largestSampleCandidate[0].Size } else { "" }
        SampleBoundary = "Sample/third-party candidate means keep untracked unless project ownership, redistribution approval, and documentation alternative are accepted."
        DefaultAction = [string]$largeReport.Summary.DefaultAction
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
        -Percent 87 `
        -Done "LiDAR/camera schema docs, compatibility notes, fixtures, fixture validator with camera base64/JPEG/byteSize and simulationQuality enum checks, preserved row/col/returnIndex contract, local mock contract validator with camera image invariant checks, schema review policy, server transport contract notes, weak HTTP callback handling, 2xx acceptance tracking, outbound HTTP POST loopback acceptance automation, and exportable contract review report are in place. The payload contract report now includes a server acceptance readiness matrix for endpoint ownership, authentication, retry/timeout, batching/backpressure, response schema, and real judging-server acceptance evidence." `
        -Remaining "Judging server approval, real server acceptance evidence, final endpoint/auth/retry/batching owner decisions, and server-owned response schema tests remain."),
    (New-WorkArea `
        -Name "Local project asset decisions" `
        -Percent 89 `
        -Done "Decision points are reported, unclassified untracked files and staged decision paths are gated, large/sample folders include content summaries, per-decision GitState/CommitReadiness/ReviewQueue/DecisionOwner/DecisionStatus/EvidenceNeeded/EvidenceStatus/EvidenceSatisfied/DecisionChecklist fields are exported, review queues separate ReadyToStage/NeedsOwnerDecision/KeepLocal paths, unresolved owner/evidence metadata is documented and validated, ReadyToStage now requires AcceptedForRepository with complete evidence plus reviewer/date/source evidence, duplicate normalized evidence paths are rejected, an evidence template exporter is available, the evidence workflow and staged decision gate are covered by temp-project automation, runtime config validation inspects the real local project and emits a Game.ini RecommendedDecision, WBP metadata/Git/setup-contract decision reporting is available, and empty DTCore runtime override Game.ini files are now classified as KeepLocal local placeholders instead of owner-acceptance candidates. Local asset reports now include ReviewPriority, CommitBlocker, BlockingReason, NextReviewAction, ActionPlan, large-content RequiredAcceptance, DecisionBlockers, and TopBlockers. The evidence template now exports Summary, pending evidence counts, and TopBlockingPaths for owner review. The currently unused large Content folders are now classified as KeepLocal unused cleanup candidates instead of repository-acceptance candidates. The pre-commit summary now consumes the large-content and monitor-WBP decision reports, surfacing unused cleanup candidate count/size, sample/third-party candidate count/size, WBP Git/evidence state, missing WBP acceptance items, the largest cleanup/sample blockers, and the remaining repository-acceptance candidate paths. The focused monitor WBP decision report and runtime config decision report now reuse the local asset decision engine, accept EvidencePath, expose ReviewQueue/CommitReadiness/EvidenceStatus/MissingEvidenceCount/ReadyToStage, export manual acceptance checklists, and can fail on incomplete evidence as opt-in pre-commit gates. The large content decision report now flags BuiltDataHeavy, LargestFileRisk, StorageRiskReason, RedistributionReviewRequired, SampleRiskReason, UnusedLocalCleanupCandidate, RepositoryAcceptanceRequired, and CleanupReason for owner review. The project readiness wrapper now accepts SourceRepoRoot so source docs/policies can be checked while local Unreal asset/config decisions are scanned from the real project root, and the pre-commit summary can include the fast readiness JSON result with skipped-step evidence boundaries." `
        -Remaining "Manual WBP editor-open/binding/PIE acceptance, optional manual cleanup/removal of unused local Content assets after map/WBP dependency checks, PixelStreaming project ownership/license/documentation-alternative acceptance, non-empty Game.ini endpoint/credential review if values are added later, and any final AcceptedForRepository evidence remain."),
    (New-WorkArea `
        -Name "Real sensor adapters" `
        -Percent 81 `
        -Done "ROS2/Livox/RealSense placeholders, normalized frame handoff path, replay samples, LiDAR JSON live bridge component, camera JSON live bridge component, generic live JSON payload bridge helper, optional HTTP and UDP JSON live bridge components, HTTP payload automation, local HTTP loopback POST smoke automation, local UDP datagram smoke automation, DTCore WebSocket transaction handler, safe routing guards, WebSocket live sample payload, editor sample/push helpers, transaction routing automation, static adapter-plan validation, exportable WebSocket transaction registration checklist, optional data-table registration evidence automation, editor-only row repair commandlet, DT_TransactionCode row pass evidence, row-based handler parse/process evidence, broker smoke evidence report tooling, WebSocket LiDAR smoke evidence workflow wrapper, brokerless DTCore dispatch automation, opt-in brokerless workflow execution, hardened camera JSON live payload validation with transport/recorder evidence, expanded malformed camera schema rejection coverage, and a consolidated real-sensor adapter readiness report are in place. The readiness report ranks deployment paths and exports RequiredEvidence, DeploymentBlockers, NextAction, and DeploymentActionPlan entries for replay, HTTP, WebSocket, UDP, and SDK routes. Broker smoke reporting now requires concrete evidence fields such as run id, map/PIE session, log path, before/after frame counts, target point count, cached payload bytes or hash, broker client command, operator, and notes before marking the smoke complete." `
        -Remaining "Actual SDK/ROS2/Livox/RealSense connections, completed deployment STOMP/WebSocket broker PIE smoke evidence using the required evidence schema, HTTP/UDP deployment exposure and credential decisions, final production adapter owner approval, and successful real-frame smoke tests remain."),
    (New-WorkArea `
        -Name "Large point cloud rendering" `
        -Percent 69 `
        -Done "Server payload and preview policies are separated with preview caps, runtime warnings, point-cloud-only clamps, batched ISM AddInstances live preview uploads, procedural CSV high-density preview automation, instanced fallback automation, CSV preview parse/build/load telemetry, generous headless procedural performance-budget automation, exportable CSV preview performance evidence from Unreal automation logs, static preview-policy validation, and a high-density renderer decision report covering CPU fallback, Niagara, custom GPU buffers, and external viewer workflows. The renderer decision report consumes local CSV preview performance evidence, separates ISM smoke from procedural dense evidence, rejects failure lines inside the selected automation run block, records decision gates, recommends a Niagara point-renderer spike while preserving CPU preview fallback, and now exports GPU spike action-plan, renderer phase, viewport smoke, fallback-preservation, and dense-frame evidence gates for the future GPU/Niagara path." `
        -Remaining "Niagara spike implementation, actual viewport screenshot/nonblank pixel evidence, renderer-specific dense-frame performance validation, fallback-preservation verification after GPU integration, and final GPU asset/module ownership remain."),
    (New-WorkArea `
        -Name "LAZ export" `
        -Percent 55 `
        -Done "Placeholder behavior is explicit, tested as LAS-compatible source export, covered by static placeholder-policy validation, supported by a compression-path decision report, and now has an opt-in external compressor path with missing-compressor guard automation plus a positive external process-contract automation that verifies `{input}`/`{output}` handling and non-empty `.laz` output creation. A local compressor readiness report scans compressor/reader candidates, records that tool readiness is not readable-output evidence, is included in the project readiness gate, accepts explicit `.laz` evidence with an optional known-reader probe plus blocked-probe reporting, and is now surfaced in the pre-commit summary as a LAZ decision/readiness boundary." `
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
$wbpDecisionSummary = Get-WbpDecisionSummary -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
$lazExportDecisionSummary = Get-LazExportDecisionSummary -ProjectRoot $SourceRepoRoot
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
    WbpDecisionSummary = $wbpDecisionSummary
    LazExportDecisionSummary = $lazExportDecisionSummary
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
    Write-Host "Unused cleanup size: $($largeContentDecisionSummary.UnusedCleanupSize)"
    Write-Host "Largest cleanup candidate: $($largeContentDecisionSummary.LargestCleanupCandidatePath) ($($largeContentDecisionSummary.LargestCleanupCandidateSize))"
    Write-Host "Repository-acceptance candidates: $($largeContentDecisionSummary.RepositoryAcceptanceCandidateCount)"
    if ($largeContentDecisionSummary.RepositoryAcceptanceCandidatePaths.Count -gt 0) {
        Write-Host "Repository-acceptance paths: $(@($largeContentDecisionSummary.RepositoryAcceptanceCandidatePaths) -join ', ')"
    }
    Write-Host "Boundary: $($largeContentDecisionSummary.CleanupBoundary)"

    Write-Section "Sample and third-party decisions"
    Write-Host "Sample/third-party candidates: $($largeContentDecisionSummary.SampleCandidateCount)"
    Write-Host "Sample/third-party size: $($largeContentDecisionSummary.SampleCandidateSize)"
    Write-Host "Largest sample candidate: $($largeContentDecisionSummary.LargestSampleCandidatePath) ($($largeContentDecisionSummary.LargestSampleCandidateSize))"
    if ($largeContentDecisionSummary.SampleCandidatePaths.Count -gt 0) {
        Write-Host "Sample/third-party paths: $(@($largeContentDecisionSummary.SampleCandidatePaths) -join ', ')"
    }
    Write-Host "Boundary: $($largeContentDecisionSummary.SampleBoundary)"
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
    Write-Host "Unexpected WBP staged: $($wbpDecisionSummary.UnexpectedWbpStaged)"
    Write-Host "Must remain untracked: $($wbpDecisionSummary.MustRemainUntracked)"
    Write-Host "Blocking reason: $($wbpDecisionSummary.BlockingReason)"
    Write-Host "Next review action: $($wbpDecisionSummary.NextReviewAction)"
    Write-Host "Boundary: $($wbpDecisionSummary.Boundary)"
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
