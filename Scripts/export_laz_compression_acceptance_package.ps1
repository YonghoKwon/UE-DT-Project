param(
    [string]$ProjectRoot = "",
    [string]$OutputRoot = "",
    [string]$CompressorPath = "",
    [string]$ReaderPath = "",
    [string]$LazEvidencePath = "",
    [switch]$RunReaderProbe,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }
    return (Split-Path -Parent $PSScriptRoot)
}

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "Required script not found: $ScriptPath"
    }

    $output = @(& powershell -ExecutionPolicy Bypass -File $ScriptPath @Parameters -Json)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Convert-ToMarkdownCell {
    param([object]$Value)
    if ($null -eq $Value) { return "" }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

$ProjectRoot = Resolve-ProjectRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\LazCompressionAcceptance"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$decisionScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_decision_report.ps1"
$readinessScript = Join-Path $ProjectRoot "Scripts\export_laz_compressor_readiness_report.ps1"
$policyScript = Join-Path $ProjectRoot "Scripts\validate_laz_placeholder_policy.ps1"
$templateScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_acceptance_template.ps1"
$validatorScript = Join-Path $ProjectRoot "Scripts\validate_laz_compression_acceptance_evidence.ps1"
$runbookScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_acceptance_runbook.ps1"

$decisionJsonPath = Join-Path $OutputRoot "laz_compression_decision.json"
$decisionMarkdownPath = Join-Path $OutputRoot "laz_compression_decision.md"
$readinessJsonPath = Join-Path $OutputRoot "laz_compressor_readiness.json"
$readinessMarkdownPath = Join-Path $OutputRoot "laz_compressor_readiness.md"
$policyJsonPath = Join-Path $OutputRoot "laz_placeholder_policy.json"
$evidenceJsonPath = Join-Path $OutputRoot "laz_compression_acceptance.evidence.json"
$evidenceMarkdownPath = Join-Path $OutputRoot "laz_compression_acceptance_template.md"
$validationJsonPath = Join-Path $OutputRoot "laz_compression_acceptance_validation.json"
$manifestJsonPath = Join-Path $OutputRoot "laz_compression_acceptance_package.json"
$manifestMarkdownPath = Join-Path $OutputRoot "README.md"
$runbookMarkdownPath = Join-Path $OutputRoot "laz_compression_acceptance_runbook.md"

$decision = Invoke-JsonScript -ScriptPath $decisionScript -Parameters @{
    ProjectRoot = $ProjectRoot
    JsonPath = $decisionJsonPath
    MarkdownPath = $decisionMarkdownPath
}

$readinessParams = @{
    ProjectRoot = $ProjectRoot
    JsonPath = $readinessJsonPath
    MarkdownPath = $readinessMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($CompressorPath)) { $readinessParams.CompressorPath = $CompressorPath }
if (-not [string]::IsNullOrWhiteSpace($ReaderPath)) { $readinessParams.ReaderPath = $ReaderPath }
if (-not [string]::IsNullOrWhiteSpace($LazEvidencePath)) { $readinessParams.LazEvidencePath = $LazEvidencePath }
if ($RunReaderProbe) { $readinessParams.RunReaderProbe = $true }
$readiness = Invoke-JsonScript -ScriptPath $readinessScript -Parameters $readinessParams

$policy = Invoke-JsonScript -ScriptPath $policyScript -Parameters @{
    ProjectRoot = $ProjectRoot
}
$policy | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $policyJsonPath -Encoding UTF8

$template = Invoke-JsonScript -ScriptPath $templateScript -Parameters @{
    ProjectRoot = $ProjectRoot
    OutputPath = $evidenceJsonPath
}
& powershell -ExecutionPolicy Bypass -File $templateScript -ProjectRoot $ProjectRoot -OutputPath $evidenceMarkdownPath | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "LAZ acceptance markdown template generation failed with exit code $LASTEXITCODE"
}
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    EvidencePath = $evidenceJsonPath
}
$validation | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $validationJsonPath -Encoding UTF8

