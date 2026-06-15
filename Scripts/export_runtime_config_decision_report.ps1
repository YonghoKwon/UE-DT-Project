param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
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

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$gameIniPath = Join-Path $ProjectRoot "Config\Game.ini"
$runtimeOverride = Read-RuntimeOverrideSection -Path $gameIniPath

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
    Path = "Config\Game.ini"
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

$report = [PSCustomObject]@{
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    GameIniPath = $gameIniPath
    GameIniPresent = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
    RuntimeOverridePresent = $runtimeOverride.Present
    RuntimeOverrideKeys = $runtimeOverride.Keys
    EmptyRuntimeOverrideKeys = $runtimeOverride.EmptyKeys
    NonEmptyRuntimeOverrideKeys = $runtimeOverride.NonEmptyKeys
    ValuesRedacted = $true
    RiskLevel = $riskLevel
    Recommendation = $recommendation
    EvidenceDraft = $evidenceDraft
}

$lines = @(
    "# Runtime Config Decision Report",
    "",
    "- Generated: $($report.GeneratedAt)",
    "- Project: $($report.ProjectRoot)",
    "- Game.ini present: $($report.GameIniPresent)",
    "- Runtime override present: $($report.RuntimeOverridePresent)",
    "- Runtime override keys: $(@($report.RuntimeOverrideKeys) -join ', ')",
    "- Non-empty runtime override keys: $(@($report.NonEmptyRuntimeOverrideKeys) -join ', ')",
    "- Values redacted: $($report.ValuesRedacted)",
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
