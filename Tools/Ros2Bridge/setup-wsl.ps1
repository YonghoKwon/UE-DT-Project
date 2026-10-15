param(
    [ValidateSet('install','up','down','status','remove','run','stop')][string]$Action = 'status',
    [switch]$WithImageDecoder,
    [string]$ReportRoot = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (!$ReportRoot) { $ReportRoot = Join-Path $root 'Saved/Reports/ROS2-01/wsl' }
$arguments = @((Join-Path $PSScriptRoot 'wsl_env.py'), $Action, '--report', $ReportRoot)
if ($WithImageDecoder) { $arguments += '--with-image-decoder' }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw "ROS2-01 $Action failed (exit $LASTEXITCODE). See $ReportRoot; do not change existing WSL or security settings." }
