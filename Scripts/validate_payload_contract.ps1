param(
    [string]$FixtureRoot = "",
    [string]$SchemaDocsRoot = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultFixtureRoot {
    $projectRoot = Split-Path -Parent $PSScriptRoot
    return Join-Path $projectRoot "Samples\payload_fixtures"
}

function Get-DefaultSchemaDocsRoot {
    $projectRoot = Split-Path -Parent $PSScriptRoot
    return Join-Path $projectRoot "docs"
}

function Read-JsonFile {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Fixture not found: $Path"
    }

    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Test-TimestampUtc {
    param([string]$Value)

    $parsed = [DateTimeOffset]::MinValue
    return -not [string]::IsNullOrWhiteSpace($Value) -and $Value.EndsWith("Z") -and [DateTimeOffset]::TryParse($Value, [ref]$parsed) -and $parsed.Offset -eq [TimeSpan]::Zero
}

function Test-WholeNumber {
    param([object]$Value)

    if (-not ($Value -is [ValueType])) {
        return $false
    }

    $number = [double]$Value
    return -not [double]::IsNaN($number) -and -not [double]::IsInfinity($number) -and [Math]::Abs($number - [Math]::Round($number)) -lt 0.000001
}

function Test-PositiveFiniteNumber {
    param([object]$Value)

    if (-not ($Value -is [ValueType])) {
        return $false
    }

    $number = [double]$Value
    return -not [double]::IsNaN($number) -and -not [double]::IsInfinity($number) -and $number -gt 0.0
}

function Test-NumberArray {
    param(
        [object]$Value,
        [int]$ExpectedLength
    )

    if ($null -eq $Value -or $Value.Count -ne $ExpectedLength) {
        return $false
    }

    foreach ($entry in $Value) {
        if (-not ($entry -is [ValueType])) {
            return $false
        }

        $number = [double]$entry
        if ([double]::IsNaN($number) -or [double]::IsInfinity($number)) {
            return $false
        }
    }

    return $true
}

function Get-DecodedBase64Bytes {
    param(
        [string]$Value,
        [string]$Context
    )

    try {
        return [Convert]::FromBase64String($Value)
    }
    catch {
        throw "$Context must be valid base64"
    }
}

function Test-SimulationQuality {
    param([object]$Value)

    return ($Value -is [string]) -and $Value -in @("Debug", "RealTimePreview", "Balanced", "FullSpec")
}

if ([string]::IsNullOrWhiteSpace($FixtureRoot)) {
    $FixtureRoot = Get-DefaultFixtureRoot
}
if ([string]::IsNullOrWhiteSpace($SchemaDocsRoot)) {
    $SchemaDocsRoot = Get-DefaultSchemaDocsRoot
}

$FixtureRoot = (Resolve-Path -LiteralPath $FixtureRoot).Path
$SchemaDocsRoot = (Resolve-Path -LiteralPath $SchemaDocsRoot).Path

$fixtureValidator = Join-Path $PSScriptRoot "validate_payload_fixtures.ps1"
if (-not (Test-Path -LiteralPath $fixtureValidator)) {
    throw "validate_payload_fixtures.ps1 not found: $fixtureValidator"
}

& $fixtureValidator -FixtureRoot $FixtureRoot -SchemaDocsRoot $SchemaDocsRoot -Json | Out-Null

$lidarPath = Join-Path $FixtureRoot "virtual_lidar_v1_sample.json"
$cameraPath = Join-Path $FixtureRoot "virtual_camera_v1_sample.json"
$lidar = Read-JsonFile -Path $lidarPath
$camera = Read-JsonFile -Path $cameraPath

$accepted = @()

Assert-Condition ($lidar.schemaVersion -eq "virtual-lidar.v1") "LiDAR contract rejects schemaVersion '$($lidar.schemaVersion)'"
Assert-Condition ($lidar.sensorType -eq "virtual_lidar") "LiDAR contract rejects sensorType '$($lidar.sensorType)'"
Assert-Condition (Test-TimestampUtc $lidar.timestampUtc) "LiDAR contract requires UTC timestampUtc"
Assert-Condition ($lidar.frameId -ge 0) "LiDAR contract requires non-negative frameId"
Assert-Condition ($lidar.rayCount -ge $lidar.hitPointCount) "LiDAR contract requires rayCount >= hitPointCount"
Assert-Condition ($lidar.totalPointCount -ge $lidar.hitPointCount) "LiDAR contract requires totalPointCount >= hitPointCount"
Assert-Condition ($lidar.payloadPointCount -eq $lidar.points.Count) "LiDAR contract requires payloadPointCount to match points count"
Assert-Condition ($lidar.payloadPolicy.pointSelection -in @("hit_only", "hit_and_miss")) "LiDAR contract rejects payloadPolicy.pointSelection '$($lidar.payloadPolicy.pointSelection)'"
Assert-Condition ($lidar.previewPolicy.maxPoints -ge 0) "LiDAR contract requires non-negative previewPolicy.maxPoints"
foreach ($point in $lidar.points) {
    Assert-Condition ($point.worldLocation.Count -eq 3) "LiDAR contract requires 3D worldLocation"
    Assert-Condition ($point.localDirection.Count -eq 3) "LiDAR contract requires 3D localDirection"
    Assert-Condition ($point.semanticLabel -and $point.semanticLabel.Length -gt 0) "LiDAR contract requires semanticLabel"
    Assert-Condition ($point.row -ge 0) "LiDAR contract requires non-negative row"
    Assert-Condition ($point.col -ge 0) "LiDAR contract requires non-negative col"
    Assert-Condition ($point.returnIndex -ge 0) "LiDAR contract requires non-negative returnIndex"
    Assert-Condition ($point.gridCoordSource -in @("point_metadata", "derived_from_point_index")) "LiDAR contract rejects gridCoordSource '$($point.gridCoordSource)'"
    if ($point.gridCoordValid) {
        Assert-Condition ($point.gridCoordSource -eq "point_metadata") "LiDAR contract requires valid grid coords to use point_metadata"
    }
    else {
        Assert-Condition ($point.gridCoordSource -eq "derived_from_point_index") "LiDAR contract requires fallback grid coords to use derived_from_point_index"
    }
}
$accepted += [PSCustomObject]@{
    SensorType = $lidar.sensorType
    SchemaVersion = $lidar.schemaVersion
    SensorId = $lidar.sensorId
    Accepted = $true
    RuleSet = "mock-judging-server-lidar.v1"
    PayloadPointCount = $lidar.payloadPointCount
}

Assert-Condition ($camera.schemaVersion -eq "virtual-camera.v1") "Camera contract rejects schemaVersion '$($camera.schemaVersion)'"
Assert-Condition ($camera.sensorType -eq "virtual_camera") "Camera contract rejects sensorType '$($camera.sensorType)'"
Assert-Condition (-not [string]::IsNullOrWhiteSpace($camera.sensorId)) "Camera contract requires non-empty sensorId"
Assert-Condition (-not [string]::IsNullOrWhiteSpace($camera.manufacturer)) "Camera contract requires non-empty manufacturer"
Assert-Condition (-not [string]::IsNullOrWhiteSpace($camera.model)) "Camera contract requires non-empty model"
Assert-Condition (Test-TimestampUtc $camera.timestampUtc) "Camera contract requires UTC timestampUtc"
Assert-Condition ((Test-WholeNumber $camera.frameId) -and $camera.frameId -ge 0) "Camera contract requires non-negative integral frameId"
Assert-Condition ((Test-WholeNumber $camera.width) -and $camera.width -gt 0 -and (Test-WholeNumber $camera.height) -and $camera.height -gt 0) "Camera contract requires positive integral image dimensions"
Assert-Condition ((Test-WholeNumber $camera.byteSize) -and $camera.byteSize -gt 0) "Camera contract requires positive integral byteSize"
Assert-Condition (Test-PositiveFiniteNumber $camera.horizontalFov) "Camera contract requires positive horizontalFov"
Assert-Condition (Test-PositiveFiniteNumber $camera.verticalFov) "Camera contract requires positive verticalFov"
Assert-Condition (Test-SimulationQuality $camera.simulationQuality) "Camera contract requires simulationQuality string in Debug, RealTimePreview, Balanced, FullSpec"
Assert-Condition ($camera.encoding -eq "jpeg/base64") "Camera contract requires jpeg/base64 encoding"
Assert-Condition (-not [string]::IsNullOrWhiteSpace($camera.image)) "Camera contract requires non-empty image payload"
Assert-Condition (Test-NumberArray $camera.sensorTransform.location 3) "Camera contract requires 3D sensorTransform.location"
Assert-Condition (Test-NumberArray $camera.sensorTransform.rotation 3) "Camera contract requires 3D sensorTransform.rotation"
Assert-Condition (Test-NumberArray $camera.sensorTransform.forward 3) "Camera contract requires 3D sensorTransform.forward"
Assert-Condition (Test-NumberArray $camera.sensorTransform.up 3) "Camera contract requires 3D sensorTransform.up"
Assert-Condition (Test-NumberArray $camera.sensorWorldLocation 3) "Camera contract requires 3D sensorWorldLocation"
$decodedCameraImage = Get-DecodedBase64Bytes -Value $camera.image -Context "Camera contract image"
Assert-Condition ($decodedCameraImage.Count -eq [int]$camera.byteSize) "Camera contract requires byteSize to match decoded image bytes"
Assert-Condition ($decodedCameraImage.Count -ge 2 -and $decodedCameraImage[0] -eq 0xff -and $decodedCameraImage[1] -eq 0xd8) "Camera contract requires JPEG bytes starting with FF D8"
$accepted += [PSCustomObject]@{
    SensorType = $camera.sensorType
    SchemaVersion = $camera.schemaVersion
    SensorId = $camera.sensorId
    Accepted = $true
    RuleSet = "mock-judging-server-camera.v1"
    ByteSize = $camera.byteSize
}

$report = [PSCustomObject]@{
    FixtureRoot = $FixtureRoot
    SchemaDocsRoot = $SchemaDocsRoot
    Contract = "mock-judging-server.v1"
    AcceptedPayloads = $accepted
    Summary = [PSCustomObject]@{
        AcceptedCount = $accepted.Count
        RejectedCount = 0
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Payload contract fixtures are accepted by mock judging server rules."
    Write-Host "Accepted payloads: $($report.Summary.AcceptedCount)"
    foreach ($payload in $accepted) {
        Write-Host "$($payload.SensorType): $($payload.SchemaVersion) $($payload.SensorId) -> $($payload.RuleSet)"
    }
}
