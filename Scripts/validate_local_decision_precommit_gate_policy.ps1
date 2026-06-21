param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$SourceRepoRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Invoke-Gate {
    param(
        [string]$GateScript,
        [string]$ProjectRoot,
        [string]$SourceRepoRoot,
        [switch]$RequireAcceptedLocalDecisionEvidence
    )

    $arguments = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $GateScript,
        "-ProjectRoot", $ProjectRoot,
        "-SourceRepoRoot", $SourceRepoRoot,
        "-Json"
    )
    if ($RequireAcceptedLocalDecisionEvidence) {
        $arguments += "-RequireAcceptedLocalDecisionEvidence"
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = @(& powershell @arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = ($output -join "`n")
        Report = if ($exitCode -eq 0) { (($output -join "`n") | ConvertFrom-Json) } else { $null }
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

$gateScript = Join-Path $SourceRepoRoot "Scripts\invoke_local_decision_precommit_gate.ps1"
$goalProgressBlockerReportScript = Join-Path $SourceRepoRoot "Scripts\export_goal_progress_blocker_report.ps1"
$largePolicyScript = Join-Path $SourceRepoRoot "Scripts\validate_large_content_decision_policy.ps1"
$localAssetDoc = Join-Path $SourceRepoRoot "docs\local_asset_report.md"
$remainingDoc = Join-Path $SourceRepoRoot "docs\remaining_work.md"
$readme = Join-Path $SourceRepoRoot "README.md"

foreach ($required in @($gateScript, $goalProgressBlockerReportScript, $largePolicyScript, $localAssetDoc, $remainingDoc, $readme)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file not found: $required"
    }
}

$requiredTexts = @(
    [PSCustomObject]@{ Path = $gateScript; Pattern = "RequireAcceptedLocalDecisionEvidence"; Label = "Gate exposes strict evidence option" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "CurrentLocalBurdenAllowed"; Label = "Gate reports current local burden allowance" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "StagedDecisionPointCount"; Label = "Gate reports staged decision point count" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "StagedDecisionPaths"; Label = "Gate reports staged decision paths" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "DTCoreInvariantValid"; Label = "Gate reports DTCore invariant" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "PixelStreamingMustRemainUntracked"; Label = "Gate reports PixelStreaming untracked boundary" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "PixelStreamingOutOfScopeThisIteration"; Label = "Gate reports PixelStreaming current-scope exclusion" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "PixelStreamingCountsTowardRemainingWork"; Label = "Gate reports PixelStreaming remaining-work exclusion" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "WbpAcceptanceEvidenceComplete"; Label = "Gate reports WBP evidence completion" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "WbpAcceptanceMissingCount"; Label = "Gate reports WBP missing evidence count" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "FailureReasons"; Label = "Gate reports failure reasons" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "Warnings"; Label = "Gate reports warnings" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "Default mode reports incomplete WBP evidence without failing"; Label = "Gate documents default WBP evidence behavior" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "strict mode requires accepted local decision evidence"; Label = "Gate documents strict WBP evidence behavior" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "DryRunOnly = `$true"; Label = "Gate declares read-only behavior" },
    [PSCustomObject]@{ Path = $gateScript; Pattern = "StagesFiles = `$false"; Label = "Gate declares no staging" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "Saved\Reports\GoalProgress"; Label = "Progress blocker report writes under Saved" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "BlockedExternalEvidenceCount"; Label = "Progress blocker report counts external blockers" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "CodexAdvanceableBlockedCount"; Label = "Progress blocker report counts Codex-advanceable blockers" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "ExternalOnlyBlockedCount"; Label = "Progress blocker report counts external-only blockers" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "CanCodexAdvanceWithoutExternalInput"; Label = "Progress blocker report separates Codex-advanceable work" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "NextCodexRecommendedArea"; Label = "Progress blocker report suggests next Codex area" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "ShouldResetCodexThread"; Label = "Progress blocker report explains reset guidance" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "ResetRecommendation"; Label = "Progress blocker report gives reset recommendation" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "UsageVisibility"; Label = "Progress blocker report clarifies quota visibility" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "PixelStreamingCountsTowardRemainingWork = `$false"; Label = "Progress blocker report excludes PixelStreaming" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "ModifiesAssets = `$false"; Label = "Progress blocker report declares no asset edits" },
    [PSCustomObject]@{ Path = $goalProgressBlockerReportScript; Pattern = "StagesFiles = `$false"; Label = "Progress blocker report declares no staging" },
    [PSCustomObject]@{ Path = $largePolicyScript; Pattern = "invoke_local_decision_precommit_gate.ps1"; Label = "Large policy references local decision gate" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "invoke_local_decision_precommit_gate.ps1"; Label = "Local asset doc documents local decision gate" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "invoke_local_decision_precommit_gate.ps1"; Label = "Remaining work documents local decision gate" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_goal_progress_blocker_report.ps1"; Label = "Remaining work documents progress blocker report" },
    [PSCustomObject]@{ Path = $readme; Pattern = "invoke_local_decision_precommit_gate.ps1"; Label = "README documents local decision gate" },
    [PSCustomObject]@{ Path = $readme; Pattern = "export_goal_progress_blocker_report.ps1"; Label = "README documents progress blocker report" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$defaultGate = Invoke-Gate -GateScript $gateScript -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
if ($defaultGate.ExitCode -ne 0) {
    throw "Default local decision pre-commit gate must pass for the current known-local state: $($defaultGate.Output)"
}

$defaultSummary = $defaultGate.Report.Summary
if (-not [bool]$defaultSummary.GatePassed) {
    throw "Default local decision pre-commit gate report must set GatePassed=true."
}
if (-not [bool]$defaultSummary.CurrentLocalBurdenAllowed) {
    throw "Default local decision pre-commit gate must allow the current known-local burden."
}
if ([int]$defaultSummary.StagedDecisionPointCount -ne 0) {
    throw "Default local decision pre-commit gate must report zero staged decision points."
}
if (-not [bool]$defaultSummary.DTCoreInvariantValid) {
    throw "Default local decision pre-commit gate must report DTCoreInvariantValid=true."
}
if ([bool]$defaultSummary.PixelStreamingStaged) {
    throw "Default local decision pre-commit gate must report PixelStreamingStaged=false."
}
if (-not [bool]$defaultSummary.PixelStreamingMustRemainUntracked) {
    throw "Default local decision pre-commit gate must report PixelStreamingMustRemainUntracked=true."
}
if (-not [bool]$defaultSummary.PixelStreamingOutOfScopeThisIteration) {
    throw "Default local decision pre-commit gate must report PixelStreamingOutOfScopeThisIteration=true."
}
if ([bool]$defaultSummary.PixelStreamingCountsTowardRemainingWork) {
    throw "Default local decision pre-commit gate must report PixelStreamingCountsTowardRemainingWork=false."
}
if ([bool]$defaultSummary.WbpAcceptanceEvidenceComplete) {
    throw "Default local decision pre-commit gate should not claim WBP acceptance evidence is complete."
}
if (@($defaultSummary.Warnings).Count -eq 0) {
    throw "Default local decision pre-commit gate should surface warnings for incomplete manual evidence."
}

$strictGate = Invoke-Gate -GateScript $gateScript -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot -RequireAcceptedLocalDecisionEvidence
if ($strictGate.ExitCode -eq 0) {
    throw "Strict local decision pre-commit gate must fail while WBP acceptance evidence is incomplete."
}
if ($strictGate.Output -notmatch "Monitor WBP acceptance evidence is incomplete") {
    throw "Strict gate failure should mention incomplete WBP acceptance evidence."
}

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    DefaultGateExitCode = $defaultGate.ExitCode
    StrictGateExitCode = $strictGate.ExitCode
    CheckedContracts = @($requiredTexts)
    Summary = [PSCustomObject]@{
        DefaultGatePasses = ($defaultGate.ExitCode -eq 0)
        StrictGateFailsUntilEvidenceComplete = ($strictGate.ExitCode -ne 0)
        CurrentLocalBurdenAllowed = [bool]$defaultSummary.CurrentLocalBurdenAllowed
        DTCoreInvariantValid = [bool]$defaultSummary.DTCoreInvariantValid
        StagedDecisionPointCount = [int]$defaultSummary.StagedDecisionPointCount
        PixelStreamingMustRemainUntracked = [bool]$defaultSummary.PixelStreamingMustRemainUntracked
        PixelStreamingOutOfScopeThisIteration = [bool]$defaultSummary.PixelStreamingOutOfScopeThisIteration
        PixelStreamingCountsTowardRemainingWork = [bool]$defaultSummary.PixelStreamingCountsTowardRemainingWork
        WbpAcceptanceEvidenceComplete = [bool]$defaultSummary.WbpAcceptanceEvidenceComplete
        WarningCount = @($defaultSummary.Warnings).Count
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Local decision pre-commit gate policy is valid."
    Write-Host "Default gate passes: $($report.Summary.DefaultGatePasses)"
    Write-Host "Strict gate fails until evidence complete: $($report.Summary.StrictGateFailsUntilEvidenceComplete)"
    Write-Host "Current local burden allowed: $($report.Summary.CurrentLocalBurdenAllowed)"
    Write-Host "DTCore invariant valid: $($report.Summary.DTCoreInvariantValid)"
    Write-Host "Staged decision points: $($report.Summary.StagedDecisionPointCount)"
    Write-Host "PixelStreaming must remain untracked: $($report.Summary.PixelStreamingMustRemainUntracked)"
    Write-Host "PixelStreaming out of current scope: $($report.Summary.PixelStreamingOutOfScopeThisIteration)"
    Write-Host "PixelStreaming counts toward remaining work: $($report.Summary.PixelStreamingCountsTowardRemainingWork)"
    Write-Host "WBP evidence complete: $($report.Summary.WbpAcceptanceEvidenceComplete)"
}
