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

function Get-EvidenceFileSize {
    param(
        [string]$ProjectRoot,
        [object]$Value
    )

    $candidate = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $Value
    if ((Test-NonEmptyString $candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        return (Get-Item -LiteralPath $candidate).Length
    }
    return 0
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
$evidenceSections = if ($evidence -and $evidence.PSObject.Properties.Name -contains "EvidenceSections") { $evidence.EvidenceSections } else { $null }
$requiredEvidence = if ($evidence -and $evidence.PSObject.Properties.Name -contains "RequiredEvidence") { @($evidence.RequiredEvidence) } else { @() }
$requiredNames = @(
    "Compressor selection",
    "Produced LAZ output",
    "Known reader validation",
    "Placeholder distinction",
    "Automation or repeatable command",
    "Owner acceptance"
)
$requiredSectionNames = @(
    "CompressorSelection",
    "ProducedLazEvidence",
    "KnownReaderValidation",
    "PlaceholderDistinction",
    "RepeatableCommand",
    "OwnerAcceptance"
)
$knownReaderNames = @("lasinfo", "pdal", "pdal info", "lastools lasinfo")

$checks = [System.Collections.Generic.List[object]]::new()
Add-Check -Checks $checks -Name "Evidence file present" -Passed $evidenceFilePresent -Detail $EvidencePath
Add-Check -Checks $checks -Name "Schema version is LazCompressionAcceptanceEvidenceV1" -Passed ($evidence -and [string]$evidence.SchemaVersion -eq "LazCompressionAcceptanceEvidenceV1") -Detail "SchemaVersion=$($evidence.SchemaVersion)"
Add-Check -Checks $checks -Name "Acceptance metadata present" -Passed ($null -ne $metadata) -Detail "AcceptanceMetadata"
Add-Check -Checks $checks -Name "EvidenceSections present" -Passed ($null -ne $evidenceSections) -Detail "EvidenceSections"

if ($metadata) {
    foreach ($field in @("EvidenceRunId", "EnvironmentName", "Operator", "CompressorPath", "CompressorVersion", "ReaderPath", "LazEvidencePath", "ReaderProbeReportPath")) {
        Add-Check -Checks $checks -Name "$field recorded" -Passed (Test-NonEmptyString $metadata.$field) -Detail $field
    }
    Add-Check -Checks $checks -Name "LAZ evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.LazEvidencePath) -Detail "LazEvidencePath=$($metadata.LazEvidencePath)"
    Add-Check -Checks $checks -Name "Reader probe report path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.ReaderProbeReportPath) -Detail "ReaderProbeReportPath=$($metadata.ReaderProbeReportPath)"
}

if ($evidenceSections) {
    foreach ($sectionName in $requiredSectionNames) {
        Add-Check -Checks $checks -Name "$sectionName section present" -Passed ($evidenceSections.PSObject.Properties.Name -contains $sectionName) -Detail $sectionName
    }

    $compressorSelection = if ($evidenceSections.PSObject.Properties.Name -contains "CompressorSelection") { $evidenceSections.CompressorSelection } else { $null }
    if ($compressorSelection) {
        foreach ($field in @("SelectedWorkflow", "ToolOrLibraryName", "Version", "License", "RedistributionOwner", "UnrealPackagingDecision", "EvidencePath")) {
            Add-Check -Checks $checks -Name "CompressorSelection.$field recorded" -Passed (Test-NonEmptyString $compressorSelection.$field) -Detail $field
        }
        Add-Check -Checks $checks -Name "CompressorSelection evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $compressorSelection.EvidencePath) -Detail "EvidencePath=$($compressorSelection.EvidencePath)"
    }

    $producedLaz = if ($evidenceSections.PSObject.Properties.Name -contains "ProducedLazEvidence") { $evidenceSections.ProducedLazEvidence } else { $null }
    if ($producedLaz) {
        foreach ($field in @("LazEvidencePath", "ProducerCommand", "SourceLasPath")) {
            Add-Check -Checks $checks -Name "ProducedLazEvidence.$field recorded" -Passed (Test-NonEmptyString $producedLaz.$field) -Detail $field
        }
        $resolvedLazPath = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $producedLaz.LazEvidencePath
        $resolvedSourceLasPath = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $producedLaz.SourceLasPath
        $actualLazSize = Get-EvidenceFileSize -ProjectRoot $ProjectRoot -Value $producedLaz.LazEvidencePath
        $lazEvidencePathText = ([string]$producedLaz.LazEvidencePath).ToLowerInvariant()
        Add-Check -Checks $checks -Name "ProducedLazEvidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $producedLaz.LazEvidencePath) -Detail "LazEvidencePath=$($producedLaz.LazEvidencePath)"
        Add-Check -Checks $checks -Name "ProducedLazEvidence path has .laz extension" -Passed ([System.IO.Path]::GetExtension([string]$producedLaz.LazEvidencePath).ToLowerInvariant() -eq ".laz") -Detail "LazEvidencePath=$($producedLaz.LazEvidencePath)"
        Add-Check -Checks $checks -Name "ProducedLazEvidence path is not laz_source placeholder" -Passed ((Test-NonEmptyString $producedLaz.LazEvidencePath) -and -not $lazEvidencePathText.Contains("_laz_source")) -Detail "LazEvidencePath=$($producedLaz.LazEvidencePath)"
        Add-Check -Checks $checks -Name "ProducedLazEvidence file is non-empty" -Passed ($actualLazSize -gt 0) -Detail "ActualSize=$actualLazSize"
        Add-Check -Checks $checks -Name "ProducedLazEvidence output byte size recorded" -Passed ([int64]$producedLaz.OutputByteSize -gt 0) -Detail "OutputByteSize=$($producedLaz.OutputByteSize)"
        Add-Check -Checks $checks -Name "ProducedLazEvidence output byte size matches file" -Passed ($actualLazSize -gt 0 -and [int64]$producedLaz.OutputByteSize -eq $actualLazSize) -Detail "OutputByteSize=$($producedLaz.OutputByteSize) ActualSize=$actualLazSize"
        Add-Check -Checks $checks -Name "ProducedLazEvidence source LAS path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $producedLaz.SourceLasPath) -Detail "SourceLasPath=$($producedLaz.SourceLasPath)"
        Add-Check -Checks $checks -Name "ProducedLazEvidence LAZ path differs from source LAS path" -Passed ((Test-NonEmptyString $resolvedLazPath) -and (Test-NonEmptyString $resolvedSourceLasPath) -and $resolvedLazPath -ne $resolvedSourceLasPath) -Detail "Laz=$resolvedLazPath SourceLas=$resolvedSourceLasPath"
        Add-Check -Checks $checks -Name "ProducedLazEvidence produced by accepted workflow" -Passed ([bool]$producedLaz.ProducedByAcceptedWorkflow) -Detail "ProducedByAcceptedWorkflow=$($producedLaz.ProducedByAcceptedWorkflow)"
        Add-Check -Checks $checks -Name "ProducedLazEvidence produced by export or accepted post-process" -Passed ([bool]$producedLaz.ProducedByExportLastPointCloudLazOrAcceptedPostProcess) -Detail "ProducedByExportLastPointCloudLazOrAcceptedPostProcess=$($producedLaz.ProducedByExportLastPointCloudLazOrAcceptedPostProcess)"
    }

    $readerValidation = if ($evidenceSections.PSObject.Properties.Name -contains "KnownReaderValidation") { $evidenceSections.KnownReaderValidation } else { $null }
    if ($readerValidation) {
        foreach ($field in @("ReaderName", "ReaderPath", "ReaderVersion", "ReaderProbeReportPath", "ReaderOutputEvidencePath", "ReaderProbeLazEvidencePath")) {
            Add-Check -Checks $checks -Name "KnownReaderValidation.$field recorded" -Passed (Test-NonEmptyString $readerValidation.$field) -Detail $field
        }
        $resolvedReaderProbeLazPath = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $readerValidation.ReaderProbeLazEvidencePath
        Add-Check -Checks $checks -Name "KnownReaderValidation reader name is known" -Passed ($knownReaderNames -contains ([string]$readerValidation.ReaderName).ToLowerInvariant()) -Detail "ReaderName=$($readerValidation.ReaderName)"
        Add-Check -Checks $checks -Name "KnownReaderValidation probe report exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $readerValidation.ReaderProbeReportPath) -Detail "ReaderProbeReportPath=$($readerValidation.ReaderProbeReportPath)"
        Add-Check -Checks $checks -Name "KnownReaderValidation reader output evidence exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $readerValidation.ReaderOutputEvidencePath) -Detail "ReaderOutputEvidencePath=$($readerValidation.ReaderOutputEvidencePath)"
        Add-Check -Checks $checks -Name "KnownReaderValidation probe LAZ path matches produced LAZ evidence" -Passed ((Test-NonEmptyString $resolvedReaderProbeLazPath) -and (Test-NonEmptyString $resolvedLazPath) -and $resolvedReaderProbeLazPath -eq $resolvedLazPath) -Detail "ReaderProbeLaz=$resolvedReaderProbeLazPath ProducedLaz=$resolvedLazPath"
        $probeExitCodeRecorded = ($readerValidation.PSObject.Properties.Name -contains "ProbeExitCode")
        $probeExitCodeText = [string]$readerValidation.ProbeExitCode
        $probeExitCodeValue = 0
        $probeExitCodeIsValidInteger = (Test-NonEmptyString $probeExitCodeText) -and [int]::TryParse($probeExitCodeText, [ref]$probeExitCodeValue)
        Add-Check -Checks $checks -Name "KnownReaderValidation probe exit code is zero" -Passed ($probeExitCodeRecorded -and $probeExitCodeIsValidInteger -and $probeExitCodeValue -eq 0) -Detail "ProbeExitCode=$($readerValidation.ProbeExitCode)"
        Add-Check -Checks $checks -Name "KnownReaderValidation probe ran against same LAZ evidence path" -Passed ([bool]$readerValidation.ProbeWasRunAgainstSameLazEvidencePath) -Detail "ProbeWasRunAgainstSameLazEvidencePath=$($readerValidation.ProbeWasRunAgainstSameLazEvidencePath)"
        Add-Check -Checks $checks -Name "KnownReaderValidation probe succeeded" -Passed ([bool]$readerValidation.ProbeSucceeded) -Detail "ProbeSucceeded=$($readerValidation.ProbeSucceeded)"
    }

    $placeholderDistinction = if ($evidenceSections.PSObject.Properties.Name -contains "PlaceholderDistinction") { $evidenceSections.PlaceholderDistinction } else { $null }
    if ($placeholderDistinction) {
        Add-Check -Checks $checks -Name "PlaceholderDistinction not LAS source placeholder" -Passed ([bool]$placeholderDistinction.NotLasSourcePlaceholder) -Detail "NotLasSourcePlaceholder=$($placeholderDistinction.NotLasSourcePlaceholder)"
        Add-Check -Checks $checks -Name "PlaceholderDistinction not copy surrogate" -Passed ([bool]$placeholderDistinction.NotCopySurrogate) -Detail "NotCopySurrogate=$($placeholderDistinction.NotCopySurrogate)"
        Add-Check -Checks $checks -Name "PlaceholderDistinction not external compressor copy-surrogate output" -Passed ([bool]$placeholderDistinction.NotExternalCompressorCopySurrogateOutput) -Detail "NotExternalCompressorCopySurrogateOutput=$($placeholderDistinction.NotExternalCompressorCopySurrogateOutput)"
        Add-Check -Checks $checks -Name "PlaceholderDistinction LAZ path does not contain laz_source prefix" -Passed ([bool]$placeholderDistinction.LazPathDoesNotContainLazSourcePrefix) -Detail "LazPathDoesNotContainLazSourcePrefix=$($placeholderDistinction.LazPathDoesNotContainLazSourcePrefix)"
        Add-Check -Checks $checks -Name "PlaceholderDistinction evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $placeholderDistinction.EvidencePath) -Detail "EvidencePath=$($placeholderDistinction.EvidencePath)"
    }

    $repeatableCommand = if ($evidenceSections.PSObject.Properties.Name -contains "RepeatableCommand") { $evidenceSections.RepeatableCommand } else { $null }
    if ($repeatableCommand) {
        foreach ($field in @("Command", "WorkingDirectory", "EvidencePath")) {
            Add-Check -Checks $checks -Name "RepeatableCommand.$field recorded" -Passed (Test-NonEmptyString $repeatableCommand.$field) -Detail $field
        }
        Add-Check -Checks $checks -Name "RepeatableCommand evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $repeatableCommand.EvidencePath) -Detail "EvidencePath=$($repeatableCommand.EvidencePath)"
    }

    $ownerAcceptance = if ($evidenceSections.PSObject.Properties.Name -contains "OwnerAcceptance") { $evidenceSections.OwnerAcceptance } else { $null }
    if ($ownerAcceptance) {
        Add-Check -Checks $checks -Name "OwnerAcceptance accepted" -Passed ([bool]$ownerAcceptance.Accepted) -Detail "Accepted=$($ownerAcceptance.Accepted)"
        foreach ($field in @("Owner", "Reviewer", "ReviewedAtUtc", "EvidencePath")) {
            Add-Check -Checks $checks -Name "OwnerAcceptance.$field recorded" -Passed (Test-NonEmptyString $ownerAcceptance.$field) -Detail $field
        }
        Add-Check -Checks $checks -Name "OwnerAcceptance evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $ownerAcceptance.EvidencePath) -Detail "EvidencePath=$($ownerAcceptance.EvidencePath)"
    }
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
$missingAcceptanceChecks = @($failedChecks | ForEach-Object { [string]$_.Name })
$complete = ($failedChecks.Count -eq 0)
$report = [PSCustomObject]@{
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    EvidencePath = $EvidencePath
    Checks = $checks
    Summary = [PSCustomObject]@{
        EvidenceFilePresent = [bool]$evidenceFilePresent
        RequiredEvidenceCount = $requiredNames.Count
        RequiredEvidenceSectionCount = $requiredSectionNames.Count
        PassedCheckCount = [int]($checks.Count - $failedChecks.Count)
        FailedCheckCount = [int]$failedChecks.Count
        MissingAcceptanceChecks = $missingAcceptanceChecks
        TopMissingAcceptanceChecks = @($missingAcceptanceChecks | Select-Object -First 8)
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
