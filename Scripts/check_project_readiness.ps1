param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$ProjectPath = "",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [string[]]$TestGroups = @(
        "M7AT10.EditorSmoke",
        "M7AT10.SensorReplay",
        "M7AT10.SensorRecorder",
        "M7AT10.RealSensorSource",
        "M7AT10.SensorManager",
        "M7AT10.SensorMonitor"
    ),
    [switch]$SkipBuild,
    [switch]$SkipSmoke,
    [switch]$SkipPayloadFixtures,
    [switch]$SkipPayloadContract,
    [switch]$SkipPayloadSchemaReviewPolicy,
    [switch]$SkipServerTransportContract,
    [switch]$SkipRealSensorAdapterPlan,
    [switch]$SkipWebSocketLidarLiveSample,
    [switch]$SkipWebSocketTransactionRegistrationReport,
    [switch]$SkipPointCloudPreviewPolicy,
    [switch]$SkipLazPlaceholderPolicy,
    [switch]$SkipMonitorWidgetPolicy,
    [switch]$SkipRuntimeConfigPolicy,
    [switch]$SkipLargeContentDecisionPolicy,
    [switch]$FailOnLargeContentCandidates,
    [switch]$AllowOpenEditor,
    [switch]$FailOnGeneratedOutput,
    [switch]$FailOnStagedDecisionPoints,
    [string[]]$FailOnCategory = @()
)

$ErrorActionPreference = "Stop"

function Invoke-ScriptStep {
    param(
        [string]$Label,
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host $ScriptPath

    & $ScriptPath @Parameters
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $ProjectRoot "m7at10_dt.uproject"
}
if (-not (Test-Path -LiteralPath $ProjectPath)) {
    throw "Project file not found: $ProjectPath"
}

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$AssetReportScript = Join-Path $ScriptRoot "report_local_project_status.ps1"
$PayloadFixtureScript = Join-Path $ScriptRoot "validate_payload_fixtures.ps1"
$PayloadContractScript = Join-Path $ScriptRoot "validate_payload_contract.ps1"
$PayloadSchemaReviewPolicyScript = Join-Path $ScriptRoot "validate_payload_schema_review_policy.ps1"
$ServerTransportContractScript = Join-Path $ScriptRoot "validate_server_transport_contract.ps1"
$RealSensorAdapterPlanScript = Join-Path $ScriptRoot "validate_real_sensor_adapter_plan.ps1"
$WebSocketLidarLiveSampleScript = Join-Path $ScriptRoot "validate_websocket_lidar_live_sample.ps1"
$WebSocketTransactionRegistrationReportScript = Join-Path $ScriptRoot "export_websocket_transaction_registration_report.ps1"
$PointCloudPreviewPolicyScript = Join-Path $ScriptRoot "validate_point_cloud_preview_policy.ps1"
$LazPlaceholderPolicyScript = Join-Path $ScriptRoot "validate_laz_placeholder_policy.ps1"
$MonitorWidgetPolicyScript = Join-Path $ScriptRoot "validate_monitor_widget_policy.ps1"
$RuntimeConfigPolicyScript = Join-Path $ScriptRoot "validate_runtime_config_policy.ps1"
$LargeContentDecisionPolicyScript = Join-Path $ScriptRoot "validate_large_content_decision_policy.ps1"
$SmokeScript = Join-Path $ScriptRoot "run_smoke_tests.ps1"

if (-not (Test-Path -LiteralPath $AssetReportScript)) {
    throw "report_local_project_status.ps1 not found: $AssetReportScript"
}
if (-not (Test-Path -LiteralPath $PayloadFixtureScript)) {
    throw "validate_payload_fixtures.ps1 not found: $PayloadFixtureScript"
}
if (-not (Test-Path -LiteralPath $PayloadContractScript)) {
    throw "validate_payload_contract.ps1 not found: $PayloadContractScript"
}
if (-not (Test-Path -LiteralPath $PayloadSchemaReviewPolicyScript)) {
    throw "validate_payload_schema_review_policy.ps1 not found: $PayloadSchemaReviewPolicyScript"
}
if (-not (Test-Path -LiteralPath $ServerTransportContractScript)) {
    throw "validate_server_transport_contract.ps1 not found: $ServerTransportContractScript"
}
if (-not (Test-Path -LiteralPath $RealSensorAdapterPlanScript)) {
    throw "validate_real_sensor_adapter_plan.ps1 not found: $RealSensorAdapterPlanScript"
}
if (-not (Test-Path -LiteralPath $WebSocketLidarLiveSampleScript)) {
    throw "validate_websocket_lidar_live_sample.ps1 not found: $WebSocketLidarLiveSampleScript"
}
if (-not (Test-Path -LiteralPath $WebSocketTransactionRegistrationReportScript)) {
    throw "export_websocket_transaction_registration_report.ps1 not found: $WebSocketTransactionRegistrationReportScript"
}
if (-not (Test-Path -LiteralPath $PointCloudPreviewPolicyScript)) {
    throw "validate_point_cloud_preview_policy.ps1 not found: $PointCloudPreviewPolicyScript"
}
if (-not (Test-Path -LiteralPath $LazPlaceholderPolicyScript)) {
    throw "validate_laz_placeholder_policy.ps1 not found: $LazPlaceholderPolicyScript"
}
if (-not (Test-Path -LiteralPath $MonitorWidgetPolicyScript)) {
    throw "validate_monitor_widget_policy.ps1 not found: $MonitorWidgetPolicyScript"
}
if (-not (Test-Path -LiteralPath $RuntimeConfigPolicyScript)) {
    throw "validate_runtime_config_policy.ps1 not found: $RuntimeConfigPolicyScript"
}
if (-not (Test-Path -LiteralPath $LargeContentDecisionPolicyScript)) {
    throw "validate_large_content_decision_policy.ps1 not found: $LargeContentDecisionPolicyScript"
}
if (-not (Test-Path -LiteralPath $SmokeScript)) {
    throw "run_smoke_tests.ps1 not found: $SmokeScript"
}

$AssetReportParams = @{
    ProjectRoot = $ProjectRoot
    FailOnUnclassifiedUntracked = $true
}
if ($FailOnGeneratedOutput) {
    $AssetReportParams.FailOnGeneratedOutput = $true
}
if ($FailOnStagedDecisionPoints) {
    $AssetReportParams.FailOnStagedDecisionPoints = $true
}
if ($FailOnCategory.Count -gt 0) {
    $AssetReportParams.FailOnCategory = $FailOnCategory
}

Invoke-ScriptStep `
    -Label "Local asset report" `
    -ScriptPath $AssetReportScript `
    -Parameters $AssetReportParams

if (-not $SkipPayloadFixtures) {
    Invoke-ScriptStep `
        -Label "Payload fixture validation" `
        -ScriptPath $PayloadFixtureScript `
        -Parameters @{}
}
else {
    Write-Host ""
    Write-Host "Payload fixture validation skipped by -SkipPayloadFixtures."
}

