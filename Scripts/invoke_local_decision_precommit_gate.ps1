param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [switch]$RequireAcceptedLocalDecisionEvidence,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Invoke-JsonScript {
    param(
        [string]$Label,
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "$Label script not found: $ScriptPath"
    }

    $arguments = @("-ExecutionPolicy", "Bypass", "-File", $ScriptPath)
    foreach ($key in @($Parameters.Keys | Sort-Object)) {
        $value = $Parameters[$key]
        if ($value -is [System.Management.Automation.SwitchParameter]) {
            if ([bool]$value) {
                $arguments += "-$key"
            }
        }
        elseif ($value -is [bool]) {
            if ($value) {
                $arguments += "-$key"
            }
        }
        elseif ($null -ne $value) {
            $arguments += "-$key"
            $arguments += $value
        }
    }
    $arguments += "-Json"

    $output = @(& powershell @arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Add-Step {
    param(
        [System.Collections.Generic.List[object]]$Steps,
        [string]$Name,
        [bool]$Passed,
        [string]$Detail,
        [string]$Boundary = ""
    )

    $Steps.Add([PSCustomObject]@{
        Name = $Name
        Passed = $Passed
        Detail = $Detail
        Boundary = $Boundary
    }) | Out-Null
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
    $EvidencePath = Join-Path $SourceRepoRoot "docs\local_asset_decisions.evidence.json"
}

$dtCoreGuardScript = Join-Path $SourceRepoRoot "Scripts\validate_dtcore_submodule_guard.ps1"
$assetReportScript = Join-Path $SourceRepoRoot "Scripts\report_local_project_status.ps1"
$sampleDecisionScript = Join-Path $SourceRepoRoot "Scripts\export_sample_content_decision_report.ps1"
$largeContentPolicyScript = Join-Path $SourceRepoRoot "Scripts\validate_large_content_decision_policy.ps1"
$runtimeConfigPolicyScript = Join-Path $SourceRepoRoot "Scripts\validate_runtime_config_policy.ps1"
$runtimeConfigDecisionScript = Join-Path $SourceRepoRoot "Scripts\export_runtime_config_decision_report.ps1"
$monitorWidgetPolicyScript = Join-Path $SourceRepoRoot "Scripts\validate_monitor_widget_policy.ps1"
$wbpDecisionScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_decision_report.ps1"
$wbpPreflightScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_preflight_report.ps1"
$wbpEvidenceScript = Join-Path $SourceRepoRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"
$precommitSummaryScript = Join-Path $SourceRepoRoot "Scripts\report_precommit_summary.ps1"

$steps = [System.Collections.Generic.List[object]]::new()

$dtCore = Invoke-JsonScript -Label "DTCore submodule guard" -ScriptPath $dtCoreGuardScript -Parameters @{
    ProjectRoot = $ProjectRoot
    FailOnViolation = $true
}
Add-Step -Steps $steps -Name "DTCore submodule invariant" -Passed ([bool]$dtCore.Summary.DTCoreInvariantValid) -Detail "Expected=$($dtCore.Summary.ExpectedCommit) Actual=$($dtCore.Summary.ActualCommit)" -Boundary $dtCore.Summary.Boundary

$localAssets = Invoke-JsonScript -Label "Local asset staged decision gate" -ScriptPath $assetReportScript -Parameters @{
    ProjectRoot = $ProjectRoot
    FailOnUnclassifiedUntracked = $true
    FailOnStagedDecisionPoints = $true
}
Add-Step -Steps $steps -Name "Local asset staged decision gate" -Passed (-not [bool]$localAssets.Summary.HasStagedDecisionPoints -and -not [bool]$localAssets.Summary.HasUnclassifiedUntracked) -Detail "DecisionPoints=$($localAssets.Summary.PresentDecisionPoints) StagedDecisionPoints=$($localAssets.Summary.StagedDecisionPointCount) Unclassified=$($localAssets.Summary.UnclassifiedUntrackedCount)" -Boundary "Known local decision paths may remain untracked, but staged blocked decision paths fail this gate."

$sampleDecision = Invoke-JsonScript -Label "Sample content decision gate" -ScriptPath $sampleDecisionScript -Parameters @{
    ProjectRoot = $ProjectRoot
}
Add-Step -Steps $steps -Name "Sample content staged path gate" -Passed (-not [bool]$sampleDecision.Summary.UnexpectedSampleStaged -and [int]$sampleDecision.Summary.StagedSamplePathCount -eq 0) -Detail "Samples=$($sampleDecision.Summary.SampleCandidateCount) StagedSamplePaths=$($sampleDecision.Summary.StagedSamplePathCount) SetupDoc=$($sampleDecision.Summary.SetupDocumentationPath)" -Boundary "Samples/PixelStreaming remains untracked unless ownership and redistribution evidence are accepted."

$largePolicy = Invoke-JsonScript -Label "Large content decision policy" -ScriptPath $largeContentPolicyScript -Parameters @{
    ProjectRoot = $SourceRepoRoot
    LocalProjectRoot = $ProjectRoot
}
Add-Step -Steps $steps -Name "Large content decision policy" -Passed ([bool]$largePolicy.Summary.Valid) -Detail "CleanupCandidates=$($largePolicy.Summary.CleanupPlanCandidateCount) SampleMissingAcceptance=$($largePolicy.Summary.SampleDecisionMissingAcceptanceCount)" -Boundary "Confirmed-unused Content folders stay archived/local; copied samples stay local until accepted."

$runtimePolicy = Invoke-JsonScript -Label "Runtime config policy" -ScriptPath $runtimeConfigPolicyScript -Parameters @{
    ProjectRoot = $SourceRepoRoot
    LocalProjectRoot = $ProjectRoot
}
Add-Step -Steps $steps -Name "Runtime config policy" -Passed ([bool]$runtimePolicy.Summary.Valid) -Detail "GameIniPresent=$($runtimePolicy.GameIniPresent) RecommendedDecision=$($runtimePolicy.RecommendedDecision) NonEmptyKeys=$(@($runtimePolicy.NonEmptyRuntimeOverrideKeys).Count)" -Boundary "Empty DTCore runtime override Game.ini remains local; non-empty endpoint or credential-like values fail policy."

$runtimeDecision = Invoke-JsonScript -Label "Runtime config decision report" -ScriptPath $runtimeConfigDecisionScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}
Add-Step -Steps $steps -Name "Runtime config decision report" -Passed ([bool]$runtimeDecision.Summary.Valid -and -not [bool]$runtimeDecision.Summary.UnexpectedGameIniStaged) -Detail "ReviewQueue=$($runtimeDecision.Summary.ReviewQueue) CommitReadiness=$($runtimeDecision.Summary.CommitReadiness) MissingEvidence=$($runtimeDecision.Summary.MissingEvidenceCount)" -Boundary "Config/Game.ini can remain local when empty runtime override values are classified as KeepLocal."

$monitorPolicy = Invoke-JsonScript -Label "Monitor widget policy" -ScriptPath $monitorWidgetPolicyScript -Parameters @{
    ProjectRoot = $SourceRepoRoot
}
Add-Step -Steps $steps -Name "Monitor widget static policy" -Passed ([bool]$monitorPolicy.Summary.Valid) -Detail "RequiredFiles=$($monitorPolicy.Summary.RequiredFileCount) RequiredContracts=$($monitorPolicy.Summary.RequiredContractCount)" -Boundary "Static monitor policy alignment is required before considering the local WBP."

$wbpDecision = Invoke-JsonScript -Label "Monitor WBP decision report" -ScriptPath $wbpDecisionScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}
Add-Step -Steps $steps -Name "Monitor WBP decision report" -Passed ([bool]$wbpDecision.Summary.Valid -and -not [bool]$wbpDecision.Summary.UnexpectedWbpStaged) -Detail "ReviewQueue=$($wbpDecision.Summary.ReviewQueue) CommitReadiness=$($wbpDecision.Summary.CommitReadiness) MissingEvidence=$($wbpDecision.Summary.MissingEvidenceCount)" -Boundary "The WBP can remain local/untracked while evidence is incomplete; staging is blocked."

