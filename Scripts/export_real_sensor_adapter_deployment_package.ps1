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

function Get-SensitivePatternHits {
    param([string[]]$Paths)

    $patterns = @(
        "Bearer\s+[A-Za-z0-9._\-]+",
        "(?i)(token|password|secret|credential)\s*[:=]\s*[^`r`n\s]+",
        "https?://[^\s)]+"
    )
    $hits = @()
    foreach ($path in $Paths) {
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            continue
        }
        $text = Get-Content -LiteralPath $path -Raw
        foreach ($pattern in $patterns) {
            foreach ($match in [regex]::Matches($text, $pattern)) {
                if ($pattern -like "http*" -and $match.Value -match "^https?://(127\.0\.0\.1|localhost|\[::1\])") {
                    continue
                }
                $hits += [PSCustomObject]@{
                    Path = $path
                    Pattern = $pattern
                    Match = $match.Value
                }
            }
        }
    }
    return $hits
}

$ProjectRoot = Resolve-ProjectRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\RealSensorAdapterDeployment"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$readinessScript = Join-Path $ProjectRoot "Scripts\export_real_sensor_adapter_readiness_report.ps1"
$planValidatorScript = Join-Path $ProjectRoot "Scripts\validate_real_sensor_adapter_plan.ps1"
$sampleValidatorScript = Join-Path $ProjectRoot "Scripts\validate_websocket_lidar_live_sample.ps1"
$registrationScript = Join-Path $ProjectRoot "Scripts\export_websocket_transaction_registration_report.ps1"
$brokerSmokeScript = Join-Path $ProjectRoot "Scripts\export_websocket_broker_smoke_report.ps1"
$smokeWorkflowScript = Join-Path $ProjectRoot "Scripts\run_websocket_lidar_smoke_evidence.ps1"
$templateScript = Join-Path $ProjectRoot "Scripts\export_real_sensor_adapter_deployment_template.ps1"
$validatorScript = Join-Path $ProjectRoot "Scripts\validate_real_sensor_adapter_deployment_evidence.ps1"

$readinessJsonPath = Join-Path $OutputRoot "real_sensor_adapter_readiness.json"
$readinessMarkdownPath = Join-Path $OutputRoot "real_sensor_adapter_readiness.md"
$planJsonPath = Join-Path $OutputRoot "real_sensor_adapter_plan_validation.json"
$sampleJsonPath = Join-Path $OutputRoot "websocket_lidar_live_sample_validation.json"
$registrationRoot = Join-Path $OutputRoot "WebSocketTransactionRegistration"
$brokerRoot = Join-Path $OutputRoot "WebSocketBrokerSmoke"
$evidenceJsonPath = Join-Path $OutputRoot "real_sensor_adapter_deployment.evidence.json"
$evidenceMarkdownPath = Join-Path $OutputRoot "real_sensor_adapter_deployment_template.md"
$validationJsonPath = Join-Path $OutputRoot "real_sensor_adapter_deployment_validation.json"
$manifestJsonPath = Join-Path $OutputRoot "real_sensor_adapter_deployment_package.json"
$manifestMarkdownPath = Join-Path $OutputRoot "README.md"

$readiness = Invoke-JsonScript -ScriptPath $readinessScript -Parameters @{
    ProjectRoot = $ProjectRoot
    JsonPath = $readinessJsonPath
    MarkdownPath = $readinessMarkdownPath
}
$plan = Invoke-JsonScript -ScriptPath $planValidatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
}
$sample = Invoke-JsonScript -ScriptPath $sampleValidatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
}
$registration = Invoke-JsonScript -ScriptPath $registrationScript -Parameters @{
    ProjectRoot = $ProjectRoot
    OutputRoot = $registrationRoot
}
$brokerSmoke = Invoke-JsonScript -ScriptPath $brokerSmokeScript -Parameters @{
    ProjectRoot = $ProjectRoot
    OutputRoot = $brokerRoot
}
$template = Invoke-JsonScript -ScriptPath $templateScript -Parameters @{
    ProjectRoot = $ProjectRoot
    OutputPath = $evidenceJsonPath
}
& powershell -ExecutionPolicy Bypass -File $templateScript -ProjectRoot $ProjectRoot -OutputPath $evidenceMarkdownPath | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Real sensor deployment markdown template generation failed with exit code $LASTEXITCODE"
}
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    EvidencePath = $evidenceJsonPath
}
$validation | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $validationJsonPath -Encoding UTF8

