param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
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

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$wbpRelativePath = "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset"
$wbpPath = Join-Path $ProjectRoot $wbpRelativePath
$setupDocPath = Join-Path $ProjectRoot "docs\widget_designer_setup.md"
$assetReportScript = Join-Path $ProjectRoot "Scripts\report_local_project_status.ps1"

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
    "M7AT10.SensorMonitor.LidarStatusTextContract"
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

$recommendedDecision = "NotPresent"
$riskLevel = "None"
$recommendation = "WBP_VirtualSensorMonitor.uasset is not present in the local project."
if ($wbpItem) {
    $recommendedDecision = "PendingOwnerDecision"
    $riskLevel = "Medium"
    $recommendation = "Keep the local WBP untracked until it is opened in Unreal Editor, optional bindings are checked, PIE smoke passes, and the project owner accepts it as the production monitor WBP."
}

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
    RiskLevel = $riskLevel
    RecommendedDecision = $recommendedDecision
    Recommendation = $recommendation
    EvidenceDraft = $evidenceDraft
}

$lines = @(
    "# Monitor WBP Decision Report",
    "",
    "- Generated: $($report.GeneratedAt)",
    "- Project: $($report.ProjectRoot)",
    "- WBP present: $($report.WbpPresent)",
    "- WBP path: $($report.WbpPath)",
    "- Git state: $($report.GitState)",
    "- Size: $($report.WbpSize)",
    "- Last write: $($report.WbpLastWriteTime)",
    "- Setup doc present: $($report.SetupDocPresent)",
    "- Missing setup terms: $(@($report.MissingSetupTerms) -join ', ')",
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
