param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$ProjectPath = "",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [string[]]$TestGroups = @(
        "MA0T10.EditorSmoke",
        "MA0T10.SensorReplay",
        "MA0T10.SensorRecorder",
        "MA0T10.RealSensorSource",
        "MA0T10.SensorManager",
        "MA0T10.SensorMonitor"
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
    [switch]$SkipWebSocketBrokerSmokeReport,
    [switch]$SkipWebSocketLidarSmokeEvidence,
    [switch]$SkipPointCloudPreviewPolicy,
    [switch]$SkipLazPlaceholderPolicy,
    [switch]$SkipMonitorWidgetPolicy,
    [switch]$SkipRuntimeConfigPolicy,
    [switch]$SkipLargeContentDecisionPolicy,
    [switch]$SkipLocalAssetDecisionEvidenceWorkflow,
    [switch]$FailOnLargeContentCandidates,
    [switch]$AllowOpenEditor,
    [switch]$FailOnGeneratedOutput,
    [switch]$FailOnStagedDecisionPoints,
    [string[]]$FailOnCategory = @(),
    [switch]$Json
)

$ErrorActionPreference = "Stop"
$script:JsonMode = [bool]$Json
$script:ReadinessSteps = [System.Collections.Generic.List[object]]::new()

function Write-ReadinessHost {
    param([string]$Message = "")

    if (-not $script:JsonMode) {
        Write-Host $Message
    }
}

function Add-ReadinessStep {
    param(
        [string]$Label,
        [string]$Status,
        [string]$ScriptPath = "",
        [object]$Parameters = $null,
        [string]$Message = ""
    )

    $script:ReadinessSteps.Add([PSCustomObject]@{
        Label = $Label
        Status = $Status
        ScriptPath = $ScriptPath
        Parameters = $Parameters
        Message = $Message
    }) | Out-Null
}

