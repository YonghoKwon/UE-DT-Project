param(
    [string]$ProjectRoot = "",
    [string]$OutputPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }
    return (Split-Path -Parent $PSScriptRoot)
}

function New-EvidenceItem {
    param(
        [string]$Name,
        [string]$RequiredEvidence,
        [string]$Owner = "SensorDeploymentOwner"
    )

    return [PSCustomObject]@{
        Name = $Name
        Owner = $Owner
        Status = "Pending"
        RequiredEvidence = $RequiredEvidence
        EvidencePath = ""
        Reviewer = ""
        ReviewedAtUtc = ""
        Notes = ""
    }
}

function Write-MarkdownTemplate {
    param(
        [object]$Template,
        [string]$Path
    )

    $lines = @()
    $lines += "# Real Sensor Adapter Deployment Evidence Template"
    $lines += ""
    $lines += "Generated UTC: $($Template.GeneratedUtc)"
    $lines += ""
    $lines += "This template records evidence only. Do not write endpoint URLs, tokens, passwords, secrets, or credentials into it."
    $lines += ""
    $lines += "## Summary"
    $lines += ""
    $lines += "- Required evidence count: $($Template.Summary.RequiredEvidenceCount)"
    $lines += "- Deployment path section count: $($Template.Summary.DeploymentPathSectionCount)"
    $lines += "- Deployment path sections: $($Template.Summary.DeploymentPathSections -join ', ')"
    $lines += "- Selected deployment path count: $($Template.Summary.SelectedDeploymentPathCount)"
    $lines += "- Pending evidence count: $($Template.Summary.PendingEvidenceCount)"
    $lines += "- Current ready to claim deployment: $($Template.Summary.ReadyToClaimRealSensorDeployment)"
    $lines += "- Connects to external endpoint: $($Template.Summary.ConnectsToExternalEndpoint)"
    $lines += "- Runs SDK hardware: $($Template.Summary.RunsSdkHardware)"
    $lines += "- Does not connect to broker: $($Template.Summary.DoesNotConnectToBroker)"
    $lines += "- Does not connect to SDK: $($Template.Summary.DoesNotConnectToSdk)"
    $lines += "- Writes endpoint values: $($Template.Summary.WritesEndpointValues)"
    $lines += "- Writes credential values: $($Template.Summary.WritesCredentialValues)"
    $lines += ""
    $lines += "## Deployment Metadata"
    $lines += ""
    $lines += "- Evidence run id:"
    $lines += "- Environment name:"
    $lines += "- Operator:"
    $lines += "- Map name:"
    $lines += "- PIE/session id:"
    $lines += "- Selected input path:"
    $lines += "- Broker smoke report path:"
    $lines += "- HTTP/UDP smoke report path:"
    $lines += "- SDK smoke report path:"
    $lines += "- Log path:"
    $lines += "- Secret scan result path:"
    $lines += ""
    $lines += "## Deployment Path Evidence Sections"
    $lines += ""
    $lines += "| Path | Selected | Purpose | Required evidence | Ready to claim production deployment |"
    $lines += "| --- | --- | --- | --- | --- |"
    foreach ($section in $Template.DeploymentPathEvidenceSections.PSObject.Properties) {
        $required = @($section.Value.RequiredEvidence) -join "; "
        $lines += "| $($section.Name) | $($section.Value.Selected) | $($section.Value.Purpose) | $required | $($section.Value.ReadyToClaimProductionDeployment) |"
    }
    $lines += ""
    $lines += "## Required Evidence"
    $lines += ""
    $lines += "| Item | Owner | Status | Required evidence | Evidence path | Reviewer | Reviewed UTC | Notes |"
    $lines += "| --- | --- | --- | --- | --- | --- | --- | --- |"
    foreach ($item in $Template.RequiredEvidence) {
        $lines += "| $($item.Name) | $($item.Owner) | $($item.Status) | $($item.RequiredEvidence) | $($item.EvidencePath) | $($item.Reviewer) | $($item.ReviewedAtUtc) | $($item.Notes) |"
    }
    $lines += ""
    $lines += "## Safety Boundary"
    $lines += ""
    $lines += $Template.SafetyBoundary

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $lines -Encoding UTF8
}

