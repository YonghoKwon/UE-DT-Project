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

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$requiredFiles = @(
    [PSCustomObject]@{ Label = "LiDAR component header"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h" },
    [PSCustomObject]@{ Label = "LiDAR component implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp" },
    [PSCustomObject]@{ Label = "Monitor widget header"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h" },
    [PSCustomObject]@{ Label = "Monitor widget implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp" },
    [PSCustomObject]@{ Label = "Replay automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "LiDAR payload schema"; Path = "docs\lidar_payload_schema.md" },
    [PSCustomObject]@{ Label = "Remaining work document"; Path = "docs\remaining_work.md" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$lidarHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h"
$lidarCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp"
$monitorHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h"
$monitorCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp"
$replayTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp"
$schemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$decisionReportScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_decision_report.ps1"
$readinessReportScript = Join-Path $ProjectRoot "Scripts\export_laz_compressor_readiness_report.ps1"
$acceptancePackageScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_acceptance_package.ps1"
$acceptanceTemplateScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_acceptance_template.ps1"
$acceptanceValidatorScript = Join-Path $ProjectRoot "Scripts\validate_laz_compression_acceptance_evidence.ps1"
$precommitSummaryScript = Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"

Assert-FileExists -Path $decisionReportScript -Label "LAZ compression decision report script"
Assert-FileExists -Path $readinessReportScript -Label "LAZ compressor readiness report script"
Assert-FileExists -Path $acceptancePackageScript -Label "LAZ compression acceptance package script"
Assert-FileExists -Path $acceptanceTemplateScript -Label "LAZ compression acceptance template script"
Assert-FileExists -Path $acceptanceValidatorScript -Label "LAZ compression acceptance evidence validator"
Assert-FileExists -Path $precommitSummaryScript -Label "Pre-commit summary script"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "ExportLastPointCloudLaz"; Label = "LAZ export API remains explicit" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "bUseExternalLazCompressor"; Label = "External compressor opt-in setting exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "ExternalLazCompressorArguments"; Label = "External compressor argument template exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "GetLastLazExportStatusText"; Label = "LAZ export status telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "WasLastLazExportAttempted"; Label = "LAZ export attempt telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "DidLastLazExportSucceed"; Label = "LAZ export success telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "WasLastLazExportPlaceholderOnly"; Label = "LAZ placeholder telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "WasLastLazExternalCompressorRequested"; Label = "LAZ external request telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "DidLastLazExternalCompressorSucceed"; Label = "LAZ external success telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "DidLastLazProduceOutputFile"; Label = "LAZ produced-output telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "WasLastLazTrueCompressionValidated"; Label = "LAZ true-validation telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "GetLastLazExternalCompressorReturnCode"; Label = "LAZ compressor return-code telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "GetLastLazOutputSizeBytes"; Label = "LAZ output-size telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "GetLastLazExportWarningText"; Label = "LAZ warning telemetry getter exists" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "LAZ compression is not integrated"; Label = "Runtime warning states LAZ is not integrated" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "PlaceholderOnlyLasSource"; Label = "Runtime telemetry records placeholder-only LAZ state" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "ExternalCompressorSucceeded"; Label = "Runtime telemetry records external compressor success" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "RunExternalLazCompressor"; Label = "External compressor helper exists" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "ExecProcess"; Label = "External compressor uses explicit process execution" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = 'BuildExportPath(TEXT("laz"), FileNamePrefix)'; Label = "External compressor writes distinct laz output path" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = 'Arguments.Contains(TEXT("{input}"))'; Label = "External compressor requires input placeholder" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = 'Arguments.Contains(TEXT("{output}"))'; Label = "External compressor requires output placeholder" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "FullLazOutputPath"; Label = "External compressor uses absolute output path" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "MakePlatformFilename"; Label = "External compressor uses platform-native paths" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "OutputSize <= 0"; Label = "External compressor rejects empty LAZ output" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "_laz_source"; Label = "Placeholder uses laz_source prefix" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "ExportLastPointCloudLasToPath"; Label = "Placeholder writes LAS-compatible source" },
    [PSCustomObject]@{ Path = $monitorHeader; Pattern = "external compressor"; Label = "Monitor setting comments clarify external compressor opt-in" },
    [PSCustomObject]@{ Path = $monitorCpp; Pattern = "ExportLastPointCloudLaz"; Label = "Monitor routes LAZ option through placeholder API" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "M7AT10.SensorReplay.LazPlaceholderWritesLasSource"; Label = "LAZ placeholder automation test name" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "M7AT10.SensorReplay.LazExternalCompressorMissingFails"; Label = "Missing external compressor automation test name" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "M7AT10.SensorReplay.LazExternalCompressorFakeWritesOutput"; Label = "External compressor fake-success automation test name" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "does not create compressed .laz files"; Label = "Automation asserts no compressed LAZ output" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "missing external compressor does not create .laz files"; Label = "Automation asserts missing compressor creates no LAZ" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "external compressor success creates one .laz output"; Label = "Automation asserts external compressor creates LAZ output" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "LAZ placeholder telemetry marks placeholder-only"; Label = "Automation asserts placeholder telemetry" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "missing compressor telemetry records attempt"; Label = "Automation asserts missing compressor telemetry" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "external success telemetry records success"; Label = "Automation asserts external compressor success telemetry" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "external success telemetry records return code"; Label = "Automation asserts external compressor return code" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "external success telemetry is not true LAZ validated"; Label = "Automation asserts external success is not readable LAZ proof" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "process-contract surrogate"; Label = "Automation documents success test as process surrogate" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "_laz_source_"; Label = "Automation checks laz_source LAS output" },
    [PSCustomObject]@{ Path = $schemaDoc; Pattern = 'does not pretend to create a compressed `.laz` file'; Label = "Schema doc clarifies placeholder" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "True LAZ Compression"; Label = "Remaining work tracks true LAZ" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "intentionally a placeholder"; Label = "Remaining work states placeholder status" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_laz_compression_decision_report.ps1"; Label = "Remaining work documents LAZ decision report" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_laz_compressor_readiness_report.ps1"; Label = "Remaining work documents LAZ compressor readiness report" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "Native LAZ library"; Label = "Decision report includes native library path" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "External CLI compressor"; Label = "Decision report includes external CLI path" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "Server/post-processing workflow"; Label = "Decision report includes server post-process path" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "Readable validation from a known point-cloud tool"; Label = "Decision report tracks readable LAZ evidence" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "RunReaderProbe"; Label = "Decision report points to reader-probe evidence capture" },
    [PSCustomObject]@{ Path = $decisionReportScript; Pattern = "CompressorReadinessReportDeclared"; Label = "Decision report consumes compressor readiness evidence" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReadyForRealLazAutomation"; Label = "Readiness report states real LAZ automation readiness" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ToolCandidateDiscoveryOnly"; Label = "Readiness report separates tool discovery from acceptance evidence" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "CompressorCandidateIsAcceptanceEvidence"; Label = "Readiness report blocks compressor candidate as acceptance evidence" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReaderCandidateIsAcceptanceEvidence"; Label = "Readiness report blocks reader candidate as acceptance evidence" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReadableOutputEvidencePresent"; Label = "Readiness report tracks readable output evidence" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReadableLazAcceptanceEvidencePresent"; Label = "Readiness report tracks readable LAZ acceptance evidence" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReadyToClaimReadableLazAcceptance"; Label = "Readiness report exposes readable LAZ acceptance gate" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReaderProbeRequiresProducedLaz"; Label = "Readiness report requires produced LAZ for reader probe acceptance" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "LazEvidencePath"; Label = "Readiness report accepts explicit LAZ evidence path" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "RunReaderProbe"; Label = "Readiness report can run reader probe" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReaderProbeSucceeded"; Label = "Readiness report records reader probe result" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ReaderProbeBlockedReason"; Label = "Readiness report records reader probe block reason" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ProbeToolVersions"; Label = "Readiness report keeps tool version probes opt-in" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "ToolVersionProbesRequested"; Label = "Readiness report exposes tool version probe state" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "KnownPointCloudReader"; Label = "Readiness report requires a known point-cloud reader" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "laszip"; Label = "Readiness report checks laszip candidate" },
    [PSCustomObject]@{ Path = $readinessReportScript; Pattern = "pdal"; Label = "Readiness report checks pdal candidate" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "Saved\Reports\LazCompressionAcceptance"; Label = "Acceptance package writes local Saved report bundle" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "export_laz_compression_decision_report.ps1"; Label = "Acceptance package consumes LAZ decision report" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "export_laz_compressor_readiness_report.ps1"; Label = "Acceptance package consumes LAZ readiness report" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "validate_laz_placeholder_policy.ps1"; Label = "Acceptance package consumes LAZ placeholder policy" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "DoesNotRunCompressor = `$true"; Label = "Acceptance package declares no compressor execution" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "Reader probing runs only when explicitly requested"; Label = "Acceptance package documents opt-in reader probe boundary" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "ReadableOutputEvidencePresent"; Label = "Acceptance package exposes readable output evidence" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "ToolCandidateDiscoveryOnly"; Label = "Acceptance package exposes tool discovery-only boundary" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "AcceptancePackageIsReadableLazProof"; Label = "Acceptance package states generated bundle is not readable LAZ proof" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "GeneratedReportDoesNotMeanAcceptancePassed"; Label = "Acceptance package states generated report is not acceptance" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "ReadyToClaimReadableLazAcceptance"; Label = "Acceptance package exposes readable LAZ acceptance gate" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "ReadyForRealLazAutomation"; Label = "Acceptance package exposes real LAZ readiness" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "WritesLazOutput = `$false"; Label = "Acceptance package declares it does not write LAZ output" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "ProbesToolVersionsByDefault = `$false"; Label = "Acceptance package does not probe tool versions by default" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "ReadyToClaimTrueLaz"; Label = "Acceptance package exposes true LAZ claim gate" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "export_laz_compression_acceptance_template.ps1"; Label = "Acceptance package writes fillable LAZ evidence template" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "validate_laz_compression_acceptance_evidence.ps1"; Label = "Acceptance package validates LAZ evidence draft" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "AcceptanceEvidenceComplete"; Label = "Acceptance package exposes evidence completion state" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "TopMissingAcceptanceChecks"; Label = "Acceptance package exposes top missing evidence checks" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "ReadyToClaimTrueLazBlockers"; Label = "Acceptance package exposes true LAZ blocker list" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "AcceptanceEvidenceSectionCount"; Label = "Acceptance package exposes evidence section count" },
    [PSCustomObject]@{ Path = $acceptancePackageScript; Pattern = "RequiredEvidenceSections"; Label = "Acceptance package exposes required evidence section names" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "LazCompressionAcceptanceEvidenceV1"; Label = "Acceptance template declares schema version" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "EvidenceSections"; Label = "Acceptance template declares structured evidence sections" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "CompressorSelection"; Label = "Acceptance template declares compressor selection section" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "ProducedLazEvidence"; Label = "Acceptance template declares produced LAZ evidence section" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "ProducedByExportLastPointCloudLazOrAcceptedPostProcess"; Label = "Acceptance template records accepted production path" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "KnownReaderValidation"; Label = "Acceptance template declares known reader validation section" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "ProbeExitCode"; Label = "Acceptance template records reader probe exit code" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "ProbeWasRunAgainstSameLazEvidencePath"; Label = "Acceptance template records reader probe target path" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "ReaderProbeLazEvidencePath"; Label = "Acceptance template records concrete reader probe LAZ path" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "NotExternalCompressorCopySurrogateOutput"; Label = "Acceptance template records copy surrogate distinction" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "RepeatableCommand"; Label = "Acceptance template declares repeatable command section" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "OwnerAcceptance"; Label = "Acceptance template declares owner acceptance section" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "RunsCompressor = `$false"; Label = "Acceptance template declares compressor does not run" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "RunsReaderProbe = `$false"; Label = "Acceptance template declares reader probe does not run" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "WritesLazOutput = `$false"; Label = "Acceptance template declares it writes no LAZ output" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "Produced LAZ output"; Label = "Acceptance template requires produced LAZ output evidence" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "Known reader validation"; Label = "Acceptance template requires known reader validation" },
    [PSCustomObject]@{ Path = $acceptanceTemplateScript; Pattern = "Placeholder distinction"; Label = "Acceptance template requires placeholder distinction evidence" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "FailOnIncompleteEvidence"; Label = "Acceptance validator can fail on incomplete evidence" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "CurrentReadyToClaimTrueLaz"; Label = "Acceptance validator exposes true LAZ readiness" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "EvidenceSections present"; Label = "Acceptance validator requires evidence sections" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = '"SelectedWorkflow", "ToolOrLibraryName", "Version", "License", "RedistributionOwner", "UnrealPackagingDecision", "EvidencePath"'; Label = "Acceptance validator checks compressor selection fields" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "ProducedLazEvidence produced by accepted workflow"; Label = "Acceptance validator checks accepted workflow production flag" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "ProducedLazEvidence path has .laz extension"; Label = "Acceptance validator checks LAZ extension" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "ProducedLazEvidence file is non-empty"; Label = "Acceptance validator checks non-empty LAZ output" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "ProducedLazEvidence output byte size matches file"; Label = "Acceptance validator checks LAZ size match" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "ProducedLazEvidence source LAS path exists"; Label = "Acceptance validator checks source LAS existence" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "ProducedLazEvidence LAZ path differs from source LAS path"; Label = "Acceptance validator checks LAZ and LAS paths differ" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "ProducedLazEvidence produced by export or accepted post-process"; Label = "Acceptance validator checks accepted production path" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "KnownReaderValidation reader name is known"; Label = "Acceptance validator checks known reader identity" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "KnownReaderValidation reader output evidence exists"; Label = "Acceptance validator checks reader output evidence" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "KnownReaderValidation probe LAZ path matches produced LAZ evidence"; Label = "Acceptance validator checks reader probe concrete target path" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "KnownReaderValidation probe exit code is zero"; Label = "Acceptance validator checks reader exit code" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "KnownReaderValidation probe ran against same LAZ evidence path"; Label = "Acceptance validator checks reader target path" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "PlaceholderDistinction not external compressor copy-surrogate output"; Label = "Acceptance validator rejects copy surrogate output" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "MissingAcceptanceChecks"; Label = "Acceptance validator exposes missing check names" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "KnownReaderValidation probe succeeded"; Label = "Acceptance validator checks reader probe success" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "OwnerAcceptance accepted"; Label = "Acceptance validator checks owner acceptance flag" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "Known reader validation"; Label = "Acceptance validator checks known reader validation evidence" },
    [PSCustomObject]@{ Path = $acceptanceValidatorScript; Pattern = "LAZ evidence path exists"; Label = "Acceptance validator checks LAZ evidence path" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "Get-LazExportDecisionSummary"; Label = "Pre-commit summary includes LAZ decision summary helper" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "export_laz_compression_decision_report.ps1"; Label = "Pre-commit summary consumes LAZ decision report" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "export_laz_compressor_readiness_report.ps1"; Label = "Pre-commit summary consumes LAZ readiness report" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "TrueCompressionIntegrated"; Label = "Pre-commit summary surfaces true compression boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ReadableOutputEvidencePresent"; Label = "Pre-commit summary surfaces readable output evidence boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ToolCandidateDiscoveryOnly"; Label = "Pre-commit summary surfaces tool discovery-only boundary" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ReadableLazAcceptanceEvidencePresent"; Label = "Pre-commit summary surfaces readable LAZ acceptance evidence" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ReadyToClaimReadableLazAcceptance"; Label = "Pre-commit summary surfaces readable LAZ acceptance gate" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ReadyForRealLazAutomation"; Label = "Pre-commit summary surfaces real LAZ automation readiness" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "LazCompressorSelectionPresent"; Label = "Pre-commit summary surfaces compressor selection evidence state" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ProducedLazEvidencePresent"; Label = "Pre-commit summary surfaces produced LAZ evidence state" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "KnownReaderValidationPresent"; Label = "Pre-commit summary surfaces known reader validation state" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "RepeatableCommandPresent"; Label = "Pre-commit summary surfaces repeatable command evidence state" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "LazOwnerAcceptancePresent"; Label = "Pre-commit summary surfaces LAZ owner acceptance state" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ReadyToClaimTrueLaz"; Label = "Pre-commit summary avoids claiming true LAZ too early" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$tempEvidencePath = Join-Path ([System.IO.Path]::GetTempPath()) ("m7at10_laz_policy_probe_{0}.laz" -f ([Guid]::NewGuid().ToString("N")))
try {
    Set-Content -LiteralPath $tempEvidencePath -Value "not real laz" -Encoding ASCII
    $missingReaderPath = Join-Path ([System.IO.Path]::GetTempPath()) ("missing_laz_reader_{0}.exe" -f ([Guid]::NewGuid().ToString("N")))
    $readinessJson = & powershell -ExecutionPolicy Bypass -File $readinessReportScript -ProjectRoot $ProjectRoot -ReaderPath $missingReaderPath -LazEvidencePath $tempEvidencePath -RunReaderProbe -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Readiness report negative reader-probe contract failed with exit code $LASTEXITCODE"
    }
    $readinessReport = $readinessJson | ConvertFrom-Json
    if ($readinessReport.Summary.ReadyForRealLazAutomation) {
        throw "Readiness report must not be ready when the requested reader tool is missing."
    }
    if ($readinessReport.Summary.ReadableOutputEvidencePresent) {
        throw "ReadableOutputEvidencePresent must remain false when the requested reader tool is missing."
    }
    if (-not $readinessReport.Summary.ReaderProbeRequestedByUser) {
        throw "ReaderProbeRequestedByUser must be true when -RunReaderProbe is passed."
    }
    if ($readinessReport.Summary.ReaderProbeBlockedReason -ne "ReaderToolMissing") {
        throw "ReaderProbeBlockedReason should be ReaderToolMissing for missing reader path."
    }
}
finally {
    if (Test-Path -LiteralPath $tempEvidencePath) {
        Remove-Item -LiteralPath $tempEvidencePath -Force
    }
}

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    CheckedFiles = @($requiredFiles | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Path = Join-Path $ProjectRoot $_.Path
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
        RequiredFileCount = $requiredFiles.Count
        RequiredContractCount = $requiredTexts.Count
        PlaceholderIsExplicit = $true
        WritesLasSourceOnly = $true
        AutomationCoverageDeclared = $true
        ExternalCompressorOptInDeclared = $true
        ExternalCompressorSuccessCovered = $true
        DecisionReportDeclared = $true
        CompressorReadinessReportDeclared = $true
        AcceptancePackageDeclared = $true
        AcceptanceTemplateDeclared = $true
        AcceptanceEvidenceValidatorDeclared = $true
        PrecommitSummaryDeclared = $true
        ReadableEvidenceProbeDeclared = $true
        ToolVersionProbeOptInDeclared = $true
        MissingReaderProbeGuardCovered = $true
        TrueCompressionStillOpen = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "LAZ placeholder policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Placeholder is explicit: $($report.Summary.PlaceholderIsExplicit)"
    Write-Host "Writes LAS source only: $($report.Summary.WritesLasSourceOnly)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
    Write-Host "External compressor opt-in declared: $($report.Summary.ExternalCompressorOptInDeclared)"
    Write-Host "External compressor success covered: $($report.Summary.ExternalCompressorSuccessCovered)"
    Write-Host "Decision report declared: $($report.Summary.DecisionReportDeclared)"
    Write-Host "Compressor readiness report declared: $($report.Summary.CompressorReadinessReportDeclared)"
    Write-Host "Acceptance package declared: $($report.Summary.AcceptancePackageDeclared)"
    Write-Host "Acceptance template declared: $($report.Summary.AcceptanceTemplateDeclared)"
    Write-Host "Acceptance evidence validator declared: $($report.Summary.AcceptanceEvidenceValidatorDeclared)"
    Write-Host "Pre-commit summary declared: $($report.Summary.PrecommitSummaryDeclared)"
    Write-Host "Readable evidence probe declared: $($report.Summary.ReadableEvidenceProbeDeclared)"
    Write-Host "Tool version probe opt-in declared: $($report.Summary.ToolVersionProbeOptInDeclared)"
    Write-Host "Missing reader probe guard covered: $($report.Summary.MissingReaderProbeGuardCovered)"
    Write-Host "True compression still open: $($report.Summary.TrueCompressionStillOpen)"
}
