param(
    [string]$ProjectRoot = "",
    [string]$EvidencePath = "",
    [switch]$FailOnIncompleteEvidence,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }
    return (Split-Path -Parent $PSScriptRoot)
}

function Test-NonEmptyString {
    param([object]$Value)
    return -not [string]::IsNullOrWhiteSpace([string]$Value)
}

function Test-CompletedStatus {
    param([object]$Value)
    return @("Recorded", "Complete", "Passed", "Accepted", "NotSelectedAccepted") -contains [string]$Value
}

function Resolve-EvidenceFile {
    param(
        [string]$ProjectRoot,
        [object]$Value
    )

    if (-not (Test-NonEmptyString $Value)) {
        return ""
    }
    $pathText = [string]$Value
    if ([System.IO.Path]::IsPathRooted($pathText)) {
        return $pathText
    }
    return (Join-Path $ProjectRoot $pathText)
}

function Test-EvidenceFilePath {
    param(
        [string]$ProjectRoot,
        [object]$Value
    )

    $candidate = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $Value
    return ((Test-NonEmptyString $candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf))
}

function Add-Check {
    param(
        [System.Collections.Generic.List[object]]$Checks,
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $Checks.Add([PSCustomObject]@{
        Name = $Name
        Passed = $Passed
        Detail = $Detail
    }) | Out-Null
}

function Get-SensitivePatternHits {
    param([string[]]$Paths)

    $patterns = @(
        "(?i)Bearer\s+[A-Za-z0-9._\-]+",
        "(?i)(api[_-]?key|token|password|secret|credential)\s*[:=]\s*[^`r`n\s]+",
        "(?i)(https?|wss?)://[^\s)]+"
    )
    $hits = @()
    foreach ($path in $Paths) {
        if ([string]::IsNullOrWhiteSpace($path) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
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
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $ProjectRoot "Saved\Reports\RealSensorAdapterDeployment\real_sensor_adapter_deployment.evidence.json"
}

$evidenceFilePresent = Test-Path -LiteralPath $EvidencePath -PathType Leaf
$evidence = $null
if ($evidenceFilePresent) {
    $evidence = Get-Content -LiteralPath $EvidencePath -Raw | ConvertFrom-Json
}

$metadata = if ($evidence -and $evidence.PSObject.Properties.Name -contains "DeploymentMetadata") { $evidence.DeploymentMetadata } else { $null }
$deploymentPathSections = if ($evidence -and $evidence.PSObject.Properties.Name -contains "DeploymentPathEvidenceSections") { $evidence.DeploymentPathEvidenceSections } else { $null }
$requiredEvidence = if ($evidence -and $evidence.PSObject.Properties.Name -contains "RequiredEvidence") { @($evidence.RequiredEvidence) } else { @() }
$requiredDeploymentPathNames = @(
    "ReplayBaseline",
    "HttpJsonLive",
    "WebSocketDTCore",
    "UdpJsonLive",
    "Ros2Bridge",
    "LivoxSdk",
    "RealSenseSdk"
)
$requiredNames = @(
    "SelectedDeploymentPath",
    "BrokerPieSmoke",
    "HttpDeploymentSmoke",
    "UdpDeploymentSmoke",
    "SdkRos2Evidence",
    "LivoxEvidence",
    "RealSenseEvidence",
    "JudgingServerHandoff",
    "CredentialRedaction",
    "OwnerAcceptance"
)

$checks = [System.Collections.Generic.List[object]]::new()
Add-Check -Checks $checks -Name "Evidence file present" -Passed $evidenceFilePresent -Detail $EvidencePath
Add-Check -Checks $checks -Name "Schema version is RealSensorAdapterDeploymentEvidenceV1" -Passed ($evidence -and [string]$evidence.SchemaVersion -eq "RealSensorAdapterDeploymentEvidenceV1") -Detail "SchemaVersion=$($evidence.SchemaVersion)"
Add-Check -Checks $checks -Name "Deployment metadata present" -Passed ($null -ne $metadata) -Detail "DeploymentMetadata"
Add-Check -Checks $checks -Name "Deployment path evidence sections present" -Passed ($null -ne $deploymentPathSections) -Detail "DeploymentPathEvidenceSections"

$selectedDeploymentPathCount = 0
if ($deploymentPathSections) {
    foreach ($deploymentPathName in $requiredDeploymentPathNames) {
        $sectionPresent = ($deploymentPathSections.PSObject.Properties.Name -contains $deploymentPathName)
        Add-Check -Checks $checks -Name "$deploymentPathName deployment path section present" -Passed $sectionPresent -Detail $deploymentPathName
        if ($sectionPresent) {
            $section = $deploymentPathSections.$deploymentPathName
            if ([bool]$section.Selected) {
                $selectedDeploymentPathCount++
            }
            Add-Check -Checks $checks -Name "$deploymentPathName does not claim production deployment" -Passed (-not [bool]$section.ReadyToClaimProductionDeployment) -Detail "ReadyToClaimProductionDeployment=$($section.ReadyToClaimProductionDeployment)"
        }
    }
}

if ($metadata) {
    foreach ($field in @("EvidenceRunId", "EnvironmentName", "Operator", "MapName", "PieOrSessionId", "SelectedInputPath", "LogPath", "SecretScanResultPath", "OwnerHeldEndpointReference", "RedactionNotes")) {
        Add-Check -Checks $checks -Name "$field recorded" -Passed (Test-NonEmptyString $metadata.$field) -Detail $field
    }
    Add-Check -Checks $checks -Name "At least one smoke report path exists" -Passed (
        (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.BrokerSmokeReportPath) -or
        (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.HttpUdpSmokeReportPath) -or
        (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.SdkSmokeReportPath)
    ) -Detail "Broker/HTTP-UDP/SDK smoke report path"
    Add-Check -Checks $checks -Name "Log path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.LogPath) -Detail "LogPath=$($metadata.LogPath)"
    Add-Check -Checks $checks -Name "Secret scan result path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $metadata.SecretScanResultPath) -Detail "SecretScanResultPath=$($metadata.SecretScanResultPath)"
}

$evidencePaths = @()
foreach ($requiredName in $requiredNames) {
    $item = @($requiredEvidence | Where-Object { [string]$_.Name -eq $requiredName } | Select-Object -First 1)
    $present = ($item.Count -gt 0)
    $complete = $present -and (Test-CompletedStatus $item[0].Status) -and (Test-NonEmptyString $item[0].EvidencePath) -and (Test-NonEmptyString $item[0].Reviewer) -and (Test-NonEmptyString $item[0].ReviewedAtUtc) -and (Test-NonEmptyString $item[0].Notes)
    Add-Check -Checks $checks -Name "$requiredName evidence complete" -Passed $complete -Detail $(if ($present) { "Status=$($item[0].Status) EvidencePath=$($item[0].EvidencePath)" } else { "Missing" })
    if ($present) {
        $resolved = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $item[0].EvidencePath
        if (Test-NonEmptyString $resolved) {
            $evidencePaths += $resolved
        }
        Add-Check -Checks $checks -Name "$requiredName evidence path exists" -Passed (Test-EvidenceFilePath -ProjectRoot $ProjectRoot -Value $item[0].EvidencePath) -Detail "EvidencePath=$($item[0].EvidencePath)"
    }
}

$sensitivePaths = @($EvidencePath) + $evidencePaths
if ($metadata) {
    foreach ($field in @("BrokerSmokeReportPath", "HttpUdpSmokeReportPath", "SdkSmokeReportPath", "LogPath", "SecretScanResultPath")) {
        $resolved = Resolve-EvidenceFile -ProjectRoot $ProjectRoot -Value $metadata.$field
        if (Test-NonEmptyString $resolved) {
            $sensitivePaths += $resolved
        }
    }
}
$sensitiveHits = @(Get-SensitivePatternHits -Paths @($sensitivePaths | Select-Object -Unique))
Add-Check -Checks $checks -Name "No sensitive endpoint or credential patterns" -Passed ($sensitiveHits.Count -eq 0) -Detail "SensitivePatternHitCount=$($sensitiveHits.Count)"

$brokerPieSmoke = @($requiredEvidence | Where-Object { [string]$_.Name -eq "BrokerPieSmoke" } | Select-Object -First 1)
$sdkEvidence = @($requiredEvidence | Where-Object { @("SdkRos2Evidence", "LivoxEvidence", "RealSenseEvidence") -contains [string]$_.Name })
$brokerPieSmokeEvidencePresent = ($brokerPieSmoke.Count -gt 0 -and (Test-CompletedStatus $brokerPieSmoke[0].Status) -and (Test-NonEmptyString $brokerPieSmoke[0].EvidencePath))
$sdkHardwareEvidencePresent = (@($sdkEvidence | Where-Object { (Test-CompletedStatus $_.Status) -and [string]$_.Status -ne "NotSelectedAccepted" -and (Test-NonEmptyString $_.EvidencePath) }).Count -gt 0)

$failedChecks = @($checks | Where-Object { -not $_.Passed })
$complete = ($failedChecks.Count -eq 0)
$report = [PSCustomObject]@{
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    EvidencePath = $EvidencePath
    Checks = $checks
    SensitivePatternHits = $sensitiveHits
    Summary = [PSCustomObject]@{
        EvidenceFilePresent = [bool]$evidenceFilePresent
        RequiredEvidenceCount = $requiredNames.Count
        DeploymentPathSectionCount = if ($deploymentPathSections) { [int]@($deploymentPathSections.PSObject.Properties).Count } else { 0 }
        RequiredDeploymentPathSectionCount = $requiredDeploymentPathNames.Count
        SelectedDeploymentPathCount = [int]$selectedDeploymentPathCount
        PassedCheckCount = [int]($checks.Count - $failedChecks.Count)
        FailedCheckCount = [int]$failedChecks.Count
        SensitivePatternHitCount = [int]$sensitiveHits.Count
        Complete = [bool]$complete
        BrokerPieSmokeEvidencePresent = [bool]$brokerPieSmokeEvidencePresent
        SdkHardwareEvidencePresent = [bool]$sdkHardwareEvidencePresent
        PreDeploymentEvidenceOnly = -not [bool]$complete
        BrokerlessDispatchIsDeploymentEvidence = $false
        LoopbackSmokeIsDeploymentBrokerEvidence = $false
        StaticTransactionRegistrationIsBrokerAcceptance = $false
        RealBrokerOrSdkAcceptanceEvidencePresent = ([bool]$brokerPieSmokeEvidencePresent -or [bool]$sdkHardwareEvidencePresent)
        ExternalBrokerSmokeEvidencePresent = [bool]$brokerPieSmokeEvidencePresent
        ExternalSdkHardwareEvidencePresent = [bool]$sdkHardwareEvidencePresent
        DeploymentBrokerAcceptanceComplete = ([bool]$complete -and [bool]$brokerPieSmokeEvidencePresent)
        SdkDeploymentAcceptanceComplete = ([bool]$complete -and [bool]$sdkHardwareEvidencePresent)
        AcceptancePackageIsEvidenceShell = $true
        AcceptancePackageIsDeploymentProof = $false
        GeneratedReportDoesNotMeanDeploymentPassed = -not [bool]$complete
        CurrentReadyToClaimRealSensorDeployment = [bool]$complete
        ReadyToClaimRealSensorDeployment = [bool]$complete
        FailOnIncompleteEvidence = [bool]$FailOnIncompleteEvidence
        ExternalConnectionAttempted = $false
        CredentialValuesWritten = $false
        DoesNotConnectToBroker = $true
        DoesNotConnectToSdk = $true
        DoesNotWriteEndpointValues = $true
        DoesNotWriteCredentialValues = $true
        DoesNotModifyDTCore = $true
        ModifiesAssets = $false
        StagesFiles = $false
        WritesEndpointValues = $false
        WritesCredentialValues = $false
        Boundary = "Real sensor deployment remains unclaimed until selected path, policy, live smoke, transport/SDK evidence, judging-server handoff, secret scan, and owner acceptance are recorded."
        Valid = $true
    }
}

if ($FailOnIncompleteEvidence -and -not $complete) {
    throw "Real sensor adapter deployment evidence is incomplete. FailedCheckCount=$($failedChecks.Count)"
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Real sensor adapter deployment evidence checked."
    Write-Host "Evidence file present: $($report.Summary.EvidenceFilePresent)"
    Write-Host "Passed checks: $($report.Summary.PassedCheckCount)"
    Write-Host "Failed checks: $($report.Summary.FailedCheckCount)"
    Write-Host "Sensitive pattern hit count: $($report.Summary.SensitivePatternHitCount)"
    Write-Host "Ready to claim deployment: $($report.Summary.CurrentReadyToClaimRealSensorDeployment)"
    Write-Host "Boundary: $($report.Summary.Boundary)"
}
