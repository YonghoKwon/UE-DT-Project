param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$SourceRepoRoot = "",
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

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
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

$wbpRelativePath = "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset"
$decisionReportScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_decision_report.ps1"
if (-not (Test-Path -LiteralPath $decisionReportScript -PathType Leaf)) {
    throw "export_monitor_wbp_decision_report.ps1 not found: $decisionReportScript"
}

$decisionJson = & powershell -ExecutionPolicy Bypass -File $decisionReportScript -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Monitor WBP decision report failed with exit code $LASTEXITCODE"
}
$decisionReport = $decisionJson | ConvertFrom-Json
$assetHash = ""
if (Test-Path -LiteralPath $decisionReport.WbpPath -PathType Leaf) {
    $assetHash = (Get-FileHash -LiteralPath $decisionReport.WbpPath -Algorithm SHA256).Hash
}

$optionalBindings = @(
    "SensorModeToggleButton",
    "CaptureOnceButton",
    "ExportCurrentFrameButton",
    "PointCloudOnlyToggle",
    "PreviewStrideSpinBox",
    "MaxPreviewPointsSpinBox",
    "PreviewHitOnlyButton",
    "LidarViewModeComboBox",
    "SensorStatusText",
    "CameraPreviewImage"
)

$pieSmokeChecks = @(
    "WBP opens in the intended map without compile or load errors.",
    "Camera/LiDAR mode switching updates the visible status text.",
    "PointCloudOnly toggle updates manager state without losing collision trace behavior.",
    "Preview stride and max preview point controls update preview policy values.",
    "Capture once and export current frame controls produce expected status/result text.",
    "Slab angle/deviation/confidence status row is visible or explicitly marked unavailable.",
    "Transport/performance warning row is visible when warning text exists."
)

