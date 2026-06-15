param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$OutputPath = "docs\local_asset_decisions.evidence.json",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-OutputPath {
    param(
        [string]$ProjectRoot,
        [string]$OutputPath
    )

    if ([System.IO.Path]::IsPathRooted($OutputPath)) {
        return $OutputPath
    }
    return Join-Path $ProjectRoot $OutputPath
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$assetReportScript = Join-Path $scriptRoot "report_local_project_status.ps1"
if (-not (Test-Path -LiteralPath $assetReportScript)) {
    throw "report_local_project_status.ps1 not found: $assetReportScript"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$resolvedOutputPath = Resolve-OutputPath -ProjectRoot $ProjectRoot -OutputPath $OutputPath

$jsonText = & powershell -ExecutionPolicy Bypass -File $assetReportScript -ProjectRoot $ProjectRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Local asset report failed with exit code $LASTEXITCODE"
}

$report = $jsonText | ConvertFrom-Json
$decisionEntries = @(
    $report.DecisionPoints |
        Where-Object { $_.State -eq "present" -and $_.Category -notlike "Generated*" } |
        ForEach-Object {
            [PSCustomObject]@{
                Path = $_.Path
                DecisionOwner = $_.DecisionOwner
                DecisionStatus = $_.DecisionStatus
                AcceptedBy = ""
                AcceptedAt = ""
                EvidenceSource = ""
                Notes = ""
                Evidence = @(
                    $_.EvidenceNeeded |
                        ForEach-Object {
                            [PSCustomObject]@{
                                Name = $_
                                Status = "Pending"
                                Source = ""
                                Note = ""
                            }
                        }
                )
            }
        }
)

$template = [PSCustomObject]@{
    Schema = "LocalAssetDecisionEvidenceV1"
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $report.ProjectRoot
    Instructions = @(
        "DecisionOwner is review-routing metadata, not accepted ownership.",
        "ReadyToStage requires AcceptedForRepository.",
        "AcceptedForRepository requires complete EvidenceNeeded.",
        "ReadyToStage requires complete evidence.",
        "Recorded evidence must name reviewer, date, and source.",
        "PendingOwnerDecision remains NeedsOwnerDecision.",
        "EvidencePending remains NeedsOwnerDecision.",
        "Generated output remains KeepLocal."
    )
    Decisions = $decisionEntries
}

if ($Json) {
    $template | ConvertTo-Json -Depth 8
    return
}

$parent = Split-Path -Parent $resolvedOutputPath
if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}

$template | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedOutputPath -Encoding UTF8
Write-Host "Wrote local asset decision evidence template: $resolvedOutputPath"
