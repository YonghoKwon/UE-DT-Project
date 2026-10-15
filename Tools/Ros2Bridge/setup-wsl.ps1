param(
    [ValidateSet('install','up','down','status','remove','run','stop')][string]$Action = 'status',
    [switch]$WithImageDecoder,
    [switch]$EnableControl,
    [switch]$PublishClock,
    [string]$Lidar = 'SLAB-LIDAR-001',
    [string]$CaptureSensor = 'VCAM-001',
    [int]$RelayPort = 18099,
    [int]$ApiPort = 8099,
    [string]$ReportRoot = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (!$ReportRoot) { $ReportRoot = Join-Path $root 'Saved/Reports/ROS2-01/wsl' }
$arguments = @((Join-Path $PSScriptRoot 'wsl_env.py'), $Action, '--report', $ReportRoot)
if ($WithImageDecoder) { $arguments += '--with-image-decoder' }
if ($EnableControl) { $arguments += @('--enable-control', '--lidar', $Lidar, '--capture-sensor', $CaptureSensor, '--relay-port', $RelayPort, '--api-port', $ApiPort) }
if ($PublishClock) { $arguments += '--publish-clock' }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw "ROS2-01 $Action failed (exit $LASTEXITCODE). See $ReportRoot; do not change existing WSL or security settings." }
