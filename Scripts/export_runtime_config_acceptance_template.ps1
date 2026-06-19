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

$configRelativePath = "Config\Game.ini"
$gameIniPath = Join-Path $ProjectRoot $configRelativePath
$runtimeOverride = Read-RuntimeOverrideSection -Path $gameIniPath
$configHash = ""
if (Test-Path -LiteralPath $gameIniPath -PathType Leaf) {
    $configHash = (Get-FileHash -LiteralPath $gameIniPath -Algorithm SHA256).Hash
}

$gitStatusLines = @()
Push-Location $ProjectRoot
try {
    $gitStatusLines = @(git status --porcelain=v1 --untracked-files=all -- $configRelativePath)
}
finally {
    Pop-Location
}

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

$currentReadyToStage = $false

$requiredEvidence = @(
    [PSCustomObject]@{
        Name = "Manual diff review"
        Status = "Pending"
        Required = $true
        EvidenceRunId = ""
        Operator = ""
        ReviewedAt = ""
        ConfigPath = $configRelativePath
        ValuesPrinted = $false
        ValuesRedacted = $true
        ReviewedKeys = $runtimeOverride.Keys
        Notes = "Review key names and diff context without printing endpoint, token, password, or credential values."
    },
    [PSCustomObject]@{
        Name = "No endpoint or credential values"
        Status = "Pending"
        Required = $true
        EvidenceRunId = ""
        Operator = ""
        ReviewedAt = ""
        NonEmptyRuntimeOverrideKeys = $runtimeOverride.NonEmptyKeys
        EndpointCredentialLeakageDetected = ($runtimeOverride.NonEmptyKeys.Count -gt 0)
        SecretScanTool = ""
        SecretScanResultPath = ""
        Notes = "If any runtime override key is non-empty, keep this file local or scrub it before staging."
    },
    [PSCustomObject]@{
        Name = "Shared-defaults decision"
        Status = "Pending"
        Required = $true
        ConfigOwner = ""
        DecisionAt = ""
        Decision = "KeepLocal"
        ApprovedSharedDefaultKeys = @()
        RejectedLocalOnlyKeys = $runtimeOverride.Keys
        Notes = "Config owner decides whether values are local-only runtime overrides or intentional project-wide defaults."
    },
    [PSCustomObject]@{
        Name = "Runtime config policy pass"
        Status = "Pending"
        Required = $true
        EvidenceRunId = ""
        Operator = ""
        RanAt = ""
        Command = 'powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt"'
        Result = ""
        LogPath = ""
        Notes = "Policy must pass against the real local project root before any repository acceptance."
    }
)

$template = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    DryRunOnly = $true
    ModifiesConfig = $false
    StagesConfig = $false
    ValuesRedacted = $true
    ConfigRelativePath = $configRelativePath
    ConfigPresent = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
    ConfigGitState = $gitState
    ConfigHashAlgorithm = "SHA256"
    ConfigHash = $configHash
    RuntimeOverridePresent = [bool]$runtimeOverride.Present
    RuntimeOverrideKeys = $runtimeOverride.Keys
    EmptyRuntimeOverrideKeys = $runtimeOverride.EmptyKeys
    NonEmptyRuntimeOverrideKeys = $runtimeOverride.NonEmptyKeys
    CurrentReviewQueue = if (Test-Path -LiteralPath $gameIniPath -PathType Leaf) { "KeepLocal" } else { "NotPresent" }
    CurrentCommitReadiness = if (Test-Path -LiteralPath $gameIniPath -PathType Leaf) { "KeepLocalByDecision" } else { "NotPresent" }
    CurrentEvidenceStatus = if (Test-Path -LiteralPath $gameIniPath -PathType Leaf) { "KeepLocalByDefault" } else { "NotPresent" }
    CurrentReadyToStage = $currentReadyToStage
    MustRemainLocalUntilAccepted = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
    RequiredEvidence = $requiredEvidence
    LocalAssetDecisionEvidenceDraft = [PSCustomObject]@{
        Path = $configRelativePath
        DecisionOwner = "ConfigOwnerRequired"
        DecisionStatus = "KeepLocal"
        AcceptedBy = ""
        AcceptedAt = ""
        EvidenceSource = "Scripts/export_runtime_config_acceptance_template.ps1"
        Notes = "Fill this only after manual diff review, no endpoint/credential confirmation, shared-default decision, and runtime config policy pass evidence are complete."
        Evidence = @(
            [PSCustomObject]@{ Name = "Manual diff review"; Status = "Pending"; Source = ""; Note = "Attach redacted diff review evidence." },
            [PSCustomObject]@{ Name = "No endpoint or credential values"; Status = "Pending"; Source = ""; Note = "Attach secret scan or reviewer confirmation evidence." },
            [PSCustomObject]@{ Name = "Shared-defaults decision"; Status = "Pending"; Source = ""; Note = "Attach config owner decision evidence." },
            [PSCustomObject]@{ Name = "Runtime config policy pass"; Status = "Pending"; Source = ""; Note = "Attach validation command/log evidence." }
        )
    }
    Summary = [PSCustomObject]@{
        RequiredEvidenceCount = $requiredEvidence.Count
        PendingEvidenceCount = $requiredEvidence.Count
        RuntimeOverrideKeyCount = @($runtimeOverride.Keys).Count
        NonEmptyRuntimeOverrideKeyCount = @($runtimeOverride.NonEmptyKeys).Count
        DryRunOnly = $true
        ModifiesConfig = $false
        StagesConfig = $false
        ValuesRedacted = $true
        MustRemainLocalUntilAccepted = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
        CurrentReadyToStage = $currentReadyToStage
        Valid = $true
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Runtime Config Acceptance Template") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($template.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($template.ProjectRoot)") | Out-Null
$lines.Add("- Config: $($template.ConfigRelativePath)") | Out-Null
$lines.Add("- Config present: $($template.ConfigPresent)") | Out-Null
$lines.Add("- Config git state: $($template.ConfigGitState)") | Out-Null
$lines.Add("- Values redacted: $($template.ValuesRedacted)") | Out-Null
$lines.Add("- Dry run only: $($template.DryRunOnly)") | Out-Null
$lines.Add("- Modifies config: $($template.ModifiesConfig)") | Out-Null
$lines.Add("- Stages config: $($template.StagesConfig)") | Out-Null
$lines.Add("- Must remain local until accepted: $($template.MustRemainLocalUntilAccepted)") | Out-Null
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