function Invoke-ScriptStep {
    param(
        [string]$Label,
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    Write-ReadinessHost ""
    Write-ReadinessHost "==> $Label"
    Write-ReadinessHost $ScriptPath

    $output = @(& $ScriptPath @Parameters *>&1)
    if (-not $script:JsonMode) {
        $output | ForEach-Object { Write-Host $_ }
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
    Add-ReadinessStep -Label $Label -Status "Passed" -ScriptPath $ScriptPath -Parameters $Parameters
}

function Skip-ScriptStep {
    param(
        [string]$Label,
        [string]$Reason
    )

    Write-ReadinessHost ""
    Write-ReadinessHost $Reason
    Add-ReadinessStep -Label $Label -Status "Skipped" -Message $Reason
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
    $SourceRepoRoot = Split-Path -Parent $ScriptRoot
}
if (-not (Test-Path -LiteralPath $SourceRepoRoot -PathType Container)) {
    throw "SourceRepoRoot not found: $SourceRepoRoot"
}
$SourceRepoRoot = (Resolve-Path -LiteralPath $SourceRepoRoot).Path
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $ProjectRoot "ma0t10_dt.uproject"
}
if (-not (Test-Path -LiteralPath $ProjectPath)) {
    throw "Project file not found: $ProjectPath"
}

$AssetReportScript = Join-Path $ScriptRoot "report_local_project_status.ps1"
$PayloadFixtureScript = Join-Path $ScriptRoot "validate_payload_fixtures.ps1"
$PayloadContractScript = Join-Path $ScriptRoot "validate_payload_contract.ps1"
$PayloadSchemaReviewPolicyScript = Join-Path $ScriptRoot "validate_payload_schema_review_policy.ps1"
$ServerTransportContractScript = Join-Path $ScriptRoot "validate_server_transport_contract.ps1"
$RealSensorAdapterPlanScript = Join-Path $ScriptRoot "validate_real_sensor_adapter_plan.ps1"
$WebSocketLidarLiveSampleScript = Join-Path $ScriptRoot "validate_websocket_lidar_live_sample.ps1"
$WebSocketTransactionRegistrationReportScript = Join-Path $ScriptRoot "export_websocket_transaction_registration_report.ps1"
$WebSocketBrokerSmokeReportScript = Join-Path $ScriptRoot "export_websocket_broker_smoke_report.ps1"
$WebSocketLidarSmokeEvidenceScript = Join-Path $ScriptRoot "run_websocket_lidar_smoke_evidence.ps1"
$PointCloudPreviewPolicyScript = Join-Path $ScriptRoot "validate_point_cloud_preview_policy.ps1"
$LazPlaceholderPolicyScript = Join-Path $ScriptRoot "validate_laz_placeholder_policy.ps1"
$LazCompressorReadinessReportScript = Join-Path $ScriptRoot "export_laz_compressor_readiness_report.ps1"
$MonitorWidgetPolicyScript = Join-Path $ScriptRoot "validate_monitor_widget_policy.ps1"
$RuntimeConfigPolicyScript = Join-Path $ScriptRoot "validate_runtime_config_policy.ps1"
$LargeContentDecisionPolicyScript = Join-Path $ScriptRoot "validate_large_content_decision_policy.ps1"
$LocalAssetDecisionEvidenceWorkflowScript = Join-Path $ScriptRoot "validate_local_asset_decision_evidence_workflow.ps1"
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
if (-not (Test-Path -LiteralPath $WebSocketBrokerSmokeReportScript)) {
    throw "export_websocket_broker_smoke_report.ps1 not found: $WebSocketBrokerSmokeReportScript"
}
if (-not (Test-Path -LiteralPath $WebSocketLidarSmokeEvidenceScript)) {
    throw "run_websocket_lidar_smoke_evidence.ps1 not found: $WebSocketLidarSmokeEvidenceScript"
}
if (-not (Test-Path -LiteralPath $PointCloudPreviewPolicyScript)) {
    throw "validate_point_cloud_preview_policy.ps1 not found: $PointCloudPreviewPolicyScript"
}
if (-not (Test-Path -LiteralPath $LazPlaceholderPolicyScript)) {
    throw "validate_laz_placeholder_policy.ps1 not found: $LazPlaceholderPolicyScript"
}
if (-not (Test-Path -LiteralPath $LazCompressorReadinessReportScript)) {
    throw "export_laz_compressor_readiness_report.ps1 not found: $LazCompressorReadinessReportScript"
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
if (-not (Test-Path -LiteralPath $LocalAssetDecisionEvidenceWorkflowScript)) {
    throw "validate_local_asset_decision_evidence_workflow.ps1 not found: $LocalAssetDecisionEvidenceWorkflowScript"
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
    Skip-ScriptStep -Label "Payload fixture validation" -Reason "Payload fixture validation skipped by -SkipPayloadFixtures."
}

if (-not $SkipPayloadContract) {
    Invoke-ScriptStep `
        -Label "Payload mock contract validation" `
        -ScriptPath $PayloadContractScript `
        -Parameters @{}
}
else {
    Skip-ScriptStep -Label "Payload mock contract validation" -Reason "Payload mock contract validation skipped by -SkipPayloadContract."
}

if (-not $SkipPayloadSchemaReviewPolicy) {
    Invoke-ScriptStep `
        -Label "Payload schema review policy validation" `
        -ScriptPath $PayloadSchemaReviewPolicyScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
}
else {
    Skip-ScriptStep -Label "Payload schema review policy validation" -Reason "Payload schema review policy validation skipped by -SkipPayloadSchemaReviewPolicy."
}

if (-not $SkipServerTransportContract) {
    Invoke-ScriptStep `
        -Label "Server transport contract validation" `
        -ScriptPath $ServerTransportContractScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
}
else {
    Skip-ScriptStep -Label "Server transport contract validation" -Reason "Server transport contract validation skipped by -SkipServerTransportContract."
}

if (-not $SkipRealSensorAdapterPlan) {
    Invoke-ScriptStep `
        -Label "Real sensor adapter plan validation" `
        -ScriptPath $RealSensorAdapterPlanScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
}
else {
    Skip-ScriptStep -Label "Real sensor adapter plan validation" -Reason "Real sensor adapter plan validation skipped by -SkipRealSensorAdapterPlan."
}

if (-not $SkipWebSocketLidarLiveSample) {
    Invoke-ScriptStep `
        -Label "WebSocket live LiDAR sample validation" `
        -ScriptPath $WebSocketLidarLiveSampleScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
}
else {
    Skip-ScriptStep -Label "WebSocket live LiDAR sample validation" -Reason "WebSocket live LiDAR sample validation skipped by -SkipWebSocketLidarLiveSample."
}

if (-not $SkipWebSocketTransactionRegistrationReport) {
    Invoke-ScriptStep `
        -Label "WebSocket transaction registration report" `
        -ScriptPath $WebSocketTransactionRegistrationReportScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot; NoWrite = $true }
}
else {
    Skip-ScriptStep -Label "WebSocket transaction registration report" -Reason "WebSocket transaction registration report skipped by -SkipWebSocketTransactionRegistrationReport."
}

if (-not $SkipWebSocketBrokerSmokeReport) {
    Invoke-ScriptStep `
        -Label "WebSocket broker smoke report" `
        -ScriptPath $WebSocketBrokerSmokeReportScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot; NoWrite = $true }
}
else {
    Skip-ScriptStep -Label "WebSocket broker smoke report" -Reason "WebSocket broker smoke report skipped by -SkipWebSocketBrokerSmokeReport."
}

if (-not $SkipWebSocketLidarSmokeEvidence) {
    Invoke-ScriptStep `
        -Label "WebSocket LiDAR smoke evidence workflow" `
        -ScriptPath $WebSocketLidarSmokeEvidenceScript `
        -Parameters @{ ProjectRoot = $ProjectRoot; ProjectPath = $ProjectPath; EngineRoot = $EngineRoot; NoWrite = $true }
}
else {
    Skip-ScriptStep -Label "WebSocket LiDAR smoke evidence workflow" -Reason "WebSocket LiDAR smoke evidence workflow skipped by -SkipWebSocketLidarSmokeEvidence."
}

if (-not $SkipPointCloudPreviewPolicy) {
    Invoke-ScriptStep `
        -Label "Point cloud preview policy validation" `
        -ScriptPath $PointCloudPreviewPolicyScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
}
else {
    Skip-ScriptStep -Label "Point cloud preview policy validation" -Reason "Point cloud preview policy validation skipped by -SkipPointCloudPreviewPolicy."
}

if (-not $SkipLazPlaceholderPolicy) {
    Invoke-ScriptStep `
        -Label "LAZ placeholder policy validation" `
        -ScriptPath $LazPlaceholderPolicyScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
    Invoke-ScriptStep `
        -Label "LAZ compressor readiness report" `
        -ScriptPath $LazCompressorReadinessReportScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
}
else {
    Skip-ScriptStep -Label "LAZ placeholder policy validation" -Reason "LAZ placeholder policy validation skipped by -SkipLazPlaceholderPolicy."
    Skip-ScriptStep -Label "LAZ compressor readiness report" -Reason "LAZ compressor readiness report skipped by -SkipLazPlaceholderPolicy."
}

if (-not $SkipMonitorWidgetPolicy) {
    Invoke-ScriptStep `
        -Label "Monitor widget policy validation" `
        -ScriptPath $MonitorWidgetPolicyScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot }
}
else {
    Skip-ScriptStep -Label "Monitor widget policy validation" -Reason "Monitor widget policy validation skipped by -SkipMonitorWidgetPolicy."
}

if (-not $SkipRuntimeConfigPolicy) {
    Invoke-ScriptStep `
        -Label "Runtime config policy validation" `
        -ScriptPath $RuntimeConfigPolicyScript `
        -Parameters @{ ProjectRoot = $SourceRepoRoot; LocalProjectRoot = $ProjectRoot }
}
else {
    Skip-ScriptStep -Label "Runtime config policy validation" -Reason "Runtime config policy validation skipped by -SkipRuntimeConfigPolicy."
}

if (-not $SkipLargeContentDecisionPolicy) {
    $LargeContentDecisionParams = @{ ProjectRoot = $SourceRepoRoot; LocalProjectRoot = $ProjectRoot }
    if ($FailOnLargeContentCandidates) {
        $LargeContentDecisionParams.FailIfPresent = $true
    }
    Invoke-ScriptStep `
        -Label "Large content decision policy validation" `
        -ScriptPath $LargeContentDecisionPolicyScript `
        -Parameters $LargeContentDecisionParams
}
else {
    Skip-ScriptStep -Label "Large content decision policy validation" -Reason "Large content decision policy validation skipped by -SkipLargeContentDecisionPolicy."
}

if (-not $SkipLocalAssetDecisionEvidenceWorkflow) {
    Invoke-ScriptStep `
        -Label "Local asset decision evidence workflow validation" `
        -ScriptPath $LocalAssetDecisionEvidenceWorkflowScript `
        -Parameters @{ ProjectRoot = $ProjectRoot }
}
else {
    Skip-ScriptStep -Label "Local asset decision evidence workflow validation" -Reason "Local asset decision evidence workflow validation skipped by -SkipLocalAssetDecisionEvidenceWorkflow."
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
    Skip-ScriptStep -Label "Smoke tests" -Reason "Smoke tests skipped by -SkipSmoke."
}

$report = [PSCustomObject]@{
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    ProjectPath = $ProjectPath
    EngineRoot = $EngineRoot
    Passed = $true
    StepCount = $script:ReadinessSteps.Count
    PassedStepCount = @($script:ReadinessSteps | Where-Object { $_.Status -eq "Passed" }).Count
    SkippedStepCount = @($script:ReadinessSteps | Where-Object { $_.Status -eq "Skipped" }).Count
    Steps = @($script:ReadinessSteps)
}

if ($Json) {
    $report | ConvertTo-Json -Depth 6
}
else {
    Write-Host ""
    Write-Host "Project readiness checks passed."
}
