param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
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

    $output = @(& powershell -ExecutionPolicy Bypass -File $ScriptPath @Parameters -Json)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Get-GitPorcelain {
    param(
        [string]$WorkingDirectory,
        [string]$Path
    )

    Push-Location $WorkingDirectory
    try {
        $args = @("status", "--porcelain=v1", "--untracked-files=all")
        if (-not [string]::IsNullOrWhiteSpace($Path)) {
            $args += @("--", $Path)
        }
        $output = @(& git.exe @args 2>$null)
        if ($LASTEXITCODE -ne 0) {
            return @()
        }
        return $output
    }
    finally {
        Pop-Location
    }
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

$wbpRelativePath = "Content\MA0T10\UI\WBP_VirtualSensorMonitor.uasset"
$wbpPath = Join-Path $ProjectRoot $wbpRelativePath
$decisionReportScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_decision_report.ps1"
$acceptanceTemplateScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_acceptance_template.ps1"
$archiveEvidenceScript = Join-Path $SourceRepoRoot "Scripts\export_unused_content_archive_evidence.ps1"
$setupDocPath = Join-Path $SourceRepoRoot "docs\widget_designer_setup.md"

$decisionReport = Invoke-JsonScript -ScriptPath $decisionReportScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}
$acceptanceTemplate = Invoke-JsonScript -ScriptPath $acceptanceTemplateScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}
$archiveEvidence = $null
if (Test-Path -LiteralPath $archiveEvidenceScript -PathType Leaf) {
    $archiveEvidence = Invoke-JsonScript -ScriptPath $archiveEvidenceScript -Parameters @{
        ProjectRoot = $ProjectRoot
    }
}

$wbpPresent = Test-Path -LiteralPath $wbpPath -PathType Leaf
$assetHash = if ($wbpPresent) { (Get-FileHash -LiteralPath $wbpPath -Algorithm SHA256).Hash } else { "" }
$gitLines = @(Get-GitPorcelain -WorkingDirectory $ProjectRoot -Path $wbpRelativePath)
$stagedLines = @($gitLines | Where-Object { $_ -match "^[MADRCU]" })
$untrackedLines = @($gitLines | Where-Object { $_.StartsWith("?? ") })
$worktreeModifiedLines = @($gitLines | Where-Object { $_.Length -ge 2 -and $_.Substring(1, 1) -ne " " })

$setupDocPresent = Test-Path -LiteralPath $setupDocPath -PathType Leaf
$setupTerms = @(
    "WBP_VirtualSensorMonitor",
    "Optional Bindings",
    "BindSensorManager",
    "AVirtualSensorMonitorHostActor",
    "ExportServerPayloadButton",
    "PreviewHitOnlyButton",
    "StatusText"
)
$missingSetupTerms = @()
if ($setupDocPresent) {
    foreach ($term in $setupTerms) {
        if (-not (Select-String -LiteralPath $setupDocPath -Pattern $term -SimpleMatch -Quiet)) {
            $missingSetupTerms += $term
        }
    }
}
else {
    $missingSetupTerms = $setupTerms
}

$preflightChecks = @(
    [PSCustomObject]@{
        Name = "WBP asset exists"
        Status = if ($wbpPresent) { "Passed" } else { "Blocked" }
        Evidence = $wbpPath
    },
    [PSCustomObject]@{
        Name = "WBP remains unstaged"
        Status = if ($stagedLines.Count -eq 0) { "Passed" } else { "Blocked" }
        Evidence = "StagedLineCount=$($stagedLines.Count)"
    },
    [PSCustomObject]@{
        Name = "Setup document contract"
        Status = if ($setupDocPresent -and $missingSetupTerms.Count -eq 0) { "Passed" } else { "Blocked" }
        Evidence = $setupDocPath
    },
    [PSCustomObject]@{
        Name = "Acceptance template available"
        Status = if ([bool]$acceptanceTemplate.Summary.Valid) { "Passed" } else { "Blocked" }
        Evidence = $acceptanceTemplateScript
    },
    [PSCustomObject]@{
        Name = "Manual evidence remains incomplete"
        Status = if (-not [bool]$decisionReport.Summary.ReadyToStage) { "Expected" } else { "Recorded" }
        Evidence = "MissingEvidenceCount=$($decisionReport.Summary.MissingEvidenceCount)"
    },
    [PSCustomObject]@{
        Name = "Unused content archived"
        Status = if ($archiveEvidence -and [bool]$archiveEvidence.Summary.ArchiveEvidenceComplete) { "Passed" } elseif ($archiveEvidence) { "Pending" } else { "NotAvailable" }
        Evidence = if ($archiveEvidence) { "ArchivedCount=$($archiveEvidence.Summary.ArchivedCount)" } else { "" }
    }
)

