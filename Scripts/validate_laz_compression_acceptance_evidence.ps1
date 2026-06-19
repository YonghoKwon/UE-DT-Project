param(
    [string]$ProjectRoot = "",
    [string]$EvidencePath = "",
    [switch]$FailOnIncompleteEvidence,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }
    return (Split-Path -Parent $PSScriptRoot)
}

function Test-NonEmptyString {
    param([object]$Value)
    return -not [string]::IsNullOrWhiteSpace([string]$Value)
}

function Test-CompletedStatus {
    param([object]$Value)
    return @("Recorded", "Complete", "Passed", "Accepted") -contains [string]$Value
}

function Resolve-EvidenceFile {
    param(
        [string]$ProjectRoot,
        [object]$Value
    )

    if (-not (Test-NonEmptyString $Value)) {
        return ""
    }
    $pathText = [string]$Value
    if ([System.IO.Path]::IsPathRooted($pathText)) {
        return $pathText
    }
    return (Join-Path $ProjectRoot $pathText)
}

function Test-EvidenceFilePath {
    param(
        [string]$ProjectRoot,
        [object]$Value
    )

    $candidate = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $Value
    return ((Test-NonEmptyString $candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf))
}

function Add-Check {
    param(
        [System.Collections.Generic.List[object]]$Checks,
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $Checks.Add([PSCustomObject]@{
        Name = $Name
        Passed = $Passed
        Detail = $Detail
    }) | Out-Null
}

$ProjectRoot = Resolve-ProjectRoot
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $ProjectRoot "Saved\Reports\LazCompressionAcceptance\laz_compression_acceptance.evidence.json"
}

$evidenceFilePresent = Test-Path -LiteralPath $EvidencePath -PathType Leaf
$evidence = $null
if ($evidenceFilePresent) {
    $evidence = Get-Content -LiteralPath $EvidencePath -Raw | ConvertFrom-Json
}

$metadata = if ($evidence -and $evidence.PSObject.Properties.Name -contains "AcceptanceMetadata") { $evidence.AcceptanceMetadata } else { $null }
$requiredEvidence = if ($evidence -and $evidence.PSObject.Properties.Name -contains "RequiredEvidence") { @($evidence.RequiredEvidence) } else { @() }
$requiredNames = @(
    "Compressor selection",
    "Produced LAZ output",
    "Known reader validation",
    "Placeholder distinction",
    "Automation or repeatable command",
    "Owner acceptance"
)

$checks = [System.Collections.Generic.List[object]]::new()
Add-Check -Checks $checks -Name "Evidence file present" -Passed $evidenceFilePresent -Detail $EvidencePath
Add-Check -Checks $checks -Name "Schema version is LazCompressionAcceptanceEvidenceV1" -Passed ($evidence -and [string]$evidence.SchemaVersion -eq "LazCompressionAcceptanceEvidenceV1") -Detail "SchemaVersion=$($evidence.SchemaVersion)"
Add-Check -Checks $checks -Name "Acceptance metadata present" -Passed ($null -ne $metadata) -Detail "AcceptanceMetadata"

if ($metadata) {
    foreach ($field in @("EvidenceRunId", "EnvironmentName", "Operator", "CompressorPath", "CompressorVersion", "ReaderPath", "LazEvidencePath", "ReaderProbeReportPath")) {
        Add-Check -Checks $checks -Name "$field recorded" -Passed (Test-NonEmptyString $metadata.$field) -Detail $field
    }
    Add-Check -Checks $checks -Name "LAZ evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.LazEvidencePath) -Detail "LazEvidencePath=$($metadata.LazEvidencePath)"
    Add-Check -Checks $checks -Name "Reader probe report path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.ReaderProbeReportPath) -Detail "ReaderProbeReportPath=$($metadata.ReaderProbeReportPath)"
}

foreach ($requiredName in $requiredNames) {
    $item = @($requiredEvidence | Where-Object { [string]$_.Name -eq $requiredName } | Select-Object -First 1)
    $present = ($item.Count -gt 0)
    $complete = $present -and (Test-CompletedStatus $item[0].Status) -and (Test-NonEmptyString $item[0].EvidencePath) -and (Test-NonEmptyString $item[0].Reviewer) -and (Test-NonEmptyString $item[0].ReviewedAtUtc) -and (Test-NonEmptyString $item[0].Notes)
    Add-Check -Checks $checks -Name "$requiredName evidence complete" -Passed $complete -Detail $(if ($present) { "Status=$($item[0].Status) EvidencePath=$($item[0].EvidencePath)" } else { "Missing" })
    if ($present) {
        Add-Check -Checks $checks -Name "$requiredName evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $item[0].EvidencePath) -Detail "EvidencePath=$($item[0].EvidencePath)"
    }
}

$failedChecks = @($checks | Where-Object { -not $_.Passed })
$complete = ($failedChecks.Count -eq 0)
$report = [PSCustomObject]@{
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    EvidencePath = $EvidencePath
    Checks = $checks
    Summary = [PSCustomObject]@{
        EvidenceFilePresent = [bool]$evidenceFilePresent
        RequiredEvidenceCount = $requiredNames.Count
        PassedCheckCount = [int]($checks.Count - $failedChecks.Count)
        FailedCheckCount = [int]$failedChecks.Count
        Complete = [bool]$complete
        CurrentReadyToClaimTrueLaz = [bool]$complete
        FailOnIncompleteEvidence = [bool]$FailOnIncompleteEvidence
        ModifiesAssets = $false
        StagesFiles = $false
        Boundary = "True LAZ remains unclaimed until every acceptance evidence item, produced .laz path, and known-reader probe report is recorded."
        Valid = $true
    }
}

if ($FailOnIncompleteEvidence -and -not $complete) {
    throw "LAZ compression acceptance evidence is incomplete. FailedCheckCount=$($failedChecks.Count)"
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "LAZ compression acceptance evidence checked."
    Write-Host "Evidence file present: $($report.Summary.EvidenceFilePresent)"
    Write-Host "Passed checks: $($report.Summary.PassedCheckCount)"
    Write-Host "Failed checks: $($report.Summary.FailedCheckCount)"
    Write-Host "Ready to claim true LAZ: $($report.Summary.CurrentReadyToClaimTrueLaz)"
    Write-Host "Boundary: $($report.Summary.Boundary)"
}
