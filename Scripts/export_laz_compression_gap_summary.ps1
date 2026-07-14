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

function Get-LazGapAction {
    param([string]$Check)

    $phase = "Evidence file completion"
    $target = "laz_compression_acceptance.evidence.json"
    $action = "Fill the missing LAZ acceptance field and attach an existing evidence file path."

    if ($Check -match "CompressorSelection|Compressor selection|CompressorPath|CompressorVersion|Accepted compressor|compressor|ToolOrLibraryName|SelectedWorkflow") {
        $phase = "Compressor/workflow selection"
        $target = "EvidenceSections.CompressorSelection"
        $action = "Choose native library, external CLI, or server/post-process workflow and record tool name, version, license, owner, packaging decision, and evidence path."
    }
    elseif ($Check -match "ProducedLazEvidence|Produced LAZ|LazEvidencePath|OutputByteSize|SourceLasPath|\\.laz|non-empty|byte size|accepted workflow|export or accepted post-process") {
        $phase = "Produced LAZ output"
        $target = "EvidenceSections.ProducedLazEvidence"
        $action = "Produce a non-empty .laz from the accepted workflow, record source LAS path, output byte size, producer command, and confirm it is not a *_laz_source_ placeholder."
    }
    elseif ($Check -match "KnownReaderValidation|Known reader|ReaderPath|ReaderVersion|ReaderProbe|ProbeExitCode|probe|reader|lasinfo|pdal") {
        $phase = "Known-reader validation"
        $target = "EvidenceSections.KnownReaderValidation"
        $action = "Run a known point-cloud reader against the same produced .laz, record exit code 0, version, output evidence, and probe report path."
    }
    elseif ($Check -match "PlaceholderDistinction|placeholder|copy surrogate|laz_source") {
        $phase = "Placeholder distinction"
        $target = "EvidenceSections.PlaceholderDistinction"
        $action = "Attach evidence that the output is not the LAS placeholder, not a copy-surrogate artifact, and does not use the _laz_source prefix."
    }
    elseif ($Check -match "RepeatableCommand|Command|WorkingDirectory") {
        $phase = "Repeatable command"
        $target = "EvidenceSections.RepeatableCommand"
        $action = "Record the exact repeatable command, working directory, and command evidence file for producing and validating the .laz."
    }
    elseif ($Check -match "OwnerAcceptance|Owner acceptance|Owner|Reviewer|ReviewedAtUtc|Accepted") {
        $phase = "Owner acceptance"
        $target = "EvidenceSections.OwnerAcceptance"
        $action = "Record owner/reviewer acceptance metadata after produced .laz and known-reader validation are attached."
    }
    elseif ($Check -match "Schema|Evidence file present|Acceptance metadata|EvidenceSections") {
        $phase = "Evidence shell"
        $target = "laz_compression_acceptance.evidence.json"
        $action = "Regenerate the LAZ acceptance package so the evidence shell, metadata, and required sections exist."
    }

    return [PSCustomObject]@{
        Check = $Check
        EvidencePhase = $phase
        EvidenceTarget = $target
        NextAction = $action
    }
}

