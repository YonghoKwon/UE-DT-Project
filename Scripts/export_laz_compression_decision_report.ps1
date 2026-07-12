param(
    [string]$ProjectRoot = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
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

function Test-ContainsText {
    param(
        [string]$Path,
        [string]$Pattern
    )

    return [bool](Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet)
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

function Convert-ToMarkdownCell {
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Write-MarkdownReport {
    param(
        [object]$Report,
        [string]$Path
    )

    $lines = @()
    $lines += "# LAZ Compression Decision Report"
    $lines += ""
    $lines += "Generated UTC: $($Report.GeneratedUtc)"
    $lines += ""
    $lines += "## Current Runtime State"
    $lines += ""
    $lines += "- Placeholder explicit: $($Report.Summary.PlaceholderExplicit)"
    $lines += "- Writes LAS source only: $($Report.Summary.WritesLasSourceOnly)"
    $lines += "- External compressor opt-in implemented: $($Report.Summary.ExternalCompressorOptInImplemented)"
    $lines += "- External compressor contract hardened: $($Report.Summary.ExternalCompressorContractHardened)"
    $lines += "- Missing-compressor guard covered: $($Report.Summary.MissingCompressorGuardCovered)"
    $lines += "- External compressor success covered: $($Report.Summary.ExternalCompressorSuccessCovered)"
    $lines += "- Compressor readiness report declared: $($Report.Summary.CompressorReadinessReportDeclared)"
    $lines += "- Automation coverage declared: $($Report.Summary.AutomationCoverageDeclared)"
    $lines += "- True compression integrated: $($Report.Summary.TrueCompressionIntegrated)"
    $lines += "- Recommended next decision: $($Report.Summary.RecommendedNextDecision)"
    $lines += ""
    $lines += "## Candidate Paths"
    $lines += ""
    $lines += "| Option | Runtime shape | Pros | Risks | Recommended decision |"
    $lines += "| --- | --- | --- | --- | --- |"
    foreach ($option in $Report.CandidatePaths) {
        $lines += "| $(Convert-ToMarkdownCell $option.Option) | $(Convert-ToMarkdownCell $option.RuntimeShape) | $(Convert-ToMarkdownCell ($option.Pros -join '; ')) | $(Convert-ToMarkdownCell ($option.Risks -join '; ')) | $(Convert-ToMarkdownCell $option.RecommendedDecision) |"
    }
    $lines += ""
    $lines += "## Acceptance Evidence Needed"
    $lines += ""
    foreach ($item in $Report.AcceptanceEvidenceNeeded) {
        $lines += "- $item"
    }
    $lines += ""
    $lines += "## Notes"
    $lines += ""
    $lines += "Keep `ExportLastPointCloudLaz()` warning behavior until one candidate path is accepted and verified with readable compressed `.laz` output."

    Write-TextFile -Path $Path -Lines $lines
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$lidarHeader = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\VirtualLidarSensorComp.h"
$lidarCpp = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\VirtualLidarSensorComp.cpp"
$monitorHeader = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\UI\VirtualSensorMonitorWidget.h"
$replayTests = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\Tests\LidarReplayAutomationTests.cpp"
$schemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$readinessReportScript = Join-Path $ProjectRoot "Scripts\export_laz_compressor_readiness_report.ps1"

foreach ($file in @(
    [PSCustomObject]@{ Path = $lidarHeader; Label = "LiDAR component header" },
    [PSCustomObject]@{ Path = $lidarCpp; Label = "LiDAR component implementation" },
    [PSCustomObject]@{ Path = $monitorHeader; Label = "Monitor widget header" },
    [PSCustomObject]@{ Path = $replayTests; Label = "Replay automation tests" },
    [PSCustomObject]@{ Path = $schemaDoc; Label = "LiDAR payload schema" },
    [PSCustomObject]@{ Path = $remainingDoc; Label = "Remaining work document" },
    [PSCustomObject]@{ Path = $readinessReportScript; Label = "LAZ compressor readiness report script" }
)) {
    Assert-FileExists -Path $file.Path -Label $file.Label
}

$placeholderExplicit = (Test-ContainsText -Path $lidarCpp -Pattern "LAZ compression is not integrated") -and
    (Test-ContainsText -Path $remainingDoc -Pattern "intentionally a placeholder")
$writesLasSourceOnly = (Test-ContainsText -Path $lidarCpp -Pattern "_laz_source") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "ExportLastPointCloudLasToPath")
$externalCompressorOptInImplemented = (Test-ContainsText -Path $lidarHeader -Pattern "bUseExternalLazCompressor") -and
    (Test-ContainsText -Path $lidarHeader -Pattern "ExternalLazCompressorArguments") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "RunExternalLazCompressor") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "ExecProcess")
$externalCompressorContractHardened = (Test-ContainsText -Path $lidarCpp -Pattern 'Arguments.Contains(TEXT("{input}"))') -and
    (Test-ContainsText -Path $lidarCpp -Pattern 'Arguments.Contains(TEXT("{output}"))') -and
    (Test-ContainsText -Path $lidarCpp -Pattern "FullLazOutputPath") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "MakePlatformFilename") -and
    (Test-ContainsText -Path $lidarCpp -Pattern 'BuildExportPath(TEXT("laz"), FileNamePrefix)') -and
    (Test-ContainsText -Path $lidarCpp -Pattern "OutputSize <= 0")
