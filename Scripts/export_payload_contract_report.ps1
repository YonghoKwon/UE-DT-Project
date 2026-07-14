param(
    [string]$FixtureRoot = "",
    [string]$SchemaDocsRoot = "",
    [string]$OutputRoot = "",
    [switch]$NoWrite,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-ProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath)) {
        throw "Required script not found: $ScriptPath"
    }

    $jsonText = & $ScriptPath @Parameters -Json
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code $LASTEXITCODE"
    }

    return ($jsonText | ConvertFrom-Json)
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
        [object]$FixtureReport,
        [object]$ContractReport,
        [object]$TransportContractReport,
        [object]$JudgingServerAcceptanceTemplate,
        [object[]]$ServerAcceptanceDecisions,
        [object[]]$AcceptanceEvidence,
        [object[]]$RealServerEvidenceGaps,
        [string]$Path,
        [string]$GeneratedUtc
    )

    $lines = @()
    $lines += "# Payload Contract Report"
    $lines += ""
    $lines += "Generated UTC: $GeneratedUtc"
    $lines += ""
    $lines += "## Summary"
    $lines += ""
    $lines += "- Fixture count: $($FixtureReport.Summary.FixtureCount)"
    $lines += "- Accepted payloads: $($ContractReport.Summary.AcceptedCount)"
    $lines += "- Rejected payloads: $($ContractReport.Summary.RejectedCount)"
    $lines += "- Documentation linked: $($FixtureReport.Summary.DocumentationLinked)"
    $lines += "- Transport contract valid: $($TransportContractReport.Summary.Valid)"
    $lines += "- Contract: $($ContractReport.Contract)"
    $lines += "- Judging-server acceptance template available: $($null -ne $JudgingServerAcceptanceTemplate)"
    if ($JudgingServerAcceptanceTemplate) {
        $lines += "- Judging-server required evidence count: $($JudgingServerAcceptanceTemplate.Summary.RequiredEvidenceCount)"
        $lines += "- Real judging-server acceptance present: $($JudgingServerAcceptanceTemplate.Summary.RealJudgingServerAcceptancePresent)"
    }
    $lines += ""
    $lines += "## Fixtures"
    $lines += ""
    foreach ($fixture in $FixtureReport.Fixtures) {
        $lines += "### $($fixture.SensorType)"
        $lines += ""
        $lines += "- Schema version: $($fixture.SchemaVersion)"
        $lines += "- Fixture: $($fixture.Path)"
        $lines += "- Document: $($fixture.DocumentPath)"
        $lines += "- Top-level fields: $($fixture.TopLevelFieldCount)"
        if ($fixture.PSObject.Properties.Name -contains "PointFieldCount") {
            $lines += "- Point fields: $($fixture.PointFieldCount)"
            $lines += "- Payload points: $($fixture.PayloadPointCount)"
        }
        if ($fixture.PSObject.Properties.Name -contains "TransformFieldCount") {
            $lines += "- Transform fields: $($fixture.TransformFieldCount)"
            $lines += "- Encoding: $($fixture.Encoding)"
            $lines += "- Byte size: $($fixture.ByteSize)"
        }
        $lines += "- Documented groups: $(@($fixture.DocumentedFieldGroups) -join ', ')"
        $lines += ""
    }
    $lines += "## Mock Contract Acceptance"
    $lines += ""
    foreach ($payload in $ContractReport.AcceptedPayloads) {
        $lines += "### $($payload.SensorType)"
        $lines += ""
        $lines += "- Schema version: $($payload.SchemaVersion)"
        $lines += "- Sensor id: $($payload.SensorId)"
        $lines += "- Rule set: $($payload.RuleSet)"
        if ($payload.PSObject.Properties.Name -contains "PayloadPointCount") {
            $lines += "- Payload points: $($payload.PayloadPointCount)"
        }
        if ($payload.PSObject.Properties.Name -contains "ByteSize") {
            $lines += "- Byte size: $($payload.ByteSize)"
        }
        $lines += ""
    }
    $lines += "## Server Acceptance Readiness"
    $lines += ""
    $lines += "| Decision | Current status | Required owner/evidence |"
    $lines += "| --- | --- | --- |"
    foreach ($decision in $ServerAcceptanceDecisions) {
        $lines += "| $(Convert-ToMarkdownCell $decision.Name) | $(Convert-ToMarkdownCell $decision.Status) | $(Convert-ToMarkdownCell $decision.RequiredEvidence) |"
    }
    $lines += ""
    $lines += "## Acceptance Evidence"
    $lines += ""
    foreach ($item in $AcceptanceEvidence) {
        $lines += "- $($item.Name): $($item.Status) - $($item.Evidence)"
    }
    $lines += ""
    $lines += "## Real Server Evidence Gaps"
    $lines += ""
    foreach ($gap in $RealServerEvidenceGaps) {
        $lines += "- $($gap.Name): $($gap.RequiredEvidence)"
    }
    $lines += ""
    $lines += "## Remaining Server Decisions"
    $lines += ""
    $lines += "- Replace the local mock contract with the final judging-server schema."
    $lines += "- Confirm transport endpoint, authentication, retry, and batching behavior."
    $lines += "- Capture acceptance evidence from the real judging server before freezing v1."

    $lines | Set-Content -LiteralPath $Path -Encoding UTF8
}

