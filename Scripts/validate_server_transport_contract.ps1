param(
    [string]$ProjectRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }

    return (Split-Path -Parent $PSScriptRoot)
}

function Assert-Contains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -notmatch [regex]::Escape($Pattern)) {
        throw "$Label missing from $Path. Expected text: $Pattern"
    }
}

function New-Check {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    return [PSCustomObject]@{
        Path = $Path
        Pattern = $Pattern
        Label = $Label
    }
}

$ProjectRoot = Resolve-ProjectRoot
$transportHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualSensorDataTransportComp.h"
$transportSource = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualSensorDataTransportComp.cpp"
$transportDoc = Join-Path $ProjectRoot "docs\server_transport_contract.md"
$remainingWorkDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$editorSmokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"
$transportTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\VirtualSensorDataTransportAutomationTests.cpp"
$payloadContractReportScript = Join-Path $ProjectRoot "Scripts\export_payload_contract_report.ps1"
$judgingServerAcceptanceTemplateScript = Join-Path $ProjectRoot "Scripts\export_judging_server_acceptance_template.ps1"
$judgingServerAcceptancePackageScript = Join-Path $ProjectRoot "Scripts\export_judging_server_acceptance_package.ps1"

$requiredFiles = @($transportHeader, $transportSource, $transportDoc, $remainingWorkDoc, $editorSmokeDoc, $transportTests, $payloadContractReportScript, $judgingServerAcceptanceTemplateScript, $judgingServerAcceptancePackageScript)
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Required server transport contract file not found: $file"
    }
}