$ProjectRoot = Resolve-ProjectRoot
$requiredEvidence = @(
    (New-EvidenceItem -Name "SelectedDeploymentPath" -RequiredEvidence "Deployment owner records the accepted production input path: replay, HTTP, WebSocket, UDP, ROS2, Livox, or RealSense, without endpoint or credential values."),
    (New-EvidenceItem -Name "BrokerPieSmoke" -RequiredEvidence "For WebSocket broker deployment, run id, map, PIE session, log path, source frame before/after, target point count, cached payload bytes/hash, broker client command, operator, notes, and external evidence path are recorded without broker URL/topic values."),
    (New-EvidenceItem -Name "HttpDeploymentSmoke" -RequiredEvidence "For HTTP deployment, bind/route ownership, response evidence, rate/payload policy, log path, operator, and external evidence path are recorded without raw endpoint values."),
    (New-EvidenceItem -Name "UdpDeploymentSmoke" -RequiredEvidence "For UDP deployment, bind/port ownership, packet size/loss policy, datagram smoke evidence, log path, operator, and external evidence path are recorded without raw endpoint values."),
    (New-EvidenceItem -Name "SdkRos2Evidence" -RequiredEvidence "For ROS2 deployment, topic/message schema, bridge command, timestamp/calibration policy, real-frame smoke, and external evidence path are recorded, or explicitly accepted as not selected."),
    (New-EvidenceItem -Name "LivoxEvidence" -RequiredEvidence "For Livox deployment, SDK/device version, packet normalization, calibration/timestamp policy, real-frame smoke, and external evidence path are recorded, or explicitly accepted as not selected."),
    (New-EvidenceItem -Name "RealSenseEvidence" -RequiredEvidence "For RealSense deployment, SDK/device version, camera/depth schema, calibration/timestamp policy, real-frame smoke, and external evidence path are recorded, or explicitly accepted as not selected."),
    (New-EvidenceItem -Name "JudgingServerHandoff" -RequiredEvidence "Captured payload from the selected real-sensor path is sent to the judging-server transport or saved payload path and accepted by the owner."),
    (New-EvidenceItem -Name "CredentialRedaction" -RequiredEvidence "Generated reports and evidence files are scanned for endpoint URLs, tokens, API keys, passwords, secrets, and credential values; owner-held endpoint references are used instead of raw values."),
    (New-EvidenceItem -Name "OwnerAcceptance" -RequiredEvidence "Deployment owner accepts the selected path and records reviewer/date/source evidence.")
)

