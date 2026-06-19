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
        LazEvidenceFilePresent = [bool]$readiness.Summary.LazEvidenceFilePresent
        ReaderProbeRequested = [bool]$readiness.Summary.ReaderProbeRequested
        ReaderProbeSucceeded = [bool]$readiness.Summary.ReaderProbeSucceeded
        ReadableOutputEvidencePresent = [bool]$readiness.Summary.ReadableOutputEvidencePresent
        ReadyForRealLazAutomation = [bool]$readiness.Summary.ReadyForRealLazAutomation
        TrueCompressionIntegrated = [bool]$decision.Summary.TrueCompressionIntegrated
        AcceptanceTemplateCreated = (Test-Path -LiteralPath $evidenceJsonPath -PathType Leaf)
        AcceptanceEvidenceSectionCount = [int]$template.Summary.EvidenceSectionCount
        RequiredEvidenceSections = @($template.Summary.RequiredEvidenceSections)
        AcceptanceEvidenceComplete = [bool]$validation.Summary.Complete
        AcceptanceEvidenceRequiredSectionCount = [int]$validation.Summary.RequiredEvidenceSectionCount
        AcceptanceEvidenceMissingCount = [int]$validation.Summary.FailedCheckCount
        AcceptanceEvidenceCount = [int]$decision.Summary.AcceptanceEvidenceCount
        DryRunOnly = $true
        DoesNotInstallCompressor = $true
        DoesNotRunCompressor = $true
        WritesLazOutput = $false
        ProbesToolVersionsByDefault = $false
        ModifiesAssets = $false
        StagesFiles = $false
        ReadyToClaimTrueLaz = ([bool]$decision.Summary.TrueCompressionIntegrated -and [bool]$readiness.Summary.ReadyForRealLazAutomation -and [bool]$validation.Summary.Complete)
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
$lines.Add("- LAZ evidence file present: $($manifest.Summary.LazEvidenceFilePresent)") | Out-Null
$lines.Add("- Reader probe requested: $($manifest.Summary.ReaderProbeRequested)") | Out-Null
$lines.Add("- Reader probe succeeded: $($manifest.Summary.ReaderProbeSucceeded)") | Out-Null
$lines.Add("- Readable output evidence present: $($manifest.Summary.ReadableOutputEvidencePresent)") | Out-Null
$lines.Add("- Ready for real LAZ automation: $($manifest.Summary.ReadyForRealLazAutomation)") | Out-Null
$lines.Add("- True compression integrated: $($manifest.Summary.TrueCompressionIntegrated)") | Out-Null
$lines.Add("- Acceptance template created: $($manifest.Summary.AcceptanceTemplateCreated)") | Out-Null
$lines.Add("- Acceptance evidence section count: $($manifest.Summary.AcceptanceEvidenceSectionCount)") | Out-Null
$lines.Add("- Required evidence sections: $(@($manifest.Summary.RequiredEvidenceSections) -join ', ')") | Out-Null
$lines.Add("- Acceptance evidence complete: $($manifest.Summary.AcceptanceEvidenceComplete)") | Out-Null
$lines.Add("- Required evidence section checks: $($manifest.Summary.AcceptanceEvidenceRequiredSectionCount)") | Out-Null
$lines.Add("- Missing acceptance check count: $($manifest.Summary.AcceptanceEvidenceMissingCount)") | Out-Null
$lines.Add("- Dry run only: $($manifest.DryRunOnly)") | Out-Null
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
    Write-Host "Ready for real LAZ automation: $($manifest.Summary.ReadyForRealLazAutomation)"
    Write-Host "True compression integrated: $($manifest.Summary.TrueCompressionIntegrated)"
    Write-Host "Acceptance evidence section count: $($manifest.Summary.AcceptanceEvidenceSectionCount)"
    Write-Host "Acceptance evidence complete: $($manifest.Summary.AcceptanceEvidenceComplete)"
    Write-Host "Missing acceptance check count: $($manifest.Summary.AcceptanceEvidenceMissingCount)"
}