$projectRoot = Get-ProjectRoot
if ([string]::IsNullOrWhiteSpace($FixtureRoot)) {
    $FixtureRoot = Join-Path $projectRoot "Samples\payload_fixtures"
}
if ([string]::IsNullOrWhiteSpace($SchemaDocsRoot)) {
    $SchemaDocsRoot = Join-Path $projectRoot "docs"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $projectRoot "Saved\PayloadContractReports"
}

$FixtureRoot = (Resolve-Path -LiteralPath $FixtureRoot).Path
$SchemaDocsRoot = (Resolve-Path -LiteralPath $SchemaDocsRoot).Path
if (-not $NoWrite) {
    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    $OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
}
elseif (Test-Path -LiteralPath $OutputRoot) {
    $OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
}

$fixtureValidator = Join-Path $PSScriptRoot "validate_payload_fixtures.ps1"
$contractValidator = Join-Path $PSScriptRoot "validate_payload_contract.ps1"
$transportContractValidator = Join-Path $PSScriptRoot "validate_server_transport_contract.ps1"
$judgingServerAcceptanceTemplateScript = Join-Path $PSScriptRoot "export_judging_server_acceptance_template.ps1"
$scriptParams = @{
    FixtureRoot = $FixtureRoot
    SchemaDocsRoot = $SchemaDocsRoot
}

$fixtureReport = Invoke-JsonScript -ScriptPath $fixtureValidator -Parameters $scriptParams
$contractReport = Invoke-JsonScript -ScriptPath $contractValidator -Parameters $scriptParams
$transportContractReport = Invoke-JsonScript -ScriptPath $transportContractValidator -Parameters @{ ProjectRoot = $projectRoot }
$judgingServerAcceptanceTemplate = Invoke-JsonScript -ScriptPath $judgingServerAcceptanceTemplateScript -Parameters @{ ProjectRoot = $projectRoot }
$serverAcceptanceDecisions = @(
    [PSCustomObject]@{
        Name = "Endpoint URL and environment ownership"
        Status = "Open"
        RequiredEvidence = "Judging-server endpoint owner, environment override policy, and no credential leakage in shared config."
    },
    [PSCustomObject]@{
        Name = "Authentication"
        Status = "Open"
        RequiredEvidence = "Header/token scheme, storage policy, rotation owner, and redaction rules."
    },
    [PSCustomObject]@{
        Name = "Retry and timeout policy"
        Status = "Open"
        RequiredEvidence = "Timeout, retry count, retryable status codes, idempotency decision, and failure telemetry."
    },
    [PSCustomObject]@{
        Name = "Batching"
        Status = "Open"
        RequiredEvidence = "Max frame rate, batch size, ordering guarantees, and LiDAR burst behavior."
    },
    [PSCustomObject]@{
        Name = "Backpressure"
        Status = "Open"
        RequiredEvidence = "Queue limit, drop policy, capture throttling behavior, and operator/server telemetry."
    },
    [PSCustomObject]@{
        Name = "Server response schema"
        Status = "Open"
        RequiredEvidence = "Accepted/rejected response body fields, analysis-result schema, and error-code mapping."
    }
)
$acceptanceEvidence = @(
    [PSCustomObject]@{
        Name = "Fixture schema validation"
        Status = if ($fixtureReport.Summary.Valid) { "Complete" } else { "Failing" }
        Evidence = "validate_payload_fixtures.ps1"
    },
    [PSCustomObject]@{
        Name = "Local mock judging contract"
        Status = if ($contractReport.Summary.Valid) { "Complete" } else { "Failing" }
        Evidence = "validate_payload_contract.ps1 using mock-judging-server.v1"
    },
    [PSCustomObject]@{
        Name = "Outbound HTTP loopback"
        Status = "Covered by automation"
        Evidence = "MA0T10.SensorTransport.HttpPostLoopbackAcceptance"
    },
    [PSCustomObject]@{
        Name = "Real judging-server acceptance"
        Status = "Missing"
        Evidence = "Needs accepted response from the owned server endpoint."
    }
)
$realServerEvidenceGaps = @(
    [PSCustomObject]@{
        Name = "Real endpoint acceptance"
        RequiredEvidence = "Owned judging-server endpoint returns the approved success status/body for LiDAR and camera fixtures."
    },
    [PSCustomObject]@{
        Name = "Real endpoint rejection"
        RequiredEvidence = "Owned judging-server endpoint returns the approved error status/body for malformed or rejected payloads."
    },
    [PSCustomObject]@{
        Name = "Auth failure handling"
        RequiredEvidence = "Expired/missing credential behavior is captured and does not mark payloads as accepted."
    },
    [PSCustomObject]@{
        Name = "High-frequency LiDAR behavior"
        RequiredEvidence = "Server or client evidence shows accepted batching/backpressure behavior at target capture rates."
    }
)
$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss")
$jsonPath = Join-Path $OutputRoot "payload_contract_report_$stamp.json"
$markdownPath = Join-Path $OutputRoot "payload_contract_report_$stamp.md"
$latestJsonPath = Join-Path $OutputRoot "payload_contract_report_latest.json"
$latestMarkdownPath = Join-Path $OutputRoot "payload_contract_report_latest.md"

