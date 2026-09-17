param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$project=Join-Path $root 'ma0t10_dt.uproject'
$reports=Join-Path $root 'Saved/Reports'
New-Item -ItemType Directory -Force -Path $reports | Out-Null
if(-not $SkipBuild){
    & 'C:/Program Files/Epic Games/UE_5.3/Engine/Build/BatchFiles/Build.bat' ma0t10_dtEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
    if($LASTEXITCODE -ne 0){throw 'Build failed'}
}
if(-not(Get-NetTCPConnection -State Listen -LocalPort 61616 -ErrorAction SilentlyContinue)){throw 'Start local Artemis on 61616 first; this script does not change broker configuration.'}
$probeArgs='"'+(Join-Path $root 'Tools/Artemis/stomp_probe.mjs')+'" --url ws://127.0.0.1:61616 --user artemis --password artemis --topics topic.virtual.sensor.export.0 --warmup 10 --duration 60 --timeout 130 --require-contiguous-pcd true --save-first-pcd "'+(Join-Path $reports 'geometry_external.pcd')+'" --output "'+(Join-Path $reports 'geometry_external.json')+'"'
$probe=Start-Process node -ArgumentList $probeArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $reports 'geometry_probe.log') -RedirectStandardError (Join-Path $reports 'geometry_probe.err')
$previous=$env:MA0T10_GEOMETRY_PERF
try{
    $env:MA0T10_GEOMETRY_PERF='1'
    & 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' $project -d3d12 -RenderOffscreen -windowed -unattended -nop4 -nosplash -NoSound -NoVSync -ResX=1920 -ResY=1080 '-ExecCmds=t.MaxFPS 0,r.VSync 0,r.VSyncEditor 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.LidarGeometry.StreamPerformance;Quit' '-TestExit=Automation Test Queue Empty' "-abslog=$(Join-Path $reports 'geometry_stream.log')"
    $editorExit=$LASTEXITCODE
    if(-not $probe.WaitForExit(60000)){throw 'External probe timed out'}
    $probe.Refresh()
    if($editorExit -ne 0 -or $probe.ExitCode -ne 0){throw 'Geometry stream test failed; inspect Saved/Reports/geometry_stream.log and geometry_external.json'}
    Write-Output 'Geometry stream validation passed.'
} finally {$env:MA0T10_GEOMETRY_PERF=$previous}
