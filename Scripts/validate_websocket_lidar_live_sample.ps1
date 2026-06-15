param(
    [string]$ProjectRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-HasField {
    param(
        [object]$Object,
        [string]$FieldName,
        [string]$Context
    )

    Assert-Condition `
        -Condition ($null -ne $Object -and $Object.PSObject.Properties.Name -contains $FieldName) `
        -Message "$Context is missing required field '$FieldName'"
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$samplePath = Join-Path $ProjectRoot "Samples\websocket\lidar_json_live_frame_sample.json"
$handlerCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\WebSocket\TC\LidarJsonLiveFrameTC.cpp"
$handlerHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\WebSocket\TC\LidarJsonLiveFrameTC.h"
$defaultGameIni = Join-Path $ProjectRoot "Config\DefaultGame.ini"
$planDoc = Join-Path $ProjectRoot "docs\real_sensor_adapter_plan.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"

foreach ($path in @($samplePath, $handlerCpp, $handlerHeader, $defaultGameIni, $planDoc, $smokeDoc)) {
    Assert-Condition -Condition (Test-Path -LiteralPath $path -PathType Leaf) -Message "Required file not found: $path"
}

$payload = Get-Content -LiteralPath $samplePath -Raw | ConvertFrom-Json
Assert-HasField -Object $payload -FieldName "MESSAGE_ID" -Context "WebSocket live LiDAR sample"
Assert-HasField -Object $payload -FieldName "DATA_MAP" -Context "WebSocket live LiDAR sample"
Assert-Condition -Condition ($payload.MESSAGE_ID -eq "LIDAR_JSON_LIVE_FRAME") -Message "MESSAGE_ID must be LIDAR_JSON_LIVE_FRAME"

$dataMap = $payload.DATA_MAP
Assert-HasField -Object $dataMap -FieldName "SOURCE_ID" -Context "WebSocket live LiDAR DATA_MAP"
Assert-HasField -Object $dataMap -FieldName "SEND_TRANSPORT" -Context "WebSocket live LiDAR DATA_MAP"
Assert-HasField -Object $dataMap -FieldName "PUSH_FRAME" -Context "WebSocket live LiDAR DATA_MAP"
Assert-HasField -Object $dataMap -FieldName "POINTS" -Context "WebSocket live LiDAR DATA_MAP"
Assert-Condition -Condition (-not [string]::IsNullOrWhiteSpace($dataMap.SOURCE_ID)) -Message "SOURCE_ID must be set for safe multi-source routing"
Assert-Condition -Condition ($dataMap.POINTS.Count -gt 0) -Message "POINTS must contain at least one LiDAR point"

$requiredPointFields = @("row", "col", "returnIndex", "x", "y", "z", "hit", "semanticLabel")
for ($index = 0; $index -lt $dataMap.POINTS.Count; ++$index) {
    $point = $dataMap.POINTS[$index]
    foreach ($field in $requiredPointFields) {
        Assert-HasField -Object $point -FieldName $field -Context "WebSocket live LiDAR point[$index]"
    }
    Assert-Condition -Condition ($point.row -ge 0) -Message "point[$index].row must be non-negative"
    Assert-Condition -Condition ($point.col -ge 0) -Message "point[$index].col must be non-negative"
    Assert-Condition -Condition ($point.returnIndex -ge 0) -Message "point[$index].returnIndex must be non-negative"
    Assert-Condition -Condition (-not [string]::IsNullOrWhiteSpace($point.semanticLabel)) -Message "point[$index].semanticLabel must not be empty"
}

$handlerCppText = Get-Content -LiteralPath $handlerCpp -Raw
$handlerHeaderText = Get-Content -LiteralPath $handlerHeader -Raw
$defaultGameIniText = Get-Content -LiteralPath $defaultGameIni -Raw
$planDocText = Get-Content -LiteralPath $planDoc -Raw
$smokeDocText = Get-Content -LiteralPath $smokeDoc -Raw

Assert-Condition -Condition ($handlerCppText.Contains("LIDAR_JSON_LIVE_FRAME")) -Message "Handler does not declare LIDAR_JSON_LIVE_FRAME"
Assert-Condition -Condition ($handlerCppText.Contains("SOURCE_ID is required")) -Message "Handler does not guard ambiguous SOURCE_ID routing"
Assert-Condition -Condition ($handlerCppText.Contains("TrimStartAndEndInline")) -Message "Handler does not trim JSON_LINES payloads"
Assert-Condition -Condition ($handlerHeaderText.Contains("ULidarJsonLiveFrameTC")) -Message "Handler header is missing ULidarJsonLiveFrameTC"
Assert-Condition -Condition ($defaultGameIniText.Contains("WebSocketDataTable=/Game/M7AT10/Common/DataTables/DT_TransactionCode.DT_TransactionCode")) -Message "Config/DefaultGame.ini does not point DTCore at the project WebSocket transaction data table"
Assert-Condition -Condition ($planDocText.Contains("Samples/websocket/lidar_json_live_frame_sample.json")) -Message "Adapter plan does not reference the WebSocket sample"
Assert-Condition -Condition ($smokeDocText.Contains("validate_websocket_lidar_live_sample.ps1")) -Message "Smoke test doc does not reference the WebSocket sample validator"

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    SamplePath = $samplePath
    TransactionCode = $payload.MESSAGE_ID
    SourceId = $dataMap.SOURCE_ID
    PointCount = $dataMap.POINTS.Count
    SendTransport = [bool]$dataMap.SEND_TRANSPORT
    PushFrame = [bool]$dataMap.PUSH_FRAME
    Summary = [PSCustomObject]@{
        Valid = $true
        SafeSourceRoutingDocumented = $true
        ProjectWebSocketDataTableConfigured = $true
        SamplePointCount = $dataMap.POINTS.Count
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "WebSocket live LiDAR sample is valid."
    Write-Host "Transaction code: $($report.TransactionCode)"
    Write-Host "SourceId: $($report.SourceId)"
    Write-Host "Point count: $($report.PointCount)"
    Write-Host "Sample: $($report.SamplePath)"
}