$selectedCompressorCandidate = @($readiness.ToolCandidates | Where-Object { [string]$_.Role -eq "Compressor" -and [bool]$_.Exists } | Select-Object -First 1)
$selectedReaderCandidate = @($readiness.ToolCandidates | Where-Object { [string]$_.Role -eq "Reader" -and [bool]$_.Exists } | Select-Object -First 1)
$readerProbeOutputPath = Join-Path $OutputRoot "laz_reader_probe_output.txt"
$evidenceCopyHints = [PSCustomObject]@{
    DestinationEvidencePath = $evidenceJsonPath
    ReadinessReportPath = $readinessJsonPath
    SelectedCompressorCandidate = if ($selectedCompressorCandidate.Count -gt 0) {
        [PSCustomObject]@{
            Name = [string]$selectedCompressorCandidate[0].Name
            Path = [string]$selectedCompressorCandidate[0].Path
            VersionText = [string]$selectedCompressorCandidate[0].VersionText
            SuggestedArgumentList = @($selectedCompressorCandidate[0].SuggestedArgumentList)
            CopyToSection = "EvidenceSections.CompressorSelection"
        }
    } else { $null }
    SelectedReaderCandidate = if ($selectedReaderCandidate.Count -gt 0) {
        [PSCustomObject]@{
            Name = [string]$selectedReaderCandidate[0].Name
            Path = [string]$selectedReaderCandidate[0].Path
            VersionText = [string]$selectedReaderCandidate[0].VersionText
            SuggestedArgumentList = @($selectedReaderCandidate[0].SuggestedArgumentList)
            CopyToSection = "EvidenceSections.KnownReaderValidation"
        }
    } else { $null }
    ProducedLazEvidence = [PSCustomObject]@{
        LazEvidencePath = [string]$readiness.LazEvidence.Path
        OutputByteSize = [int64]$readiness.LazEvidence.SizeBytes
        ExtensionLooksLikeLaz = [bool]$readiness.LazEvidence.ExtensionLooksLikeLaz
        NonEmpty = [bool]$readiness.LazEvidence.NonEmpty
        CopyToSection = "EvidenceSections.ProducedLazEvidence"
    }
    KnownReaderValidation = [PSCustomObject]@{
        ReaderName = [string]$readiness.ReaderProbe.Tool
        ReaderProbeLazEvidencePath = [string]$readiness.LazEvidence.Path
        ProbeExitCode = $readiness.ReaderProbe.ExitCode
        ProbeSucceeded = [bool]$readiness.ReaderProbe.Succeeded
        ProbeWasRunAgainstSameLazEvidencePath = ([bool]$readiness.ReaderProbe.Requested -and [string]$readiness.LazEvidence.Path -ne "")
        ReaderOutputEvidencePath = if ([bool]$readiness.ReaderProbe.Succeeded) { $readerProbeOutputPath } else { "" }
        CopyToSection = "EvidenceSections.KnownReaderValidation"
    }
    PlaceholderDistinction = [PSCustomObject]@{
        NotLasSourcePlaceholder = ([string]$readiness.LazEvidence.Path -ne "" -and -not ([string]$readiness.LazEvidence.Path).ToLowerInvariant().Contains("_laz_source"))
        NotCopySurrogate = [bool]$readiness.Summary.ReadableOutputEvidencePresent
        NotExternalCompressorCopySurrogateOutput = [bool]$readiness.Summary.ReadableOutputEvidencePresent
        LazPathDoesNotContainLazSourcePrefix = ([string]$readiness.LazEvidence.Path -ne "" -and -not ([string]$readiness.LazEvidence.Path).ToLowerInvariant().Contains("_laz_source"))
        EvidencePath = $readinessJsonPath
        CopyToSection = "EvidenceSections.PlaceholderDistinction"
    }
    ReadyToAutoFillAcceptanceEvidence = ([bool]$readiness.Summary.ReadableOutputEvidencePresent -and [bool]$readiness.Summary.ReaderProbeSucceeded)
    AutoFillBlockedReason = if ([bool]$readiness.Summary.ReadableOutputEvidencePresent -and [bool]$readiness.Summary.ReaderProbeSucceeded) { "" } else { [string]$readiness.Summary.ReadableLazOutputMissingReason }
}

