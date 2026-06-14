param(
    [string]$FixtureRoot = ""
)

$ErrorActionPreference = "Stop"

function Get-DefaultFixtureRoot {
    $scriptRoot = Split-Path -Parent $MyInvocation.ScriptName
    $projectRoot = Split-Path -Parent $scriptRoot
    return Join-Path $projectRoot "Samples\payload_fixtures"
}

function Read-JsonFile {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Fixture not found: $Path"
    }

    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Assert-HasField {
    param(
        [object]$Object,
        [string]$FieldName,
        [string]$Context
    )

    if (-not ($Object.PSObject.Properties.Name -contains $FieldName)) {
        throw "$Context is missing required field '$FieldName'"
    }
}

function Assert-HasFields {
    param(
        [object]$Object,
        [string[]]$FieldNames,
        [string]$Context
    )

    foreach ($fieldName in $FieldNames) {
        Assert-HasField -Object $Object -FieldName $fieldName -Context $Context
    }
}

function Assert-ArrayLength {
    param(
        [object]$ArrayValue,
        [int]$ExpectedLength,
        [string]$Context
    )

    if ($null -eq $ArrayValue -or $ArrayValue.Count -ne $ExpectedLength) {
        throw "$Context must contain $ExpectedLength values"
    }
}

if ([string]::IsNullOrWhiteSpace($FixtureRoot)) {
    $FixtureRoot = Get-DefaultFixtureRoot
}

$FixtureRoot = (Resolve-Path -LiteralPath $FixtureRoot).Path
$lidarPath = Join-Path $FixtureRoot "virtual_lidar_v1_sample.json"
$cameraPath = Join-Path $FixtureRoot "virtual_camera_v1_sample.json"

$lidar = Read-JsonFile -Path $lidarPath
$camera = Read-JsonFile -Path $cameraPath

$lidarRequiredFields = @(
    "schemaVersion",
    "sensorType",
    "sensorId",
    "manufacturer",
    "model",
    "frameId",
    "timestampUtc",
    "horizontalSamples",
    "verticalChannels",
    "rayCount",
    "totalPointCount",
    "hitPointCount",
    "payloadPointCount",
    "maxDistance",
    "semanticClassification",
    "payloadPolicy",
    "previewPolicy",
    "sensorTransform",
    "slabAnalysis",
    "points"
)

Assert-HasFields -Object $lidar -FieldNames $lidarRequiredFields -Context "LiDAR fixture"
if ($lidar.schemaVersion -ne "virtual-lidar.v1") {
    throw "LiDAR fixture schemaVersion must be virtual-lidar.v1"
}
if ($lidar.sensorType -ne "virtual_lidar") {
    throw "LiDAR fixture sensorType must be virtual_lidar"
}
if ($lidar.payloadPointCount -ne $lidar.points.Count) {
    throw "LiDAR fixture payloadPointCount must match points count"
}
Assert-HasFields -Object $lidar.payloadPolicy -FieldNames @("stride", "maxPoints", "includeMissPoints", "pointSelection") -Context "LiDAR payloadPolicy"
Assert-HasFields -Object $lidar.previewPolicy -FieldNames @("stride", "maxPoints", "hitOnly") -Context "LiDAR previewPolicy"
Assert-HasFields -Object $lidar.sensorTransform -FieldNames @("location", "rotation", "forward", "up") -Context "LiDAR sensorTransform"
Assert-ArrayLength -ArrayValue $lidar.sensorTransform.location -ExpectedLength 3 -Context "LiDAR sensorTransform.location"
Assert-ArrayLength -ArrayValue $lidar.sensorTransform.rotation -ExpectedLength 3 -Context "LiDAR sensorTransform.rotation"
Assert-HasFields -Object $lidar.slabAnalysis -FieldNames @("valid", "slabHitPointCount", "boundsMin", "boundsMax", "center", "estimatedYawDegrees", "referenceYawDegrees", "angleDeviationDegrees", "confidence", "status") -Context "LiDAR slabAnalysis"

foreach ($point in $lidar.points) {
    Assert-HasFields -Object $point -FieldNames @("pointIndex", "row", "col", "hit", "distance", "hitActor", "hitActorClass", "hitActorTags", "semanticLabel", "worldLocation", "localDirection") -Context "LiDAR point"
    Assert-ArrayLength -ArrayValue $point.worldLocation -ExpectedLength 3 -Context "LiDAR point.worldLocation"
    Assert-ArrayLength -ArrayValue $point.localDirection -ExpectedLength 3 -Context "LiDAR point.localDirection"
}

$cameraRequiredFields = @(
    "schemaVersion",
    "sensorType",
    "sensorId",
    "manufacturer",
    "model",
    "frameId",
    "timestampUtc",
    "width",
    "height",
    "byteSize",
    "horizontalFov",
    "verticalFov",
    "simulationQuality",
    "encoding",
    "sensorTransform",
    "sensorWorldLocation",
    "image"
)

Assert-HasFields -Object $camera -FieldNames $cameraRequiredFields -Context "Camera fixture"
if ($camera.schemaVersion -ne "virtual-camera.v1") {
    throw "Camera fixture schemaVersion must be virtual-camera.v1"
}
if ($camera.sensorType -ne "virtual_camera") {
    throw "Camera fixture sensorType must be virtual_camera"
}
if ($camera.encoding -ne "jpeg/base64") {
    throw "Camera fixture encoding must be jpeg/base64"
}
Assert-HasFields -Object $camera.sensorTransform -FieldNames @("location", "rotation", "forward", "up") -Context "Camera sensorTransform"
Assert-ArrayLength -ArrayValue $camera.sensorTransform.location -ExpectedLength 3 -Context "Camera sensorTransform.location"
Assert-ArrayLength -ArrayValue $camera.sensorTransform.rotation -ExpectedLength 3 -Context "Camera sensorTransform.rotation"
Assert-ArrayLength -ArrayValue $camera.sensorWorldLocation -ExpectedLength 3 -Context "Camera sensorWorldLocation"
if ([string]::IsNullOrWhiteSpace($camera.image)) {
    throw "Camera fixture image must not be empty"
}

Write-Host "Payload fixtures are valid."
