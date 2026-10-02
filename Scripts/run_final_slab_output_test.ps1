$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root 'Saved/Reports/DTCoreFinalAcceptance'
$probe=$null
$old=$env:MA0T10_SLAB_RHI
try{
    $env:MA0T10_SLAB_RHI='1'
    $args=@("`"$(Join-Path $root 'Tools/Artemis/stomp_probe.mjs')`"",'--url','ws://127.0.0.1:61616','--user','artemis','--password','artemis','--topics','topic.virtual.sensor.export.0','--duration','63','--slab-runs','2','--timeout','160','--require-contiguous-pcd','true','--metadata-ledger','true','--quiet','true','--output',"`"$(Join-Path $report 'production-slab.external.json')`"")
    $probe=Start-Process node -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $report 'production-slab.probe.log') -RedirectStandardError (Join-Path $report 'production-slab.probe.err')
    & 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' (Join-Path $root 'ma0t10_dt.uproject') -d3d12 -RenderOffscreen -unattended -nosplash -NoSound -NoVSync '-ExecCmds=r.VSyncEditor 0,r.VSync 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.SlabScenario.ProductionRuntime;Quit' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$(Join-Path $report 'production-slab.automation')" "-abslog=$(Join-Path $report 'production-slab.log')"
    if($LASTEXITCODE -ne 0){throw 'Production Slab automation failed'}
    $until=[DateTime]::UtcNow.AddSeconds(90)
    while(-not $probe.WaitForExit(1000)){if([DateTime]::UtcNow -gt $until){throw 'Production Slab external probe timeout'}}
    $external=Get-Content (Join-Path $report 'production-slab.external.json') -Raw|ConvertFrom-Json
    if(-not $external.success){throw 'Production Slab external validation failed'}
}finally{
    $env:MA0T10_SLAB_RHI=$old
    if($probe -and -not $probe.HasExited){Stop-Process -Id $probe.Id}
}
Write-Output 'Production Slab two-run output test passed.'
