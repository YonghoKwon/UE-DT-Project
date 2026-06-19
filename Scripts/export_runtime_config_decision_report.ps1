param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [switch]$FailOnIncompleteEvidence,
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Read-RuntimeOverrideSection {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [PSCustomObject]@{
            Present = $false
            Keys = @()
            EmptyKeys = @()
            NonEmptyKeys = @()
        }
    }

    $values = @{}
    $inSection = $false
    foreach ($line in @(Get-Content -LiteralPath $Path)) {
        $trimmed = $line.Trim()
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $inSection = ($trimmed -eq "[DTCoreRuntimeOverride]")
            continue
        }
        if (-not $inSection -or [string]::IsNullOrWhiteSpace($trimmed) -or -not $trimmed.Contains("=")) {
            continue
        }

        $parts = $trimmed.Split("=", 2)
        if ($parts.Count -eq 2) {
            $values[$parts[0].Trim()] = $parts[1].Trim()
        }
    }

    $keys = @($values.Keys | Sort-Object)
    return [PSCustomObject]@{
        Present = ($values.Count -gt 0 -or (Select-String -LiteralPath $Path -Pattern "[DTCoreRuntimeOverride]" -SimpleMatch -Quiet))
        Keys = $keys
        EmptyKeys = @($keys | Where-Object { [string]::IsNullOrWhiteSpace([string]$values[$_]) })
        NonEmptyKeys = @($keys | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$values[$_]) })
    }
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
$gameIniPath = Join-Path $ProjectRoot "Config\Game.ini"
$assetReportScript = Join-Path $SourceRepoRoot "Scripts\report_local_project_status.ps1"
$runtimePolicyScript = Join-Path $SourceRepoRoot "Scripts\validate_runtime_config_policy.ps1"
$runtimeAcceptanceTemplateScript = Join-Path $SourceRepoRoot "Scripts\export_runtime_config_acceptance_template.ps1"
$runtimeOverride = Read-RuntimeOverrideSection -Path $gameIniPath

$assetReportParams = @{ ProjectRoot = $ProjectRoot }
if (-not [string]::IsNullOrWhiteSpace($EvidencePath)) {
    $assetReportParams.EvidencePath = $EvidencePath
}
$assetDecisionReport = Invoke-JsonScript `
    -ScriptPath $assetReportScript `
    -Parameters $assetReportParams
$runtimePolicyReport = Invoke-JsonScript `
    -ScriptPath $runtimePolicyScript `
    -Parameters @{ ProjectRoot = $SourceRepoRoot; LocalProjectRoot = $ProjectRoot }

$configRelativePath = "Config\Game.ini"
$configDecisionPoint = @($assetDecisionReport.DecisionPoints | Where-Object { $_.Path -eq $configRelativePath }) | Select-Object -First 1
if ((Test-Path -LiteralPath $gameIniPath -PathType Leaf) -and -not $configDecisionPoint) {
    throw "Config/Game.ini decision point not found in report_local_project_status.ps1 output."
}

$decisionStatus = "NotPresent"
$recommendation = "No local Config/Game.ini decision is needed because the file is not present."
$riskLevel = "None"
if (Test-Path -LiteralPath $gameIniPath -PathType Leaf) {
    if ($runtimeOverride.NonEmptyKeys.Count -gt 0) {
        $decisionStatus = "KeepLocal"
        $riskLevel = "High"
        $recommendation = "Keep Config/Game.ini local. It contains non-empty runtime override keys and must not be staged until endpoint/credential ownership is explicitly reviewed."
    }
    elseif ($runtimeOverride.Present) {
        $decisionStatus = "KeepLocal"
        $riskLevel = "Low"
        $recommendation = "Keep Config/Game.ini local by default. The DTCore runtime override section is present but empty, so it is a local override placeholder rather than a useful shared default."
    }
    else {
        $decisionStatus = "PendingOwnerDecision"
        $riskLevel = "Medium"
        $recommendation = "Inspect Config/Game.ini manually. It exists without [DTCoreRuntimeOverride], so this script cannot classify the setting intent."
    }
}

