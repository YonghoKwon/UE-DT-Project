param(
    [string]$FixtureRoot = "",
    [string]$SchemaDocsRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultFixtureRoot {
    $scriptRoot = $PSScriptRoot
    $projectRoot = Split-Path -Parent $scriptRoot
    return Join-Path $projectRoot "Samples\payload_fixtures"
}

function Get-DefaultSchemaDocsRoot {
    $scriptRoot = $PSScriptRoot
    $projectRoot = Split-Path -Parent $scriptRoot
    return Join-Path $projectRoot "docs"
}

function Read-JsonFile {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Fixture not found: $Path"
    }

    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-MarkdownTextBlockAfterHeading {
    param(
        [string]$Path,
        [string]$Heading
    )

    $lines = @(Get-Content -LiteralPath $Path)
    $headingIndex = -1
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($lines[$i].Trim() -eq $Heading) {
            $headingIndex = $i
            break
        }
    }
    if ($headingIndex -lt 0) {
        throw "Heading '$Heading' not found in $Path"
    }

    $blockStart = -1
    for ($i = $headingIndex + 1; $i -lt $lines.Count; ++$i) {
        if ($lines[$i].Trim() -eq '```text') {
            $blockStart = $i + 1
            break
        }
    }
    if ($blockStart -lt 0) {
        throw "Text code block after '$Heading' not found in $Path"
    }

    $fields = @()
    for ($i = $blockStart; $i -lt $lines.Count; ++$i) {
        if ($lines[$i].Trim() -eq '```') {
            break
        }
        $field = $lines[$i].Trim()
        if (-not [string]::IsNullOrWhiteSpace($field)) {
            $fields += $field
        }
    }

    return $fields
}

