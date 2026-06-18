param(
    [string]$ProjectRoot = "",
    [string]$LocalProjectRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Assert-FileExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found: $Path"
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

function Read-RuntimeOverrideSection {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [PSCustomObject]@{
            Present = $false
            Values = @{}
            Keys = @()
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

    return [PSCustomObject]@{
        Present = ($values.Count -gt 0 -or (Select-String -LiteralPath $Path -Pattern "[DTCoreRuntimeOverride]" -SimpleMatch -Quiet))
        Values = $values
        Keys = @($values.Keys | Sort-Object)
        NonEmptyKeys = @($values.Keys | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$values[$_]) } | Sort-Object)
    }
}

function Get-RuntimeOverrideRecommendation {
    param(
        [object]$RuntimeOverride,
        [string[]]$KnownKeys,
        [bool]$GameIniPresent
    )

    $unknownKeys = @($RuntimeOverride.Keys | Where-Object { $_ -notin $KnownKeys } | Sort-Object)
    $nonEmptySensitiveKeys = @(
        $RuntimeOverride.NonEmptyKeys |
            Where-Object { $_ -match "url|login|passcode|token|secret|password|credential|api" } |
            Sort-Object
    )

    if (-not $GameIniPresent) {
        return [PSCustomObject]@{
            RecommendedDecision = "NoLocalOverride"
            Reason = "Local Config/Game.ini is not present."
            UnknownKeys = $unknownKeys
            NonEmptySensitiveKeys = $nonEmptySensitiveKeys
        }
    }
    if ($RuntimeOverride.NonEmptyKeys.Count -gt 0 -and $nonEmptySensitiveKeys.Count -gt 0) {
        return [PSCustomObject]@{
            RecommendedDecision = "RejectUntilScrubbed"
            Reason = "Non-empty endpoint or credential-like runtime override keys are present."
            UnknownKeys = $unknownKeys
            NonEmptySensitiveKeys = $nonEmptySensitiveKeys
        }
    }
    if ($RuntimeOverride.NonEmptyKeys.Count -gt 0 -or $unknownKeys.Count -gt 0) {
        return [PSCustomObject]@{
            RecommendedDecision = "ManualReviewRequired"
            Reason = "Runtime override keys need owner review before staging."
            UnknownKeys = $unknownKeys
            NonEmptySensitiveKeys = $nonEmptySensitiveKeys
        }
    }
    if ($RuntimeOverride.Present) {
        return [PSCustomObject]@{
            RecommendedDecision = "KeepLocal"
            Reason = "Empty DTCoreRuntimeOverride values are local override placeholders by default, not useful shared defaults."
            UnknownKeys = $unknownKeys
            NonEmptySensitiveKeys = $nonEmptySensitiveKeys
        }
    }

    return [PSCustomObject]@{
        RecommendedDecision = "ManualReviewRequired"
        Reason = "Config/Game.ini exists without DTCoreRuntimeOverride; inspect manually before staging."
        UnknownKeys = $unknownKeys
        NonEmptySensitiveKeys = $nonEmptySensitiveKeys
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($LocalProjectRoot)) {
    $LocalProjectRoot = $ProjectRoot
}
$LocalProjectRoot = (Resolve-Path -LiteralPath $LocalProjectRoot).Path

$assetReportScript = Join-Path $ProjectRoot "Scripts\report_local_project_status.ps1"
$runtimeConfigDecisionReportScript = Join-Path $ProjectRoot "Scripts\export_runtime_config_decision_report.ps1"
$localAssetDoc = Join-Path $ProjectRoot "docs\local_asset_report.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$gameIniPath = Join-Path $LocalProjectRoot "Config\Game.ini"

Assert-FileExists -Path $assetReportScript -Label "Local asset report script"
Assert-FileExists -Path $runtimeConfigDecisionReportScript -Label "Runtime config decision report script"
Assert-FileExists -Path $localAssetDoc -Label "Local asset report document"
Assert-FileExists -Path $remainingDoc -Label "Remaining work document"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "DTCoreRuntimeOverride"; Label = "Asset report detects runtime override section" },
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "endpoint or credential leakage"; Label = "Asset report warns about endpoint or credential leakage" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "local-only runtime override"; Label = "Local asset doc explains local runtime override" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = 'Current local state: empty `[DTCoreRuntimeOverride]` values.'; Label = "Remaining work documents current empty override state" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "endpoint or"; Label = "Remaining work documents endpoint review" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "credential values"; Label = "Remaining work documents credential review" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "LocalProjectRoot"; Label = "Local asset doc documents local runtime config root" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "RecommendedDecision"; Label = "Remaining work documents runtime config recommendation" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "SourceRepoRoot"; Label = "Runtime config decision report supports separate source root" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "EvidencePath"; Label = "Runtime config decision report accepts evidence path" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "FailOnIncompleteEvidence"; Label = "Runtime config decision report can fail on incomplete evidence" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "report_local_project_status.ps1"; Label = "Runtime config decision report reuses local decision engine" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "validate_runtime_config_policy.ps1"; Label = "Runtime config decision report reuses runtime policy validation" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "DecisionPoint"; Label = "Runtime config decision report exposes decision point" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "ManualAcceptanceChecklist"; Label = "Runtime config decision report exposes manual acceptance checklist" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "ReadyToStage"; Label = "Runtime config decision report exposes ready-to-stage status" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "ReviewQueue"; Label = "Runtime config decision report exposes review queue" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "CommitReadiness"; Label = "Runtime config decision report exposes commit readiness" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "MissingEvidenceCount"; Label = "Runtime config decision report counts missing evidence" },
    [PSCustomObject]@{ Path = $runtimeConfigDecisionReportScript; Pattern = "ManualConfigOwnerDecisionStillRequired"; Label = "Runtime config decision report exposes manual owner gate" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$runtimeOverride = Read-RuntimeOverrideSection -Path $gameIniPath
$knownRuntimeOverrideKeys = @("BaseApiUrl", "LocalApiUrl", "TestApiUrl", "ProdApiUrl", "WebSocketUrl", "WebSocketLogin", "WebSocketPasscode")
$recommendation = Get-RuntimeOverrideRecommendation `
    -RuntimeOverride $runtimeOverride `
    -KnownKeys $knownRuntimeOverrideKeys `
    -GameIniPresent (Test-Path -LiteralPath $gameIniPath -PathType Leaf)

if ($runtimeOverride.Present -and $runtimeOverride.NonEmptyKeys.Count -gt 0) {
    throw "Config/Game.ini contains non-empty DTCoreRuntimeOverride values: $($runtimeOverride.NonEmptyKeys -join ', '). Keep this local or scrub endpoint/credential values before staging."
}

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    LocalProjectRoot = $LocalProjectRoot
    GameIniPath = $gameIniPath
    GameIniPresent = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
    RuntimeOverridePresent = $runtimeOverride.Present
    RuntimeOverrideKeys = @($runtimeOverride.Keys)
    KnownRuntimeOverrideKeys = $knownRuntimeOverrideKeys
    UnknownRuntimeOverrideKeys = $recommendation.UnknownKeys
    NonEmptyRuntimeOverrideKeys = $runtimeOverride.NonEmptyKeys
    NonEmptySensitiveRuntimeOverrideKeys = $recommendation.NonEmptySensitiveKeys
    RecommendedDecision = $recommendation.RecommendedDecision
    Reason = $recommendation.Reason
    CheckedContracts = @($requiredTexts | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Pattern = $_.Pattern
            Path = $_.Path
        }
    })
    Summary = [PSCustomObject]@{
        RequiredContractCount = $requiredTexts.Count
        GameIniPresent = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
        RuntimeOverridePresent = $runtimeOverride.Present
        RuntimeOverrideEmptyOrAbsent = ($runtimeOverride.NonEmptyKeys.Count -eq 0)
        EndpointOrCredentialLeakageDetected = ($runtimeOverride.NonEmptyKeys.Count -gt 0)
        RecommendedDecision = $recommendation.RecommendedDecision
        RuntimeConfigDecisionReportDeclared = $true
        RuntimeConfigDecisionReportUsesAssetDecisionEngine = $true
        RuntimeConfigIncompleteEvidenceFailGateDeclared = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Runtime config policy is internally consistent."
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Game.ini present: $($report.Summary.GameIniPresent)"
    Write-Host "LocalProjectRoot: $($report.LocalProjectRoot)"
    Write-Host "Runtime override present: $($report.Summary.RuntimeOverridePresent)"
    Write-Host "Runtime override empty or absent: $($report.Summary.RuntimeOverrideEmptyOrAbsent)"
    Write-Host "Endpoint or credential leakage detected: $($report.Summary.EndpointOrCredentialLeakageDetected)"
    Write-Host "RecommendedDecision: $($report.RecommendedDecision)"
    Write-Host "Runtime config decision report declared: $($report.Summary.RuntimeConfigDecisionReportDeclared)"
    Write-Host "Runtime config decision report uses asset decision engine: $($report.Summary.RuntimeConfigDecisionReportUsesAssetDecisionEngine)"
    Write-Host "Runtime config incomplete evidence fail gate declared: $($report.Summary.RuntimeConfigIncompleteEvidenceFailGateDeclared)"
    Write-Host "Reason: $($report.Reason)"
}
