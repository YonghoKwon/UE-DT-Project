param(
    [string]$ProjectRoot = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
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

    $jsonText = & $ScriptPath @Parameters -Json
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code $LASTEXITCODE"
    }
    return $jsonText | ConvertFrom-Json
}

function Test-FileContains {
    param(
        [string]$Path,
        [string]$Pattern
    )

    return [bool](Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet)
}

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Write-MarkdownReport {
    param(
        [object]$Report,
        [string]$Path
    )

    $lines = @()
    $lines += "# Real Sensor Adapter Readiness Report"
    $lines += ""
    $lines += "Generated UTC: $($Report.GeneratedUtc)"
    $lines += ""
    $lines += "## Summary"
    $lines += ""
    $lines += "- Static adapter plan valid: $($Report.Summary.StaticAdapterPlanValid)"
    $lines += "- WebSocket sample valid: $($Report.Summary.WebSocketSampleValid)"
    $lines += "- Transaction registration static evidence valid: $($Report.Summary.TransactionRegistrationStaticValid)"
    $lines += "- Broker smoke report can run read-only: $($Report.Summary.BrokerSmokeReadOnlyValid)"
    $lines += "- Headless automation groups documented: $($Report.Summary.HeadlessAutomationGroupCount)"
    $lines += "- Deployment evidence still required: $($Report.Summary.DeploymentEvidenceStillRequired)"
    $lines += "- Real SDK integration still required: $($Report.Summary.RealSdkIntegrationStillRequired)"
    $lines += ""
    $lines += "## Implemented Bridges"
    $lines += ""
    foreach ($bridge in $Report.ImplementedBridges) {
        $lines += "- $($bridge.Name): $($bridge.Evidence)"
    }
    $lines += ""
    $lines += "## Headless Automation"
    $lines += ""
    foreach ($test in $Report.HeadlessAutomationGroups) {
        $lines += "- `$($test.Name)`: $($test.Scope)"
    }
    $lines += ""
    $lines += "## Remaining Decisions"
    $lines += ""
    foreach ($item in $Report.RemainingDecisions) {
        $lines += "- $item"
    }

    Write-TextFile -Path $Path -Lines $lines
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$adapterPlanValidator = Join-Path $scriptRoot "validate_real_sensor_adapter_plan.ps1"
$sampleValidator = Join-Path $scriptRoot "validate_websocket_lidar_live_sample.ps1"
$registrationReportScript = Join-Path $scriptRoot "export_websocket_transaction_registration_report.ps1"
$brokerSmokeReportScript = Join-Path $scriptRoot "export_websocket_broker_smoke_report.ps1"

$planDoc = Join-Path $ProjectRoot "docs\real_sensor_adapter_plan.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"
$testsCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\RealSensorSourceAutomationTests.cpp"

foreach ($file in @($planDoc, $smokeDoc, $testsCpp)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required file not found: $file"
    }
}

$adapterPlan = Invoke-JsonScript -ScriptPath $adapterPlanValidator -Parameters @{ ProjectRoot = $ProjectRoot }
$sample = Invoke-JsonScript -ScriptPath $sampleValidator -Parameters @{ ProjectRoot = $ProjectRoot }
$registration = Invoke-JsonScript -ScriptPath $registrationReportScript -Parameters @{ ProjectRoot = $ProjectRoot; NoWrite = $true }
$brokerSmoke = Invoke-JsonScript -ScriptPath $brokerSmokeReportScript -Parameters @{ ProjectRoot = $ProjectRoot; NoWrite = $true }

$headlessAutomationGroups = @(
    [PSCustomObject]@{ Name = "M7AT10.RealSensorSource"; Scope = "component-level replay, JSON live, camera JSON live, HTTP loopback, UDP datagram, WebSocket routing, and brokerless DTCore dispatch coverage" },
    [PSCustomObject]@{ Name = "M7AT10.Evidence.WebSocketTransactionRegistration"; Scope = "optional binary data-table row evidence for LIDAR_JSON_LIVE_FRAME" },
    [PSCustomObject]@{ Name = "M7AT10.RealSensorSource.JsonLiveDTCoreDispatch"; Scope = "brokerless DTCore dispatch path through UDxDataSubsystem" }
)

