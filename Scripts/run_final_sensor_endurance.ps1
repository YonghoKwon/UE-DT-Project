param([ValidateSet('Steady','Repeated')][string]$Mode='Steady')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root 'Saved/Reports/DTCoreFinalAcceptance'
$label=if($Mode -eq 'Steady'){'soak-three'}else{'repeat-three'}
$probe=$null;$editor=$null;$samples=[Collections.Generic.List[object]]::new()
$previous=@{}
$variables=@{MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE='1';MA0T10_REPEAT_STREAMS_RHI='1';MA0T10_ARTEMIS_URL='ws://127.0.0.1:61616';MA0T10_ARTEMIS_USER='artemis';MA0T10_ARTEMIS_PASSWORD='artemis';MA0T10_ACCEPTANCE_RES_X='1920';MA0T10_ACCEPTANCE_RES_Y='1080';MA0T10_STREAM_WARMUP_SECONDS='10';MA0T10_STREAM_MEASURE_SECONDS='600';MA0T10_ACCEPTANCE_LEDGER=(Join-Path $report "$label.ledger.json")}
try {
    foreach($key in $variables.Keys){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$variables[$key],'Process')}
    $group=if($Mode -eq 'Steady'){'ContinuousThreeStreamSmoke'}else{'RepeatThreeStreamAcceptance'}
    # Repeated runs have intentional idle intervals; continuity is checked within
    # each accepted request set, not by pretending the whole interval is 30/20 Hz.
    $seconds=if($Mode -eq 'Steady'){600}else{530}
    $probeArgs=@("`"$(Join-Path $root 'Tools/Artemis/stomp_probe.mjs')`"",'--url','ws://127.0.0.1:61616','--user','artemis','--password','artemis','--warmup','10','--duration',"$seconds",'--timeout','720','--metadata-ledger','true','--quiet','true','--output',"`"$(Join-Path $report "$label.external.json")`"")
    $probe=Start-Process node -ArgumentList $probeArgs -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $report "$label.probe.log") -RedirectStandardError (Join-Path $report "$label.probe.err")
    $editorArgs=@("`"$(Join-Path $root 'ma0t10_dt.uproject')`"",'-d3d12','-RenderOffscreen','-unattended','-nosplash','-NoSound','-NoVSync',"-ExecCmds=`"t.MaxFPS 0,r.VSync 0,r.VSyncEditor 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.SensorV2.Runtime.$group;Quit`"",'-TestExit="Automation Test Queue Empty"',"-ReportExportPath=`"$(Join-Path $report "$label.automation")`"","-abslog=`"$(Join-Path $report "$label.log")`"")
    $editor=Start-Process 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $editorArgs -WorkingDirectory $root -WindowStyle Hidden -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(800)
    while(-not $editor.HasExited){
        $editor.Refresh();$samples.Add([pscustomobject]@{utc=[DateTimeOffset]::UtcNow.ToString('o');privateBytes=$editor.PrivateMemorySize64;workingSet=$editor.WorkingSet64})
        if([DateTime]::UtcNow -gt $deadline){throw 'Endurance editor timeout'}
        Start-Sleep -Seconds 5
    }
    $editor.WaitForExit();if($editor.ExitCode -ne 0){throw "Endurance automation failed ($($editor.ExitCode))"}
    $probeDeadline=[DateTime]::UtcNow.AddSeconds(240)
    while(-not $probe.WaitForExit(1000)){if([DateTime]::UtcNow -gt $probeDeadline){throw 'Endurance external probe timeout'}}
    $external=Get-Content (Join-Path $report "$label.external.json") -Raw|ConvertFrom-Json
    if(-not $external.success){throw 'Endurance external validation failed'}
}finally{
    $samples|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $report "$label.memory.json") -Encoding utf8
    if($editor -and -not $editor.HasExited){Stop-Process -Id $editor.Id}
    if($probe -and -not $probe.HasExited){Stop-Process -Id $probe.Id}
    foreach($key in $variables.Keys){[Environment]::SetEnvironmentVariable($key,$previous[$key],'Process')}
}
Write-Output "Endurance $Mode finished; request matching and memory trend still require review."