$blockedCount = @($preflightChecks | Where-Object { $_.Status -eq "Blocked" }).Count
$readyForManualEditorReview = ($blockedCount -eq 0 -and $wbpPresent -and $stagedLines.Count -eq 0)

$recommendedManualCommands = @(
    "Open Unreal Editor 5.3 with C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject.",
    "Open Content/MA0T10/UI/WBP_VirtualSensorMonitor and compile/save only if the project owner accepts the asset update.",
    "Compare optional bindings against docs/widget_designer_setup.md.",
    "Run PIE in the intended map with AVirtualSensorMonitorHostActor or Level Blueprint binding.",
    "Fill Scripts/export_monitor_wbp_acceptance_template.ps1 output with editor-open, binding, PIE, and owner acceptance evidence before staging the WBP."
)

$editCapability = [PSCustomObject]@{
    DirectBinaryPatchSupported = $false
    EditorMediatedAssetEditRequired = $true
    CodexCanModifyNativeBindings = $true
    CodexCanModifyAcceptanceTooling = $true
    RequiresPreEditHash = $true
    RequiresBackupBeforeAssetEdit = $true
    RequiresEditorCompileAndSaveEvidence = $true
    RequiresPostEditHash = $true
    RequiresPieSmokeAfterEdit = $true
    RequiresOwnerAcceptanceBeforeStage = $true
    Boundary = "Do not patch WBP .uasset bytes directly. Make reusable UI behavior in C++/Blueprint-callable bindings first, then edit/save the WBP only through Unreal Editor with hash, backup, compile, PIE, and owner evidence."
}

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    WbpRelativePath = $wbpRelativePath
    WbpPath = $wbpPath
    WbpPresent = $wbpPresent
    AssetHashAlgorithm = "SHA256"
    AssetHash = $assetHash
    GitStatusLines = $gitLines
    StagedLineCount = $stagedLines.Count
    UntrackedLineCount = $untrackedLines.Count
    WorktreeModifiedLineCount = $worktreeModifiedLines.Count
    SetupDocPath = $setupDocPath
    SetupDocPresent = $setupDocPresent
    MissingSetupTerms = $missingSetupTerms
    DecisionSummary = $decisionReport.Summary
    AcceptanceTemplateSummary = $acceptanceTemplate.Summary
    ArchiveEvidenceSummary = if ($archiveEvidence) { $archiveEvidence.Summary } else { $null }
    PreflightChecks = $preflightChecks
    RecommendedManualCommands = $recommendedManualCommands
    EditCapability = $editCapability
    Summary = [PSCustomObject]@{
        WbpPresent = $wbpPresent
        WbpUntracked = ($untrackedLines.Count -gt 0)
        WbpStaged = ($stagedLines.Count -gt 0)
        WbpWorktreeModified = ($worktreeModifiedLines.Count -gt 0)
        AssetHash = $assetHash
        SetupDocContractComplete = ($setupDocPresent -and $missingSetupTerms.Count -eq 0)
        MissingSetupTermCount = $missingSetupTerms.Count
        AcceptanceTemplateAvailable = [bool]$acceptanceTemplate.Summary.Valid
        RequiredEvidenceCount = [int]$acceptanceTemplate.Summary.RequiredEvidenceCount
        PendingEvidenceCount = [int]$acceptanceTemplate.Summary.PendingEvidenceCount
        DecisionReadyToStage = [bool]$decisionReport.Summary.ReadyToStage
        DecisionMissingEvidenceCount = [int]$decisionReport.Summary.MissingEvidenceCount
        PreflightCheckCount = $preflightChecks.Count
        BlockedPreflightCheckCount = $blockedCount
        ReadyForManualEditorReview = $readyForManualEditorReview
        WbpDirectBinaryPatchSupported = [bool]$editCapability.DirectBinaryPatchSupported
        EditorMediatedAssetEditRequired = [bool]$editCapability.EditorMediatedAssetEditRequired
        CodexCanModifyNativeBindings = [bool]$editCapability.CodexCanModifyNativeBindings
        RequiresBackupBeforeAssetEdit = [bool]$editCapability.RequiresBackupBeforeAssetEdit
        RequiresEditorCompileAndSaveEvidence = [bool]$editCapability.RequiresEditorCompileAndSaveEvidence
        RequiresPieSmokeAfterEdit = [bool]$editCapability.RequiresPieSmokeAfterEdit
        RequiresOwnerAcceptanceBeforeStage = [bool]$editCapability.RequiresOwnerAcceptanceBeforeStage
        ModifiesAssets = $false
        StagesWbp = $false
        Boundary = "WBP preflight is read-only. It prepares manual editor/PIE evidence collection; it is not WBP acceptance and does not permit staging the binary asset."
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Preflight Report") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($report.ProjectRoot)") | Out-Null
$lines.Add("- WBP present: $($report.Summary.WbpPresent)") | Out-Null
$lines.Add("- WBP untracked: $($report.Summary.WbpUntracked)") | Out-Null
$lines.Add("- WBP staged: $($report.Summary.WbpStaged)") | Out-Null
$lines.Add("- Asset hash: $($report.Summary.AssetHash)") | Out-Null
$lines.Add("- Setup doc complete: $($report.Summary.SetupDocContractComplete)") | Out-Null
$lines.Add("- Decision ready to stage: $($report.Summary.DecisionReadyToStage)") | Out-Null
$lines.Add("- Decision missing evidence count: $($report.Summary.DecisionMissingEvidenceCount)") | Out-Null
$lines.Add("- Ready for manual editor review: $($report.Summary.ReadyForManualEditorReview)") | Out-Null
$lines.Add("- Direct binary patch supported: $($report.Summary.WbpDirectBinaryPatchSupported)") | Out-Null
$lines.Add("- Editor-mediated asset edit required: $($report.Summary.EditorMediatedAssetEditRequired)") | Out-Null
$lines.Add("- Codex can modify native bindings: $($report.Summary.CodexCanModifyNativeBindings)") | Out-Null
$lines.Add("- Requires backup before asset edit: $($report.Summary.RequiresBackupBeforeAssetEdit)") | Out-Null
$lines.Add("- Modifies assets: $($report.Summary.ModifiesAssets)") | Out-Null
$lines.Add("- Stages WBP: $($report.Summary.StagesWbp)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Checks") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Check | Status | Evidence |") | Out-Null
$lines.Add("| --- | --- | --- |") | Out-Null
foreach ($check in $preflightChecks) {
    $lines.Add(("| {0} | {1} | {2} |" -f `
        (Convert-ToMarkdownCell $check.Name),
        (Convert-ToMarkdownCell $check.Status),
        (Convert-ToMarkdownCell $check.Evidence))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Edit Capability") | Out-Null
$lines.Add("") | Out-Null
$lines.Add($editCapability.Boundary) | Out-Null

if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-TextFile -Path $MarkdownPath -Lines $lines
}
if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    $lines | ForEach-Object { Write-Host $_ }
}