$automationCoverageDeclared = (Test-ContainsText -Path $replayTests -Pattern "MA0T10.SensorReplay.LazPlaceholderWritesLasSource") -and
    (Test-ContainsText -Path $replayTests -Pattern "does not create compressed .laz files")
$missingCompressorGuardCovered = (Test-ContainsText -Path $replayTests -Pattern "MA0T10.SensorReplay.LazExternalCompressorMissingFails") -and
    (Test-ContainsText -Path $replayTests -Pattern "missing external compressor does not create .laz files")
$externalCompressorSuccessCovered = (Test-ContainsText -Path $replayTests -Pattern "MA0T10.SensorReplay.LazExternalCompressorFakeWritesOutput") -and
    (Test-ContainsText -Path $replayTests -Pattern "external compressor success creates one .laz output") -and
    (Test-ContainsText -Path $replayTests -Pattern "process-contract surrogate")
$compressorReadinessReportDeclared = (Test-ContainsText -Path $readinessReportScript -Pattern "ReadyForRealLazAutomation") -and
    (Test-ContainsText -Path $readinessReportScript -Pattern "ReadableOutputEvidencePresent") -and
    (Test-ContainsText -Path $readinessReportScript -Pattern "LazEvidencePath") -and
    (Test-ContainsText -Path $readinessReportScript -Pattern "RunReaderProbe") -and
    (Test-ContainsText -Path $readinessReportScript -Pattern "ReaderProbeSucceeded") -and
    (Test-ContainsText -Path $readinessReportScript -Pattern "KnownPointCloudReader") -and
    (Test-ContainsText -Path $readinessReportScript -Pattern "ReaderProbeBlockedReason") -and
    (Test-ContainsText -Path $remainingDoc -Pattern "export_laz_compressor_readiness_report.ps1")
$trueCompressionIntegrated = (Test-ContainsText -Path $lidarCpp -Pattern ".laz output is actually compressed") -or
    (Test-ContainsText -Path $replayTests -Pattern "CompressedLaz")

$candidatePaths = @(
    [PSCustomObject]@{
        Option = "Native LAZ library"
        RuntimeShape = "UE module links a supported compressor and writes `.laz` directly from `FVirtualLidarPoint`."
        Pros = @("Single in-process export path", "Best runtime UX", "Can be automated in editor tests")
        Risks = @("License review", "Windows build integration", "Binary dependency packaging")
        RecommendedDecision = "Preferred when redistribution and build ownership are approved"
    },
    [PSCustomObject]@{
        Option = "External CLI compressor"
        RuntimeShape = "Export LAS source first, then invoke a configured compressor executable as a post-process."
        Pros = @("Keeps project code small", "Easier to swap tools", "Good offline workflow")
        Risks = @("Deployment path/config required", "Process execution policy", "Harder packaged-runtime story")
        RecommendedDecision = "Acceptable for offline dataset generation"
    },
    [PSCustomObject]@{
        Option = "Server/post-processing workflow"
        RuntimeShape = "DT-Project emits LAS/JSONL and a downstream data pipeline creates LAZ."
        Pros = @("No UE binary dependency", "Scales outside editor", "Matches judging/data-lake pipelines")
        Risks = @("Not an in-editor LAZ export", "Requires server-side ownership", "Longer feedback loop")
        RecommendedDecision = "Use when LAZ is archival-only"
    }
)