$manualSteps = @(
    "Choose one LAZ path: native library, external CLI compressor, or server/post-processing workflow.",
    "Record compressor/tool name, version, license, redistribution owner, and UE 5.3 packaging decision.",
    "Produce a real `.laz` output from the accepted path; the current `_laz_source_*.las` placeholder is not enough.",
    "Run export_laz_compressor_readiness_report.ps1 with -LazEvidencePath and -RunReaderProbe against a known reader such as lasinfo or pdal.",
    "Keep ExportLastPointCloudLaz() warning/placeholder language until readable compressed output evidence is accepted.",
    "Only claim true LAZ when ReadableOutputEvidencePresent and ReadyForRealLazAutomation are true."
)

$followUpCommands = @(
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f $decisionScript, $ProjectRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f $policyScript, $ProjectRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -CompressorPath "<compressor.exe>" -ReaderPath "<lasinfo-or-pdal.exe>" -LazEvidencePath "<output.laz>" -RunReaderProbe -Json' -f $readinessScript, $ProjectRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -CompressorPath "<compressor.exe>" -ReaderPath "<lasinfo-or-pdal.exe>" -LazEvidencePath "<output.laz>" -RunReaderProbe' -f $PSCommandPath, $ProjectRoot)
)

$generatedFiles = @(
    $decisionJsonPath,
    $decisionMarkdownPath,
    $readinessJsonPath,
    $readinessMarkdownPath,
    $policyJsonPath,
    $evidenceJsonPath,
    $evidenceMarkdownPath,
    $validationJsonPath,
    $manifestJsonPath,
    $manifestMarkdownPath
)