$plan | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $planJsonPath -Encoding UTF8
$sample | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $sampleJsonPath -Encoding UTF8

$manualSteps = @(
    "Use file replay as the baseline schema path while live deployment ownership is open.",
    "Choose the production live path: HTTP JSON, WebSocket via DTCore, UDP JSON, or hardware SDK/ROS2.",
    "For WebSocket, fill the broker smoke evidence fields with run id, map, PIE session, log path, source frames, target points, cached payload bytes/hash, client command, operator, and notes.",
    "For HTTP/UDP, record bind address, port/firewall, trust/auth, rate, payload-size, and deployment smoke evidence outside secrets.",
    "For ROS2/Livox/RealSense, record SDK dependency, device, calibration, timestamp, and real-frame smoke evidence.",
    "Do not mark deployment ready until the selected path has observed live frames and owner acceptance."
)

$followUpCommands = @(
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f $readinessScript, $ProjectRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -Json' -f $planValidatorScript, $ProjectRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -NoWrite -Json' -f $smokeWorkflowScript, $ProjectRoot),
    ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -WriteReports -ObservedSourceFrame -ObservedTargetPoints -ObservedCachedPayload -Operator "<operator>" -EvidenceRunId "<run-id>"' -f $smokeWorkflowScript, $ProjectRoot)
)

$generatedFiles = @(
    $readinessJsonPath,
    $readinessMarkdownPath,
    $planJsonPath,
    $sampleJsonPath,
    [string]$registration.Summary.LatestJsonPath,
    [string]$registration.Summary.LatestMarkdownPath,
    [string]$brokerSmoke.Summary.LatestJsonPath,
    [string]$brokerSmoke.Summary.LatestMarkdownPath,
    $evidenceJsonPath,
    $evidenceMarkdownPath,
    $validationJsonPath,
    $manifestJsonPath,
    $manifestMarkdownPath
)
$sensitiveHits = @(Get-SensitivePatternHits -Paths $generatedFiles)

