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
    $lines += "- Broker smoke evidence fields complete: $($Report.Summary.BrokerSmokeEvidenceFieldsComplete)"
    $lines += "- Broker smoke missing evidence fields: $($Report.Summary.BrokerSmokeMissingEvidenceFieldCount)"
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
    $lines += "## Deployment Path Candidates"
    $lines += ""
    $lines += "| Candidate | Priority | Status | Evidence | Remaining risk | Next action |"
    $lines += "| --- | ---: | --- | --- | --- | --- |"
    foreach ($candidate in $Report.DeploymentPathCandidates) {
        $lines += "| $($candidate.Name) | $($candidate.Priority) | $($candidate.Status) | $($candidate.Evidence) | $($candidate.RemainingRisk) | $($candidate.NextAction) |"
    }
    $lines += ""
    $lines += "## Deployment Action Plan"
    $lines += ""
    $lines += "| Priority | Candidate | Blocked | Next action | Required evidence | Blockers |"
    $lines += "| ---: | --- | --- | --- | --- | --- |"
    foreach ($item in $Report.DeploymentActionPlan) {
        $lines += "| $($item.Priority) | $($item.Name) | $($item.Blocked) | $($item.NextAction) | $(@($item.RequiredEvidence) -join '; ') | $(@($item.DeploymentBlockers) -join '; ') |"
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

$deploymentPathCandidates = @(
    [PSCustomObject]@{
        Name = "File replay"
        Priority = 1
        Status = "Recommended baseline"
        Evidence = "CSV/JSONL replay adapters and samples exercise the normalized LiDAR handoff without network or hardware dependencies."
        RemainingRisk = "Not live; must be paired with later deployment smoke for real-time behavior."
        RequiredEvidence = @("Replay sample accepted", "Normalized handoff automation pass", "Downstream payload/recorder path observed")
        DeploymentBlockers = @("No live network or hardware timing evidence")
        NextAction = "Use as the baseline for schema and judgment-server testing while live bridge ownership is still open."
        Blocked = $false
    },
    [PSCustomObject]@{
        Name = "HTTP JSON live"
        Priority = 2
        Status = "Best local-live deployment candidate"
        Evidence = "Local HTTP POST loopback automation covers real request routing, game-thread marshaling, response codes, and target LiDAR handoff."
        RemainingRisk = "Bind address, port, firewall, authentication, rate limit, and payload size policy need deployment ownership."
        RequiredEvidence = @("Bind address decision", "Port/firewall decision", "Authentication or trusted-network decision", "Rate/payload size policy", "Deployment loopback or LAN smoke")
        DeploymentBlockers = @("No production bind/firewall/auth/rate ownership", "No deployment endpoint smoke")
        NextAction = "Assign network owner, lock bind/port/auth/rate policy, then run a deployment HTTP smoke with the checked payload shape."
        Blocked = $true
    },
    [PSCustomObject]@{
        Name = "WebSocket via DTCore"
        Priority = 3
        Status = "Preferred when deployment broker is already owned"
        Evidence = "Transaction handler, registration report, binary-row evidence automation, and brokerless DTCore dispatch are in place."
        RemainingRisk = "Real broker endpoint, credentials, topic, and observed source/target smoke evidence are still required."
        RequiredEvidence = @("Broker endpoint", "Credentials", "Topic/subscription", "EvidenceRunId", "MapName", "PIE session", "Operator and notes", "Log path", "Broker client command", "Source frame before/after counts", "Target LiDAR point count", "Cached payload bytes or hash")
        DeploymentBlockers = @("External broker connectivity not proven", "Credentials/topic ownership open", "Operator-observed PIE smoke incomplete")
        NextAction = "Run the broker smoke workflow against the deployment broker and record source-frame, target-point, and cached-payload observations."
        Blocked = $true
    },
    [PSCustomObject]@{
        Name = "UDP JSON live"
        Priority = 4
        Status = "Use for low-latency trusted LAN only"
        Evidence = "Local UDP datagram automation covers packet receipt and normalized LiDAR handoff."
        RemainingRisk = "No delivery guarantee; packet sizing, firewall, bind address, and trust boundary decisions remain."
        RequiredEvidence = @("Trusted LAN decision", "Bind/port/firewall decision", "Packet size policy", "Loss/retry acceptance", "Deployment datagram smoke")
        DeploymentBlockers = @("Trust boundary open", "No delivery guarantee policy", "Deployment datagram smoke incomplete")
        NextAction = "Use only after the deployment owner accepts lossy transport and trusted-network constraints."
        Blocked = $true
    },
    [PSCustomObject]@{
        Name = "ROS2/Livox/RealSense SDK"
        Priority = 5
        Status = "Hardware-specific follow-up"
        Evidence = "Placeholder components and normalized target handoff are present."
        RemainingRisk = "Actual SDK dependencies, message schemas, calibration, timestamps, and real-frame smoke are not implemented."
        RequiredEvidence = @("SDK/ROS2 dependency decision", "Message/packet schema", "Calibration/timestamp policy", "Device availability", "Real-frame smoke")
        DeploymentBlockers = @("SDK integration not implemented", "Hardware/device smoke missing", "Calibration and timestamp ownership open")
        NextAction = "Keep as follow-up until the deployment environment provides actual device/SDK access and calibration requirements."
        Blocked = $true
    }
)

$deploymentActionPlan = @(
    $deploymentPathCandidates |
        Sort-Object Priority |
        ForEach-Object {
            [PSCustomObject]@{
                Priority = $_.Priority
                Name = $_.Name
                Status = $_.Status
                Blocked = $_.Blocked
                RequiredEvidence = $_.RequiredEvidence
                DeploymentBlockers = $_.DeploymentBlockers
                NextAction = $_.NextAction
            }
        }
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
    DeploymentPathCandidates = $deploymentPathCandidates
    DeploymentActionPlan = $deploymentActionPlan
    HeadlessAutomationGroups = $headlessAutomationGroups
    RemainingDecisions = $remainingDecisions
    Summary = [PSCustomObject]@{
        StaticAdapterPlanValid = [bool]$adapterPlan.Summary.Valid
        WebSocketSampleValid = [bool]$sample.Summary.Valid
        TransactionRegistrationStaticValid = [bool]$registration.Summary.Valid
        BrokerSmokeReadOnlyValid = [bool]$brokerSmoke.Summary.Valid
        BrokerSmokeObservedCoreComplete = [bool]$brokerSmoke.Summary.ObservedCoreComplete
        BrokerSmokeEvidenceFieldsComplete = [bool]$brokerSmoke.Summary.EvidenceFieldsComplete
        BrokerSmokeMissingEvidenceFieldCount = [int]$brokerSmoke.Summary.MissingEvidenceFieldCount
        BrokerSmokeMissingEvidenceFields = $brokerSmoke.Summary.MissingEvidenceFields
        HeadlessAutomationGroupCount = $headlessAutomationGroups.Count
        DeploymentCandidateCount = $deploymentPathCandidates.Count
        DeploymentActionPlanItemCount = $deploymentActionPlan.Count
        BlockedDeploymentCandidateCount = @($deploymentActionPlan | Where-Object { $_.Blocked }).Count
        RequiredEvidenceDeclared = ($deploymentActionPlan.Count -eq 0 -or @($deploymentActionPlan | Where-Object { @($_.RequiredEvidence).Count -eq 0 }).Count -eq 0)
        DeploymentBlockersDeclared = ($deploymentActionPlan.Count -eq 0 -or @($deploymentActionPlan | Where-Object { @($_.DeploymentBlockers).Count -eq 0 }).Count -eq 0)
        NextActionsDeclared = ($deploymentActionPlan.Count -eq 0 -or @($deploymentActionPlan | Where-Object { [string]::IsNullOrWhiteSpace([string]$_.NextAction) }).Count -eq 0)
        RecommendedLiveBridge = "HTTP JSON live for local/live ingestion unless the deployment broker is already owned; WebSocket via DTCore then becomes the preferred broker path."
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
    Write-Host "Broker smoke evidence fields complete: $($report.Summary.BrokerSmokeEvidenceFieldsComplete)"
    Write-Host "Broker smoke missing evidence fields: $($report.Summary.BrokerSmokeMissingEvidenceFieldCount)"
    Write-Host "Headless automation groups: $($report.Summary.HeadlessAutomationGroupCount)"
    Write-Host "Deployment action-plan items: $($report.Summary.DeploymentActionPlanItemCount)"
    Write-Host "Blocked deployment candidates: $($report.Summary.BlockedDeploymentCandidateCount)"
    Write-Host "Deployment evidence still required: $($report.Summary.DeploymentEvidenceStillRequired)"
    Write-Host "Real SDK integration still required: $($report.Summary.RealSdkIntegrationStillRequired)"
}