$checks = @(
    (New-Check -Path $transportHeader -Pattern "EVirtualSensorTransportMode::LogOnly" -Label "Transport default is LogOnly"),
    (New-Check -Path $transportHeader -Pattern "HttpEndpoint" -Label "HTTP endpoint setting exists"),
    (New-Check -Path $transportHeader -Pattern "HttpTimeoutSeconds" -Label "HTTP timeout setting exists"),
    (New-Check -Path $transportHeader -Pattern "bAccepted" -Label "Transport result separates accepted status"),
    (New-Check -Path $transportHeader -Pattern "ResponseBody" -Label "Transport result keeps response body"),
    (New-Check -Path $transportSource -Pattern 'Request->SetVerb(TEXT("POST"))' -Label "HTTP uses POST"),
    (New-Check -Path $transportSource -Pattern 'Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"))' -Label "HTTP content type is JSON"),
    (New-Check -Path $transportSource -Pattern 'Request->SetHeader(TEXT("X-Sensor-Id"), SensorId)' -Label "HTTP sensor id header"),
    (New-Check -Path $transportSource -Pattern 'Request->SetHeader(TEXT("X-Sensor-Type"), SensorType)' -Label "HTTP sensor type header"),
    (New-Check -Path $transportSource -Pattern "FMath::Max(1, HttpTimeoutSeconds)" -Label "HTTP timeout is clamped"),
    (New-Check -Path $transportSource -Pattern "TWeakObjectPtr<UVirtualSensorDataTransportComp>" -Label "HTTP callback uses weak component pointer"),
    (New-Check -Path $transportSource -Pattern "ResponseCode >= 200 && ResponseCode < 300" -Label "HTTP acceptance requires 2xx"),
    (New-Check -Path $transportSource -Pattern "GetContentAsString" -Label "HTTP response body captured"),
    (New-Check -Path $transportDoc -Pattern "## Decisions Still Required" -Label "Transport decisions section"),
    (New-Check -Path $transportDoc -Pattern "HTTP callback behavior" -Label "HTTP callback behavior documented"),
    (New-Check -Path $transportDoc -Pattern "bAccepted: response status is in the 2xx range" -Label "2xx acceptance documented"),
    (New-Check -Path $transportDoc -Pattern "weak component pointer" -Label "Weak callback documented"),
    (New-Check -Path $transportDoc -Pattern "Authentication header or token strategy" -Label "Auth decision documented"),
    (New-Check -Path $transportDoc -Pattern "Retry policy for timeout" -Label "Retry decision documented"),
    (New-Check -Path $transportDoc -Pattern "Batching policy for high-frequency LiDAR frames" -Label "Batching decision documented"),
    (New-Check -Path $transportDoc -Pattern "Backpressure behavior" -Label "Backpressure decision documented"),
    (New-Check -Path $transportDoc -Pattern "Response body schema and whether server-side analysis results are returned" -Label "Response schema decision documented"),
    (New-Check -Path $transportDoc -Pattern "SaveToFile" -Label "SaveToFile review path documented"),
    (New-Check -Path $transportDoc -Pattern "M7AT10.SensorTransport.HttpPostLoopbackAcceptance" -Label "Transport doc references HTTP POST loopback automation"),
    (New-Check -Path $payloadContractReportScript -Pattern "ServerAcceptanceDecisions" -Label "Payload contract report exports server acceptance decisions"),
    (New-Check -Path $payloadContractReportScript -Pattern "Endpoint URL and environment ownership" -Label "Payload contract report tracks endpoint ownership"),
    (New-Check -Path $payloadContractReportScript -Pattern "Authentication" -Label "Payload contract report tracks authentication decision"),
    (New-Check -Path $payloadContractReportScript -Pattern "Retry and timeout policy" -Label "Payload contract report tracks retry decision"),
    (New-Check -Path $payloadContractReportScript -Pattern 'Name = "Batching"' -Label "Payload contract report tracks batching decision"),
    (New-Check -Path $payloadContractReportScript -Pattern 'Name = "Backpressure"' -Label "Payload contract report tracks backpressure decision"),
    (New-Check -Path $payloadContractReportScript -Pattern "RealServerEvidenceGaps" -Label "Payload contract report tracks real server evidence gaps"),
    (New-Check -Path $payloadContractReportScript -Pattern "RealJudgingServerAcceptancePresent" -Label "Payload contract report separates real server acceptance evidence"),
    (New-Check -Path $payloadContractReportScript -Pattern "JudgingServerAcceptanceTemplateAvailable" -Label "Payload contract report exposes judging server acceptance template"),
    (New-Check -Path $payloadContractReportScript -Pattern "NoWrite" -Label "Payload contract report supports no-write summary mode"),
    (New-Check -Path (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1") -Pattern "-NoWrite -Json" -Label "Precommit summary uses no-write payload contract mode"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "ValuesRedacted" -Label "Judging server acceptance template redacts values"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "ModifiesConfig" -Label "Judging server acceptance template is read-only for config"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "StagesConfig" -Label "Judging server acceptance template does not stage config"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "Endpoint ownership" -Label "Judging server acceptance template requires endpoint ownership"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "Authentication policy" -Label "Judging server acceptance template requires auth policy"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "LiDAR accepted response" -Label "Judging server acceptance template requires LiDAR acceptance evidence"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "Camera accepted response" -Label "Judging server acceptance template requires camera acceptance evidence"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "Batching and backpressure" -Label "Judging server acceptance template requires rate/backpressure evidence"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "Secret scan" -Label "Judging server acceptance template requires secret scan evidence"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "EvidenceSections" -Label "Judging server acceptance template declares structured evidence sections"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "EndpointOwnership" -Label "Judging server acceptance template declares endpoint ownership section"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "AuthenticationPolicy" -Label "Judging server acceptance template declares authentication policy section"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "ResponseSchema" -Label "Judging server acceptance template declares response schema section"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "RealEndpointSmoke" -Label "Judging server acceptance template declares real endpoint smoke section"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "RateBackpressure" -Label "Judging server acceptance template declares rate backpressure section"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "OwnerAcceptance" -Label "Judging server acceptance template declares owner acceptance section"),
    (New-Check -Path $judgingServerAcceptanceTemplateScript -Pattern "CurrentReadyToClaimRealServerAcceptance = `$false" -Label "Judging server acceptance template does not claim real server readiness"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "Saved\Reports\JudgingServerAcceptance" -Label "Judging server acceptance package writes local Saved report bundle"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "export_payload_contract_report.ps1" -Label "Judging server acceptance package consumes payload contract report"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "validate_server_transport_contract.ps1" -Label "Judging server acceptance package validates transport contract"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "export_judging_server_acceptance_template.ps1" -Label "Judging server acceptance package exports acceptance template"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "SensitivePatternHitCount" -Label "Judging server acceptance package reports sensitive pattern hits"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "EvidenceSectionCount" -Label "Judging server acceptance package exposes evidence section count"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "RequiredEvidenceSections" -Label "Judging server acceptance package exposes required evidence section names"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "DoesNotCallServer = `$true" -Label "Judging server acceptance package does not call server"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "ConnectsToExternalEndpoint = `$false" -Label "Judging server acceptance package does not connect externally"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "RealEndpointSmokeEvidencePresent = `$false" -Label "Judging server acceptance package does not claim real endpoint smoke"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "WritesEndpointValues = `$false" -Label "Judging server acceptance package declares endpoint value boundary"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "WritesCredentialValues = `$false" -Label "Judging server acceptance package declares credential value boundary"),
    (New-Check -Path $judgingServerAcceptancePackageScript -Pattern "never calls the server, never writes endpoint or credential values" -Label "Judging server acceptance package preserves no-server-call boundary"),
    (New-Check -Path $remainingWorkDoc -Pattern "server transport contract" -Label "Remaining work references transport contract"),
    (New-Check -Path $remainingWorkDoc -Pattern "validate_server_transport_contract.ps1" -Label "Remaining work references transport gate"),
    (New-Check -Path $remainingWorkDoc -Pattern "export_judging_server_acceptance_template.ps1" -Label "Remaining work references judging server acceptance template"),
    (New-Check -Path $remainingWorkDoc -Pattern "export_judging_server_acceptance_package.ps1" -Label "Remaining work references judging server acceptance package"),
    (New-Check -Path $remainingWorkDoc -Pattern "M7AT10.SensorTransport.HttpPostLoopbackAcceptance" -Label "Remaining work references HTTP POST loopback automation"),
    (New-Check -Path $editorSmokeDoc -Pattern "SharedSensorTransport.TransportMode = LogOnly" -Label "Smoke test safe transport default documented"),
    (New-Check -Path $editorSmokeDoc -Pattern "M7AT10.SensorTransport.HttpPostLoopbackAcceptance" -Label "Smoke doc references HTTP POST loopback automation"),
    (New-Check -Path $transportTests -Pattern "M7AT10.SensorTransport.HttpPostLoopbackAcceptance" -Label "HTTP POST loopback automation test name"),
    (New-Check -Path $transportTests -Pattern "EHttpServerResponseCodes::Accepted" -Label "HTTP POST loopback covers 2xx"),
    (New-Check -Path $transportTests -Pattern "EHttpServerResponseCodes::BadRequest" -Label "HTTP POST loopback covers 4xx"),
    (New-Check -Path $transportTests -Pattern "static_cast<EHttpServerResponseCodes>(503)" -Label "HTTP POST loopback covers retryable 5xx"),
    (New-Check -Path $transportTests -Pattern "HTTP 202 callback accepted" -Label "HTTP POST loopback asserts 2xx accepted"),
    (New-Check -Path $transportTests -Pattern "HTTP 400 callback not accepted" -Label "HTTP POST loopback asserts 4xx rejected"),
    (New-Check -Path $transportTests -Pattern "persistent HTTP 503 reports retry exhaustion" -Label "HTTP POST loopback asserts exhausted 5xx retry")
)

foreach ($check in $checks) {
    Assert-Contains -Path $check.Path -Pattern $check.Pattern -Label $check.Label
}

$templateJson = & powershell -ExecutionPolicy Bypass -File $judgingServerAcceptanceTemplateScript -ProjectRoot $ProjectRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Judging server acceptance template failed with exit code $LASTEXITCODE"
}
$template = $templateJson | ConvertFrom-Json
if (-not [bool]$template.Summary.ValuesRedacted) {
    throw "Judging server acceptance template must redact values."
}
if ([bool]$template.Summary.ModifiesConfig) {
    throw "Judging server acceptance template must not modify config."
}
if ([bool]$template.Summary.StagesConfig) {
    throw "Judging server acceptance template must not stage config."
}
if ([int]$template.Summary.RequiredEvidenceCount -lt 8) {
    throw "Judging server acceptance template must expose all required evidence gates."
}

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    RequiredFiles = $requiredFiles
    Checks = $checks
    Summary = [PSCustomObject]@{
        RequiredFileCount = $requiredFiles.Count
        RequiredContractCheckCount = $checks.Count
        HttpPostShapeDocumented = $true
        HttpCallbackSafetyDocumented = $true
        HttpAcceptanceSeparated = $true
        HttpPostLoopbackAutomationDeclared = $true
        OpenServerTransportDecisionsDocumented = $true
        ServerAcceptanceDecisionReportDeclared = $true
        JudgingServerAcceptanceTemplateDeclared = $true
        JudgingServerAcceptancePackageDeclared = $true
        JudgingServerAcceptanceTemplateValuesRedacted = [bool]$template.Summary.ValuesRedacted
        JudgingServerAcceptanceTemplateStagesConfig = [bool]$template.Summary.StagesConfig
        JudgingServerAcceptanceTemplateRequiredEvidenceCount = [int]$template.Summary.RequiredEvidenceCount
        JudgingServerAcceptanceTemplateEvidenceSectionCount = [int]$template.Summary.EvidenceSectionCount
        SafeEditorDefaultDocumented = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Server transport contract policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCheckCount)"
    Write-Host "HTTP POST shape documented: $($report.Summary.HttpPostShapeDocumented)"
    Write-Host "HTTP callback safety documented: $($report.Summary.HttpCallbackSafetyDocumented)"
    Write-Host "HTTP acceptance separated: $($report.Summary.HttpAcceptanceSeparated)"
    Write-Host "HTTP POST loopback automation declared: $($report.Summary.HttpPostLoopbackAutomationDeclared)"
    Write-Host "Open server transport decisions documented: $($report.Summary.OpenServerTransportDecisionsDocumented)"
    Write-Host "Server acceptance decision report declared: $($report.Summary.ServerAcceptanceDecisionReportDeclared)"
    Write-Host "Judging server acceptance template declared: $($report.Summary.JudgingServerAcceptanceTemplateDeclared)"
    Write-Host "Judging server acceptance package declared: $($report.Summary.JudgingServerAcceptancePackageDeclared)"
    Write-Host "Judging server acceptance template values redacted: $($report.Summary.JudgingServerAcceptanceTemplateValuesRedacted)"
    Write-Host "Judging server acceptance template stages config: $($report.Summary.JudgingServerAcceptanceTemplateStagesConfig)"
    Write-Host "Judging server acceptance template required evidence count: $($report.Summary.JudgingServerAcceptanceTemplateRequiredEvidenceCount)"
    Write-Host "Safe editor default documented: $($report.Summary.SafeEditorDefaultDocumented)"
}
