param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [switch]$FailOnIncompleteEvidence,
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Format-Size {
    param([int64]$Bytes)

    if ($Bytes -ge 1GB) { return "{0:N1} GB" -f ($Bytes / 1GB) }
    if ($Bytes -ge 1MB) { return "{0:N1} MB" -f ($Bytes / 1MB) }
    if ($Bytes -ge 1KB) { return "{0:N1} KB" -f ($Bytes / 1KB) }
    return "$Bytes B"
}

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

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "Required script not found: $ScriptPath"
    }

    $scriptOutput = @(& powershell -ExecutionPolicy Bypass -File $ScriptPath @Parameters -Json)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($scriptOutput -join ' ')"
    }

    return ($scriptOutput -join "`n") | ConvertFrom-Json
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
    $SourceRepoRoot = $ProjectRoot
}
if (-not (Test-Path -LiteralPath $SourceRepoRoot -PathType Container)) {
    throw "SourceRepoRoot not found: $SourceRepoRoot"
}
$SourceRepoRoot = (Resolve-Path -LiteralPath $SourceRepoRoot).Path
$wbpRelativePath = "Content\MA0T10\UI\WBP_VirtualSensorMonitor.uasset"
$wbpPath = Join-Path $ProjectRoot $wbpRelativePath
$setupDocPath = Join-Path $SourceRepoRoot "docs\widget_designer_setup.md"
$assetReportScript = Join-Path $SourceRepoRoot "Scripts\report_local_project_status.ps1"
$monitorPolicyScript = Join-Path $SourceRepoRoot "Scripts\validate_monitor_widget_policy.ps1"
$acceptanceTemplateScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_acceptance_template.ps1"

$gitStatusLines = @()
Push-Location $ProjectRoot
try {
    $gitStatusLines = @(git status --porcelain=v1 --untracked-files=all -- $wbpRelativePath)
}
finally {
    Pop-Location
}

$wbpItem = if (Test-Path -LiteralPath $wbpPath -PathType Leaf) { Get-Item -LiteralPath $wbpPath } else { $null }
$gitState = "CleanOrIgnored"
if (@($gitStatusLines | Where-Object { $_.StartsWith("?? ") }).Count -gt 0) {
    $gitState = "Untracked"
}
elseif (@($gitStatusLines | Where-Object { $_.Length -ge 2 -and $_.Substring(0, 1) -ne " " }).Count -gt 0) {
    $gitState = "Staged"
}
elseif (@($gitStatusLines | Where-Object { $_.Length -ge 2 -and $_.Substring(1, 1) -ne " " }).Count -gt 0) {
    $gitState = "TrackedModified"
}

$setupDocPresent = Test-Path -LiteralPath $setupDocPath -PathType Leaf
$requiredSetupTerms = @(
    "WBP_VirtualSensorMonitor",
    "Optional Bindings",
    "BindSensorManager",
    "AVirtualSensorMonitorHostActor",
    "GetMonitorStatusText",
    "MA0T10.SensorMonitor.LidarStatusTextContract"
)
$missingSetupTerms = @()
if ($setupDocPresent) {
    foreach ($term in $requiredSetupTerms) {
        if (-not (Select-String -LiteralPath $setupDocPath -Pattern $term -SimpleMatch -Quiet)) {
            $missingSetupTerms += $term
        }
    }
}
else {
    $missingSetupTerms = $requiredSetupTerms
}

$assetReportParams = @{ ProjectRoot = $ProjectRoot }
if (-not [string]::IsNullOrWhiteSpace($EvidencePath)) {
    $assetReportParams.EvidencePath = $EvidencePath
}
$assetDecisionReport = Invoke-JsonScript `
    -ScriptPath $assetReportScript `
    -Parameters $assetReportParams
$monitorPolicyReport = Invoke-JsonScript `
    -ScriptPath $monitorPolicyScript `
    -Parameters @{ ProjectRoot = $SourceRepoRoot }

$wbpDecisionPoint = @($assetDecisionReport.DecisionPoints | Where-Object { $_.Path -eq $wbpRelativePath }) | Select-Object -First 1
if ($wbpItem -and -not $wbpDecisionPoint) {
    throw "WBP decision point not found in report_local_project_status.ps1 output: $wbpRelativePath"
}

$recommendedDecision = "NotPresent"
$riskLevel = "None"
$recommendation = "WBP_VirtualSensorMonitor.uasset is not present in the local project."
if ($wbpItem) {
    $recommendedDecision = "PendingOwnerDecision"
    $riskLevel = "Medium"
    $recommendation = "Keep the local WBP untracked until it is opened in Unreal Editor, optional bindings are checked, PIE smoke passes, and the project owner accepts it as the production monitor WBP."
}

