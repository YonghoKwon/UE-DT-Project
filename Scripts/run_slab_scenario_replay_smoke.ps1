param([string]$BrokerUrl='ws://127.0.0.1:61616',[string]$UserName='artemis',[string]$Password='artemis')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Reports=Join-Path $Root 'Saved/Reports'
$ProbeJson=Join-Path $Reports 'slab_replay_external.json'
$Log=Join-Path $Reports 'slab_replay_runtime.log'
$ProbeArgs=@("`"$(Join-Path $Root 'Tools/Artemis/stomp_probe.mjs')`"",'--url',"`"$BrokerUrl`"",'--user',"`"$UserName`"",'--password',"`"$Password`"",'--topics','topic.virtual.sensor.export.0','--duration','63','--slab-runs','2','--timeout','190','--require-contiguous-pcd','true','--scenario-replay','true','--save-first-pcd',"`"$(Join-Path $Reports 'slab_replay_received.pcd')`"",'--output',"`"$ProbeJson`"")
$Probe=Start-Process node -ArgumentList $ProbeArgs -WorkingDirectory $Root -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $Reports 'slab_replay_probe.stdout.log') -RedirectStandardError (Join-Path $Reports 'slab_replay_probe.stderr.log')
try {
    $env:MA0T10_REPLAY_RHI='1'; $env:MA0T10_ARTEMIS_URL=$BrokerUrl; $env:MA0T10_ARTEMIS_USER=$UserName; $env:MA0T10_ARTEMIS_PASSWORD=$Password
    $env:MA0T10_REPLAY_REPORT=Join-Path $Reports 'slab_replay_runtime.json'
    & 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' (Join-Path $Root 'ma0t10_dt.uproject') -unattended -nosplash -windowed -RenderOffscreen -NoSound -ResX=1920 -ResY=1080 '-ExecCmds=Automation RunTests MA0T10.ScenarioReplay.Runtime;Quit' '-TestExit=Automation Test Queue Empty' "-abslog=$Log"
    $EditorExit=$LASTEXITCODE
    if($EditorExit -ne 0){throw "Replay RHI failed: $Log"}
    if(-not $Probe.WaitForExit(60000)){throw 'External probe did not complete'}
    $Result=Get-Content $ProbeJson -Raw | ConvertFrom-Json
    if(-not $Result.success){throw "External validation failed: $ProbeJson"}
    if(-not (Select-String -Path $Log -Pattern 'Result=\{Success\}.*Name=\{Runtime\}' -Quiet)){throw 'No runtime success result'}
    Write-Host "Scenario replay RHI and external PCD validation passed: $Reports"
} finally {
    if(-not $Probe.HasExited){$Probe.Kill()}
    Remove-Item Env:MA0T10_REPLAY_RHI,Env:MA0T10_REPLAY_REPORT,Env:MA0T10_ARTEMIS_URL,Env:MA0T10_ARTEMIS_USER,Env:MA0T10_ARTEMIS_PASSWORD -ErrorAction SilentlyContinue
}