$template = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    DryRunOnly = $true
    ModifiesAssets = $false
    StagesWbp = $false
    WbpRelativePath = $wbpRelativePath
    AssetPath = $wbpRelativePath
    WbpPresent = [bool]$decisionReport.WbpPresent
    WbpGitState = [string]$decisionReport.GitState
    AssetSizeBytes = [int64]$decisionReport.WbpSizeBytes
    WbpSize = [string]$decisionReport.WbpSize
    AssetHashAlgorithm = "SHA256"
    AssetHash = $assetHash
    CurrentReviewQueue = [string]$decisionReport.Summary.ReviewQueue
    CurrentCommitReadiness = [string]$decisionReport.Summary.CommitReadiness
    CurrentEvidenceStatus = [string]$decisionReport.Summary.EvidenceStatus
    CurrentReadyToStage = [bool]$decisionReport.Summary.ReadyToStage
    MustRemainUntrackedUntilAccepted = (-not [bool]$decisionReport.Summary.ReadyToStage)
    RequiredEvidence = @(
        [PSCustomObject]@{
            Name = "Editor open verification"
            Status = "Pending"
            Required = $true
            EvidenceRunId = ""
            Operator = ""
            VerifiedAt = ""
            UnrealVersion = "5.3"
            MapName = ""
            WidgetBlueprintPath = $wbpRelativePath
            OpenedInEditor = $false
            CompiledWithoutErrors = $false
            SaveAttemptedOrNot = ""
            EditorLogPath = ""
            ScreenshotPath = ""
            ErrorSummary = ""
            WarningSummary = ""
            Notes = "Open the WBP in Unreal Editor and verify it loads/compiles without errors."
        },
        [PSCustomObject]@{
            Name = "Optional binding check"
            Status = "Pending"
            Required = $true
            EvidenceRunId = ""
            Operator = ""
            VerifiedAt = ""
            SetupDocPath = "docs/widget_designer_setup.md"
            OptionalBindings = @(
                $optionalBindings |
                    ForEach-Object {
                        [PSCustomObject]@{
                            Name = $_
                            Present = $false
                            IsVariable = $false
                            AutoBoundOrManual = ""
                            MissingOptionalDoesNotCrash = $false
                            Notes = ""
                        }
                    }
            )
            MissingOptionalBindings = @()
            UnexpectedRequiredBindingFailures = @()
            Notes = "Optional bindings may be absent, but missing optional widgets must not crash the monitor."
        },
        [PSCustomObject]@{
            Name = "PIE smoke result"
            Status = "Pending"
            Required = $true
            EvidenceRunId = ""
            Operator = ""
            VerifiedAt = ""
            MapName = ""
            PieSession = ""
            HostActorOrLevelBlueprintBinding = ""
            SensorManagerBinding = ""
            SelectedCameraId = ""
            SelectedLidarId = ""
            LogPath = ""
            ScreenshotPath = ""
            ExportedPayloadPath = ""
            Checks = @(
                $pieSmokeChecks |
                    ForEach-Object {
                        [PSCustomObject]@{
                            Name = $_
                            Status = "NotRun"
                            Notes = ""
                        }
                    }
            )
            Notes = "Run PIE in the intended map with the production monitor host/binding path."
        },
        [PSCustomObject]@{
            Name = "Production WBP acceptance"
            Status = "Pending"
            Required = $true
            EvidenceRunId = ""
            AcceptedBy = ""
            AcceptedAt = ""
            EvidenceSource = ""
            DecisionStatus = "EvidencePending"
            OwnerDecisionNote = ""
            Notes = "Project owner confirms this binary asset is the production operator monitor widget before AcceptedForRepository."
        }
    )
    LocalAssetDecisionEvidenceDraft = [PSCustomObject]@{
        Path = $wbpRelativePath
        DecisionOwner = "ProjectOwnerRequired"
        DecisionStatus = "EvidencePending"
        AcceptedBy = ""
        AcceptedAt = ""
        EvidenceSource = "Scripts/export_monitor_wbp_acceptance_template.ps1"
        Notes = "Fill this only after Editor open, optional binding, PIE smoke, and production acceptance evidence are complete."
        Evidence = @(
            [PSCustomObject]@{ Name = "Editor open verification"; Status = "Pending"; Source = ""; Note = "Attach editor screenshot/log evidence." },
            [PSCustomObject]@{ Name = "Optional binding check"; Status = "Pending"; Source = ""; Note = "Attach binding checklist evidence." },
            [PSCustomObject]@{ Name = "PIE smoke result"; Status = "Pending"; Source = ""; Note = "Attach PIE map/session/log evidence." },
            [PSCustomObject]@{ Name = "Production WBP acceptance"; Status = "Pending"; Source = ""; Note = "Attach owner acceptance evidence." }
        )
    }
    Summary = [PSCustomObject]@{
        RequiredEvidenceCount = 4
        PendingEvidenceCount = 4
        OptionalBindingCount = $optionalBindings.Count
        PieSmokeCheckCount = $pieSmokeChecks.Count
        DryRunOnly = $true
        ModifiesAssets = $false
        StagesWbp = $false
        MustRemainUntrackedUntilAccepted = (-not [bool]$decisionReport.Summary.ReadyToStage)
        CurrentReadyToStage = [bool]$decisionReport.Summary.ReadyToStage
        Valid = $true
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Acceptance Template") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($template.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($template.ProjectRoot)") | Out-Null
$lines.Add("- WBP: $($template.WbpRelativePath)") | Out-Null
$lines.Add("- WBP present: $($template.WbpPresent)") | Out-Null
$lines.Add("- WBP git state: $($template.WbpGitState)") | Out-Null
$lines.Add("- Dry run only: $($template.DryRunOnly)") | Out-Null
$lines.Add("- Modifies assets: $($template.ModifiesAssets)") | Out-Null
$lines.Add("- Stages WBP: $($template.StagesWbp)") | Out-Null
$lines.Add("- Must remain untracked until accepted: $($template.MustRemainUntrackedUntilAccepted)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Evidence | Status | Required | Notes |") | Out-Null
$lines.Add("| --- | --- | --- | --- |") | Out-Null
foreach ($item in $template.RequiredEvidence) {
    $lines.Add(("| {0} | {1} | {2} | {3} |" -f `
        (Convert-ToMarkdownCell $item.Name),
        (Convert-ToMarkdownCell $item.Status),
        (Convert-ToMarkdownCell $item.Required),
        (Convert-ToMarkdownCell $item.Notes))) | Out-Null
}

if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-TextFile -Path $MarkdownPath -Lines $lines
}
if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $template | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}

if ($Json) {
    $template | ConvertTo-Json -Depth 10
}
else {
    $lines | ForEach-Object { Write-Host $_ }
}