$wbpPreflight = Invoke-JsonScript -Label "Monitor WBP preflight" -ScriptPath $wbpPreflightScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}
Add-Step -Steps $steps -Name "Monitor WBP preflight" -Passed ([int]$wbpPreflight.Summary.BlockedPreflightCheckCount -eq 0 -and -not [bool]$wbpPreflight.Summary.WbpStaged) -Detail "WbpPresent=$($wbpPreflight.Summary.WbpPresent) WbpStaged=$($wbpPreflight.Summary.WbpStaged) BlockedPreflightChecks=$($wbpPreflight.Summary.BlockedPreflightCheckCount)" -Boundary "Preflight readiness is not WBP acceptance; the binary remains local until evidence is complete."

$wbpEvidenceParams = @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
}
if ($RequireAcceptedLocalDecisionEvidence) {
    $wbpEvidenceParams.FailOnIncompleteEvidence = $true
}
$wbpEvidence = Invoke-JsonScript -Label "Monitor WBP acceptance evidence" -ScriptPath $wbpEvidenceScript -Parameters $wbpEvidenceParams
Add-Step -Steps $steps -Name "Monitor WBP acceptance evidence" -Passed (-not [bool]$RequireAcceptedLocalDecisionEvidence -or [bool]$wbpEvidence.Summary.ReadyToStageCandidate) -Detail "EvidenceFilePresent=$($wbpEvidence.Summary.EvidenceFilePresent) ReadyToStageCandidate=$($wbpEvidence.Summary.ReadyToStageCandidate) FailedChecks=$($wbpEvidence.Summary.FailedCheckCount)" -Boundary "Default mode reports incomplete WBP evidence without failing; strict mode requires accepted local decision evidence."