if (-not $SkipPayloadContract) {
    Invoke-ScriptStep `
        -Label "Payload mock contract validation" `
        -ScriptPath $PayloadContractScript `
        -Parameters @{}
}
else {
    Write-Host ""
    Write-Host "Payload mock contract validation skipped by -SkipPayloadContract."
}

if (-not $SkipPayloadSchemaReviewPolicy) {
    Invoke-ScriptStep `
        -Label "Payload schema review policy validation" `
        -ScriptPath $PayloadSchemaReviewPolicyScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "Payload schema review policy validation skipped by -SkipPayloadSchemaReviewPolicy."
}

if (-not $SkipServerTransportContract) {
    Invoke-ScriptStep `
        -Label "Server transport contract validation" `
        -ScriptPath $ServerTransportContractScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "Server transport contract validation skipped by -SkipServerTransportContract."
}

if (-not $SkipRealSensorAdapterPlan) {
    Invoke-ScriptStep `
        -Label "Real sensor adapter plan validation" `
        -ScriptPath $RealSensorAdapterPlanScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "Real sensor adapter plan validation skipped by -SkipRealSensorAdapterPlan."
}

if (-not $SkipWebSocketLidarLiveSample) {
    Invoke-ScriptStep `
        -Label "WebSocket live LiDAR sample validation" `
        -ScriptPath $WebSocketLidarLiveSampleScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "WebSocket live LiDAR sample validation skipped by -SkipWebSocketLidarLiveSample."
}

if (-not $SkipWebSocketTransactionRegistrationReport) {
    Invoke-ScriptStep `
        -Label "WebSocket transaction registration report" `
        -ScriptPath $WebSocketTransactionRegistrationReportScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "WebSocket transaction registration report skipped by -SkipWebSocketTransactionRegistrationReport."
}

if (-not $SkipPointCloudPreviewPolicy) {
    Invoke-ScriptStep `
        -Label "Point cloud preview policy validation" `
        -ScriptPath $PointCloudPreviewPolicyScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "Point cloud preview policy validation skipped by -SkipPointCloudPreviewPolicy."
}

if (-not $SkipLazPlaceholderPolicy) {
    Invoke-ScriptStep `
        -Label "LAZ placeholder policy validation" `
        -ScriptPath $LazPlaceholderPolicyScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "LAZ placeholder policy validation skipped by -SkipLazPlaceholderPolicy."
}

if (-not $SkipMonitorWidgetPolicy) {
    Invoke-ScriptStep `
        -Label "Monitor widget policy validation" `
        -ScriptPath $MonitorWidgetPolicyScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "Monitor widget policy validation skipped by -SkipMonitorWidgetPolicy."
}

if (-not $SkipRuntimeConfigPolicy) {
    Invoke-ScriptStep `
        -Label "Runtime config policy validation" `
        -ScriptPath $RuntimeConfigPolicyScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Write-Host ""
    Write-Host "Runtime config policy validation skipped by -SkipRuntimeConfigPolicy."
}

if (-not $SkipLargeContentDecisionPolicy) {
    $LargeContentDecisionParams = @{ ProjectRoot = $ProjectRoot }
    if ($FailOnLargeContentCandidates) {
        $LargeContentDecisionParams.FailIfPresent = $true
    }
    Invoke-ScriptStep `
        -Label "Large content decision policy validation" `
        -ScriptPath $LargeContentDecisionPolicyScript `
        -Parameters $LargeContentDecisionParams
}
else {
    Write-Host ""
    Write-Host "Large content decision policy validation skipped by -SkipLargeContentDecisionPolicy."
}

if (-not $SkipSmoke) {
    $SmokeParams = @{
        ProjectPath = $ProjectPath
        EngineRoot = $EngineRoot
        TestGroups = $TestGroups
    }
    if ($SkipBuild) {
        $SmokeParams.SkipBuild = $true
    }
    if ($AllowOpenEditor) {
        $SmokeParams.AllowOpenEditor = $true
    }

    Invoke-ScriptStep `
        -Label "Smoke tests" `
        -ScriptPath $SmokeScript `
        -Parameters $SmokeParams
}
else {
    Write-Host ""
    Write-Host "Smoke tests skipped by -SkipSmoke."
}

Write-Host ""
Write-Host "Project readiness checks passed."
