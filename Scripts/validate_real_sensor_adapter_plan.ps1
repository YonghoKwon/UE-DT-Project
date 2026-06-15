param(
    [string]$ProjectRoot = "",
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

function Assert-ContainsText {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    $match = Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet
    if (-not $match) {
        throw "$Label missing required text '$Pattern' in $Path"
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$requiredFiles = @(
    [PSCustomObject]@{ Label = "Base source component header"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.h" },
    [PSCustomObject]@{ Label = "Base source component implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.cpp" },
    [PSCustomObject]@{ Label = "Adapter placeholder header"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.h" },
    [PSCustomObject]@{ Label = "Adapter placeholder implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.cpp" },
    [PSCustomObject]@{ Label = "CSV replay header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarCsvReplaySourceComp.h" },
    [PSCustomObject]@{ Label = "CSV replay implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarCsvReplaySourceComp.cpp" },
    [PSCustomObject]@{ Label = "JSONL replay header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLinesReplaySourceComp.h" },
    [PSCustomObject]@{ Label = "JSONL replay implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLinesReplaySourceComp.cpp" },
    [PSCustomObject]@{ Label = "JSON live bridge header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.h" },
    [PSCustomObject]@{ Label = "JSON live bridge implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.cpp" },
    [PSCustomObject]@{ Label = "JSON live WebSocket transaction header"; Path = "Source\m7at10_dt\M7AT10\WebSocket\TC\LidarJsonLiveFrameTC.h" },
    [PSCustomObject]@{ Label = "JSON live WebSocket transaction implementation"; Path = "Source\m7at10_dt\M7AT10\WebSocket\TC\LidarJsonLiveFrameTC.cpp" },
    [PSCustomObject]@{ Label = "Editor module build file"; Path = "Source\m7at10_dtEditor\m7at10_dtEditor.Build.cs" },
    [PSCustomObject]@{ Label = "Editor module implementation"; Path = "Source\m7at10_dtEditor\Private\m7at10_dtEditor.cpp" },
    [PSCustomObject]@{ Label = "JSON live WebSocket transaction commandlet header"; Path = "Source\m7at10_dtEditor\Private\EnsureLidarJsonLiveFrameTransactionCommandlet.h" },
    [PSCustomObject]@{ Label = "JSON live WebSocket transaction commandlet implementation"; Path = "Source\m7at10_dtEditor\Private\EnsureLidarJsonLiveFrameTransactionCommandlet.cpp" },
    [PSCustomObject]@{ Label = "Real sensor automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\RealSensorSourceAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "CSV replay sample"; Path = "Samples\slab_replay_sample.csv" },
    [PSCustomObject]@{ Label = "JSONL replay sample"; Path = "Samples\slab_replay_sample.jsonl" },
    [PSCustomObject]@{ Label = "WebSocket JSON live LiDAR sample"; Path = "Samples\websocket\lidar_json_live_frame_sample.json" },
    [PSCustomObject]@{ Label = "WebSocket JSON live LiDAR sample validator"; Path = "Scripts\validate_websocket_lidar_live_sample.ps1" },
    [PSCustomObject]@{ Label = "WebSocket transaction registration report exporter"; Path = "Scripts\export_websocket_transaction_registration_report.ps1" },
    [PSCustomObject]@{ Label = "WebSocket broker smoke report exporter"; Path = "Scripts\export_websocket_broker_smoke_report.ps1" },
    [PSCustomObject]@{ Label = "WebSocket LiDAR smoke evidence workflow"; Path = "Scripts\run_websocket_lidar_smoke_evidence.ps1" },
    [PSCustomObject]@{ Label = "Adapter plan document"; Path = "docs\real_sensor_adapter_plan.md" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$baseHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.h"
$baseCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorSourceComp.cpp"
$stubsHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.h"
$stubsCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\RealSensorAdapterStubs.cpp"
$testsCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\RealSensorSourceAutomationTests.cpp"
$liveTcHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\WebSocket\TC\LidarJsonLiveFrameTC.h"
$liveTcCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\WebSocket\TC\LidarJsonLiveFrameTC.cpp"
$liveTcCommandletCpp = Join-Path $ProjectRoot "Source\m7at10_dtEditor\Private\EnsureLidarJsonLiveFrameTransactionCommandlet.cpp"
$editorBuildCs = Join-Path $ProjectRoot "Source\m7at10_dtEditor\m7at10_dtEditor.Build.cs"
$editorTargetCs = Join-Path $ProjectRoot "Source\m7at10_dtEditor.Target.cs"
$uproject = Join-Path $ProjectRoot "m7at10_dt.uproject"
$webSocketSample = Join-Path $ProjectRoot "Samples\websocket\lidar_json_live_frame_sample.json"
$webSocketSampleValidator = Join-Path $ProjectRoot "Scripts\validate_websocket_lidar_live_sample.ps1"
$webSocketRegistrationReportExporter = Join-Path $ProjectRoot "Scripts\export_websocket_transaction_registration_report.ps1"
$webSocketBrokerSmokeReportExporter = Join-Path $ProjectRoot "Scripts\export_websocket_broker_smoke_report.ps1"
$webSocketSmokeEvidenceWorkflow = Join-Path $ProjectRoot "Scripts\run_websocket_lidar_smoke_evidence.ps1"
$planDoc = Join-Path $ProjectRoot "docs\real_sensor_adapter_plan.md"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $baseHeader; Pattern = "ERealSensorSourceKind"; Label = "Base source kind enum" },
    [PSCustomObject]@{ Path = $baseHeader; Pattern = "JsonLiveBridge"; Label = "JSON live source kind" },
    [PSCustomObject]@{ Path = $baseHeader; Pattern = "PushPointFrameToTarget"; Label = "Normalized LiDAR handoff declaration" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.h"); Pattern = "AppendSampleWebSocketFrameInEditor"; Label = "JSON live editor sample helper" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.h"); Pattern = "PushBufferedFrameNoTransportInEditor"; Label = "JSON live editor push helper" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.cpp"); Pattern = "AppendWebSocketPayload"; Label = "JSON live WebSocket payload append helper" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.cpp"); Pattern = "ResolveSampleWebSocketPayloadPath"; Label = "JSON live sample path resolver" },
    [PSCustomObject]@{ Path = $baseCpp; Pattern = "InjectPointCloudFrame"; Label = "Target LiDAR injection" },
    [PSCustomObject]@{ Path = $baseCpp; Pattern = "MarkFramePushed"; Label = "Frame push status accounting" },
    [PSCustomObject]@{ Path = $stubsHeader; Pattern = "URos2SensorBridgeSourceComp"; Label = "ROS2 placeholder class" },
    [PSCustomObject]@{ Path = $stubsHeader; Pattern = "ULivoxLidarSourceComp"; Label = "Livox placeholder class" },
    [PSCustomObject]@{ Path = $stubsHeader; Pattern = "URealSenseCameraSourceComp"; Label = "RealSense placeholder class" },
    [PSCustomObject]@{ Path = $stubsCpp; Pattern = "integration is not implemented yet"; Label = "Placeholder status text" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.BaseState"; Label = "Base state automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.PlaceholderState"; Label = "Placeholder automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.PushFrameToTarget"; Label = "Handoff automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.JsonLiveBridgePushFrame"; Label = "JSON live bridge automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.JsonLiveTransactionParse"; Label = "JSON live transaction automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.JsonLiveTransactionRouting"; Label = "JSON live transaction routing automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.Evidence.WebSocketTransactionRegistration"; Label = "Optional WebSocket data-table registration evidence test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.JsonLiveDTCoreDispatch"; Label = "Brokerless DTCore dispatch automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "FindRow<FTransactionCodeStruct>"; Label = "Registration evidence test inspects DTCore data-table row" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "NewObject<UTransactionCodeMessage>"; Label = "Registration evidence test instantiates row handler through base class" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "registered data-table handler pushes target LiDAR points"; Label = "Registration evidence test dispatches to live source" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "EnqueueWebSocketData"; Label = "Brokerless DTCore dispatch queues WebSocket payload through subsystem" },
    [PSCustomObject]@{ Path = $liveTcHeader; Pattern = "ULidarJsonLiveFrameTC"; Label = "JSON live transaction class" },
    [PSCustomObject]@{ Path = $liveTcCpp; Pattern = "LIDAR_JSON_LIVE_FRAME"; Label = "JSON live transaction code" },
    [PSCustomObject]@{ Path = $liveTcCpp; Pattern = "AppendJsonLines"; Label = "JSON live transaction appends lines" },
    [PSCustomObject]@{ Path = $liveTcCpp; Pattern = "SOURCE_ID is required"; Label = "JSON live transaction guards ambiguous routing" },
    [PSCustomObject]@{ Path = $liveTcCommandletCpp; Pattern = "LIDAR_JSON_LIVE_FRAME"; Label = "Commandlet writes JSON live transaction row" },
    [PSCustomObject]@{ Path = $liveTcCommandletCpp; Pattern = "UPackage::SavePackage"; Label = "Commandlet saves project data table package" },
    [PSCustomObject]@{ Path = $liveTcCommandletCpp; Pattern = "NoSave"; Label = "Commandlet supports dry run" },
    [PSCustomObject]@{ Path = $editorBuildCs; Pattern = "UnrealEd"; Label = "Commandlet is isolated to editor module" },
    [PSCustomObject]@{ Path = $editorTargetCs; Pattern = "m7at10_dtEditor"; Label = "Editor target loads editor module" },
    [PSCustomObject]@{ Path = $uproject; Pattern = '"Type": "Editor"'; Label = "Project declares editor module" },
    [PSCustomObject]@{ Path = $webSocketSample; Pattern = "LIDAR_JSON_LIVE_FRAME"; Label = "WebSocket sample transaction code" },
    [PSCustomObject]@{ Path = $webSocketSample; Pattern = "JsonLiveLidarBridge"; Label = "WebSocket sample source id" },
    [PSCustomObject]@{ Path = $webSocketSampleValidator; Pattern = "SafeSourceRoutingDocumented"; Label = "WebSocket sample validator safe routing check" },
    [PSCustomObject]@{ Path = $webSocketRegistrationReportExporter; Pattern = "TransactionCodeMessageClass"; Label = "WebSocket registration report expected class" },
    [PSCustomObject]@{ Path = $webSocketRegistrationReportExporter; Pattern = "BinaryDataTableRowVerified"; Label = "WebSocket registration report binary row limitation" },
    [PSCustomObject]@{ Path = $webSocketRegistrationReportExporter; Pattern = "NoWrite"; Label = "WebSocket registration report read-only mode" },
    [PSCustomObject]@{ Path = $webSocketBrokerSmokeReportExporter; Pattern = "ObservedSourceFrame"; Label = "Broker smoke report records source frame observation" },
    [PSCustomObject]@{ Path = $webSocketBrokerSmokeReportExporter; Pattern = "DoesNotConnectToBroker"; Label = "Broker smoke report states it does not fake broker connectivity" },
    [PSCustomObject]@{ Path = $webSocketBrokerSmokeReportExporter; Pattern = "NoWrite"; Label = "Broker smoke report read-only mode" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "RunEvidenceAutomation"; Label = "Smoke evidence workflow can run registration automation" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "RunCommandletDryRun"; Label = "Smoke evidence workflow commandlet dry run is opt-in" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "EnsureLidarJsonLiveFrameTransaction"; Label = "Smoke evidence workflow can dry-run row commandlet" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "WriteReports"; Label = "Smoke evidence workflow report writing is opt-in" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "ExternalBrokerStillRequired"; Label = "Smoke evidence workflow separates broker completion" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "PushPointFrameToTarget"; Label = "Plan documents normalized handoff" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "ULidarJsonLiveSourceComp"; Label = "Plan documents JSON live bridge" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "LIDAR_JSON_LIVE_FRAME"; Label = "Plan documents JSON live transaction code" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "Samples/websocket/lidar_json_live_frame_sample.json"; Label = "Plan links WebSocket sample" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "Adapter Priority"; Label = "Plan documents adapter priority" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    CheckedFiles = @($requiredFiles | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Path = Join-Path $ProjectRoot $_.Path
        }
    })
    CheckedContracts = @($requiredTexts | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Pattern = $_.Pattern
            Path = $_.Path
        }
    })
    Summary = [PSCustomObject]@{
        RequiredFileCount = $requiredFiles.Count
        RequiredContractCount = $requiredTexts.Count
        ReplayAdaptersPresent = $true
        JsonLiveBridgePresent = $true
        JsonLiveWebSocketHandlerPresent = $true
        JsonLiveWebSocketCommandletPresent = $true
        JsonLiveWebSocketSamplePresent = $true
        JsonLiveRegistrationReportPresent = $true
        JsonLiveBrokerSmokeReportPresent = $true
        JsonLiveSmokeEvidenceWorkflowPresent = $true
        JsonLiveEditorHelpersPresent = $true
        JsonLiveRoutingAutomationPresent = $true
        JsonLiveRegistrationEvidenceAutomationPresent = $true
        JsonLiveBrokerlessDispatchAutomationPresent = $true
        PlaceholderAdaptersPresent = $true
        NormalizedHandoffDocumented = $true
        AutomationCoverageDeclared = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Real sensor adapter plan is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Replay adapters present: $($report.Summary.ReplayAdaptersPresent)"
    Write-Host "JSON live bridge present: $($report.Summary.JsonLiveBridgePresent)"
    Write-Host "JSON live WebSocket handler present: $($report.Summary.JsonLiveWebSocketHandlerPresent)"
    Write-Host "JSON live WebSocket commandlet present: $($report.Summary.JsonLiveWebSocketCommandletPresent)"
    Write-Host "JSON live WebSocket sample present: $($report.Summary.JsonLiveWebSocketSamplePresent)"
    Write-Host "JSON live registration report present: $($report.Summary.JsonLiveRegistrationReportPresent)"
    Write-Host "JSON live broker smoke report present: $($report.Summary.JsonLiveBrokerSmokeReportPresent)"
    Write-Host "JSON live smoke evidence workflow present: $($report.Summary.JsonLiveSmokeEvidenceWorkflowPresent)"
    Write-Host "JSON live editor helpers present: $($report.Summary.JsonLiveEditorHelpersPresent)"
    Write-Host "JSON live routing automation present: $($report.Summary.JsonLiveRoutingAutomationPresent)"
    Write-Host "JSON live registration evidence automation present: $($report.Summary.JsonLiveRegistrationEvidenceAutomationPresent)"
    Write-Host "JSON live brokerless dispatch automation present: $($report.Summary.JsonLiveBrokerlessDispatchAutomationPresent)"
    Write-Host "Placeholder adapters present: $($report.Summary.PlaceholderAdaptersPresent)"
    Write-Host "Normalized handoff documented: $($report.Summary.NormalizedHandoffDocumented)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
}
