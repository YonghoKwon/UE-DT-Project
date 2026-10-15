param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$Job,
    [string]$Project = '',
    [string]$Map = '',
    [double]$TimeoutSeconds = 7200,
    [double]$ShutdownGraceSeconds = 30,
    [string]$Report = '',
    [string]$WorkingDirectory = '',
    [string]$Python = 'python',
    [string[]]$UeArguments = @('-game', '-RenderOffscreen', '-d3d12', '-unattended', '-nosplash', '-NoLoadingScreen')
)
# The Python runner is authoritative on every platform; never use UE's graceful
# Windows exit code alone, and never force-terminate the child on timeout.
$arguments = @((Join-Path $PSScriptRoot 'run_job.py'), '--executable', $Executable,
    '--job', $Job, '--timeout', $TimeoutSeconds.ToString([Globalization.CultureInfo]::InvariantCulture),
    '--shutdown-grace', $ShutdownGraceSeconds.ToString([Globalization.CultureInfo]::InvariantCulture))
if ($Project) { $arguments += @('--project', $Project) }
if ($Map) { $arguments += @('--map', $Map) }
if ($Report) { $arguments += @('--report', $Report) }
if ($WorkingDirectory) { $arguments += @('--working-directory', $WorkingDirectory) }
$arguments += @('--') + $UeArguments
& $Python @arguments
exit $LASTEXITCODE
