param(
    [string]$ProjectRoot = "",
    [string]$OutputRoot = "",
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

function Get-SensitivePatternHits {
    param([string[]]$Paths)

    $patterns = @(
        "Bearer\s+[A-Za-z0-9._\-]+",
        "(?i)(token|password|secret|credential)\s*[:=]\s*[^`r`n\s]+",
        "https?://[^\s)]+"
    )
    $hits = @()
    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            continue
        }
        $text = Get-Content -LiteralPath $path -Raw
        foreach ($pattern in $patterns) {
            $matches = [regex]::Matches($text, $pattern)
            foreach ($match in $matches) {
                $hits += [PSCustomObject]@{
                    Path = $path
                    Pattern = $pattern
                    Match = $match.Value
                }
            }
        }
    }
    return $hits
}

$ProjectRoot = Resolve-ProjectRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\JudgingServerAcceptance"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$payloadReportScript = Join-Path $ProjectRoot "Scripts\export_payload_contract_report.ps1"
$transportValidatorScript = Join-Path $ProjectRoot "Scripts\validate_server_transport_contract.ps1"
$acceptanceTemplateScript = Join-Path $ProjectRoot "Scripts\export_judging_server_acceptance_template.ps1"

$payloadReportRoot = Join-Path $OutputRoot "PayloadContract"
$templateMarkdownPath = Join-Path $OutputRoot "judging_server_acceptance_template.md"
$templateJsonPath = Join-Path $OutputRoot "judging_server_acceptance_template.json"
$transportJsonPath = Join-Path $OutputRoot "server_transport_contract_validation.json"
$manifestJsonPath = Join-Path $OutputRoot "judging_server_acceptance_package.json"
$manifestMarkdownPath = Join-Path $OutputRoot "README.md"

$transportReport = Invoke-JsonScript -ScriptPath $transportValidatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
}
$payloadReport = Invoke-JsonScript -ScriptPath $payloadReportScript -Parameters @{
    OutputRoot = $payloadReportRoot
}
$template = Invoke-JsonScript -ScriptPath $acceptanceTemplateScript -Parameters @{
    ProjectRoot = $ProjectRoot
    OutputPath = $templateMarkdownPath
}

$transportReport | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $transportJsonPath -Encoding UTF8
$template | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $templateJsonPath -Encoding UTF8

$manualSteps = @(
    "Pick the owned judging-server environment outside this repository and keep endpoint URLs and credentials out of the package.",
    "Fill judging_server_acceptance_template.md/json with evidence paths, reviewer, and reviewed UTC values only.",
    "Attach LiDAR and camera accepted-response evidence from the owned endpoint.",
    "Attach rejected-payload, retry/timeout, batching/backpressure, and secret-scan evidence.",
    "Re-run export_judging_server_acceptance_package.ps1 and report_precommit_summary.ps1 after evidence is recorded.",
    "Claim real judging-server acceptance only when CurrentReadyToClaimRealServerAcceptance is true in the accepted evidence process."
)

$followUpCommands = @(
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f $transportValidatorScript, $ProjectRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -OutputRoot "{1}" -Json' -f $payloadReportScript, $payloadReportRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -OutputPath "{2}"' -f $acceptanceTemplateScript, $ProjectRoot, $templateMarkdownPath),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"), $ProjectRoot)
)

$generatedFiles = @(
    $templateMarkdownPath,
    $templateJsonPath,
    $transportJsonPath,
    $manifestJsonPath,
    $manifestMarkdownPath,
    [string]$payloadReport.Summary.LatestJsonPath,
    [string]$payloadReport.Summary.LatestMarkdownPath
)
$sensitiveHits = @(Get-SensitivePatternHits -Paths $generatedFiles)

$manifest = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    OutputRoot = $OutputRoot
    DryRunOnly = $true
    ModifiesConfig = $false
    StagesFiles = $false
    WritesEndpointValues = $false
    WritesCredentialValues = $false
    PayloadContractSummary = $payloadReport.Summary
    ServerTransportContractSummary = $transportReport.Summary
    AcceptanceTemplateSummary = $template.Summary
    ManualSteps = $manualSteps
    FollowUpCommands = $followUpCommands
    GeneratedFiles = $generatedFiles
    SensitivePatternHits = $sensitiveHits
    Summary = [PSCustomObject]@{
        PackageCreated = $true
        OutputRoot = $OutputRoot
        PayloadContractValid = [bool]$payloadReport.Summary.Valid
        ServerTransportContractValid = [bool]$transportReport.Summary.Valid
        AcceptanceTemplateCreated = (Test-Path -LiteralPath $templateMarkdownPath -PathType Leaf)
        RequiredEvidenceCount = [int]$template.Summary.RequiredEvidenceCount
        PendingEvidenceCount = [int]$template.Summary.PendingEvidenceCount
        RealJudgingServerAcceptancePresent = [bool]$template.Summary.RealJudgingServerAcceptancePresent
        CurrentReadyToClaimRealServerAcceptance = [bool]$template.Summary.CurrentReadyToClaimRealServerAcceptance
        SensitivePatternHitCount = $sensitiveHits.Count
        DryRunOnly = $true
        ModifiesConfig = $false
        StagesFiles = $false
        WritesEndpointValues = $false
        WritesCredentialValues = $false
        Boundary = "This package collects local contract and fillable judging-server acceptance evidence. It never writes endpoint or credential values, never modifies config, and never stages files."
        Valid = ([bool]$payloadReport.Summary.Valid -and [bool]$transportReport.Summary.Valid -and [bool]$template.Summary.ValuesRedacted -and $sensitiveHits.Count -eq 0)
    }
}

$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestJsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Judging Server Acceptance Package") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated UTC: $($manifest.GeneratedUtc)") | Out-Null
$lines.Add("- Project: $($manifest.ProjectRoot)") | Out-Null
$lines.Add("- Output root: $($manifest.OutputRoot)") | Out-Null
$lines.Add("- Payload contract valid: $($manifest.Summary.PayloadContractValid)") | Out-Null
$lines.Add("- Server transport contract valid: $($manifest.Summary.ServerTransportContractValid)") | Out-Null
$lines.Add("- Required evidence count: $($manifest.Summary.RequiredEvidenceCount)") | Out-Null
$lines.Add("- Pending evidence count: $($manifest.Summary.PendingEvidenceCount)") | Out-Null
$lines.Add("- Real judging-server acceptance present: $($manifest.Summary.RealJudgingServerAcceptancePresent)") | Out-Null
$lines.Add("- Ready to claim real server acceptance: $($manifest.Summary.CurrentReadyToClaimRealServerAcceptance)") | Out-Null
$lines.Add("- Sensitive pattern hit count: $($manifest.Summary.SensitivePatternHitCount)") | Out-Null
$lines.Add("- Dry run only: $($manifest.DryRunOnly)") | Out-Null
$lines.Add("- Modifies config: $($manifest.ModifiesConfig)") | Out-Null
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
    Write-Host "Judging server acceptance package created."
    Write-Host "Output root: $($manifest.OutputRoot)"
    Write-Host "Payload contract valid: $($manifest.Summary.PayloadContractValid)"
    Write-Host "Server transport contract valid: $($manifest.Summary.ServerTransportContractValid)"
    Write-Host "Pending evidence count: $($manifest.Summary.PendingEvidenceCount)"
    Write-Host "Ready to claim real server acceptance: $($manifest.Summary.CurrentReadyToClaimRealServerAcceptance)"
    Write-Host "Sensitive pattern hit count: $($manifest.Summary.SensitivePatternHitCount)"
}