$manifest = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    OutputRoot = $OutputRoot
    DryRunOnly = $true
    DoesNotInstallCompressor = $true
    DoesNotRunCompressor = $true
    WritesLazOutput = $false
    ProbesToolVersionsByDefault = $false
    RunsReaderProbeOnlyWhenRequested = [bool]$RunReaderProbe
    ModifiesAssets = $false
    StagesFiles = $false
    DecisionSummary = $decision.Summary
    ReadinessSummary = $readiness.Summary
    PolicySummary = $policy.Summary
    TemplateSummary = $template.Summary
    ValidationSummary = $validation.Summary
    EvidenceCopyHints = $evidenceCopyHints
    RunbookExporter = $runbookScript
    RunbookMarkdownPath = $runbookMarkdownPath
    ManualSteps = $manualSteps
    FollowUpCommands = $followUpCommands
    GeneratedFiles = $generatedFiles
    Summary = [PSCustomObject]@{
        PackageCreated = $true
        OutputRoot = $OutputRoot
        PlaceholderExplicit = [bool]$decision.Summary.PlaceholderExplicit
        WritesLasSourceOnly = [bool]$decision.Summary.WritesLasSourceOnly
        ExternalCompressorOptInImplemented = [bool]$decision.Summary.ExternalCompressorOptInImplemented
        CompressorCandidateFound = [bool]$readiness.Summary.CompressorCandidateFound
        ReaderCandidateFound = [bool]$readiness.Summary.ReaderCandidateFound
        ToolCandidateDiscoveryOnly = [bool]$readiness.Summary.ToolCandidateDiscoveryOnly
        ToolReadinessOnly = [bool]$readiness.Summary.ToolReadinessOnly
        ToolCandidateIsAcceptanceEvidence = [bool]$readiness.Summary.ToolCandidateIsAcceptanceEvidence
        CompressorCandidateIsAcceptanceEvidence = [bool]$readiness.Summary.CompressorCandidateIsAcceptanceEvidence
        ReaderCandidateIsAcceptanceEvidence = [bool]$readiness.Summary.ReaderCandidateIsAcceptanceEvidence
        LazEvidenceFilePresent = [bool]$readiness.Summary.LazEvidenceFilePresent
        ReaderProbeRequested = [bool]$readiness.Summary.ReaderProbeRequested
        ReaderProbeSucceeded = [bool]$readiness.Summary.ReaderProbeSucceeded
        ReaderProbeRequiresProducedLaz = [bool]$readiness.Summary.ReaderProbeRequiresProducedLaz
        ProducedLazEvidencePresent = [bool]$readiness.Summary.ProducedLazEvidencePresent
        ReaderProbeIsAcceptanceEvidence = [bool]$readiness.Summary.ReaderProbeIsAcceptanceEvidence
        ReaderProbeAcceptanceBlocked = [bool]$readiness.Summary.ReaderProbeAcceptanceBlocked
        ReaderProbeAcceptanceBlockers = @($readiness.Summary.ReaderProbeAcceptanceBlockers)
        ReadableOutputEvidencePresent = [bool]$readiness.Summary.ReadableOutputEvidencePresent
        ReadableLazAcceptanceEvidencePresent = [bool]$readiness.Summary.ReadableLazAcceptanceEvidencePresent
        ReadyToClaimReadableLazAcceptance = [bool]$readiness.Summary.ReadyToClaimReadableLazAcceptance
        ReadableLazOutputMissingReason = [string]$readiness.Summary.ReadableLazOutputMissingReason
        ReadyForRealLazAutomation = [bool]$readiness.Summary.ReadyForRealLazAutomation
        TrueCompressionIntegrated = [bool]$decision.Summary.TrueCompressionIntegrated
        AcceptanceTemplateCreated = (Test-Path -LiteralPath $evidenceJsonPath -PathType Leaf)
        AcceptanceEvidenceSectionCount = [int]$template.Summary.EvidenceSectionCount
        RequiredEvidenceSections = @($template.Summary.RequiredEvidenceSections)
        AcceptanceEvidenceComplete = [bool]$validation.Summary.Complete
        AcceptanceEvidenceRequiredSectionCount = [int]$validation.Summary.RequiredEvidenceSectionCount
        AcceptanceEvidenceMissingCount = [int]$validation.Summary.FailedCheckCount
        AcceptanceEvidenceMissingChecks = @($validation.Summary.MissingAcceptanceChecks)
        TopMissingAcceptanceChecks = @($validation.Summary.TopMissingAcceptanceChecks)
        EvidenceCopyHintsCreated = $true
        AcceptanceRunbookAvailable = $true
        ReadyToAutoFillAcceptanceEvidence = [bool]$evidenceCopyHints.ReadyToAutoFillAcceptanceEvidence
        EvidenceAutoFillBlockedReason = [string]$evidenceCopyHints.AutoFillBlockedReason
        SelectedCompressorCandidatePath = if ($evidenceCopyHints.SelectedCompressorCandidate) { [string]$evidenceCopyHints.SelectedCompressorCandidate.Path } else { "" }
        SelectedReaderCandidatePath = if ($evidenceCopyHints.SelectedReaderCandidate) { [string]$evidenceCopyHints.SelectedReaderCandidate.Path } else { "" }
        ProducedLazEvidencePath = [string]$evidenceCopyHints.ProducedLazEvidence.LazEvidencePath
        AcceptanceEvidenceCount = [int]$decision.Summary.AcceptanceEvidenceCount
        DryRunOnly = $true
        AcceptancePackageIsEvidenceShell = $true
        AcceptancePackageIsReadableLazProof = $false
        GeneratedReportDoesNotMeanAcceptancePassed = $true
        DoesNotInstallCompressor = $true
        DoesNotRunCompressor = $true
        WritesLazOutput = $false
        ProbesToolVersionsByDefault = $false
        ModifiesAssets = $false
        StagesFiles = $false
        ReadyToClaimTrueLaz = ([bool]$decision.Summary.TrueCompressionIntegrated -and [bool]$readiness.Summary.ReadyForRealLazAutomation -and [bool]$validation.Summary.Complete)
        ReadyToClaimTrueLazBlockers = @(
            if (-not [bool]$decision.Summary.TrueCompressionIntegrated) { "True compression implementation is not integrated." }
            if (-not [bool]$readiness.Summary.CompressorCandidateFound) { "Accepted compressor/native/server workflow is not selected." }
            if (-not [bool]$readiness.Summary.ReaderCandidateFound) { "Known point-cloud reader is not selected." }
            if (-not [bool]$readiness.Summary.ProducedLazEvidencePresent) { "Produced .laz evidence is missing." }
            if (-not [bool]$readiness.Summary.ReaderProbeSucceeded) { "Known-reader probe success evidence is missing." }
            if (-not [bool]$validation.Summary.Complete) { "Acceptance evidence file is incomplete." }
        )
        Boundary = "This package collects LAZ compression acceptance evidence only. It never installs tools, never runs a compressor, never writes LAZ output, never probes tool versions by default, never modifies assets, and never stages files. Reader probing runs only when explicitly requested."
        Valid = ([bool]$decision.Summary.Valid -and [bool]$readiness.Summary.Valid -and [bool]$policy.Summary.Valid)
    }
}