$manualAcceptanceChecklist = @(
    [PSCustomObject]@{
        Name = "Editor open verification"
        Required = $true
        Status = if ($wbpDecisionPoint -and @($wbpDecisionPoint.MissingEvidence) -notcontains "Editor open verification") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Open WBP_VirtualSensorMonitor in Unreal Editor and verify it loads and compiles without errors."
    },
    [PSCustomObject]@{
        Name = "Optional binding check"
        Required = $true
        Status = if ($setupDocPresent -and $missingSetupTerms.Count -eq 0 -and $wbpDecisionPoint -and @($wbpDecisionPoint.MissingEvidence) -notcontains "Optional binding check") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Compare named widgets against docs/widget_designer_setup.md Optional Bindings and confirm missing optional widgets do not crash."
    },
    [PSCustomObject]@{
        Name = "PIE smoke result"
        Required = $true
        Status = if ($wbpDecisionPoint -and @($wbpDecisionPoint.MissingEvidence) -notcontains "PIE smoke result") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Run PIE in the intended map and verify camera/LiDAR switching, preview controls, server payload export, and slab status text."
    },
    [PSCustomObject]@{
        Name = "Production WBP acceptance"
        Required = $true
        Status = if ($wbpDecisionPoint -and @($wbpDecisionPoint.MissingEvidence) -notcontains "Production WBP acceptance") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Project owner accepts this binary asset as the production monitor WBP and records AcceptedForRepository evidence."
    }
)

$evidenceDraft = [PSCustomObject]@{
    Path = $wbpRelativePath
    DecisionOwner = "ProjectOwnerRequired"
    DecisionStatus = if ($wbpItem) { "EvidencePending" } else { "NotPresent" }
    AcceptedBy = ""
    AcceptedAt = ""
    EvidenceSource = "Scripts/export_monitor_wbp_decision_report.ps1"
    Notes = $recommendation
    Evidence = @(
        [PSCustomObject]@{ Name = "Editor open verification"; Status = "Pending"; Source = ""; Note = "Open WBP_VirtualSensorMonitor in Unreal Editor and verify it loads without compile errors." },
        [PSCustomObject]@{ Name = "Optional binding check"; Status = if ($setupDocPresent -and $missingSetupTerms.Count -eq 0) { "Pending" } else { "Blocked" }; Source = ""; Note = "Check named widgets against docs/widget_designer_setup.md optional binding list." },
        [PSCustomObject]@{ Name = "PIE smoke result"; Status = "Pending"; Source = ""; Note = "Run PIE in intended map with AVirtualSensorMonitorHostActor or Level Blueprint binding." },
        [PSCustomObject]@{ Name = "Production WBP acceptance"; Status = "Pending"; Source = ""; Note = "Project owner confirms this binary asset is the production operator monitor widget." }
    )
}