$manifest = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    OutputRoot = $OutputRoot
    DryRunOnly = $true
    DoesNotConnectToBroker = $true
    DoesNotConnectToSdk = $true
    ExternalConnectionAttempted = $false
    CredentialValuesWritten = $false
    ModifiesAssets = $false
    StagesFiles = $false
    WritesEndpointValues = $false
    WritesCredentialValues = $false
    ReadinessSummary = $readiness.Summary
    PlanValidationSummary = $plan.Summary
    SampleValidationSummary = $sample.Summary
    TransactionRegistrationSummary = $registration.Summary
    BrokerSmokeSummary = $brokerSmoke.Summary
    TemplateSummary = $template.Summary
    ValidationSummary = $validation.Summary
    ManualSteps = $manualSteps
    FollowUpCommands = $followUpCommands
    GeneratedFiles = $generatedFiles
    SensitivePatternHits = $sensitiveHits
    Summary = [PSCustomObject]@{
        PackageCreated = $true
        OutputRoot = $OutputRoot
        StaticAdapterPlanValid = [bool]$readiness.Summary.StaticAdapterPlanValid
        WebSocketSampleValid = [bool]$readiness.Summary.WebSocketSampleValid
        TransactionRegistrationStaticValid = [bool]$readiness.Summary.TransactionRegistrationStaticValid
        BrokerSmokeReadOnlyValid = [bool]$readiness.Summary.BrokerSmokeReadOnlyValid
        BrokerSmokeEvidenceFieldsComplete = [bool]$readiness.Summary.BrokerSmokeEvidenceFieldsComplete
        BrokerSmokeMissingEvidenceFieldCount = [int]$readiness.Summary.BrokerSmokeMissingEvidenceFieldCount
        DeploymentEvidenceStillRequired = [bool]$readiness.Summary.DeploymentEvidenceStillRequired
        RealSdkIntegrationStillRequired = [bool]$readiness.Summary.RealSdkIntegrationStillRequired
        DeploymentEvidenceTemplateCreated = (Test-Path -LiteralPath $evidenceJsonPath -PathType Leaf)
        DeploymentEvidenceFilePresent = [bool]$validation.Summary.EvidenceFilePresent
        DeploymentEvidenceComplete = [bool]$validation.Summary.Complete
        DeploymentEvidenceMissingCount = [int]$validation.Summary.FailedCheckCount
        DeploymentPathSectionCount = [int]$template.Summary.DeploymentPathSectionCount
        RequiredDeploymentPathSections = @($template.Summary.DeploymentPathSections)
        SelectedDeploymentPathCount = [int]$template.Summary.SelectedDeploymentPathCount
        CurrentReadyToClaimRealSensorDeployment = [bool]$validation.Summary.CurrentReadyToClaimRealSensorDeployment
        ReadyToClaimRealSensorDeployment = [bool]$validation.Summary.ReadyToClaimRealSensorDeployment
        BrokerPieSmokeEvidencePresent = [bool]$validation.Summary.BrokerPieSmokeEvidencePresent
        SdkHardwareEvidencePresent = [bool]$validation.Summary.SdkHardwareEvidencePresent
        BlockedDeploymentCandidateCount = [int]$readiness.Summary.BlockedDeploymentCandidateCount
        SensitivePatternHitCount = $sensitiveHits.Count
        DryRunOnly = $true
        DoesNotConnectToBroker = $true
        DoesNotConnectToSdk = $true
        ExternalConnectionAttempted = $false
        CredentialValuesWritten = $false
        ModifiesAssets = $false
        StagesFiles = $false
        WritesEndpointValues = $false
        WritesCredentialValues = $false
        Boundary = "This package collects real-sensor adapter deployment evidence drafts only. It never connects to the external broker or SDKs, never modifies assets, never stages files, and never writes endpoint or credential values."
        Valid = ([bool]$readiness.Summary.Valid -and [bool]$plan.Summary.Valid -and [bool]$sample.Summary.Valid -and [bool]$registration.Summary.Valid -and [bool]$brokerSmoke.Summary.Valid -and $sensitiveHits.Count -eq 0)
    }
}