$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestJsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# LAZ Compression Acceptance Package") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated UTC: $($manifest.GeneratedUtc)") | Out-Null
$lines.Add("- Project: $($manifest.ProjectRoot)") | Out-Null
$lines.Add("- Output root: $($manifest.OutputRoot)") | Out-Null
$lines.Add("- Placeholder explicit: $($manifest.Summary.PlaceholderExplicit)") | Out-Null
$lines.Add("- Writes LAS source only: $($manifest.Summary.WritesLasSourceOnly)") | Out-Null
$lines.Add("- External compressor opt-in implemented: $($manifest.Summary.ExternalCompressorOptInImplemented)") | Out-Null
$lines.Add("- Compressor candidate found: $($manifest.Summary.CompressorCandidateFound)") | Out-Null
$lines.Add("- Reader candidate found: $($manifest.Summary.ReaderCandidateFound)") | Out-Null
$lines.Add("- Tool candidate discovery only: $($manifest.Summary.ToolCandidateDiscoveryOnly)") | Out-Null
$lines.Add("- Tool candidate is acceptance evidence: $($manifest.Summary.ToolCandidateIsAcceptanceEvidence)") | Out-Null
$lines.Add("- Compressor candidate is acceptance evidence: $($manifest.Summary.CompressorCandidateIsAcceptanceEvidence)") | Out-Null
$lines.Add("- Reader candidate is acceptance evidence: $($manifest.Summary.ReaderCandidateIsAcceptanceEvidence)") | Out-Null
$lines.Add("- LAZ evidence file present: $($manifest.Summary.LazEvidenceFilePresent)") | Out-Null
$lines.Add("- Reader probe requested: $($manifest.Summary.ReaderProbeRequested)") | Out-Null
$lines.Add("- Reader probe succeeded: $($manifest.Summary.ReaderProbeSucceeded)") | Out-Null
$lines.Add("- Reader probe requires produced LAZ: $($manifest.Summary.ReaderProbeRequiresProducedLaz)") | Out-Null
$lines.Add("- Produced LAZ evidence present: $($manifest.Summary.ProducedLazEvidencePresent)") | Out-Null
$lines.Add("- Reader probe is acceptance evidence: $($manifest.Summary.ReaderProbeIsAcceptanceEvidence)") | Out-Null
$lines.Add("- Reader probe acceptance blocked: $($manifest.Summary.ReaderProbeAcceptanceBlocked)") | Out-Null
$lines.Add("- Readable output evidence present: $($manifest.Summary.ReadableOutputEvidencePresent)") | Out-Null
$lines.Add("- Readable LAZ acceptance evidence present: $($manifest.Summary.ReadableLazAcceptanceEvidencePresent)") | Out-Null
$lines.Add("- Ready to claim readable LAZ acceptance: $($manifest.Summary.ReadyToClaimReadableLazAcceptance)") | Out-Null
$lines.Add("- Readable LAZ output missing reason: $($manifest.Summary.ReadableLazOutputMissingReason)") | Out-Null
$lines.Add("- Ready for real LAZ automation: $($manifest.Summary.ReadyForRealLazAutomation)") | Out-Null
$lines.Add("- True compression integrated: $($manifest.Summary.TrueCompressionIntegrated)") | Out-Null
$lines.Add("- Acceptance template created: $($manifest.Summary.AcceptanceTemplateCreated)") | Out-Null
$lines.Add("- Acceptance evidence section count: $($manifest.Summary.AcceptanceEvidenceSectionCount)") | Out-Null
$lines.Add("- Required evidence sections: $(@($manifest.Summary.RequiredEvidenceSections) -join ', ')") | Out-Null
$lines.Add("- Acceptance evidence complete: $($manifest.Summary.AcceptanceEvidenceComplete)") | Out-Null
$lines.Add("- Required evidence section checks: $($manifest.Summary.AcceptanceEvidenceRequiredSectionCount)") | Out-Null
$lines.Add("- Missing acceptance check count: $($manifest.Summary.AcceptanceEvidenceMissingCount)") | Out-Null
$lines.Add("- Top missing acceptance checks: $($manifest.Summary.TopMissingAcceptanceChecks -join ', ')") | Out-Null
$lines.Add("- Evidence copy hints created: $($manifest.Summary.EvidenceCopyHintsCreated)") | Out-Null
$lines.Add("- Ready to auto-fill acceptance evidence: $($manifest.Summary.ReadyToAutoFillAcceptanceEvidence)") | Out-Null
$lines.Add("- Evidence auto-fill blocked reason: $($manifest.Summary.EvidenceAutoFillBlockedReason)") | Out-Null
$lines.Add("- Selected compressor candidate path: $($manifest.Summary.SelectedCompressorCandidatePath)") | Out-Null
$lines.Add("- Selected reader candidate path: $($manifest.Summary.SelectedReaderCandidatePath)") | Out-Null
$lines.Add("- Produced LAZ evidence path: $($manifest.Summary.ProducedLazEvidencePath)") | Out-Null
$lines.Add("- Ready to claim true LAZ blockers: $($manifest.Summary.ReadyToClaimTrueLazBlockers -join '; ')") | Out-Null
$lines.Add("- Dry run only: $($manifest.DryRunOnly)") | Out-Null
$lines.Add("- Acceptance package is evidence shell: $($manifest.Summary.AcceptancePackageIsEvidenceShell)") | Out-Null
$lines.Add("- Acceptance package is readable LAZ proof: $($manifest.Summary.AcceptancePackageIsReadableLazProof)") | Out-Null
$lines.Add("- Generated report does not mean acceptance passed: $($manifest.Summary.GeneratedReportDoesNotMeanAcceptancePassed)") | Out-Null
$lines.Add("- Does not run compressor: $($manifest.DoesNotRunCompressor)") | Out-Null
$lines.Add("- Writes LAZ output: $($manifest.WritesLazOutput)") | Out-Null
$lines.Add("- Probes tool versions by default: $($manifest.ProbesToolVersionsByDefault)") | Out-Null
$lines.Add("- Modifies assets: $($manifest.ModifiesAssets)") | Out-Null
$lines.Add("- Stages files: $($manifest.StagesFiles)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Generated Files") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Path |") | Out-Null
$lines.Add("| --- |") | Out-Null
foreach ($file in $generatedFiles) {
    $lines.Add(("| {0} |" -f (Convert-ToMarkdownCell $file))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Evidence Copy Hints") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Destination evidence path: $($manifest.EvidenceCopyHints.DestinationEvidencePath)") | Out-Null
$lines.Add("- Readiness report path: $($manifest.EvidenceCopyHints.ReadinessReportPath)") | Out-Null
$lines.Add("- Ready to auto-fill acceptance evidence: $($manifest.EvidenceCopyHints.ReadyToAutoFillAcceptanceEvidence)") | Out-Null
$lines.Add("- Auto-fill blocked reason: $($manifest.EvidenceCopyHints.AutoFillBlockedReason)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Section | Field | Suggested value |") | Out-Null
$lines.Add("| --- | --- | --- |") | Out-Null
if ($manifest.EvidenceCopyHints.SelectedCompressorCandidate) {
    $lines.Add(("| CompressorSelection | ToolOrLibraryName | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.SelectedCompressorCandidate.Name))) | Out-Null
    $lines.Add(("| CompressorSelection | EvidencePath | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.ReadinessReportPath))) | Out-Null
}
if ($manifest.EvidenceCopyHints.SelectedReaderCandidate) {
    $lines.Add(("| KnownReaderValidation | ReaderName | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.SelectedReaderCandidate.Name))) | Out-Null
    $lines.Add(("| KnownReaderValidation | ReaderPath | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.SelectedReaderCandidate.Path))) | Out-Null
}
$lines.Add(("| ProducedLazEvidence | LazEvidencePath | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.ProducedLazEvidence.LazEvidencePath))) | Out-Null
$lines.Add(("| ProducedLazEvidence | OutputByteSize | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.ProducedLazEvidence.OutputByteSize))) | Out-Null
$lines.Add(("| KnownReaderValidation | ReaderProbeLazEvidencePath | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.KnownReaderValidation.ReaderProbeLazEvidencePath))) | Out-Null
$lines.Add(("| KnownReaderValidation | ProbeExitCode | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.KnownReaderValidation.ProbeExitCode))) | Out-Null
$lines.Add(("| PlaceholderDistinction | EvidencePath | {0} |" -f (Convert-ToMarkdownCell $manifest.EvidenceCopyHints.PlaceholderDistinction.EvidencePath))) | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Manual Steps") | Out-Null
$lines.Add("") | Out-Null
for ($index = 0; $index -lt $manualSteps.Count; $index++) {
    $lines.Add(("{0}. {1}" -f ($index + 1), $manualSteps[$index])) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Follow-up Commands") | Out-Null
$lines.Add("") | Out-Null
foreach ($command in $followUpCommands) {
    $lines.Add('```powershell') | Out-Null
    $lines.Add($command) | Out-Null
    $lines.Add('```') | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($manifest.Summary.Boundary)") | Out-Null

Write-TextFile -Path $manifestMarkdownPath -Lines $lines

if ($Json) {
    $manifest | ConvertTo-Json -Depth 10
}
else {
    Write-Host "LAZ compression acceptance package created."
    Write-Host "Output root: $($manifest.OutputRoot)"
    Write-Host "Readable output evidence present: $($manifest.Summary.ReadableOutputEvidencePresent)"
    Write-Host "Readable LAZ acceptance evidence present: $($manifest.Summary.ReadableLazAcceptanceEvidencePresent)"
    Write-Host "Ready to claim readable LAZ acceptance: $($manifest.Summary.ReadyToClaimReadableLazAcceptance)"
    Write-Host "Acceptance package is evidence shell: $($manifest.Summary.AcceptancePackageIsEvidenceShell)"
    Write-Host "Acceptance package is readable LAZ proof: $($manifest.Summary.AcceptancePackageIsReadableLazProof)"
    Write-Host "Ready for real LAZ automation: $($manifest.Summary.ReadyForRealLazAutomation)"
    Write-Host "True compression integrated: $($manifest.Summary.TrueCompressionIntegrated)"
    Write-Host "Acceptance evidence section count: $($manifest.Summary.AcceptanceEvidenceSectionCount)"
    Write-Host "Acceptance evidence complete: $($manifest.Summary.AcceptanceEvidenceComplete)"
    Write-Host "Missing acceptance check count: $($manifest.Summary.AcceptanceEvidenceMissingCount)"
    Write-Host "Evidence copy hints created: $($manifest.Summary.EvidenceCopyHintsCreated)"
    Write-Host "Ready to auto-fill acceptance evidence: $($manifest.Summary.ReadyToAutoFillAcceptanceEvidence)"
}
