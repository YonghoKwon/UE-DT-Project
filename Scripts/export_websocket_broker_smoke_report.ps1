param(
    [string]$ProjectRoot = "",
    [string]$OutputRoot = "",
    [string]$BrokerUrl = "",
    [string]$Topic = "",
    [string]$SourceId = "JsonLiveLidarBridge",
    [string]$Operator = "",
    [string]$Notes = "",
    [switch]$ObservedSourceFrame,
    [switch]$ObservedTargetPoints,
    [switch]$ObservedCachedPayload,
    [switch]$ObservedTransportResult,
    [switch]$NoWrite,
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

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    Assert-FileExists -Path $ScriptPath -Label "Required script"
    $jsonText = & $ScriptPath @Parameters -Json
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code $LASTEXITCODE"
    }
    return $jsonText | ConvertFrom-Json
}

function Get-IniValue {
    param(
        [string]$Path,
        [string]$Key
    )

    $line = Select-String -LiteralPath $Path -Pattern "^$([regex]::Escape($Key))=" | Select-Object -First 1
    if ($null -eq $line) {
        return ""
    }

    return ($line.Line -replace "^$([regex]::Escape($Key))=", "").Trim()
}

function Get-IniArrayValues {
    param(
        [string]$Path,
        [string]$Key
    )

    $values = @()
    $pattern = "^\+$([regex]::Escape($Key))="
    foreach ($line in Select-String -LiteralPath $Path -Pattern $pattern) {
        $value = ($line.Line -replace $pattern, "").Trim()
        if ($value.StartsWith('"') -and $value.EndsWith('"')) {
            $value = $value.Substring(1, $value.Length - 2)
        }
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $values += $value
        }
    }
    return $values
}

function Write-MarkdownReport {
    param(
        [object]$Report,
        [string]$Path
    )

    $lines = @()
    $lines += "# WebSocket Broker Smoke Report"
    $lines += ""
    $lines += "Generated UTC: $($Report.GeneratedUtc)"
    $lines += ""
    $lines += "## Scope"
    $lines += ""
    $lines += "This report records deployment STOMP/WebSocket smoke evidence. It does not connect to the broker by itself."
    $lines += ""
    $lines += "## Broker Inputs"
    $lines += ""
    $lines += "- Broker URL: ``$($Report.Broker.BrokerUrl)``"
    $lines += "- Topic: ``$($Report.Broker.Topic)``"
    $lines += "- SourceId: ``$($Report.Broker.SourceId)``"
    $lines += "- Operator: ``$($Report.Broker.Operator)``"
    $lines += ""
    $lines += "## Preconditions"
    $lines += ""
    $lines += "- Sample payload valid: $($Report.Preconditions.SamplePayloadValid)"
    $lines += "- Data-table evidence automation: $($Report.Preconditions.DataTableEvidenceAutomation)"
    $lines += "- Configured data table: ``$($Report.Preconditions.ConfiguredDataTable)``"
    $lines += "- Config topics: $(@($Report.Preconditions.ConfiguredTopics) -join ', ')"
    $lines += ""
    $lines += "## Operator Observations"
    $lines += ""
    $lines += "- Source frame updated: $($Report.Observed.SourceFrame)"
    $lines += "- Target points updated: $($Report.Observed.TargetPoints)"
    $lines += "- Cached payload updated: $($Report.Observed.CachedPayload)"
    $lines += "- Transport result observed: $($Report.Observed.TransportResult)"
    $lines += ""
    $lines += "## Manual Smoke Steps"
    $lines += ""
    foreach ($step in $Report.ManualSmokeSteps) {
        $lines += "$($step.Order). $($step.Text)"
    }
    $lines += ""
    $lines += "## Notes"
    $lines += ""
    $lines += $Report.Notes

    $lines | Set-Content -LiteralPath $Path -Encoding UTF8
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\WebSocketBrokerSmoke"
}
if ($NoWrite) {
    $OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
}
else {
    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    $OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
}

$defaultGameIni = Join-Path $ProjectRoot "Config\DefaultGame.ini"
$samplePath = Join-Path $ProjectRoot "Samples\websocket\lidar_json_live_frame_sample.json"
$sampleValidator = Join-Path $ProjectRoot "Scripts\validate_websocket_lidar_live_sample.ps1"
$registrationReport = Join-Path $ProjectRoot "Scripts\export_websocket_transaction_registration_report.ps1"
$planDoc = Join-Path $ProjectRoot "docs\real_sensor_adapter_plan.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"

foreach ($file in @(
    [PSCustomObject]@{ Path = $defaultGameIni; Label = "DefaultGame.ini" },
    [PSCustomObject]@{ Path = $samplePath; Label = "WebSocket sample payload" },
    [PSCustomObject]@{ Path = $sampleValidator; Label = "WebSocket sample validator" },
    [PSCustomObject]@{ Path = $registrationReport; Label = "WebSocket transaction registration report" },
    [PSCustomObject]@{ Path = $planDoc; Label = "Real sensor adapter plan" },
    [PSCustomObject]@{ Path = $smokeDoc; Label = "Editor smoke test doc" }
)) {
    Assert-FileExists -Path $file.Path -Label $file.Label
}