$implementedBridges = @(
    [PSCustomObject]@{ Name = "CSV/JSONL replay"; Evidence = "Feeds saved LiDAR frames into URealSensorSourceComp::PushPointFrameToTarget." },
    [PSCustomObject]@{ Name = "LiDAR JSON live"; Evidence = "Shared AppendLivePayloadJson path covers WebSocket, HTTP, UDP, and Blueprint-fed frames." },
    [PSCustomObject]@{ Name = "Camera JSON live"; Evidence = "InjectExternalJsonPayload validates virtual-camera.v1 payloads without SceneCapture readback." },
    [PSCustomObject]@{ Name = "HTTP JSON live"; Evidence = "Loopback POST automation covers a real local HTTP request path." },
    [PSCustomObject]@{ Name = "UDP JSON live"; Evidence = "Datagram automation covers a real local UDP packet path." },
    [PSCustomObject]@{ Name = "DTCore WebSocket transaction"; Evidence = "Static registration report plus optional data-table evidence automation cover the handler row." }
)

$remainingDecisions = @(
    "Choose production input path priority among SDK, ROS2, WebSocket, HTTP, UDP, and replay.",
    "Connect actual ROS2/Livox/RealSense SDKs or bridge processes and capture real-frame smoke evidence.",
    "Run deployment broker smoke against the real STOMP/WebSocket endpoint with credentials and topic.",
    "Approve network exposure, bind addresses, ports, authentication, and rate limits for HTTP/UDP bridges.",
    "Decide whether adapter abstractions stay in DT-Project or move to DTCore after the schema stabilizes."
)

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$report = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    ProjectRoot = $ProjectRoot
    SourceReports = [PSCustomObject]@{
        AdapterPlan = $adapterPlan.Summary
        WebSocketSample = $sample.Summary
        TransactionRegistration = $registration.Summary
        BrokerSmoke = $brokerSmoke.Summary
    }
    ImplementedBridges = $implementedBridges
    HeadlessAutomationGroups = $headlessAutomationGroups
    RemainingDecisions = $remainingDecisions
    Summary = [PSCustomObject]@{
        StaticAdapterPlanValid = [bool]$adapterPlan.Summary.Valid
        WebSocketSampleValid = [bool]$sample.Summary.Valid
        TransactionRegistrationStaticValid = [bool]$registration.Summary.Valid
        BrokerSmokeReadOnlyValid = [bool]$brokerSmoke.Summary.Valid
        HeadlessAutomationGroupCount = $headlessAutomationGroups.Count
        DeploymentEvidenceStillRequired = $true
        RealSdkIntegrationStillRequired = $true
        PlanDocumentsPriority = (Test-FileContains -Path $planDoc -Pattern "Adapter Priority")
        SmokeDocNamesRealSensorAutomation = (Test-FileContains -Path $smokeDoc -Pattern "Automation RunTests M7AT10.RealSensorSource")
        Valid = ([bool]$adapterPlan.Summary.Valid -and [bool]$sample.Summary.Valid -and [bool]$registration.Summary.Valid -and [bool]$brokerSmoke.Summary.Valid)
    }
}

if (-not $report.Summary.Valid) {
    throw "Real sensor adapter readiness report failed. Static=$($report.Summary.StaticAdapterPlanValid) Sample=$($report.Summary.WebSocketSampleValid) Registration=$($report.Summary.TransactionRegistrationStaticValid) BrokerSmoke=$($report.Summary.BrokerSmokeReadOnlyValid)"
}

if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}
if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-MarkdownReport -Report $report -Path $MarkdownPath
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Real sensor adapter readiness report is ready."
    Write-Host "Static adapter plan valid: $($report.Summary.StaticAdapterPlanValid)"
    Write-Host "WebSocket sample valid: $($report.Summary.WebSocketSampleValid)"
    Write-Host "Transaction registration static evidence valid: $($report.Summary.TransactionRegistrationStaticValid)"
    Write-Host "Broker smoke read-only report valid: $($report.Summary.BrokerSmokeReadOnlyValid)"
    Write-Host "Headless automation groups: $($report.Summary.HeadlessAutomationGroupCount)"
    Write-Host "Deployment evidence still required: $($report.Summary.DeploymentEvidenceStillRequired)"
    Write-Host "Real SDK integration still required: $($report.Summary.RealSdkIntegrationStillRequired)"
}
