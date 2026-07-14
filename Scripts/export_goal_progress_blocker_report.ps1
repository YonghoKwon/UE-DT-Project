param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$OutputRoot = "",
    [int]$ObservedCodexUsagePercent = -1,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "Required script not found: $ScriptPath"
    }

    $arguments = @("-ExecutionPolicy", "Bypass", "-File", $ScriptPath)
    foreach ($entry in $Parameters.GetEnumerator()) {
        if ($entry.Value -is [bool]) {
            if ([bool]$entry.Value) {
                $arguments += "-$($entry.Key)"
            }
            continue
        }

        $arguments += "-$($entry.Key)"
        $arguments += [string]$entry.Value
    }
    $arguments += "-Json"

    $output = @(& powershell @arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Convert-ToMarkdownCell {
    param([object]$Value)
    if ($null -eq $Value) { return "" }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
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

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\GoalProgress"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$precommitScript = Join-Path $SourceRepoRoot "Scripts\report_precommit_summary.ps1"
$precommit = Invoke-JsonScript -ScriptPath $precommitScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    IncludeReadiness = $true
}

$externalEvidenceItems = @(
    [PSCustomObject]@{
        Area = "Production WBP"
        Status = if ([bool]$precommit.WbpAcceptanceEvidenceSummary.ReadyToStageMonitorWbpAsset) { "Ready" } else { "EvidencePending" }
        Executor = "ManualEditorWithCodexTooling"
        CanCodexAdvanceWithoutExternalInput = $true
        EstimatedEffort = "Hours to a day if Unreal Editor is available; longer if production map bindings need changes."
        BlockingEvidence = "Unreal Editor open/compile, optional binding check, PIE smoke, display-data visual match, post-edit hash report, owner acceptance"
        NextAction = "Open WBP in Unreal Editor, run PIE smoke, fill monitor_wbp_acceptance.evidence.json, then run strict validation."
    },
    [PSCustomObject]@{
        Area = "True LAZ"
        Status = if ([bool]$precommit.LazExportDecisionSummary.ReadyToClaimTrueLaz) { "Ready" } else { "EvidencePending" }
        Executor = "CodexCanPrepareExternalToolRequired"
        CanCodexAdvanceWithoutExternalInput = $true
        EstimatedEffort = "Hours if a compressor and reader are installed; days if tool selection/licensing must be decided."
        BlockingEvidence = "Accepted compressor/reader selection, produced readable .laz, known-reader probe, owner acceptance"
        NextAction = "Select compressor/reader path, generate .laz, run readiness report with -LazEvidencePath and -RunReaderProbe."
    },
    [PSCustomObject]@{
        Area = "Judging server"
        Status = if ([bool]$precommit.JudgingServerAcceptanceSummary.ReadyToUseRealJudgingServer) { "Ready" } else { "EvidencePending" }
        Executor = "ExternalServerOwnerRequired"
        CanCodexAdvanceWithoutExternalInput = $false
        EstimatedEffort = "Days to a week depending on endpoint/auth/response ownership."
        BlockingEvidence = "Endpoint/auth/retry/batching decisions, real accepted/rejected response evidence, owner approval"
        NextAction = "Fill judging-server acceptance evidence after real endpoint owner decisions."
    },
    [PSCustomObject]@{
        Area = "Real sensor deployment"
        Status = if ([bool]$precommit.RealSensorDeploymentSummary.ReadyForRealDeployment) { "Ready" } else { "EvidencePending" }
        Executor = "ExternalHardwareOrBrokerRequired"
        CanCodexAdvanceWithoutExternalInput = $false
        EstimatedEffort = "Several days to 1+ weeks depending on selected sensor path and available hardware/broker."
        BlockingEvidence = "Selected live path, broker/SDK credentials, live-frame smoke, judging-server handoff"
        NextAction = "Pick production path first: replay, HTTP, WebSocket, UDP, ROS2, Livox, or RealSense."
    },
    [PSCustomObject]@{
        Area = "GPU point cloud renderer"
        Status = if ([bool]$precommit.PointCloudRendererDecisionSummary.ReadyToClaimGpuRenderer) { "Ready" } else { "EvidencePending" }
        Executor = "CodexCanImplementEditorVerificationRequired"
        CanCodexAdvanceWithoutExternalInput = $true
        EstimatedEffort = "Several focused commits plus editor viewport/performance evidence."
        BlockingEvidence = "Niagara/custom GPU implementation, viewport screenshot/nonblank evidence, dense-frame performance validation"
        NextAction = "Implement Niagara spike only after CPU fallback and viewport evidence plan are accepted."
    }
)

$blockedItems = @($externalEvidenceItems | Where-Object { [string]$_.Status -ne "Ready" })
$codexAdvanceableItems = @($blockedItems | Where-Object { [bool]$_.CanCodexAdvanceWithoutExternalInput })
$externalOnlyItems = @($blockedItems | Where-Object { -not [bool]$_.CanCodexAdvanceWithoutExternalInput })
$hasObservedUsage = $ObservedCodexUsagePercent -ge 0
if ($ObservedCodexUsagePercent -gt 100) {
    throw "ObservedCodexUsagePercent must be between 0 and 100 when provided."
}
$observedUsageRemainingPercent = if ($hasObservedUsage) { 100 - $ObservedCodexUsagePercent } else { $null }
$usageResetGuidance = if ($hasObservedUsage -and $ObservedCodexUsagePercent -ge 90) {
    "Consider starting a fresh thread after the next clean commit if the UI is slow or summaries start dropping repo details."
}
elseif ($hasObservedUsage -and $ObservedCodexUsagePercent -ge 80) {
    "No immediate reset required. Continue in commit-sized slices and reset only if context quality or UI responsiveness degrades."
}
else {
    "No usage-based reset signal recorded by this report."
}
$reportJsonPath = Join-Path $OutputRoot "goal_progress_blocker_report.json"
$reportMarkdownPath = Join-Path $OutputRoot "goal_progress_blocker_report.md"

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    DryRunOnly = $true
    ModifiesAssets = $false
    StagesFiles = $false
    OutputRoot = $OutputRoot
    ReportJsonPath = $reportJsonPath
    ReportMarkdownPath = $reportMarkdownPath
    OverallPercent = [int]$precommit.OverallPercent
    RemainingPercent = [int](100 - [int]$precommit.OverallPercent)
    WorkAreas = $precommit.WorkAreas
    ExternalEvidenceItems = $externalEvidenceItems
    Summary = [PSCustomObject]@{
        OverallPercent = [int]$precommit.OverallPercent
        RemainingPercent = [int](100 - [int]$precommit.OverallPercent)
        BlockedExternalEvidenceCount = $blockedItems.Count
        CodexAdvanceableBlockedCount = $codexAdvanceableItems.Count
        ExternalOnlyBlockedCount = $externalOnlyItems.Count
        PixelStreamingCountsTowardRemainingWork = $false
        LargeUnusedContentCountsTowardCoreScope = $false
        NextCodexRecommendedArea = if ($codexAdvanceableItems.Count -gt 0) { [string]$codexAdvanceableItems[0].Area } else { "" }
        EstimatedRemainingEngineeringTurns = "Several focused commits for tooling, but final completion depends on manual/external evidence."
        EstimatedCalendarTime = "Roughly several days for local WBP/LAZ evidence if tools are available; 1-2+ weeks if real judging-server, sensor, and GPU evidence must be completed end-to-end."
        ShouldResetCodexThread = "No immediate reset needed. Continue by commit-sized slices; start a new thread only if interaction becomes slow or context quality drops."
        ResetRecommendation = "Do not reset solely because progress is $($precommit.OverallPercent)%. Reset only when the UI becomes slow, the task focus changes, or the thread summary starts losing important repo state."
        ObservedCodexUsagePercent = if ($hasObservedUsage) { $ObservedCodexUsagePercent } else { $null }
        ObservedCodexUsageRemainingPercent = $observedUsageRemainingPercent
        UsageResetGuidance = $usageResetGuidance
        UsageVisibility = "This report cannot read Codex GPT Plus quota directly. ObservedCodexUsagePercent is user-supplied when provided; project progress is measured separately."
        Valid = $true
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportJsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Goal Progress Blocker Report") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Overall progress: $($report.Summary.OverallPercent)%") | Out-Null
$lines.Add("- Remaining progress: $($report.Summary.RemainingPercent)%") | Out-Null
$lines.Add("- Blocked external evidence items: $($report.Summary.BlockedExternalEvidenceCount)") | Out-Null
$lines.Add("- Codex-advanceable blocked items: $($report.Summary.CodexAdvanceableBlockedCount)") | Out-Null
$lines.Add("- External-only blocked items: $($report.Summary.ExternalOnlyBlockedCount)") | Out-Null
$lines.Add("- Next Codex recommended area: $($report.Summary.NextCodexRecommendedArea)") | Out-Null
$lines.Add("- PixelStreaming counts toward remaining work: $($report.Summary.PixelStreamingCountsTowardRemainingWork)") | Out-Null
$lines.Add("- Large unused content counts toward core scope: $($report.Summary.LargeUnusedContentCountsTowardCoreScope)") | Out-Null
$lines.Add("- Estimated calendar time: $($report.Summary.EstimatedCalendarTime)") | Out-Null
$lines.Add("- Thread reset guidance: $($report.Summary.ShouldResetCodexThread)") | Out-Null
$lines.Add("- Reset recommendation: $($report.Summary.ResetRecommendation)") | Out-Null
$lines.Add("- Observed Codex usage percent: $($report.Summary.ObservedCodexUsagePercent)") | Out-Null
$lines.Add("- Observed Codex usage remaining percent: $($report.Summary.ObservedCodexUsageRemainingPercent)") | Out-Null
$lines.Add("- Usage reset guidance: $($report.Summary.UsageResetGuidance)") | Out-Null
$lines.Add("- Usage visibility: $($report.Summary.UsageVisibility)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Remaining External Evidence") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Area | Status | Executor | Codex can advance | Estimated effort | Blocking evidence | Next action |") | Out-Null
$lines.Add("| --- | --- | --- | --- | --- | --- | --- |") | Out-Null
foreach ($item in $externalEvidenceItems) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} |" -f `
        (Convert-ToMarkdownCell $item.Area),
        (Convert-ToMarkdownCell $item.Status),
        (Convert-ToMarkdownCell $item.Executor),
        (Convert-ToMarkdownCell $item.CanCodexAdvanceWithoutExternalInput),
        (Convert-ToMarkdownCell $item.EstimatedEffort),
        (Convert-ToMarkdownCell $item.BlockingEvidence),
        (Convert-ToMarkdownCell $item.NextAction))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Work Areas") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Area | Percent | Remaining |") | Out-Null
$lines.Add("| --- | --- | --- |") | Out-Null
foreach ($area in $precommit.WorkAreas) {
    $lines.Add(("| {0} | {1}% | {2} |" -f `
        (Convert-ToMarkdownCell $area.Name),
        (Convert-ToMarkdownCell $area.Percent),
        (Convert-ToMarkdownCell $area.Remaining))) | Out-Null
}

Write-TextFile -Path $reportMarkdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Goal progress blocker report created."
    Write-Host "Overall progress: $($report.Summary.OverallPercent)%"
    Write-Host "Remaining progress: $($report.Summary.RemainingPercent)%"
    Write-Host "Blocked external evidence items: $($report.Summary.BlockedExternalEvidenceCount)"
    if ($hasObservedUsage) {
        Write-Host "Observed Codex usage: $($report.Summary.ObservedCodexUsagePercent)% used, $($report.Summary.ObservedCodexUsageRemainingPercent)% remaining"
        Write-Host "Usage reset guidance: $($report.Summary.UsageResetGuidance)"
    }
    Write-Host "Report: $reportMarkdownPath"
}
