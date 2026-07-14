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
$decisionScript = Join-Path $ProjectRoot "Scripts\export_laz_compression_decision_report.ps1"
$policyScript = Join-Path $ProjectRoot "Scripts\validate_laz_placeholder_policy.ps1"
$readinessScript = Join-Path $ProjectRoot "Scripts\export_laz_compressor_readiness_report.ps1"
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

$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    EvidencePath = $evidencePath
}

$jsonPath = Join-Path $OutputRoot "laz_compression_acceptance_runbook.json"
$markdownPath = Join-Path $OutputRoot "laz_compression_acceptance_runbook.md"
$commands = @(
    [PSCustomObject]@{ Step = 1; Name = "Refresh LAZ acceptance package"; Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}"' -f $packageScript, $ProjectRoot) },
    [PSCustomObject]@{ Step = 2; Name = "Confirm placeholder boundary"; Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f $policyScript, $ProjectRoot) },
    [PSCustomObject]@{ Step = 3; Name = "Review LAZ implementation options"; Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f $decisionScript, $ProjectRoot) },
    [PSCustomObject]@{ Step = 4; Name = "Discover compressor and reader readiness"; Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -CompressorPath "<compressor.exe>" -ReaderPath "<lasinfo-or-pdal.exe>" -Json' -f $readinessScript, $ProjectRoot) },
    [PSCustomObject]@{ Step = 5; Name = "Produce real LAZ"; Command = "Produce a non-empty .laz output from the accepted compressor/native/server workflow. The current *_laz_source_*.las placeholder is not proof." },
    [PSCustomObject]@{ Step = 6; Name = "Run known-reader probe"; Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -CompressorPath "<compressor.exe>" -ReaderPath "<lasinfo-or-pdal.exe>" -LazEvidencePath "<output.laz>" -RunReaderProbe -Json' -f $readinessScript, $ProjectRoot) },
    [PSCustomObject]@{ Step = 7; Name = "Fill acceptance evidence"; Command = "Copy EvidenceCopyHints from laz_compression_acceptance_package.json into laz_compression_acceptance.evidence.json, including compressor selection, produced LAZ evidence, known reader validation, placeholder distinction, repeatable command, and owner acceptance." },
    [PSCustomObject]@{ Step = 8; Name = "Validate LAZ acceptance evidence"; Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -EvidencePath "{2}" -Json' -f $validatorScript, $ProjectRoot, $evidencePath) },
    [PSCustomObject]@{ Step = 9; Name = "Strict gate before claiming true LAZ"; Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -EvidencePath "{2}" -FailOnIncompleteEvidence' -f $validatorScript, $ProjectRoot, $evidencePath) }
)

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
    ValidationSummary = $validation.Summary
    Commands = $commands
    Summary = [PSCustomObject]@{
        RunbookCreated = $true
        CommandCount = $commands.Count
        AcceptanceEvidenceComplete = [bool]$validation.Summary.Complete
        AcceptanceEvidenceMissingCount = [int]$validation.Summary.FailedCheckCount
        ReadyToClaimTrueLaz = [bool]$package.Summary.ReadyToClaimTrueLaz
        ReadyToClaimTrueLazBlockers = @($package.Summary.ReadyToClaimTrueLazBlockers)
        ReadyToAutoFillAcceptanceEvidence = [bool]$package.Summary.ReadyToAutoFillAcceptanceEvidence
        Boundary = "This runbook is a Saved/Reports guide for LAZ acceptance. It does not install tools, run compressors, write LAZ output, modify assets, or stage files."
        Valid = $true
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# LAZ Compression Acceptance Runbook") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated UTC: $($report.GeneratedUtc)") | Out-Null
$lines.Add("- Evidence file: $($report.EvidencePath)") | Out-Null
$lines.Add("- Acceptance evidence complete: $($report.Summary.AcceptanceEvidenceComplete)") | Out-Null
$lines.Add("- Missing acceptance check count: $($report.Summary.AcceptanceEvidenceMissingCount)") | Out-Null
$lines.Add("- Ready to claim true LAZ: $($report.Summary.ReadyToClaimTrueLaz)") | Out-Null
$lines.Add("- Ready to auto-fill acceptance evidence: $($report.Summary.ReadyToAutoFillAcceptanceEvidence)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Execution Steps") | Out-Null
$lines.Add("") | Out-Null
foreach ($command in $commands) {
    $lines.Add("### $($command.Step). $($command.Name)") | Out-Null
    $lines.Add("") | Out-Null
    if ($command.Command -match "^powershell ") {
        $lines.Add('```powershell') | Out-Null
        $lines.Add($command.Command) | Out-Null
        $lines.Add('```') | Out-Null
    }
    else {
        $lines.Add($command.Command) | Out-Null
    }
    $lines.Add("") | Out-Null
}
$lines.Add("## True LAZ Blockers") | Out-Null
$lines.Add("") | Out-Null
foreach ($blocker in @($report.Summary.ReadyToClaimTrueLazBlockers)) {
    $lines.Add("- $blocker") | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null
Write-TextFile -Path $markdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "LAZ compression acceptance runbook exported."
    Write-Host "JSON: $jsonPath"
    Write-Host "Markdown: $markdownPath"
    Write-Host "Command count: $($report.Summary.CommandCount)"
    Write-Host "Ready to claim true LAZ: $($report.Summary.ReadyToClaimTrueLaz)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