$report = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    FixtureReport = $fixtureReport
    ContractReport = $contractReport
    TransportContractReport = $transportContractReport
    JudgingServerAcceptanceTemplate = $judgingServerAcceptanceTemplate
    ServerAcceptanceDecisions = $serverAcceptanceDecisions
    AcceptanceEvidence = $acceptanceEvidence
    RealServerEvidenceGaps = $realServerEvidenceGaps
    Summary = [PSCustomObject]@{
        FixtureCount = $fixtureReport.Summary.FixtureCount
        AcceptedPayloadCount = $contractReport.Summary.AcceptedCount
        RejectedPayloadCount = $contractReport.Summary.RejectedCount
        TransportContractValid = [bool]$transportContractReport.Summary.Valid
        Contract = $contractReport.Contract
        ServerAcceptanceDecisionCount = $serverAcceptanceDecisions.Count
        AcceptanceEvidenceCount = $acceptanceEvidence.Count
        RealServerEvidenceGapCount = $realServerEvidenceGaps.Count
        JudgingServerAcceptanceTemplateAvailable = ($null -ne $judgingServerAcceptanceTemplate)
        JudgingServerRequiredEvidenceCount = [int]$judgingServerAcceptanceTemplate.Summary.RequiredEvidenceCount
        JudgingServerPendingEvidenceCount = [int]$judgingServerAcceptanceTemplate.Summary.PendingEvidenceCount
        JudgingServerEvidenceSectionCount = [int]$judgingServerAcceptanceTemplate.Summary.EvidenceSectionCount
        JudgingServerRequiredEvidenceSections = @($judgingServerAcceptanceTemplate.Summary.RequiredEvidenceSections)
        JudgingServerTemplateValuesRedacted = [bool]$judgingServerAcceptanceTemplate.Summary.ValuesRedacted
        JudgingServerTemplateStagesConfig = [bool]$judgingServerAcceptanceTemplate.Summary.StagesConfig
        OpenServerAcceptanceDecisionCount = @($serverAcceptanceDecisions | Where-Object { $_.Status -eq "Open" }).Count
        RealJudgingServerAcceptancePresent = $false
        RecommendedNextAction = "Get judging-server owner approval for endpoint/auth/retry/batching/backpressure/response schema, then capture real endpoint acceptance evidence."
        OutputRoot = $OutputRoot
        JsonPath = $jsonPath
        MarkdownPath = $markdownPath
        LatestJsonPath = $latestJsonPath
        LatestMarkdownPath = $latestMarkdownPath
        NoWrite = [bool]$NoWrite
        Valid = ($fixtureReport.Summary.Valid -and $contractReport.Summary.Valid -and $transportContractReport.Summary.Valid)
    }
}

if (-not $NoWrite) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $latestJsonPath -Encoding UTF8
    Write-MarkdownReport -FixtureReport $fixtureReport -ContractReport $contractReport -TransportContractReport $transportContractReport -JudgingServerAcceptanceTemplate $judgingServerAcceptanceTemplate -ServerAcceptanceDecisions $serverAcceptanceDecisions -AcceptanceEvidence $acceptanceEvidence -RealServerEvidenceGaps $realServerEvidenceGaps -Path $markdownPath -GeneratedUtc $generatedUtc
    Write-MarkdownReport -FixtureReport $fixtureReport -ContractReport $contractReport -TransportContractReport $transportContractReport -JudgingServerAcceptanceTemplate $judgingServerAcceptanceTemplate -ServerAcceptanceDecisions $serverAcceptanceDecisions -AcceptanceEvidence $acceptanceEvidence -RealServerEvidenceGaps $realServerEvidenceGaps -Path $latestMarkdownPath -GeneratedUtc $generatedUtc
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    if ($NoWrite) {
        Write-Host "Payload contract report generated in no-write mode."
        Write-Host "No files were written."
    }
    else {
        Write-Host "Payload contract report exported."
        Write-Host "JSON: $jsonPath"
        Write-Host "Markdown: $markdownPath"
        Write-Host "Latest JSON: $latestJsonPath"
        Write-Host "Latest Markdown: $latestMarkdownPath"
    }
}
