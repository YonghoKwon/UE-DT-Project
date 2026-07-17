param(
    [string]$ProjectRoot = "",
    [string]$OutputRoot = "",
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

function Write-MarkdownReport {
    param(
        [object]$Report,
        [string]$Path
    )

    $lines = @()
    $lines += "# WebSocket Transaction Registration Report"
    $lines += ""
    $lines += "Generated UTC: $($Report.GeneratedUtc)"
    $lines += ""
    $lines += "## Expected DTCore Data Table Row"
    $lines += ""
    $lines += "- Data table: ``$($Report.ExpectedRegistration.DataTablePath)``"
    $lines += "- Row / TransactionCodeName: ``$($Report.ExpectedRegistration.TransactionCodeName)``"
    $lines += "- TransactionCodeMessageClass: ``$($Report.ExpectedRegistration.TransactionCodeMessageClass)``"
    $lines += "- Runtime handler class: ``$($Report.ExpectedRegistration.RuntimeHandlerClass)``"
    $lines += "- Optional evidence automation: ``$($Report.ExpectedRegistration.OptionalAutomationTestName)``"
    $lines += ""
    $lines += "## Static Evidence"
    $lines += ""
    $lines += "- Config points to expected table: $($Report.Summary.ConfigPointsToExpectedTable)"
    $lines += "- Handler class exists: $($Report.Summary.HandlerClassExists)"
    $lines += "- Sample payload valid: $($Report.Summary.SamplePayloadValid)"
    $lines += "- Safe SOURCE_ID routing documented: $($Report.Summary.SafeSourceRoutingDocumented)"
    $lines += ""
    $lines += "## Sample Payload"
    $lines += ""
    $lines += "- Path: ``$($Report.Sample.PayloadPath)``"
    $lines += "- MESSAGE_ID: ``$($Report.Sample.MessageId)``"
    $lines += "- SOURCE_ID: ``$($Report.Sample.SourceId)``"
    $lines += "- Point count: $($Report.Sample.PointCount)"
    $lines += "- PUSH_FRAME: $($Report.Sample.PushFrame)"
    $lines += "- SEND_TRANSPORT: $($Report.Sample.SendTransport)"
    $lines += ""
    $lines += "## Manual PIE Smoke Steps"
    $lines += ""
    foreach ($step in $Report.ManualSmokeSteps) {
        $lines += "$($step.Order). $($step.Text)"
    }
    $lines += ""
    $lines += "## Limitation"
    $lines += ""
    $lines += "This report does not edit or deserialize the binary `DT_TransactionCode.uasset`. Treat it as static registration evidence and a checklist until the row is verified in Unreal Editor or by an editor utility."
    $lines += ""
    $lines += "After adding the row, run the optional automation test listed above to verify the configured data table loads and resolves to the expected handler class."

    $lines | Set-Content -LiteralPath $Path -Encoding UTF8
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\WebSocketTransactionRegistration"
}
if ($NoWrite) {
    $OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
}
else {
    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    $OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
}

$defaultGameIni = Join-Path $ProjectRoot "Config\DefaultGame.ini"
$handlerHeader = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\WebSocket\TC\LidarJsonLiveFrameTC.h"
$handlerCpp = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\WebSocket\TC\LidarJsonLiveFrameTC.cpp"
$samplePath = Join-Path $ProjectRoot "Samples\websocket\lidar_json_live_frame_sample.json"
$sampleValidator = Join-Path $ProjectRoot "Scripts\validate_websocket_lidar_live_sample.ps1"
$planDoc = Join-Path $ProjectRoot "docs\real_sensor_adapter_plan.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"

foreach ($file in @(
    [PSCustomObject]@{ Path = $defaultGameIni; Label = "DefaultGame.ini" },
    [PSCustomObject]@{ Path = $handlerHeader; Label = "WebSocket handler header" },
    [PSCustomObject]@{ Path = $handlerCpp; Label = "WebSocket handler implementation" },
    [PSCustomObject]@{ Path = $samplePath; Label = "WebSocket sample payload" },
    [PSCustomObject]@{ Path = $sampleValidator; Label = "WebSocket sample validator" },
    [PSCustomObject]@{ Path = $planDoc; Label = "Real sensor adapter plan" },
    [PSCustomObject]@{ Path = $smokeDoc; Label = "Editor smoke test doc" }
)) {
    Assert-FileExists -Path $file.Path -Label $file.Label
}

$sampleReportJson = & $sampleValidator -ProjectRoot $ProjectRoot -Json
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "WebSocket sample validator failed with exit code $LASTEXITCODE"
}
$sampleReport = $sampleReportJson | ConvertFrom-Json

$configuredTablePath = Get-IniValue -Path $defaultGameIni -Key "WebSocketDataTable"
$expectedTablePath = "/Game/MA0T10/Common/DataTables/DT_TransactionCode.DT_TransactionCode"
$transactionCodeName = "LIDAR_JSON_LIVE_FRAME"
$messageClass = "/Script/ma0t10_dt.LidarJsonLiveFrameTC"
$runtimeHandlerClass = "ULidarJsonLiveFrameTC"
$optionalAutomationTestName = "MA0T10.Evidence.WebSocketTransactionRegistration"