$evidenceDraft = [PSCustomObject]@{
    Path = $configRelativePath
    DecisionOwner = "ConfigOwnerRequired"
    DecisionStatus = $decisionStatus
    AcceptedBy = ""
    AcceptedAt = ""
    EvidenceSource = "Scripts/export_runtime_config_decision_report.ps1"
    Notes = $recommendation
    Evidence = @(
        [PSCustomObject]@{ Name = "Manual diff review"; Status = if ($decisionStatus -eq "KeepLocal") { "Complete" } else { "Pending" }; Source = "Scripts/export_runtime_config_decision_report.ps1"; Note = "Runtime override keys classified without printing values." },
        [PSCustomObject]@{ Name = "No endpoint or credential values"; Status = if ($runtimeOverride.NonEmptyKeys.Count -eq 0) { "Complete" } else { "Pending" }; Source = "Scripts/export_runtime_config_decision_report.ps1"; Note = if ($runtimeOverride.NonEmptyKeys.Count -eq 0) { "All parsed runtime override values are empty." } else { "Non-empty keys detected; values intentionally redacted." } },
        [PSCustomObject]@{ Name = "Shared-defaults decision"; Status = "Pending"; Source = ""; Note = "Requires project owner decision before AcceptedForRepository." },
        [PSCustomObject]@{ Name = "Runtime config policy pass"; Status = if ($runtimeOverride.NonEmptyKeys.Count -eq 0) { "Complete" } else { "Pending" }; Source = "Scripts/validate_runtime_config_policy.ps1"; Note = "Policy allows empty or absent runtime overrides." }
    )
}

$manualAcceptanceChecklist = @(
    [PSCustomObject]@{
        Name = "Manual diff review"
        Status = if ($configDecisionPoint -and @($configDecisionPoint.MissingEvidence) -notcontains "Manual diff review") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Review Config/Game.ini manually without printing sensitive values."
    },
    [PSCustomObject]@{
        Name = "No endpoint or credential values"
        Status = if ($runtimeOverride.NonEmptyKeys.Count -eq 0 -and $configDecisionPoint -and @($configDecisionPoint.MissingEvidence) -notcontains "No endpoint or credential values") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Confirm Config/Game.ini contains no endpoint, credential, token, password, or secret values."
    },
    [PSCustomObject]@{
        Name = "Shared-defaults decision"
        Status = if ($configDecisionPoint -and @($configDecisionPoint.MissingEvidence) -notcontains "Shared-defaults decision") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Config owner decides whether this file remains local or becomes accepted shared defaults."
    },
    [PSCustomObject]@{
        Name = "Runtime config policy pass"
        Status = if ([bool]$runtimePolicyReport.Summary.Valid -and $configDecisionPoint -and @($configDecisionPoint.MissingEvidence) -notcontains "Runtime config policy pass") { "Recorded" } else { "Missing" }
        EvidenceNeeded = "Run validate_runtime_config_policy.ps1 against the local project and record the result."
    }
)

