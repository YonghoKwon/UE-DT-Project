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
$lidarDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$cameraDoc = Join-Path $ProjectRoot "docs\camera_payload_schema.md"
$remainingWorkDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$contractReportScript = Join-Path $ProjectRoot "Scripts\export_payload_contract_report.ps1"
$payloadContractValidator = Join-Path $ProjectRoot "Scripts\validate_payload_contract.ps1"
$lidarCpp = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\VirtualLidarScanComponent.cpp"
$lidarFixture = Join-Path $ProjectRoot "Samples\payload_fixtures\virtual_lidar_v1_sample.json"

$requiredFiles = @($lidarDoc, $cameraDoc, $remainingWorkDoc, $contractReportScript, $payloadContractValidator, $lidarCpp, $lidarFixture)
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Required payload schema review file not found: $file"
    }
}

$checks = @(
    (New-Check -Path $lidarDoc -Pattern "## Compatibility Notes" -Label "LiDAR compatibility notes section"),
    (New-Check -Path $lidarDoc -Pattern "Unreal centimeters" -Label "LiDAR distance unit note"),
    (New-Check -Path $lidarDoc -Pattern "Unreal world coordinates" -Label "LiDAR coordinate frame note"),
    (New-Check -Path $lidarDoc -Pattern "ISO-8601 UTC" -Label "LiDAR timestamp note"),
    (New-Check -Path $lidarDoc -Pattern "virtual-lidar.v1-full-hit" -Label "LiDAR full-hit compatibility example"),
    (New-Check -Path $lidarDoc -Pattern "virtual-lidar.v1-strided-hit" -Label "LiDAR strided compatibility example"),
    (New-Check -Path $lidarDoc -Pattern "virtual-lidar.v1-hit-and-miss" -Label "LiDAR hit-and-miss compatibility example"),
    (New-Check -Path $lidarDoc -Pattern '`hit_and_miss` when `includeMissPoints = true`' -Label "LiDAR hit-and-miss policy value"),
    (New-Check -Path $lidarDoc -Pattern '`previewPolicy` is never a server measurement contract' -Label "LiDAR preview/server separation note"),
    (New-Check -Path $cameraDoc -Pattern "## Compatibility Notes" -Label "Camera compatibility notes section"),
    (New-Check -Path $cameraDoc -Pattern "ISO-8601 UTC" -Label "Camera timestamp note"),
    (New-Check -Path $cameraDoc -Pattern "Unreal world coordinates" -Label "Camera coordinate frame note"),
    (New-Check -Path $cameraDoc -Pattern "JPEG byte count before base64 encoding" -Label "Camera byte size note"),
    (New-Check -Path $cameraDoc -Pattern "virtual-camera.v1-jpeg-base64" -Label "Camera jpeg compatibility example"),
    (New-Check -Path $cameraDoc -Pattern "virtual-camera.v1-preview-only" -Label "Camera preview-only compatibility example"),
    (New-Check -Path $remainingWorkDoc -Pattern "Schema compatibility examples are documented" -Label "Remaining work records compatibility progress"),
    (New-Check -Path $remainingWorkDoc -Pattern "validate_payload_schema_review_policy.ps1" -Label "Remaining work records static review gate"),
    (New-Check -Path $contractReportScript -Pattern "Remaining Server Decisions" -Label "Contract report keeps server decision section"),
    (New-Check -Path $contractReportScript -Pattern "ServerAcceptanceDecisions" -Label "Contract report exports server acceptance decisions"),
    (New-Check -Path $contractReportScript -Pattern "TransportContractReport" -Label "Contract report includes transport contract validation summary"),
    (New-Check -Path $contractReportScript -Pattern "RealServerEvidenceGaps" -Label "Contract report exports real server evidence gaps"),
    (New-Check -Path $contractReportScript -Pattern "Endpoint URL and environment ownership" -Label "Contract report tracks endpoint decision"),
    (New-Check -Path $contractReportScript -Pattern "Authentication" -Label "Contract report tracks authentication decision"),
    (New-Check -Path $contractReportScript -Pattern "Retry and timeout policy" -Label "Contract report tracks retry decision"),
    (New-Check -Path $contractReportScript -Pattern 'Name = "Batching"' -Label "Contract report tracks batching decision separately"),
    (New-Check -Path $contractReportScript -Pattern 'Name = "Backpressure"' -Label "Contract report tracks backpressure decision separately"),
    (New-Check -Path $contractReportScript -Pattern "RealJudgingServerAcceptancePresent" -Label "Contract report separates real judging-server acceptance"),
    (New-Check -Path $payloadContractValidator -Pattern '@("hit_only", "hit_and_miss")' -Label "Payload contract validator accepts canonical point selection values"),
    (New-Check -Path $lidarFixture -Pattern '"pointSelection": "hit_only"' -Label "LiDAR fixture records canonical hit-only selection"),
    (New-Check -Path $lidarCpp -Pattern 'PayloadPolicyObject->SetNumberField(TEXT("stride"), ServerPayloadStride)' -Label "LiDAR C++ writes server payload stride"),
    (New-Check -Path $lidarCpp -Pattern 'PayloadPolicyObject->SetNumberField(TEXT("maxPoints"), MaxServerPayloadPoints)' -Label "LiDAR C++ writes server max payload points"),
    (New-Check -Path $lidarCpp -Pattern 'PayloadPolicyObject->SetBoolField(TEXT("includeMissPoints"), bIncludeMissPointsInServerPayload)' -Label "LiDAR C++ writes include-miss payload flag"),
    (New-Check -Path $lidarCpp -Pattern 'PayloadPolicyObject->SetStringField(TEXT("pointSelection"), bIncludeMissPointsInServerPayload ? TEXT("hit_and_miss") : TEXT("hit_only"))' -Label "LiDAR C++ writes canonical point selection"),
    (New-Check -Path $lidarCpp -Pattern 'Root->SetObjectField(TEXT("slabAnalysis"), SlabObject)' -Label "LiDAR C++ writes slab analysis metadata"),
    (New-Check -Path $lidarCpp -Pattern 'Root->SetArrayField(TEXT("points"), JsonPoints)' -Label "LiDAR C++ writes server points array"),
    (New-Check -Path $lidarCpp -Pattern 'AddVectorArray(O, TEXT("worldLocation"), P.WorldLocation)' -Label "LiDAR C++ writes point world location"),
    (New-Check -Path $lidarCpp -Pattern 'AddVectorArray(O, TEXT("localDirection"), P.LocalDirection)' -Label "LiDAR C++ writes point local direction"),
    (New-Check -Path $lidarCpp -Pattern 'O->SetStringField(TEXT("semanticLabel"),' -Label "LiDAR C++ writes point semantic label"),
    (New-Check -Path $lidarCpp -Pattern 'O->SetNumberField(TEXT("row"),' -Label "LiDAR C++ writes point row"),
    (New-Check -Path $lidarCpp -Pattern 'O->SetNumberField(TEXT("col"),' -Label "LiDAR C++ writes point column"),
    (New-Check -Path $lidarCpp -Pattern 'O->SetNumberField(TEXT("returnIndex"),' -Label "LiDAR C++ writes point return index")
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
        LidarCompatibilityExamplesDocumented = $true
        CameraCompatibilityExamplesDocumented = $true
        StaticReviewGateDocumented = $true
        ServerAcceptanceMatrixDocumented = $true
        ServerPayloadPolicyChecked = $true
        PointSelectionContractChecked = $true
        SlabMetadataContractChecked = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Payload schema review policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCheckCount)"
    Write-Host "LiDAR compatibility examples documented: $($report.Summary.LidarCompatibilityExamplesDocumented)"
    Write-Host "Camera compatibility examples documented: $($report.Summary.CameraCompatibilityExamplesDocumented)"
    Write-Host "Static review gate documented: $($report.Summary.StaticReviewGateDocumented)"
    Write-Host "Server acceptance matrix documented: $($report.Summary.ServerAcceptanceMatrixDocumented)"
    Write-Host "Server payload policy checked: $($report.Summary.ServerPayloadPolicyChecked)"
    Write-Host "Point selection contract checked: $($report.Summary.PointSelectionContractChecked)"
    Write-Host "Slab metadata contract checked: $($report.Summary.SlabMetadataContractChecked)"
}
