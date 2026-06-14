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

$requiredFiles = @($transportHeader, $transportSource, $transportDoc, $remainingWorkDoc, $editorSmokeDoc)
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
    (New-Check -Path $transportDoc -Pattern "SaveToFile" -Label "SaveToFile review path documented"),
    (New-Check -Path $remainingWorkDoc -Pattern "server transport contract" -Label "Remaining work references transport contract"),
    (New-Check -Path $remainingWorkDoc -Pattern "validate_server_transport_contract.ps1" -Label "Remaining work references transport gate"),
    (New-Check -Path $editorSmokeDoc -Pattern "SharedSensorTransport.TransportMode = LogOnly" -Label "Smoke test safe transport default documented")
)

foreach ($check in $checks) {
    Assert-Contains -Path $check.Path -Pattern $check.Pattern -Label $check.Label
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
        OpenServerTransportDecisionsDocumented = $true
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
    Write-Host "Open server transport decisions documented: $($report.Summary.OpenServerTransportDecisionsDocumented)"
    Write-Host "Safe editor default documented: $($report.Summary.SafeEditorDefaultDocumented)"
}