$deploymentPathSections = [PSCustomObject]@{
    ReplayBaseline = [PSCustomObject]@{
        Selected = $false
        Purpose = "Use saved CSV/JSONL frames as the baseline normalized-frame and payload-contract path."
        RequiredEvidence = @("Replay fixture path", "Target LiDAR/camera handoff observation", "Saved payload or recorder output", "Judging-server handoff evidence when used for acceptance")
        ReadyToClaimProductionDeployment = $false
    }
    HttpJsonLive = [PSCustomObject]@{
        Selected = $false
        Purpose = "Receive normalized JSON frames through the DT-Project HTTP bridge."
        RequiredEvidence = @("Endpoint ownership reference without raw URL", "Bind/firewall/auth policy", "Live POST smoke evidence", "Rate and payload-size policy", "Judging-server handoff evidence")
        ReadyToClaimProductionDeployment = $false
    }
    WebSocketDTCore = [PSCustomObject]@{
        Selected = $false
        Purpose = "Route deployment broker frames through DTCore WebSocket transaction handling into DT-Project live source components."
        RequiredEvidence = @("Broker ownership reference without raw URL/topic", "DT_TransactionCode row evidence", "Deployment broker PIE smoke", "Source frame before/after counts", "Target point count and cached payload evidence")
        ReadyToClaimProductionDeployment = $false
    }
    UdpJsonLive = [PSCustomObject]@{
        Selected = $false
        Purpose = "Receive normalized JSON datagrams through the DT-Project UDP bridge."
        RequiredEvidence = @("Bind/port ownership reference", "Packet size/loss policy", "Datagram smoke evidence", "Rate/backpressure policy", "Judging-server handoff evidence")
        ReadyToClaimProductionDeployment = $false
    }
    Ros2Bridge = [PSCustomObject]@{
        Selected = $false
        Purpose = "Normalize ROS2 topic messages into the shared LiDAR/camera handoff path."
        RequiredEvidence = @("Topic/message schema", "Bridge command", "Timestamp/calibration policy", "Real-frame smoke evidence", "Owner acceptance or not-selected acceptance")
        ReadyToClaimProductionDeployment = $false
    }
    LivoxSdk = [PSCustomObject]@{
        Selected = $false
        Purpose = "Normalize Livox SDK packet streams into FVirtualLidarPoint frames."
        RequiredEvidence = @("SDK/device version", "Packet normalization mapping", "Calibration/timestamp policy", "Real-frame smoke evidence", "Owner acceptance or not-selected acceptance")
        ReadyToClaimProductionDeployment = $false
    }
    RealSenseSdk = [PSCustomObject]@{
        Selected = $false
        Purpose = "Normalize RealSense camera/depth frames into the virtual camera or LiDAR-compatible payload path."
        RequiredEvidence = @("SDK/device version", "Camera/depth schema", "Calibration/timestamp policy", "Real-frame smoke evidence", "Owner acceptance or not-selected acceptance")
        ReadyToClaimProductionDeployment = $false
    }
}
$deploymentPathNames = @($deploymentPathSections.PSObject.Properties | Select-Object -ExpandProperty Name)
$pendingEvidence = @($requiredEvidence | Where-Object { $_.Status -ne "Recorded" })
$template = [PSCustomObject]@{
    SchemaVersion = "RealSensorAdapterDeploymentEvidenceV1"
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    DeploymentMetadata = [PSCustomObject]@{
        EvidenceRunId = ""
        EnvironmentName = ""
        Operator = ""
        MapName = ""
        PieOrSessionId = ""
        SelectedInputPath = ""
        BrokerSmokeReportPath = ""
        HttpUdpSmokeReportPath = ""
        SdkSmokeReportPath = ""
        LogPath = ""
        SecretScanResultPath = ""
        OwnerHeldEndpointReference = ""
        RedactionNotes = ""
    }
    PreDeploymentEvidence = [PSCustomObject]@{
        BrokerlessDTCoreDispatch = "Brokerless/sample/commandlet evidence is useful pre-deployment evidence, but it is not deployment broker PIE smoke."
        SampleValidation = "Static sample validation is not a substitute for external endpoint or hardware evidence."
    }
    DeploymentPathEvidenceSections = $deploymentPathSections
    RequiredEvidence = $requiredEvidence
    SafetyBoundary = "This template records deployment evidence only. It does not connect to brokers or SDKs, does not modify assets, does not stage files, and must not contain endpoint or credential values."
    Summary = [PSCustomObject]@{
        RequiredEvidenceCount = $requiredEvidence.Count
        DeploymentPathSectionCount = $deploymentPathNames.Count
        DeploymentPathSections = $deploymentPathNames
        SelectedDeploymentPathCount = 0
        PendingEvidenceCount = $pendingEvidence.Count
        ReadyToClaimRealSensorDeployment = $false
        ConnectsToExternalEndpoint = $false
        RunsSdkHardware = $false
        DoesNotConnectToBroker = $true
        DoesNotConnectToSdk = $true
        ModifiesAssets = $false
        StagesFiles = $false
        WritesEndpointValues = $false
        WritesCredentialValues = $false
    }
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    if ([System.IO.Path]::GetExtension($OutputPath).ToLowerInvariant() -eq ".json") {
        $parent = Split-Path -Parent $OutputPath
        if (-not [string]::IsNullOrWhiteSpace($parent)) {
            New-Item -ItemType Directory -Force -Path $parent | Out-Null
        }
        $template | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
    }
    else {
        Write-MarkdownTemplate -Template $template -Path $OutputPath
    }
}

if ($Json) {
    $template | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Real sensor adapter deployment evidence template generated."
    Write-Host "Required evidence count: $($template.Summary.RequiredEvidenceCount)"
    Write-Host "Pending evidence count: $($template.Summary.PendingEvidenceCount)"
    Write-Host "Ready to claim deployment: $($template.Summary.ReadyToClaimRealSensorDeployment)"
    if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
        Write-Host "Output: $OutputPath"
    }
}