$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestJsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Real Sensor Adapter Deployment Package") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated UTC: $($manifest.GeneratedUtc)") | Out-Null
$lines.Add("- Project: $($manifest.ProjectRoot)") | Out-Null
$lines.Add("- Output root: $($manifest.OutputRoot)") | Out-Null
$lines.Add("- Static adapter plan valid: $($manifest.Summary.StaticAdapterPlanValid)") | Out-Null
$lines.Add("- WebSocket sample valid: $($manifest.Summary.WebSocketSampleValid)") | Out-Null
$lines.Add("- Broker smoke evidence fields complete: $($manifest.Summary.BrokerSmokeEvidenceFieldsComplete)") | Out-Null
$lines.Add("- Broker smoke missing evidence fields: $($manifest.Summary.BrokerSmokeMissingEvidenceFieldCount)") | Out-Null
$lines.Add("- Deployment evidence still required: $($manifest.Summary.DeploymentEvidenceStillRequired)") | Out-Null
$lines.Add("- Real SDK integration still required: $($manifest.Summary.RealSdkIntegrationStillRequired)") | Out-Null
$lines.Add("- Deployment evidence template created: $($manifest.Summary.DeploymentEvidenceTemplateCreated)") | Out-Null
$lines.Add("- Deployment evidence complete: $($manifest.Summary.DeploymentEvidenceComplete)") | Out-Null
$lines.Add("- Deployment evidence missing checks: $($manifest.Summary.DeploymentEvidenceMissingCount)") | Out-Null
$lines.Add("- Deployment path sections: $($manifest.Summary.RequiredDeploymentPathSections -join ', ')") | Out-Null
$lines.Add("- Deployment path section count: $($manifest.Summary.DeploymentPathSectionCount)") | Out-Null
$lines.Add("- Selected deployment path count: $($manifest.Summary.SelectedDeploymentPathCount)") | Out-Null
$lines.Add("- Ready to claim real sensor deployment: $($manifest.Summary.CurrentReadyToClaimRealSensorDeployment)") | Out-Null
$lines.Add("- Broker PIE smoke evidence present: $($manifest.Summary.BrokerPieSmokeEvidencePresent)") | Out-Null
$lines.Add("- SDK hardware evidence present: $($manifest.Summary.SdkHardwareEvidencePresent)") | Out-Null
$lines.Add("- Blocked deployment candidates: $($manifest.Summary.BlockedDeploymentCandidateCount)") | Out-Null
$lines.Add("- Sensitive pattern hit count: $($manifest.Summary.SensitivePatternHitCount)") | Out-Null
$lines.Add("- Dry run only: $($manifest.DryRunOnly)") | Out-Null
$lines.Add("- Does not connect to broker: $($manifest.DoesNotConnectToBroker)") | Out-Null
$lines.Add("- Does not connect to SDK: $($manifest.DoesNotConnectToSdk)") | Out-Null
$lines.Add("- Modifies assets: $($manifest.ModifiesAssets)") | Out-Null
$lines.Add("- Stages files: $($manifest.StagesFiles)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Generated Files") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Path |") | Out-Null
$lines.Add("| --- |") | Out-Null
foreach ($file in $generatedFiles) {
    $lines.Add(("| {0} |" -f (Convert-ToMarkdownCell $file))) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Manual Steps") | Out-Null
$lines.Add("") | Out-Null
for ($index = 0; $index -lt $manualSteps.Count; $index++) {
    $lines.Add(("{0}. {1}" -f ($index + 1), $manualSteps[$index])) | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("## Follow-up Commands") | Out-Null
$lines.Add("") | Out-Null
foreach ($command in $followUpCommands) {
    $lines.Add('```powershell') | Out-Null
    $lines.Add($command) | Out-Null
    $lines.Add('```') | Out-Null
}
$lines.Add("") | Out-Null
$lines.Add("Boundary: $($manifest.Summary.Boundary)") | Out-Null

Write-TextFile -Path $manifestMarkdownPath -Lines $lines

if ($Json) {
    $manifest | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Real sensor adapter deployment package created."
    Write-Host "Output root: $($manifest.OutputRoot)"
    Write-Host "Static adapter plan valid: $($manifest.Summary.StaticAdapterPlanValid)"
    Write-Host "WebSocket sample valid: $($manifest.Summary.WebSocketSampleValid)"
    Write-Host "Broker smoke evidence fields complete: $($manifest.Summary.BrokerSmokeEvidenceFieldsComplete)"
    Write-Host "Deployment evidence still required: $($manifest.Summary.DeploymentEvidenceStillRequired)"
    Write-Host "Real SDK integration still required: $($manifest.Summary.RealSdkIntegrationStillRequired)"
    Write-Host "Deployment evidence complete: $($manifest.Summary.DeploymentEvidenceComplete)"
    Write-Host "Ready to claim real sensor deployment: $($manifest.Summary.CurrentReadyToClaimRealSensorDeployment)"
    Write-Host "Sensitive pattern hit count: $($manifest.Summary.SensitivePatternHitCount)"
}