function Assert-FieldListEqual {
    param(
        [string[]]$Actual,
        [string[]]$Expected,
        [string]$Context
    )

    $missing = @($Expected | Where-Object { $Actual -notcontains $_ })
    $extra = @($Actual | Where-Object { $Expected -notcontains $_ })
    if ($missing.Count -gt 0 -or $extra.Count -gt 0) {
        throw "$Context field list mismatch. Missing=[$($missing -join ', ')] Extra=[$($extra -join ', ')]"
    }
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
if ([string]::IsNullOrWhiteSpace($SchemaDocsRoot)) {
    $SchemaDocsRoot = Get-DefaultSchemaDocsRoot
}

$FixtureRoot = (Resolve-Path -LiteralPath $FixtureRoot).Path
$SchemaDocsRoot = (Resolve-Path -LiteralPath $SchemaDocsRoot).Path
$lidarPath = Join-Path $FixtureRoot "virtual_lidar_v1_sample.json"
$cameraPath = Join-Path $FixtureRoot "virtual_camera_v1_sample.json"
$lidarDocPath = Join-Path $SchemaDocsRoot "lidar_payload_schema.md"
$cameraDocPath = Join-Path $SchemaDocsRoot "camera_payload_schema.md"

$lidar = Read-JsonFile -Path $lidarPath
$camera = Read-JsonFile -Path $cameraPath

$lidarRequiredFields = Get-MarkdownTextBlockAfterHeading -Path $lidarDocPath -Heading "## Top-Level Fields"
$lidarPointFields = Get-MarkdownTextBlockAfterHeading -Path $lidarDocPath -Heading "## Point Fields"
$lidarSlabAnalysisFields = Get-MarkdownTextBlockAfterHeading -Path $lidarDocPath -Heading "## Slab Analysis"

Assert-FieldListEqual `
    -Actual $lidarRequiredFields `
    -Expected @("schemaVersion", "sensorType", "sensorId", "manufacturer", "model", "frameId", "timestampUtc", "horizontalSamples", "verticalChannels", "rayCount", "totalPointCount", "hitPointCount", "payloadPointCount", "maxDistance", "semanticClassification", "payloadPolicy", "previewPolicy", "sensorTransform", "slabAnalysis", "points") `
    -Context "LiDAR documented top-level fields"

Assert-FieldListEqual `
    -Actual $lidarPointFields `
    -Expected @("pointIndex", "row", "col", "returnIndex", "gridCoordValid", "gridCoordSource", "hit", "distance", "hitActor", "hitActorClass", "hitActorTags", "semanticLabel", "worldLocation", "localDirection") `
    -Context "LiDAR documented point fields"

Assert-FieldListEqual `
    -Actual $lidarSlabAnalysisFields `
    -Expected @("valid", "slabHitPointCount", "boundsMin", "boundsMax", "center", "estimatedYawDegrees", "referenceYawDegrees", "angleDeviationDegrees", "confidence", "status") `
    -Context "LiDAR documented slabAnalysis fields"

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
Assert-HasFields -Object $lidar.slabAnalysis -FieldNames $lidarSlabAnalysisFields -Context "LiDAR slabAnalysis"

foreach ($point in $lidar.points) {
    Assert-HasFields -Object $point -FieldNames $lidarPointFields -Context "LiDAR point"
    Assert-ArrayLength -ArrayValue $point.worldLocation -ExpectedLength 3 -Context "LiDAR point.worldLocation"
    Assert-ArrayLength -ArrayValue $point.localDirection -ExpectedLength 3 -Context "LiDAR point.localDirection"
    if ($point.gridCoordSource -notin @("point_metadata", "derived_from_point_index")) {
        throw "LiDAR point gridCoordSource is not recognized: $($point.gridCoordSource)"
    }
}

$cameraRequiredFields = Get-MarkdownTextBlockAfterHeading -Path $cameraDocPath -Heading "## Top-Level Fields"
$cameraTransformFields = Get-MarkdownTextBlockAfterHeading -Path $cameraDocPath -Heading "## Transform"

Assert-FieldListEqual `
    -Actual $cameraRequiredFields `
    -Expected @("schemaVersion", "sensorType", "sensorId", "manufacturer", "model", "frameId", "timestampUtc", "width", "height", "byteSize", "horizontalFov", "verticalFov", "simulationQuality", "encoding", "sensorTransform", "sensorWorldLocation", "image") `
    -Context "Camera documented top-level fields"

Assert-FieldListEqual `
    -Actual $cameraTransformFields `
    -Expected @("location", "rotation", "forward", "up") `
    -Context "Camera documented sensorTransform fields"

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
Assert-HasFields -Object $camera.sensorTransform -FieldNames $cameraTransformFields -Context "Camera sensorTransform"
Assert-ArrayLength -ArrayValue $camera.sensorTransform.location -ExpectedLength 3 -Context "Camera sensorTransform.location"
Assert-ArrayLength -ArrayValue $camera.sensorTransform.rotation -ExpectedLength 3 -Context "Camera sensorTransform.rotation"
Assert-ArrayLength -ArrayValue $camera.sensorWorldLocation -ExpectedLength 3 -Context "Camera sensorWorldLocation"
if ([string]::IsNullOrWhiteSpace($camera.image)) {
    throw "Camera fixture image must not be empty"
}

$report = [PSCustomObject]@{
    FixtureRoot = $FixtureRoot
    SchemaDocsRoot = $SchemaDocsRoot
    Fixtures = @(
        [PSCustomObject]@{
            SensorType = "virtual_lidar"
            SchemaVersion = $lidar.schemaVersion
            Path = $lidarPath
            DocumentPath = $lidarDocPath
            TopLevelFieldCount = $lidarRequiredFields.Count
            PointFieldCount = $lidarPointFields.Count
            PayloadPointCount = $lidar.payloadPointCount
            DocumentedFieldGroups = @("Top-Level Fields", "Point Fields", "Slab Analysis")
        },
        [PSCustomObject]@{
            SensorType = "virtual_camera"
            SchemaVersion = $camera.schemaVersion
            Path = $cameraPath
            DocumentPath = $cameraDocPath
            TopLevelFieldCount = $cameraRequiredFields.Count
            TransformFieldCount = $cameraTransformFields.Count
            ByteSize = $camera.byteSize
            Encoding = $camera.encoding
            DocumentedFieldGroups = @("Top-Level Fields", "Transform")
        }
    )
    Summary = [PSCustomObject]@{
        FixtureCount = 2
        SchemaVersions = @($lidar.schemaVersion, $camera.schemaVersion)
        DocumentationLinked = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Payload fixtures are valid."
    Write-Host "Fixture count: $($report.Summary.FixtureCount)"
    foreach ($fixture in $report.Fixtures) {
        Write-Host "$($fixture.SensorType): $($fixture.SchemaVersion) -> $($fixture.Path)"
    }
}
