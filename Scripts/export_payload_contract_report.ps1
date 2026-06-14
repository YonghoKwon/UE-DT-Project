param(
    [string]$FixtureRoot = "",
    [string]$SchemaDocsRoot = "",
    [string]$OutputRoot = "",
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

function Write-MarkdownReport {
    param(
        [object]$FixtureReport,
        [object]$ContractReport,
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
    $lines += "- Contract: $($ContractReport.Contract)"
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
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$fixtureValidator = Join-Path $PSScriptRoot "validate_payload_fixtures.ps1"
$contractValidator = Join-Path $PSScriptRoot "validate_payload_contract.ps1"
$scriptParams = @{
    FixtureRoot = $FixtureRoot
    SchemaDocsRoot = $SchemaDocsRoot
}

$fixtureReport = Invoke-JsonScript -ScriptPath $fixtureValidator -Parameters $scriptParams
$contractReport = Invoke-JsonScript -ScriptPath $contractValidator -Parameters $scriptParams
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
    Summary = [PSCustomObject]@{
        FixtureCount = $fixtureReport.Summary.FixtureCount
        AcceptedPayloadCount = $contractReport.Summary.AcceptedCount
        RejectedPayloadCount = $contractReport.Summary.RejectedCount
        Contract = $contractReport.Contract
        OutputRoot = $OutputRoot
        JsonPath = $jsonPath
        MarkdownPath = $markdownPath
        LatestJsonPath = $latestJsonPath
        LatestMarkdownPath = $latestMarkdownPath
        Valid = ($fixtureReport.Summary.Valid -and $contractReport.Summary.Valid)
    }
}

$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $latestJsonPath -Encoding UTF8
Write-MarkdownReport -FixtureReport $fixtureReport -ContractReport $contractReport -Path $markdownPath -GeneratedUtc $generatedUtc
Write-MarkdownReport -FixtureReport $fixtureReport -ContractReport $contractReport -Path $latestMarkdownPath -GeneratedUtc $generatedUtc

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Payload contract report exported."
    Write-Host "JSON: $jsonPath"
    Write-Host "Markdown: $markdownPath"
    Write-Host "Latest JSON: $latestJsonPath"
    Write-Host "Latest Markdown: $latestMarkdownPath"
}