$report = [PSCustomObject]@{
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    GameIniPath = $gameIniPath
    GameIniPresent = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
    RuntimeOverridePresent = $runtimeOverride.Present
    RuntimeOverrideKeys = $runtimeOverride.Keys
    EmptyRuntimeOverrideKeys = $runtimeOverride.EmptyKeys
    NonEmptyRuntimeOverrideKeys = $runtimeOverride.NonEmptyKeys
    ValuesRedacted = $true
    RiskLevel = $riskLevel
    RecommendedDecision = $decisionStatus
    Recommendation = $recommendation
    DecisionPoint = $configDecisionPoint
    ManualAcceptanceChecklist = $manualAcceptanceChecklist
    EvidenceDraft = $evidenceDraft
    AcceptanceTemplate = [PSCustomObject]@{
        Script = $runtimeAcceptanceTemplateScript
        Available = (Test-Path -LiteralPath $runtimeAcceptanceTemplateScript -PathType Leaf)
        RequiredEvidenceCount = 4
        RequiredEvidence = @("Manual diff review", "No endpoint or credential values", "Shared-defaults decision", "Runtime config policy pass")
        ValuesRedacted = $true
        Boundary = "The acceptance template is read-only, redacts values, and does not replace config owner acceptance."
    }
    Summary = [PSCustomObject]@{
        Valid = $true
        RuntimePolicyValid = [bool]$runtimePolicyReport.Summary.Valid
        EvidencePath = $EvidencePath
        GameIniDecisionPointPresent = [bool]$configDecisionPoint
        GameIniPresent = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
        GameIniGitState = if ($configDecisionPoint) { [string]$configDecisionPoint.GitState } else { "NotPresent" }
        ReviewQueue = if ($configDecisionPoint) { [string]$configDecisionPoint.ReviewQueue } else { "NotPresent" }
        CommitReadiness = if ($configDecisionPoint) { [string]$configDecisionPoint.CommitReadiness } else { "NotPresent" }
        EvidenceStatus = if ($configDecisionPoint) { [string]$configDecisionPoint.EvidenceStatus } else { "NotPresent" }
        EvidenceSatisfied = if ($configDecisionPoint) { [bool]$configDecisionPoint.EvidenceSatisfied } else { $false }
        MissingEvidenceCount = if ($configDecisionPoint) { @($configDecisionPoint.MissingEvidence).Count } else { 0 }
        ManualAcceptanceChecklistCount = $manualAcceptanceChecklist.Count
        ManualAcceptanceMissingCount = @($manualAcceptanceChecklist | Where-Object { $_.Status -ne "Recorded" }).Count
        AcceptanceTemplateAvailable = (Test-Path -LiteralPath $runtimeAcceptanceTemplateScript -PathType Leaf)
        AcceptanceTemplateRequiredEvidenceCount = 4
        ReadyToStage = if ($configDecisionPoint) { [string]$configDecisionPoint.ReviewQueue -eq "ReadyToStage" } else { $false }
        StagingBlocked = if ($configDecisionPoint) { [bool]$configDecisionPoint.CommitBlocker } else { $false }
        ManualConfigOwnerDecisionStillRequired = (Test-Path -LiteralPath $gameIniPath -PathType Leaf) -and (-not $configDecisionPoint -or ([string]$configDecisionPoint.ReviewQueue -notin @("ReadyToStage", "KeepLocal")))
        FailOnIncompleteEvidence = [bool]$FailOnIncompleteEvidence
    }
}

if ($FailOnIncompleteEvidence -and $report.GameIniPresent -and -not $report.Summary.ReadyToStage) {
    throw "Runtime config evidence is incomplete. ReviewQueue=$($report.Summary.ReviewQueue) EvidenceStatus=$($report.Summary.EvidenceStatus) MissingEvidenceCount=$($report.Summary.MissingEvidenceCount). Record complete AcceptedForRepository evidence before staging Config/Game.ini."
}

$lines = @(
    "# Runtime Config Decision Report",
    "",
    "- Generated: $($report.GeneratedAt)",
    "- Project: $($report.ProjectRoot)",
    "- Source repo: $($report.SourceRepoRoot)",
    "- Game.ini present: $($report.GameIniPresent)",
    "- Runtime override present: $($report.RuntimeOverridePresent)",
    "- Runtime override keys: $(@($report.RuntimeOverrideKeys) -join ', ')",
    "- Non-empty runtime override keys: $(@($report.NonEmptyRuntimeOverrideKeys) -join ', ')",
    "- Values redacted: $($report.ValuesRedacted)",
    "- Review queue: $($report.Summary.ReviewQueue)",
    "- Commit readiness: $($report.Summary.CommitReadiness)",
    "- Evidence status: $($report.Summary.EvidenceStatus)",
    "- Missing evidence count: $($report.Summary.MissingEvidenceCount)",
    "- Ready to stage: $($report.Summary.ReadyToStage)",
    "- Staging blocked: $($report.Summary.StagingBlocked)",
    "- Manual config owner decision still required: $($report.Summary.ManualConfigOwnerDecisionStillRequired)",
    "- Acceptance template available: $($report.Summary.AcceptanceTemplateAvailable)",
    "- Acceptance template required evidence: $($report.Summary.AcceptanceTemplateRequiredEvidenceCount)",
    "- Risk level: $($report.RiskLevel)",
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
    "The generated evidence draft is advisory. It can support a KeepLocal decision, but AcceptedForRepository still requires explicit owner acceptance."
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
