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
    [PSCustomObject]@{ Label = "Camera JSON live bridge header"; Path = "Source\m7at10_dt\M7AT10\Sensor\CameraJsonLiveSourceComp.h" },
    [PSCustomObject]@{ Label = "Camera JSON live bridge implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\CameraJsonLiveSourceComp.cpp" },
    [PSCustomObject]@{ Label = "JSON live bridge header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.h" },
    [PSCustomObject]@{ Label = "JSON live bridge implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.cpp" },
    [PSCustomObject]@{ Label = "HTTP JSON live bridge header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarHttpJsonLiveSourceComp.h" },
    [PSCustomObject]@{ Label = "HTTP JSON live bridge implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarHttpJsonLiveSourceComp.cpp" },
    [PSCustomObject]@{ Label = "UDP JSON live bridge header"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarUdpJsonLiveSourceComp.h" },
    [PSCustomObject]@{ Label = "UDP JSON live bridge implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\LidarUdpJsonLiveSourceComp.cpp" },
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
$cameraCompHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Camera\VirtualCameraComp.h"
$cameraCompCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Camera\VirtualCameraComp.cpp"
$cameraLiveHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\CameraJsonLiveSourceComp.h"
$cameraLiveCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\CameraJsonLiveSourceComp.cpp"
$httpLiveHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarHttpJsonLiveSourceComp.h"
$httpLiveCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarHttpJsonLiveSourceComp.cpp"
$udpLiveHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarUdpJsonLiveSourceComp.h"
$udpLiveCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarUdpJsonLiveSourceComp.cpp"
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
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.h"); Pattern = "AppendLivePayloadJson"; Label = "Generic live JSON payload bridge helper" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.h"); Pattern = "PushBufferedFrameNoTransportInEditor"; Label = "JSON live editor push helper" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.cpp"); Pattern = "AppendWebSocketPayload"; Label = "JSON live WebSocket payload append helper" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\LidarJsonLiveSourceComp.cpp"); Pattern = "ResolveSampleWebSocketPayloadPath"; Label = "JSON live sample path resolver" },
    [PSCustomObject]@{ Path = $cameraCompHeader; Pattern = "InjectExternalJsonPayload"; Label = "Camera external JSON payload injection API" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "ReadExternalPayloadMetadata"; Label = "Camera external JSON payload validator" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "virtual-camera.v1"; Label = "Camera external JSON schema check" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "FBase64::Decode"; Label = "Camera external JSON validates base64 image" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "DecodedImageBytes.Num"; Label = "Camera external JSON validates byteSize against decoded bytes" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "ParseIso8601"; Label = "Camera external JSON validates timestamp format" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "DecodedImageBytes[0] != 0xff"; Label = "Camera external JSON validates JPEG magic bytes" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "sensorTransform"; Label = "Camera external JSON validates transform object" },
    [PSCustomObject]@{ Path = $cameraCompCpp; Pattern = "PayloadFrameId, JsonPayload"; Label = "Camera external JSON records payload frame id" },
    [PSCustomObject]@{ Path = $cameraLiveHeader; Pattern = "UCameraJsonLiveSourceComp"; Label = "Camera JSON live bridge class" },
    [PSCustomObject]@{ Path = $cameraLiveHeader; Pattern = "TargetCamera"; Label = "Camera JSON live target camera setting" },
    [PSCustomObject]@{ Path = $cameraLiveCpp; Pattern = "InjectExternalJsonPayload"; Label = "Camera JSON live reuses camera external payload injection" },
    [PSCustomObject]@{ Path = $httpLiveHeader; Pattern = "ULidarHttpJsonLiveSourceComp"; Label = "HTTP JSON live bridge class" },
    [PSCustomObject]@{ Path = $httpLiveHeader; Pattern = "RoutePath"; Label = "HTTP JSON live route path setting" },
    [PSCustomObject]@{ Path = $httpLiveHeader; Pattern = "MaxRequestBytes"; Label = "HTTP JSON live request size guard" },
    [PSCustomObject]@{ Path = $httpLiveHeader; Pattern = "bSendTransportForReceivedFrames"; Label = "HTTP JSON live transport send guard" },
    [PSCustomObject]@{ Path = $httpLiveHeader; Pattern = "bLastRequestProcessedOnGameThread"; Label = "HTTP JSON live game-thread status flag" },
    [PSCustomObject]@{ Path = $httpLiveHeader; Pattern = "bAcceptingHttpRequests"; Label = "HTTP JSON live accepting-request lifecycle guard" },
    [PSCustomObject]@{ Path = $httpLiveHeader; Pattern = "ActiveRequestGeneration"; Label = "HTTP JSON live stale callback generation guard" },
    [PSCustomObject]@{ Path = $httpLiveCpp; Pattern = "FHttpServerModule"; Label = "HTTP JSON live uses Unreal HTTPServer module" },
    [PSCustomObject]@{ Path = $httpLiveCpp; Pattern = "BindRoute"; Label = "HTTP JSON live binds POST route" },
    [PSCustomObject]@{ Path = $httpLiveCpp; Pattern = "StartAllListeners"; Label = "HTTP JSON live starts HTTPServer listeners" },
    [PSCustomObject]@{ Path = $httpLiveCpp; Pattern = "AsyncTask(ENamedThreads::GameThread"; Label = "HTTP JSON live marshals requests to game thread" },
    [PSCustomObject]@{ Path = $httpLiveCpp; Pattern = "IsInGameThread"; Label = "HTTP JSON live processing checks game thread" },
    [PSCustomObject]@{ Path = $httpLiveCpp; Pattern = "AppendLivePayloadJson"; Label = "HTTP JSON live reuses generic payload handoff" },
    [PSCustomObject]@{ Path = $udpLiveHeader; Pattern = "ULidarUdpJsonLiveSourceComp"; Label = "UDP JSON live bridge class" },
    [PSCustomObject]@{ Path = $udpLiveHeader; Pattern = "BindAddress"; Label = "UDP JSON live bind address setting" },
    [PSCustomObject]@{ Path = $udpLiveHeader; Pattern = "bSendTransportForReceivedFrames"; Label = "UDP JSON live transport send guard" },
    [PSCustomObject]@{ Path = $udpLiveCpp; Pattern = "FUdpSocketReceiver"; Label = "UDP JSON live receiver uses Unreal UDP receiver" },
    [PSCustomObject]@{ Path = $udpLiveCpp; Pattern = "AsyncTask(ENamedThreads::GameThread"; Label = "UDP JSON live marshals datagrams to game thread" },
    [PSCustomObject]@{ Path = $udpLiveCpp; Pattern = "AppendLivePayloadJson"; Label = "UDP JSON live reuses generic payload handoff" },
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
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.CameraJsonLiveBridgePushFrame"; Label = "Camera JSON live bridge automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "virtual-camera.v1"; Label = "Camera JSON live automation uses camera payload schema" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "camera external payload transport submitted"; Label = "Camera JSON live automation checks transport path" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "camera external payload transport path uses payload sensor id"; Label = "Camera JSON live automation checks transport payload sensor id" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "camera external payload recorder frame count"; Label = "Camera JSON live automation checks recorder path" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "invalid base64 camera JSON live push is rejected"; Label = "Camera JSON live automation rejects invalid base64" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "byte mismatch camera JSON live push is rejected"; Label = "Camera JSON live automation rejects byteSize mismatch" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "keeps previous camera payload"; Label = "Camera JSON live automation preserves payload on rejection" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.HttpJsonLiveBridgePayload"; Label = "HTTP JSON live bridge automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.HttpJsonLiveBridgeLoopbackPost"; Label = "HTTP JSON live loopback POST automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "FHttpModule::Get().CreateRequest"; Label = "HTTP JSON live loopback smoke uses real HTTP client" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "OnProcessRequestComplete"; Label = "HTTP JSON live loopback smoke waits on HTTP callback" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "GetHttpManager().Tick"; Label = "HTTP JSON live loopback smoke ticks HTTP manager in commandlet" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "FindFreeLoopbackTcpPort"; Label = "HTTP JSON live loopback smoke probes a free port" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "http://127.0.0.1"; Label = "HTTP JSON live loopback smoke targets localhost" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "HTTP loopback POST response code"; Label = "HTTP JSON live loopback smoke asserts response code" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "LastReceivedRequestBytes"; Label = "HTTP JSON live loopback smoke checks received bytes" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "HTTP JSON live payload processed on game thread"; Label = "HTTP JSON live automation checks game-thread processing" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "HTTP JSON live route unbinds on stop"; Label = "HTTP JSON live automation checks lifecycle stop" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.UdpJsonLiveBridgePayload"; Label = "UDP JSON live bridge automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.RealSensorSource.UdpJsonLiveBridgeDatagram"; Label = "UDP JSON live datagram smoke automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "GetBoundPort"; Label = "UDP datagram smoke uses ephemeral bound port" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "SendTo"; Label = "UDP datagram smoke sends real local datagram" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "generic live payload"; Label = "Generic live payload alias automation coverage" },
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
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "RunBrokerlessDTCoreDispatchAutomation"; Label = "Smoke evidence workflow can run brokerless DTCore dispatch automation" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "M7AT10.RealSensorSource.JsonLiveDTCoreDispatch"; Label = "Smoke evidence workflow names brokerless dispatch automation" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "RunCommandletDryRun"; Label = "Smoke evidence workflow commandlet dry run is opt-in" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "EnsureLidarJsonLiveFrameTransaction"; Label = "Smoke evidence workflow can dry-run row commandlet" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "WriteReports"; Label = "Smoke evidence workflow report writing is opt-in" },
    [PSCustomObject]@{ Path = $webSocketSmokeEvidenceWorkflow; Pattern = "ExternalBrokerStillRequired"; Label = "Smoke evidence workflow separates broker completion" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Source\m7at10_dt\m7at10_dt.Build.cs"); Pattern = "HTTPServer"; Label = "Runtime module includes inbound HTTP server dependency" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "PushPointFrameToTarget"; Label = "Plan documents normalized handoff" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "ULidarJsonLiveSourceComp"; Label = "Plan documents JSON live bridge" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "LIDAR_JSON_LIVE_FRAME"; Label = "Plan documents JSON live transaction code" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "Samples/websocket/lidar_json_live_frame_sample.json"; Label = "Plan links WebSocket sample" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "DefaultBindAddress"; Label = "Plan documents HTTPServer default bind override" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "ListenerOverrides"; Label = "Plan documents HTTPServer listener override" },
    [PSCustomObject]@{ Path = $planDoc; Pattern = "BindAddress=any"; Label = "Plan documents HTTPServer all-interface override" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "docs\server_transport_contract.md"); Pattern = "DefaultBindAddress"; Label = "Transport contract documents HTTPServer default bind override" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "docs\server_transport_contract.md"); Pattern = "ListenerOverrides"; Label = "Transport contract documents HTTPServer listener override" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "docs\server_transport_contract.md"); Pattern = "BindAddress=any"; Label = "Transport contract documents HTTPServer all-interface override" },
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
        CameraJsonLiveBridgePresent = $true
        JsonLiveHttpBridgePresent = $true
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
        JsonLiveHttpBridgeAutomationPresent = $true
        JsonLiveHttpLoopbackSmokePresent = $true
        CameraJsonLiveBridgeAutomationPresent = $true
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
    Write-Host "Camera JSON live bridge present: $($report.Summary.CameraJsonLiveBridgePresent)"
    Write-Host "JSON live HTTP bridge present: $($report.Summary.JsonLiveHttpBridgePresent)"
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
    Write-Host "JSON live HTTP bridge automation present: $($report.Summary.JsonLiveHttpBridgeAutomationPresent)"
    Write-Host "JSON live HTTP loopback smoke present: $($report.Summary.JsonLiveHttpLoopbackSmokePresent)"
    Write-Host "Camera JSON live bridge automation present: $($report.Summary.CameraJsonLiveBridgeAutomationPresent)"
    Write-Host "Placeholder adapters present: $($report.Summary.PlaceholderAdaptersPresent)"
    Write-Host "Normalized handoff documented: $($report.Summary.NormalizedHandoffDocumented)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
}
