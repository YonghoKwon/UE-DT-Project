param(
    [string]$ProjectRoot = "",
    [string]$OutputRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }
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

    $output = @(& powershell -ExecutionPolicy Bypass -File $ScriptPath @Parameters -Json)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Convert-ToMarkdownCell {
    param([object]$Value)
    if ($null -eq $Value) { return "" }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Get-DeploymentGapAction {
    param([string]$Check)

    $phase = "Evidence shell"
    $target = "real_sensor_adapter_deployment.evidence.json"
    $action = "Regenerate the deployment package and fill the missing evidence field."

    if ($Check -match "SelectedDeploymentPath|SelectedInputPath|Selected deployment|deployment path") {
        $phase = "Selected deployment path"
        $target = "DeploymentMetadata.SelectedInputPath"
        $action = "Choose the production path: replay baseline, HTTP JSON live, DTCore WebSocket, UDP JSON, ROS2, Livox, or RealSense."
    }
    elseif ($Check -match "BrokerPieSmoke|WebSocketDTCore|Broker|broker|PIE|source frame|target point|cached payload|client command") {
        $phase = "Broker/WebSocket smoke"
        $target = "RequiredEvidence.BrokerPieSmoke"
        $action = "Run the deployment broker/PIE smoke and record run id, map, PIE session, log, source frame counts, target points, cached payload bytes/hash, and broker client command."
    }
    elseif ($Check -match "HttpDeploymentSmoke|HttpJsonLive|HTTP|bind|port|firewall|auth|rate") {
        $phase = "HTTP live smoke"
        $target = "RequiredEvidence.HttpDeploymentSmoke"
        $action = "Assign bind/port/auth/rate ownership and capture an HTTP deployment smoke report without storing endpoint secrets."
    }
    elseif ($Check -match "UdpDeploymentSmoke|UdpJsonLive|UDP|datagram|trusted") {
        $phase = "UDP live smoke"
        $target = "RequiredEvidence.UdpDeploymentSmoke"
        $action = "Use UDP only after trusted-network and packet-loss policy are accepted, then record deployment datagram smoke evidence."
    }
    elseif ($Check -match "SdkRos2Evidence|Ros2Bridge|ROS2|SDK") {
        $phase = "ROS2/SDK evidence"
        $target = "RequiredEvidence.SdkRos2Evidence"
        $action = "Record SDK/ROS2 dependency, message schema, device availability, calibration/timestamp policy, and real-frame smoke evidence."
    }
    elseif ($Check -match "Livox") {
        $phase = "Livox hardware evidence"
        $target = "RequiredEvidence.LivoxEvidence"
        $action = "Connect the accepted Livox path, record SDK/device/calibration metadata, and attach real-frame smoke evidence."
    }
    elseif ($Check -match "RealSense") {
        $phase = "RealSense hardware evidence"
        $target = "RequiredEvidence.RealSenseEvidence"
        $action = "Connect the accepted RealSense path, record camera/depth metadata, and attach real-frame smoke evidence."
    }
    elseif ($Check -match "JudgingServerHandoff|judging-server|handoff") {
        $phase = "Judging-server handoff"
        $target = "RequiredEvidence.JudgingServerHandoff"
        $action = "Record that the selected real/replayed live frame reaches the shared server payload/export path and accepted judging-server handoff."
    }
    elseif ($Check -match "CredentialRedaction|Secret|credential|endpoint|token|password|sensitive") {
        $phase = "Credential redaction"
        $target = "RequiredEvidence.CredentialRedaction"
        $action = "Attach secret-scan evidence and keep endpoint URLs, credentials, tokens, and passwords outside repository files."
    }
    elseif ($Check -match "OwnerAcceptance|Owner|Reviewer|ReviewedAtUtc") {
        $phase = "Owner acceptance"
        $target = "RequiredEvidence.OwnerAcceptance"
        $action = "Record deployment-owner acceptance only after the selected path has real broker/SDK/live smoke evidence."
    }
    elseif ($Check -match "LogPath|MapName|PieOrSessionId|EvidenceRunId|Operator") {
        $phase = "Run metadata"
        $target = "DeploymentMetadata"
        $action = "Fill evidence run id, operator, map, PIE/session id, log path, and review notes."
    }

    return [PSCustomObject]@{
        Check = $Check
        EvidencePhase = $phase
        EvidenceTarget = $target
        NextAction = $action
    }
}

$ProjectRoot = Resolve-ProjectRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\RealSensorAdapterDeployment"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
if (-not $OutputRoot.StartsWith((Join-Path $ProjectRoot "Saved"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be under ProjectRoot\Saved: $OutputRoot"
}

$packageScript = Join-Path $ProjectRoot "Scripts\export_real_sensor_adapter_deployment_package.ps1"
$validatorScript = Join-Path $ProjectRoot "Scripts\validate_real_sensor_adapter_deployment_evidence.ps1"
$evidencePath = Join-Path $OutputRoot "real_sensor_adapter_deployment.evidence.json"

$package = Invoke-JsonScript -ScriptPath $packageScript -Parameters @{
    ProjectRoot = $ProjectRoot
    OutputRoot = $OutputRoot
}
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    EvidencePath = $evidencePath
}

$missingChecks = @($validation.Checks | Where-Object { -not [bool]$_.Passed } | ForEach-Object { [string]$_.Name })
$gapActions = @($missingChecks | ForEach-Object { Get-DeploymentGapAction -Check ([string]$_) })
$phaseSummaries = @(
    $gapActions |
        Group-Object -Property EvidencePhase |
        Sort-Object -Property @{ Expression = {
            switch ($_.Name) {
                "Selected deployment path" { 0 }
                "Broker/WebSocket smoke" { 1 }
                "HTTP live smoke" { 2 }
                "Credential redaction" { 3 }
                "Judging-server handoff" { 4 }
                "Owner acceptance" { 5 }
                default { 20 }
            }
        }; Ascending = $true }, @{ Expression = "Count"; Descending = $true }, @{ Expression = "Name"; Ascending = $true } |
        ForEach-Object {
            $first = @($_.Group | Select-Object -First 1)
            [PSCustomObject]@{
                EvidencePhase = [string]$_.Name
                MissingCheckCount = [int]$_.Count
                FirstEvidenceTarget = if ($first.Count -gt 0) { [string]$first[0].EvidenceTarget } else { "" }
                FirstNextAction = if ($first.Count -gt 0) { [string]$first[0].NextAction } else { "" }
            }
        }
)
$nextAction = if ($phaseSummaries.Count -gt 0) { [string]$phaseSummaries[0].FirstNextAction } else { "Real sensor deployment evidence is complete; run the strict validator before claiming deployment readiness." }

$jsonPath = Join-Path $OutputRoot "real_sensor_adapter_gap_summary.json"
$markdownPath = Join-Path $OutputRoot "real_sensor_adapter_gap_summary.md"
$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    OutputRoot = $OutputRoot
    EvidencePath = $evidencePath
    JsonPath = $jsonPath
    MarkdownPath = $markdownPath
    DryRunOnly = $true
    DoesNotConnectToBroker = $true
    DoesNotConnectToSdk = $true
    ExternalConnectionAttempted = $false
    CredentialValuesWritten = $false
    WritesEndpointValues = $false
    WritesCredentialValues = $false
    DoesNotModifyDTCore = $true
    ModifiesAssets = $false
    StagesFiles = $false
    PackageSummary = $package.Summary
    ValidationSummary = $validation.Summary
    PhaseSummaries = $phaseSummaries
    GapActions = $gapActions
    Summary = [PSCustomObject]@{
        GapSummaryCreated = $true
        MissingEvidenceCheckCount = $missingChecks.Count
        MissingPhaseCount = $phaseSummaries.Count
        ReadyToClaimRealSensorDeployment = [bool]$validation.Summary.ReadyToClaimRealSensorDeployment
        RealBrokerOrSdkAcceptanceEvidencePresent = [bool]$package.Summary.RealBrokerOrSdkAcceptanceEvidencePresent
        SelectedDeploymentPathCount = [int]$package.Summary.SelectedDeploymentPathCount
        DeploymentPathSectionCount = [int]$package.Summary.DeploymentPathSectionCount
        SensitivePatternHitCount = [int]$package.Summary.SensitivePatternHitCount
        NextManualAction = $nextAction
        PackagePath = [string](Join-Path $OutputRoot "README.md")
        Boundary = "This summary only refreshes Saved/Reports real-sensor deployment artifacts. It never connects to brokers or SDKs, writes endpoint/credential values, modifies DTCore/assets, or stages files."
        Valid = ([bool]$package.Summary.Valid -and [bool]$validation.Summary.Valid)
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Real Sensor Adapter Gap Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated UTC: $($report.GeneratedUtc)") | Out-Null
$lines.Add("- Evidence file: $($report.EvidencePath)") | Out-Null
$lines.Add("- Missing evidence checks: $($report.Summary.MissingEvidenceCheckCount)") | Out-Null
$lines.Add("- Missing phases: $($report.Summary.MissingPhaseCount)") | Out-Null
$lines.Add("- Deployment path sections: $($report.Summary.DeploymentPathSectionCount)") | Out-Null
$lines.Add("- Selected deployment path count: $($report.Summary.SelectedDeploymentPathCount)") | Out-Null
$lines.Add("- Real broker or SDK acceptance evidence present: $($report.Summary.RealBrokerOrSdkAcceptanceEvidencePresent)") | Out-Null
$lines.Add("- Ready to claim real sensor deployment: $($report.Summary.ReadyToClaimRealSensorDeployment)") | Out-Null
$lines.Add("- Sensitive pattern hit count: $($report.Summary.SensitivePatternHitCount)") | Out-Null
$lines.Add("- Next manual action: $($report.Summary.NextManualAction)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Phase Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Phase | Missing checks | First evidence target | First next action |") | Out-Null
$lines.Add("| --- | --- | --- | --- |") | Out-Null
foreach ($phase in $phaseSummaries) {
    $lines.Add(("| {0} | {1} | {2} | {3} |" -f `
        (Convert-ToMarkdownCell $phase.EvidencePhase),
        (Convert-ToMarkdownCell $phase.MissingCheckCount),
        (Convert-ToMarkdownCell $phase.FirstEvidenceTarget),
        (Convert-ToMarkdownCell $phase.FirstNextAction))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Generated Artifacts") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Package: $($report.Summary.PackagePath)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null
Write-TextFile -Path $markdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Real sensor adapter gap summary exported."
    Write-Host "JSON: $jsonPath"
    Write-Host "Markdown: $markdownPath"
    Write-Host "Missing evidence checks: $($report.Summary.MissingEvidenceCheckCount)"
    Write-Host "Next manual action: $($report.Summary.NextManualAction)"
    Write-Host "Ready to claim real sensor deployment: $($report.Summary.ReadyToClaimRealSensorDeployment)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
