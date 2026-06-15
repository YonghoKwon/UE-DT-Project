param(
    [string]$ProjectRoot = "",
    [switch]$FailIfPresent,
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

function Invoke-AssetReportJson {
    param([string]$ProjectRoot)

    $scriptPath = Join-Path $PSScriptRoot "report_local_project_status.ps1"
    Assert-FileExists -Path $scriptPath -Label "Local asset report script"
    $jsonText = & $scriptPath -ProjectRoot $ProjectRoot -Json
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "report_local_project_status.ps1 failed with exit code $LASTEXITCODE"
    }
    return ($jsonText | ConvertFrom-Json)
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$localAssetDoc = Join-Path $ProjectRoot "docs\local_asset_report.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
Assert-FileExists -Path $localAssetDoc -Label "Local asset report document"
Assert-FileExists -Path $remainingDoc -Label "Remaining work document"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "LargeContentCandidate"; Label = "Local asset doc defines large content category" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "SampleOrThirdParty"; Label = "Local asset doc defines sample category" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "extension counts"; Label = "Local asset doc explains extension counts" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "largest files"; Label = "Local asset doc explains largest files" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "GitState"; Label = "Local asset doc explains git state" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "CommitReadiness"; Label = "Local asset doc explains commit readiness" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "DecisionChecklist"; Label = "Local asset doc explains decision checklist" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "asset source, license, production"; Label = "Local asset doc explains source/license/dependency checks" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Content/ChemicalPlantEnv"; Label = "Remaining work tracks ChemicalPlantEnv" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Samples/PixelStreaming"; Label = "Remaining work tracks PixelStreaming" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "map/WBP dependency check"; Label = "Remaining work requires dependency check" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "GitState"; Label = "Remaining work tracks git state reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "CommitReadiness"; Label = "Remaining work tracks commit readiness reporting" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "owner/source/license"; Label = "Remaining work tracks source/license evidence" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$assetReport = Invoke-AssetReportJson -ProjectRoot $ProjectRoot
$decisionCandidates = @(
    $assetReport.DecisionPoints |
        Where-Object {
            $_.State -eq "present" -and
            $_.Category -in @("LargeContentCandidate", "SampleOrThirdParty")
        } |
        Sort-Object Category, Path
)

$totalBytes = [int64](($decisionCandidates | Measure-Object -Property SizeBytes -Sum).Sum)
$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    Candidates = @($decisionCandidates | ForEach-Object {
        [PSCustomObject]@{
            Path = $_.Path
            Category = $_.Category
            FileCount = $_.FileCount
            SizeBytes = [int64]$_.SizeBytes
            Size = $_.Size
            Recommendation = $_.Recommendation
            ExtensionCounts = $_.ContentSummary.ExtensionCounts
            LargestFiles = $_.ContentSummary.LargestFiles
        }
    })
    CheckedContracts = @($requiredTexts | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Pattern = $_.Pattern
            Path = $_.Path
        }
    })
    Summary = [PSCustomObject]@{
        CandidateCount = $decisionCandidates.Count
        TotalSizeBytes = $totalBytes
        TotalSize = if ($totalBytes -ge 1GB) { "{0:N1} GB" -f ($totalBytes / 1GB) } elseif ($totalBytes -ge 1MB) { "{0:N1} MB" -f ($totalBytes / 1MB) } else { "$totalBytes B" }
        StrictFailureRequested = [bool]$FailIfPresent
        ExplicitOwnershipDecisionRequired = ($decisionCandidates.Count -gt 0)
        Valid = $true
    }
}

if ($FailIfPresent -and $decisionCandidates.Count -gt 0) {
    $paths = @($decisionCandidates | ForEach-Object { "$($_.Path)=$($_.Size)" }) -join ", "
    throw "Large content or sample/third-party candidates are present: $paths. Keep them untracked until explicit ownership, dependency, and size decisions are complete."
}

if ($Json) {
    $report | ConvertTo-Json -Depth 7
}
else {
    Write-Host "Large content decision policy is internally consistent."
    Write-Host "Candidate count: $($report.Summary.CandidateCount)"
    Write-Host "Total candidate size: $($report.Summary.TotalSize)"
    Write-Host "Explicit ownership decision required: $($report.Summary.ExplicitOwnershipDecisionRequired)"
    foreach ($candidate in $report.Candidates) {
        Write-Host "$($candidate.Category): $($candidate.Path) files=$($candidate.FileCount) size=$($candidate.Size)"
    }
}