$ProjectRoot = Resolve-ProjectRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\LazCompressionAcceptance"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
if (-not $OutputRoot.StartsWith((Join-Path $ProjectRoot "Saved"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be under ProjectRoot\Saved: $OutputRoot"
}

$packageScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_acceptance_package.ps1"
$runbookScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_acceptance_runbook.ps1"
$validatorScript = Join-Path $ProjectRoot "Scripts\validate_laz_compression_acceptance_evidence.ps1"
$evidencePath = Join-Path $OutputRoot "laz_compression_acceptance.evidence.json"

$packageParams = @{
    ProjectRoot = $ProjectRoot
    OutputRoot = $OutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($CompressorPath)) { $packageParams.CompressorPath = $CompressorPath }
if (-not [string]::IsNullOrWhiteSpace($ReaderPath)) { $packageParams.ReaderPath = $ReaderPath }
if (-not [string]::IsNullOrWhiteSpace($LazEvidencePath)) { $packageParams.LazEvidencePath = $LazEvidencePath }
if ($RunReaderProbe) { $packageParams.RunReaderProbe = $true }

$package = Invoke-JsonScript -ScriptPath $packageScript -Parameters $packageParams
$runbook = Invoke-JsonScript -ScriptPath $runbookScript -Parameters $packageParams
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    EvidencePath = $evidencePath
}

$missingChecks = @($validation.Summary.MissingAcceptanceChecks)
$gapActions = @($missingChecks | ForEach-Object { Get-LazGapAction -Check ([string]$_) })
$phaseSummaries = @(
    $gapActions |
        Group-Object -Property EvidencePhase |
        Sort-Object -Property @{ Expression = "Count"; Descending = $true }, @{ Expression = "Name"; Ascending = $true } |
        ForEach-Object {
            $first = @($_.Group | Select-Object -First 1)
            [PSCustomObject]@{
                EvidencePhase = [string]$_.Name
                MissingCheckCount = [int]$_.Count
                FirstEvidenceTarget = if ($first.Count -gt 0) { [string]$first[0].EvidenceTarget } else { "" }
                FirstNextAction = if ($first.Count -gt 0) { [string]$first[0].NextAction } else { "" }
            }
        }
)
$nextAction = if ($phaseSummaries.Count -gt 0) { [string]$phaseSummaries[0].FirstNextAction } else { "LAZ acceptance evidence is complete; run the strict validator before claiming true LAZ." }

$jsonPath = Join-Path $OutputRoot "laz_compression_gap_summary.json"
$markdownPath = Join-Path $OutputRoot "laz_compression_gap_summary.md"
$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    OutputRoot = $OutputRoot
    EvidencePath = $evidencePath
    JsonPath = $jsonPath
    MarkdownPath = $markdownPath
    DryRunOnly = $true
    DoesNotInstallCompressor = $true
    DoesNotRunCompressor = $true
    WritesLazOutput = $false
    ModifiesAssets = $false
    StagesFiles = $false
    PackageSummary = $package.Summary
    RunbookSummary = $runbook.Summary
    ValidationSummary = $validation.Summary
    PhaseSummaries = $phaseSummaries
    GapActions = $gapActions
    Summary = [PSCustomObject]@{
        GapSummaryCreated = $true
        MissingAcceptanceCheckCount = $missingChecks.Count
        MissingPhaseCount = $phaseSummaries.Count
        ReadyToClaimTrueLaz = [bool]$package.Summary.ReadyToClaimTrueLaz
        ReadyToAutoFillAcceptanceEvidence = [bool]$package.Summary.ReadyToAutoFillAcceptanceEvidence
        ReadableOutputEvidencePresent = [bool]$package.Summary.ReadableOutputEvidencePresent
        NextManualAction = $nextAction
        PackagePath = [string](Join-Path $OutputRoot "README.md")
        RunbookPath = [string]$runbook.MarkdownPath
        Boundary = "This summary only refreshes Saved/Reports LAZ acceptance artifacts. It does not install tools, run a compressor, write LAZ output, modify assets, or stage files."
        Valid = $true
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# LAZ Compression Gap Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated UTC: $($report.GeneratedUtc)") | Out-Null
$lines.Add("- Evidence file: $($report.EvidencePath)") | Out-Null
$lines.Add("- Missing acceptance checks: $($report.Summary.MissingAcceptanceCheckCount)") | Out-Null
$lines.Add("- Missing phases: $($report.Summary.MissingPhaseCount)") | Out-Null
$lines.Add("- Readable output evidence present: $($report.Summary.ReadableOutputEvidencePresent)") | Out-Null
$lines.Add("- Ready to auto-fill acceptance evidence: $($report.Summary.ReadyToAutoFillAcceptanceEvidence)") | Out-Null
$lines.Add("- Ready to claim true LAZ: $($report.Summary.ReadyToClaimTrueLaz)") | Out-Null
$lines.Add("- Next manual action: $($report.Summary.NextManualAction)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Phase Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Phase | Missing checks | First evidence target | First next action |") | Out-Null
$lines.Add("| --- | --- | --- | --- |") | Out-Null
foreach ($phase in $phaseSummaries) {
    $lines.Add(("| {0} | {1} | {2} | {3} |" -f `
        (Convert-ToMarkdownCell $phase.EvidencePhase),
        (Convert-ToMarkdownCell $phase.MissingCheckCount),
        (Convert-ToMarkdownCell $phase.FirstEvidenceTarget),
        (Convert-ToMarkdownCell $phase.FirstNextAction))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Generated Artifacts") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Package: $($report.Summary.PackagePath)") | Out-Null
$lines.Add("- Runbook: $($report.Summary.RunbookPath)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null
Write-TextFile -Path $markdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "LAZ compression gap summary exported."
    Write-Host "JSON: $jsonPath"
    Write-Host "Markdown: $markdownPath"
    Write-Host "Missing acceptance checks: $($report.Summary.MissingAcceptanceCheckCount)"
    Write-Host "Next manual action: $($report.Summary.NextManualAction)"
    Write-Host "Ready to claim true LAZ: $($report.Summary.ReadyToClaimTrueLaz)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