$handlerHeaderText = Get-Content -LiteralPath $handlerHeader -Raw
$handlerCppText = Get-Content -LiteralPath $handlerCpp -Raw
$planDocText = Get-Content -LiteralPath $planDoc -Raw
$smokeDocText = Get-Content -LiteralPath $smokeDoc -Raw

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss")
$jsonPath = Join-Path $OutputRoot "websocket_transaction_registration_$stamp.json"
$markdownPath = Join-Path $OutputRoot "websocket_transaction_registration_$stamp.md"
$latestJsonPath = Join-Path $OutputRoot "websocket_transaction_registration_latest.json"
$latestMarkdownPath = Join-Path $OutputRoot "websocket_transaction_registration_latest.md"

$report = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    ProjectRoot = $ProjectRoot
    ExpectedRegistration = [PSCustomObject]@{
        DataTablePath = $expectedTablePath
        TransactionCodeName = $transactionCodeName
        TransactionCodeMessageClass = $messageClass
        RuntimeHandlerClass = $runtimeHandlerClass
        OptionalAutomationTestName = $optionalAutomationTestName
    }
    Config = [PSCustomObject]@{
        DefaultGameIniPath = $defaultGameIni
        ConfiguredWebSocketDataTable = $configuredTablePath
    }
    Sample = [PSCustomObject]@{
        PayloadPath = $samplePath
        MessageId = $sampleReport.TransactionCode
        SourceId = $sampleReport.SourceId
        PointCount = $sampleReport.PointCount
        SendTransport = $sampleReport.SendTransport
        PushFrame = $sampleReport.PushFrame
    }
    ManualSmokeSteps = @(
        [PSCustomObject]@{ Order = 1; Text = "Open `/Game/MA0T10/Common/DataTables/DT_TransactionCode` in Unreal Editor." },
        [PSCustomObject]@{ Order = 2; Text = "Add or confirm a row named `LIDAR_JSON_LIVE_FRAME`." },
        [PSCustomObject]@{ Order = 3; Text = "Set `TransactionCodeName` to `LIDAR_JSON_LIVE_FRAME` and `TransactionCodeMessageClass` to `/Script/ma0t10_dt.LidarJsonLiveFrameTC`." },
        [PSCustomObject]@{ Order = 4; Text = "Place a `ULidarJsonLiveSourceComponent` with `SourceId = JsonLiveLidarBridge` and a target `UVirtualLidarScanComponent`." },
        [PSCustomObject]@{ Order = 5; Text = "Send `Samples/websocket/lidar_json_live_frame_sample.json` through the deployment WebSocket broker in PIE." },
        [PSCustomObject]@{ Order = 6; Text = "Confirm source frame/point counts, target LiDAR cached server payload, and optional transport result." }
    )
    Output = [PSCustomObject]@{
        OutputRoot = $OutputRoot
        JsonPath = $jsonPath
        MarkdownPath = $markdownPath
        LatestJsonPath = $latestJsonPath
        LatestMarkdownPath = $latestMarkdownPath
    }
    Summary = [PSCustomObject]@{
        Valid = $true
        ConfigPointsToExpectedTable = ($configuredTablePath -eq $expectedTablePath)
        HandlerClassExists = ($handlerHeaderText.Contains($runtimeHandlerClass) -and $handlerCppText.Contains($transactionCodeName))
        SamplePayloadValid = [bool]$sampleReport.Summary.Valid
        SafeSourceRoutingDocumented = ($handlerCppText.Contains("SOURCE_ID is required") -and $planDocText.Contains("SOURCE_ID") -and $smokeDocText.Contains("LIDAR_JSON_LIVE_FRAME"))
        BinaryDataTableRowVerified = $false
        BinaryDataTableRowVerificationNote = "Static script does not modify or inspect DT_TransactionCode.uasset rows; verify this row in Unreal Editor, then run MA0T10.Evidence.WebSocketTransactionRegistration."
    }
}

if (-not $report.Summary.ConfigPointsToExpectedTable) {
    throw "Config/DefaultGame.ini WebSocketDataTable is '$configuredTablePath', expected '$expectedTablePath'"
}
if (-not $report.Summary.HandlerClassExists) {
    throw "ULidarJsonLiveFrameTC handler evidence is incomplete."
}
if (-not $report.Summary.SamplePayloadValid) {
    throw "WebSocket live LiDAR sample is invalid."
}
if (-not $report.Summary.SafeSourceRoutingDocumented) {
    throw "Safe SOURCE_ID routing or smoke documentation is incomplete."
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
        Write-Host "WebSocket transaction registration report checked without writing files."
    }
    else {
        Write-Host "WebSocket transaction registration report exported."
    }
    Write-Host "Expected row: $transactionCodeName"
    Write-Host "Expected class: $messageClass"
    Write-Host "Configured table: $configuredTablePath"
    if (-not $NoWrite) {
        Write-Host "JSON: $jsonPath"
        Write-Host "Markdown: $markdownPath"
        Write-Host "Latest JSON: $latestJsonPath"
        Write-Host "Latest Markdown: $latestMarkdownPath"
    }
}
