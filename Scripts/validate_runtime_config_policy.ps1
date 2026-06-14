param(
    [string]$ProjectRoot = "",
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
        NonEmptyKeys = @($values.Keys | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$values[$_]) } | Sort-Object)
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$assetReportScript = Join-Path $ProjectRoot "Scripts\report_local_project_status.ps1"
$localAssetDoc = Join-Path $ProjectRoot "docs\local_asset_report.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$gameIniPath = Join-Path $ProjectRoot "Config\Game.ini"

Assert-FileExists -Path $assetReportScript -Label "Local asset report script"
Assert-FileExists -Path $localAssetDoc -Label "Local asset report document"
Assert-FileExists -Path $remainingDoc -Label "Remaining work document"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "DTCoreRuntimeOverride"; Label = "Asset report detects runtime override section" },
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "endpoint or credential leakage"; Label = "Asset report warns about endpoint or credential leakage" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "local-only runtime override"; Label = "Local asset doc explains local runtime override" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = 'Current local state: empty `[DTCoreRuntimeOverride]` values.'; Label = "Remaining work documents current empty override state" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "endpoint or"; Label = "Remaining work documents endpoint review" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "credential values"; Label = "Remaining work documents credential review" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$runtimeOverride = Read-RuntimeOverrideSection -Path $gameIniPath
if ($runtimeOverride.Present -and $runtimeOverride.NonEmptyKeys.Count -gt 0) {
    throw "Config/Game.ini contains non-empty DTCoreRuntimeOverride values: $($runtimeOverride.NonEmptyKeys -join ', '). Keep this local or scrub endpoint/credential values before staging."
}

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    GameIniPath = $gameIniPath
    GameIniPresent = (Test-Path -LiteralPath $gameIniPath -PathType Leaf)
    RuntimeOverridePresent = $runtimeOverride.Present
    RuntimeOverrideKeys = @($runtimeOverride.Values.Keys | Sort-Object)
    NonEmptyRuntimeOverrideKeys = $runtimeOverride.NonEmptyKeys
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
    Write-Host "Runtime override present: $($report.Summary.RuntimeOverridePresent)"
    Write-Host "Runtime override empty or absent: $($report.Summary.RuntimeOverrideEmptyOrAbsent)"
    Write-Host "Endpoint or credential leakage detected: $($report.Summary.EndpointOrCredentialLeakageDetected)"
}
