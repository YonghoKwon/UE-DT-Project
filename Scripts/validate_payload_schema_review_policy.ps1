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

$requiredFiles = @($lidarDoc, $cameraDoc, $remainingWorkDoc, $contractReportScript)
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
    (New-Check -Path $lidarDoc -Pattern '`previewPolicy` is never a server measurement contract' -Label "LiDAR preview/server separation note"),
    (New-Check -Path $cameraDoc -Pattern "## Compatibility Notes" -Label "Camera compatibility notes section"),
    (New-Check -Path $cameraDoc -Pattern "ISO-8601 UTC" -Label "Camera timestamp note"),
    (New-Check -Path $cameraDoc -Pattern "Unreal world coordinates" -Label "Camera coordinate frame note"),
    (New-Check -Path $cameraDoc -Pattern "JPEG byte count before base64 encoding" -Label "Camera byte size note"),
    (New-Check -Path $cameraDoc -Pattern "virtual-camera.v1-jpeg-base64" -Label "Camera jpeg compatibility example"),
    (New-Check -Path $cameraDoc -Pattern "virtual-camera.v1-preview-only" -Label "Camera preview-only compatibility example"),
    (New-Check -Path $remainingWorkDoc -Pattern "Schema compatibility examples are documented" -Label "Remaining work records compatibility progress"),
    (New-Check -Path $remainingWorkDoc -Pattern "validate_payload_schema_review_policy.ps1" -Label "Remaining work records static review gate"),
    (New-Check -Path $contractReportScript -Pattern "Remaining Server Decisions" -Label "Contract report keeps server decision section")
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
}
