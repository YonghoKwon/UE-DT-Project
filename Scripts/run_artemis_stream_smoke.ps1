param(
    [string]$BrokerUrl = "ws://127.0.0.1:61616",
    # Empty means MA0T10_ARTEMIS_USER / MA0T10_ARTEMIS_PASSWORD from the environment.
    [string]$UserName = "",
    [string]$Password = "",
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot/artemis_credentials.ps1"
$Credential = Resolve-Ma0t10ArtemisCredential $UserName $Password
$Password = $null
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Editor = "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $ProjectRoot "ma0t10_dt.uproject"
$ReportDir = Join-Path $ProjectRoot "Saved\Reports"
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
$ProbeReport = Join-Path $ReportDir "artemis_sensor_stream_smoke.json"
$ProbeStdOut = Join-Path $ReportDir "artemis_sensor_stream_probe.stdout.log"
$ProbeStdErr = Join-Path $ReportDir "artemis_sensor_stream_probe.stderr.log"

$ProbeScript = Join-Path $ProjectRoot "Tools\Artemis\stomp_probe.mjs"
$ProbeArgs = "`"$ProbeScript`" --url `"$BrokerUrl`" --timeout $TimeoutSeconds --output `"$ProbeReport`""
$Probe = Start-Process -FilePath "node" -ArgumentList $ProbeArgs -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $ProbeStdOut -RedirectStandardError $ProbeStdErr

try {
    $env:MA0T10_RUN_ARTEMIS_SMOKE = "1"
    $env:MA0T10_ARTEMIS_URL = $BrokerUrl
    & $Editor $Project -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests MA0T10.SensorStream.ArtemisIntegration;Quit' '-TestExit=Automation Test Queue Empty' -log
    if ($LASTEXITCODE -ne 0) { throw "Unreal Artemis automation failed with exit code $LASTEXITCODE" }
    if (-not $Probe.WaitForExit(($TimeoutSeconds + 10) * 1000)) {
        $Probe.Kill()
        throw "STOMP probe timed out"
    }
	$Probe.WaitForExit()
	$Probe.Refresh()
    if ($null -ne $Probe.ExitCode -and $Probe.ExitCode -ne 0) { throw "STOMP probe failed with exit code $($Probe.ExitCode). See $ProbeStdErr" }
    $Result = Get-Content $ProbeReport -Raw | ConvertFrom-Json
    if (-not $Result.success) { throw "STOMP probe report did not pass: $($Result.reason)" }
    Write-Host "Artemis three-stream smoke passed."
    Write-Host "Report: $ProbeReport"
    $Result.messages | Format-Table destination, sensorId, dataKind, frameId, bytes, schema
}
finally {
    Remove-Item Env:MA0T10_RUN_ARTEMIS_SMOKE -ErrorAction SilentlyContinue
    Remove-Item Env:MA0T10_ARTEMIS_URL -ErrorAction SilentlyContinue
    Restore-Ma0t10ArtemisCredential $Credential
    if (-not $Probe.HasExited) { $Probe.Kill() }
}