$report = [PSCustomObject]@{
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    WbpPath = $wbpPath
    WbpRelativePath = $wbpRelativePath
    WbpPresent = [bool]$wbpItem
    WbpSizeBytes = if ($wbpItem) { [int64]$wbpItem.Length } else { 0 }
    WbpSize = if ($wbpItem) { Format-Size ([int64]$wbpItem.Length) } else { "0 B" }
    WbpLastWriteTime = if ($wbpItem) { $wbpItem.LastWriteTime.ToString("s") } else { "" }
    GitState = $gitState
    SetupDocPath = $setupDocPath
    SetupDocPresent = $setupDocPresent
    RequiredSetupTerms = $requiredSetupTerms
    MissingSetupTerms = $missingSetupTerms
    DecisionPoint = $wbpDecisionPoint
    ManualAcceptanceChecklist = $manualAcceptanceChecklist
    RiskLevel = $riskLevel
    RecommendedDecision = $recommendedDecision
    Recommendation = $recommendation
    EvidenceDraft = $evidenceDraft
    AcceptanceTemplate = [PSCustomObject]@{
        Script = $acceptanceTemplateScript
        Available = (Test-Path -LiteralPath $acceptanceTemplateScript -PathType Leaf)
        RequiredEvidenceCount = 4
        RequiredEvidence = @("Editor open verification", "Optional binding check", "PIE smoke result", "Production WBP acceptance")
        RequiredRuntimeFields = @("EvidenceRunId", "Operator", "VerifiedAt", "UnrealVersion", "MapName", "PieSession", "LogPath", "ScreenshotPath", "AssetHash")
        Boundary = "The acceptance template is read-only and does not replace Unreal Editor verification or project owner acceptance."
    }
    Summary = [PSCustomObject]@{
        Valid = $true
        MonitorPolicyValid = [bool]$monitorPolicyReport.Summary.Valid
        EvidencePath = $EvidencePath
        WbpDecisionPointPresent = [bool]$wbpDecisionPoint
        WbpPresent = [bool]$wbpItem
        WbpGitState = $gitState
        ReviewQueue = if ($wbpDecisionPoint) { [string]$wbpDecisionPoint.ReviewQueue } else { "NotPresent" }
        CommitReadiness = if ($wbpDecisionPoint) { [string]$wbpDecisionPoint.CommitReadiness } else { "NotPresent" }
        EvidenceStatus = if ($wbpDecisionPoint) { [string]$wbpDecisionPoint.EvidenceStatus } else { "NotPresent" }
        EvidenceSatisfied = if ($wbpDecisionPoint) { [bool]$wbpDecisionPoint.EvidenceSatisfied } else { $false }
        MissingEvidenceCount = if ($wbpDecisionPoint) { @($wbpDecisionPoint.MissingEvidence).Count } else { 0 }
        ManualAcceptanceChecklistCount = $manualAcceptanceChecklist.Count
        ManualAcceptanceMissingCount = @($manualAcceptanceChecklist | Where-Object { $_.Status -ne "Recorded" }).Count
        AcceptanceTemplateAvailable = (Test-Path -LiteralPath $acceptanceTemplateScript -PathType Leaf)
        AcceptanceTemplateRequiredEvidenceCount = 4
        ReadyToStage = if ($wbpDecisionPoint) { [string]$wbpDecisionPoint.ReviewQueue -eq "ReadyToStage" } else { $false }
        StagingBlocked = if ($wbpDecisionPoint) { [bool]$wbpDecisionPoint.CommitBlocker } else { $false }
        SetupDocContractComplete = ($setupDocPresent -and $missingSetupTerms.Count -eq 0)
        ManualEditorVerificationStillRequired = (-not $wbpDecisionPoint -or [string]$wbpDecisionPoint.ReviewQueue -ne "ReadyToStage")
        FailOnIncompleteEvidence = [bool]$FailOnIncompleteEvidence
    }
}

if ($FailOnIncompleteEvidence -and $report.WbpPresent -and -not $report.Summary.ReadyToStage) {
    throw "Monitor WBP evidence is incomplete. ReviewQueue=$($report.Summary.ReviewQueue) EvidenceStatus=$($report.Summary.EvidenceStatus) MissingEvidenceCount=$($report.Summary.MissingEvidenceCount). Record complete AcceptedForRepository evidence before staging $wbpRelativePath."
}

$lines = @(
    "# Monitor WBP Decision Report",
    "",
    "- Generated: $($report.GeneratedAt)",
    "- Project: $($report.ProjectRoot)",
    "- Source repo: $($report.SourceRepoRoot)",
    "- WBP present: $($report.WbpPresent)",
    "- WBP path: $($report.WbpPath)",
    "- Git state: $($report.GitState)",
    "- Size: $($report.WbpSize)",
    "- Last write: $($report.WbpLastWriteTime)",
    "- Setup doc present: $($report.SetupDocPresent)",
    "- Missing setup terms: $(@($report.MissingSetupTerms) -join ', ')",
    "- Review queue: $($report.Summary.ReviewQueue)",
    "- Commit readiness: $($report.Summary.CommitReadiness)",
    "- Evidence status: $($report.Summary.EvidenceStatus)",
    "- Missing evidence count: $($report.Summary.MissingEvidenceCount)",
    "- Ready to stage: $($report.Summary.ReadyToStage)",
    "- Staging blocked: $($report.Summary.StagingBlocked)",
    "- Manual editor verification still required: $($report.Summary.ManualEditorVerificationStillRequired)",
    "- Acceptance template available: $($report.Summary.AcceptanceTemplateAvailable)",
    "- Acceptance template required evidence: $($report.Summary.AcceptanceTemplateRequiredEvidenceCount)",
    "- Risk level: $($report.RiskLevel)",
    "- RecommendedDecision: $($report.RecommendedDecision)",
    "- Recommendation: $($report.Recommendation)",
    "",
    "## Evidence Draft",
    "",
    "- Path: $($report.EvidenceDraft.Path)",
    "- Decision owner: $($report.EvidenceDraft.DecisionOwner)",
    "- Decision status: $($report.EvidenceDraft.DecisionStatus)",
    "- Evidence source: $($report.EvidenceDraft.EvidenceSource)",
    "",
    "## Manual Acceptance Checklist",
    ""
)

foreach ($item in $manualAcceptanceChecklist) {
    $lines += "- $($item.Name): $($item.Status) - $($item.EvidenceNeeded)"
}

$lines += @(
    "",
    "This report cannot replace Unreal Editor verification. It separates file metadata and setup-contract checks from the remaining manual WBP evidence."
)

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