$sampleReport = Invoke-JsonScript -ScriptPath $sampleValidator -Parameters @{ ProjectRoot = $ProjectRoot }
$registration = Invoke-JsonScript -ScriptPath $registrationReport -Parameters @{ ProjectRoot = $ProjectRoot; NoWrite = $true }

$configuredTopics = @(Get-IniArrayValues -Path $defaultGameIni -Key "WebSocketTopics")
if ($configuredTopics.Count -eq 0) {
    $configuredTopics = @("topic.cep.output.0")
}

if ([string]::IsNullOrWhiteSpace($Topic)) {
    $Topic = $configuredTopics[0]
}

$configuredBrokerUrl = Get-IniValue -Path $defaultGameIni -Key "WebSocketUrl"
if ([string]::IsNullOrWhiteSpace($BrokerUrl)) {
    $BrokerUrl = $configuredBrokerUrl
}
if ([string]::IsNullOrWhiteSpace($BrokerUrl)) {
    $BrokerUrl = "ws://localhost:61616"
}

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss")
$jsonPath = Join-Path $OutputRoot "websocket_broker_smoke_$stamp.json"
$markdownPath = Join-Path $OutputRoot "websocket_broker_smoke_$stamp.md"
$latestJsonPath = Join-Path $OutputRoot "websocket_broker_smoke_latest.json"
$latestMarkdownPath = Join-Path $OutputRoot "websocket_broker_smoke_latest.md"

$report = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    ProjectRoot = $ProjectRoot
    Broker = [PSCustomObject]@{
        BrokerUrl = $BrokerUrl
        Topic = $Topic
        SourceId = $SourceId
        Operator = $Operator
    }
    Preconditions = [PSCustomObject]@{
        SamplePayloadValid = [bool]$sampleReport.Summary.Valid
        SamplePayloadPath = $samplePath
        SamplePointCount = $sampleReport.PointCount
        DataTableEvidenceAutomation = $registration.ExpectedRegistration.OptionalAutomationTestName
        ConfiguredDataTable = $registration.Config.ConfiguredWebSocketDataTable
        ConfiguredTopics = $configuredTopics
    }
    Observed = [PSCustomObject]@{
        SourceFrame = [bool]$ObservedSourceFrame
        TargetPoints = [bool]$ObservedTargetPoints
        CachedPayload = [bool]$ObservedCachedPayload
        TransportResult = [bool]$ObservedTransportResult
        Complete = ([bool]$ObservedSourceFrame -and [bool]$ObservedTargetPoints -and [bool]$ObservedCachedPayload)
    }
    ManualSmokeSteps = @(
        [PSCustomObject]@{ Order = 1; Text = "Run M7AT10.Evidence.WebSocketTransactionRegistration against the same project build." },
        [PSCustomObject]@{ Order = 2; Text = "Open PIE with a ULidarJsonLiveSourceComp whose SourceId is '$SourceId' and whose TargetLidar is set." },
        [PSCustomObject]@{ Order = 3; Text = "Send '$samplePath' to topic '$Topic' on broker '$BrokerUrl'." },
        [PSCustomObject]@{ Order = 4; Text = "Confirm the JSON live source frame count changes." },
        [PSCustomObject]@{ Order = 5; Text = "Confirm target LiDAR point count and cached server payload update." },
        [PSCustomObject]@{ Order = 6; Text = "If SEND_TRANSPORT is enabled, capture the transport result or saved payload path." }
    )
    Output = [PSCustomObject]@{
        OutputRoot = $OutputRoot
        JsonPath = $jsonPath
        MarkdownPath = $markdownPath
        LatestJsonPath = $latestJsonPath
        LatestMarkdownPath = $latestMarkdownPath
    }
    Notes = $Notes
    Summary = [PSCustomObject]@{
        Valid = $true
        NoWrite = [bool]$NoWrite
        BrokerSmokeComplete = ([bool]$ObservedSourceFrame -and [bool]$ObservedTargetPoints -and [bool]$ObservedCachedPayload)
        RequiresExternalBroker = $true
        DoesNotConnectToBroker = $true
    }
}

if (-not $report.Preconditions.SamplePayloadValid) {
    throw "WebSocket sample payload is not valid."
}
if ($configuredTopics -notcontains $Topic) {
    throw "Topic '$Topic' is not listed in Config/DefaultGame.ini WebSocketTopics: $($configuredTopics -join ', ')"
}

if (-not $NoWrite) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $latestJsonPath -Encoding UTF8
    Write-MarkdownReport -Report $report -Path $markdownPath
    Write-MarkdownReport -Report $report -Path $latestMarkdownPath
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    if ($NoWrite) {
        Write-Host "WebSocket broker smoke report checked without writing files."
    }
    else {
        Write-Host "WebSocket broker smoke report exported."
    }
    Write-Host "Broker URL: $BrokerUrl"
    Write-Host "Topic: $Topic"
    Write-Host "SourceId: $SourceId"
    Write-Host "Observed source frame: $([bool]$ObservedSourceFrame)"
    Write-Host "Observed target points: $([bool]$ObservedTargetPoints)"
    Write-Host "Observed cached payload: $([bool]$ObservedCachedPayload)"
    if (-not $NoWrite) {
        Write-Host "JSON: $jsonPath"
        Write-Host "Markdown: $markdownPath"
        Write-Host "Latest JSON: $latestJsonPath"
        Write-Host "Latest Markdown: $latestMarkdownPath"
    }
}