$precommit = Invoke-JsonScript -Label "Pre-commit summary" -ScriptPath $precommitSummaryScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}
Add-Step -Steps $steps -Name "Pre-commit summary generation" -Passed ($null -ne $precommit.OverallPercent) -Detail "OverallPercent=$($precommit.OverallPercent)" -Boundary "Progress summary is planning evidence, not full editor/server acceptance."

$failedSteps = @($steps | Where-Object { -not [bool]$_.Passed })
$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
    RequireAcceptedLocalDecisionEvidence = [bool]$RequireAcceptedLocalDecisionEvidence
    Steps = @($steps)
    FailedSteps = $failedSteps
    Summary = [PSCustomObject]@{
        GatePassed = ($failedSteps.Count -eq 0)
        StepCount = $steps.Count
        PassedStepCount = @($steps | Where-Object { [bool]$_.Passed }).Count
        FailedStepCount = $failedSteps.Count
        OverallPercent = [int]$precommit.OverallPercent
        DTCoreInvariantValid = [bool]$dtCore.Summary.DTCoreInvariantValid
        StagedDecisionPointCount = [int]$localAssets.Summary.StagedDecisionPointCount
        StagedDecisionPaths = @($localAssets.DecisionPoints | Where-Object { [string]$_.GitState -eq "Staged" } | Select-Object -ExpandProperty Path)
        UnclassifiedUntrackedCount = [int]$localAssets.Summary.UnclassifiedUntrackedCount
        CurrentLocalBurdenAllowed = (
            [int]$localAssets.Summary.StagedDecisionPointCount -eq 0 -and
            [int]$localAssets.Summary.UnclassifiedUntrackedCount -eq 0 -and
            -not [bool]$sampleDecision.Summary.UnexpectedSampleStaged -and
            -not [bool]$wbpPreflight.Summary.WbpStaged
        )
        ConfigDecisionSummary = $runtimeDecision.Summary
        WbpDecisionSummary = $wbpDecision.Summary
        WbpAcceptanceEvidenceComplete = [bool]$wbpEvidence.Summary.ReadyToStageCandidate
        WbpAcceptanceMissingCount = [int]$wbpEvidence.Summary.FailedCheckCount
        UnexpectedSampleStaged = [bool]$sampleDecision.Summary.UnexpectedSampleStaged
        StagedSamplePathCount = [int]$sampleDecision.Summary.StagedSamplePathCount
        PixelStreamingStaged = [bool]$sampleDecision.Summary.UnexpectedSampleStaged
        PixelStreamingMustRemainUntracked = ([int]$sampleDecision.Summary.MustRemainUntrackedCount -gt 0)
        PixelStreamingOutOfScopeThisIteration = ([int]$sampleDecision.Summary.ExcludedFromCurrentScopeCount -gt 0)
        PixelStreamingCountsTowardRemainingWork = ([int]$sampleDecision.Summary.CountsTowardRemainingWorkCount -gt 0)
        SampleDecisionSummary = $sampleDecision.Summary
        LargeContentPolicyValid = [bool]$largePolicy.Summary.Valid
        LargeContentCandidateCount = [int]$largePolicy.Summary.LargeDecisionReportCandidateCount
        WbpStaged = [bool]$wbpPreflight.Summary.WbpStaged
        WbpEvidenceReadyToStageCandidate = [bool]$wbpEvidence.Summary.ReadyToStageCandidate
        WbpEvidenceFailedCheckCount = [int]$wbpEvidence.Summary.FailedCheckCount
        FailureReasons = @($failedSteps | ForEach-Object { "$($_.Name): $($_.Detail)" })
        Warnings = @(
            if (-not [bool]$wbpEvidence.Summary.ReadyToStageCandidate) {
                "WBP acceptance evidence is incomplete in default reporting mode."
            }
            if ([int]$sampleDecision.Summary.ExcludedFromCurrentScopeCount -gt 0) {
                "Samples/PixelStreaming is out of current scope and remains local/untracked."
            }
        )
        DryRunOnly = $true
        ModifiesAssets = $false
        StagesFiles = $false
        Boundary = "This local decision pre-commit gate is read-only. It fails accidental staging/invariant violations, while default mode reports manual WBP evidence gaps without requiring acceptance."
    }
}

if ($failedSteps.Count -gt 0) {
    throw "Local decision pre-commit gate failed. FailedStepCount=$($failedSteps.Count)"
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Local decision pre-commit gate passed."
    Write-Host "Overall progress: $($report.Summary.OverallPercent)%"
    Write-Host "Steps: $($report.Summary.PassedStepCount)/$($report.Summary.StepCount) passed"
    Write-Host "DTCore invariant valid: $($report.Summary.DTCoreInvariantValid)"
    Write-Host "Staged decision points: $($report.Summary.StagedDecisionPointCount)"
    Write-Host "Unexpected sample staged: $($report.Summary.UnexpectedSampleStaged)"
    Write-Host "WBP staged: $($report.Summary.WbpStaged)"
    Write-Host "WBP evidence ready to stage candidate: $($report.Summary.WbpEvidenceReadyToStageCandidate)"
    Write-Host "Boundary: $($report.Summary.Boundary)"
}