$acceptanceEvidence = @(
    "Chosen compressor/tool name, version, license, and redistribution decision",
    "Windows build/package instructions for UE 5.3",
    "A `.laz` file produced from `ExportLastPointCloudLaz()` or the accepted post-process path",
    "Readable validation from a known point-cloud tool, captured through export_laz_compressor_readiness_report.ps1 -LazEvidencePath ... -RunReaderProbe; generic successful executables are not enough",
    "Automation that distinguishes compressed `.laz` output from the current `_laz_source_*.las` placeholder"
)

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$report = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    ProjectRoot = $ProjectRoot
    CurrentEvidence = [PSCustomObject]@{
        LidarHeader = $lidarHeader
        LidarImplementation = $lidarCpp
        MonitorHeader = $monitorHeader
        ReplayTests = $replayTests
        SchemaDoc = $schemaDoc
        RemainingWorkDoc = $remainingDoc
    }
    CandidatePaths = $candidatePaths
    AcceptanceEvidenceNeeded = $acceptanceEvidence
    Summary = [PSCustomObject]@{
        PlaceholderExplicit = $placeholderExplicit
        WritesLasSourceOnly = $writesLasSourceOnly
        ExternalCompressorOptInImplemented = $externalCompressorOptInImplemented
        ExternalCompressorContractHardened = $externalCompressorContractHardened
        MissingCompressorGuardCovered = $missingCompressorGuardCovered
        ExternalCompressorSuccessCovered = $externalCompressorSuccessCovered
        CompressorReadinessReportDeclared = $compressorReadinessReportDeclared
        AutomationCoverageDeclared = $automationCoverageDeclared
        TrueCompressionIntegrated = $trueCompressionIntegrated
        CandidatePathCount = $candidatePaths.Count
        AcceptanceEvidenceCount = $acceptanceEvidence.Count
        RecommendedNextDecision = "Configure and verify an accepted external compressor, or choose NativeLibrary/ServerPostProcess before claiming true LAZ."
        Valid = ($placeholderExplicit -and $writesLasSourceOnly -and $externalCompressorOptInImplemented -and $externalCompressorContractHardened -and $missingCompressorGuardCovered -and $externalCompressorSuccessCovered -and $compressorReadinessReportDeclared -and $automationCoverageDeclared -and -not $trueCompressionIntegrated)
    }
}

if (-not $report.Summary.Valid) {
    throw "LAZ decision report invariants failed. PlaceholderExplicit=$placeholderExplicit WritesLasSourceOnly=$writesLasSourceOnly ExternalCompressorOptIn=$externalCompressorOptInImplemented ExternalCompressorContractHardened=$externalCompressorContractHardened MissingCompressorGuard=$missingCompressorGuardCovered ExternalCompressorSuccess=$externalCompressorSuccessCovered CompressorReadiness=$compressorReadinessReportDeclared AutomationCoverageDeclared=$automationCoverageDeclared TrueCompressionIntegrated=$trueCompressionIntegrated"
}

if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}
if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-MarkdownReport -Report $report -Path $MarkdownPath
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "LAZ compression decision report is ready."
    Write-Host "Project root: $ProjectRoot"
    Write-Host "Candidate paths: $($report.Summary.CandidatePathCount)"
    Write-Host "Acceptance evidence items: $($report.Summary.AcceptanceEvidenceCount)"
    Write-Host "Placeholder explicit: $($report.Summary.PlaceholderExplicit)"
    Write-Host "Writes LAS source only: $($report.Summary.WritesLasSourceOnly)"
    Write-Host "External compressor opt-in implemented: $($report.Summary.ExternalCompressorOptInImplemented)"
    Write-Host "External compressor contract hardened: $($report.Summary.ExternalCompressorContractHardened)"
    Write-Host "Missing-compressor guard covered: $($report.Summary.MissingCompressorGuardCovered)"
    Write-Host "External compressor success covered: $($report.Summary.ExternalCompressorSuccessCovered)"
    Write-Host "Compressor readiness report declared: $($report.Summary.CompressorReadinessReportDeclared)"
    Write-Host "True compression integrated: $($report.Summary.TrueCompressionIntegrated)"
    Write-Host "Recommended next decision: $($report.Summary.RecommendedNextDecision)"
}
