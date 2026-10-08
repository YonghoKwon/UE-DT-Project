param([string]$Label='after',[int]$Trials=1,[switch]$AllOutputs,[switch]$Repeated,[int]$Width=1920,[int]$Height=1080,[switch]$Trace,
 [ValidateRange(2,100)][int]$RunCount=10,[switch]$ResourceDiagnostics,[switch]$PreviewOff,[switch]$MemoryTrace,
 [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$ReportGroup='SlabRepeatRunStability',[switch]$PlacementAB)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root ('Saved/Reports/'+$ReportGroup+'/'+$Label)
New-Item -ItemType Directory -Force $report|Out-Null
$variables=@{MA0T10_SLAB_RHI='1';MA0T10_SLAB_REQUIRE_DATA='1';MA0T10_SLAB_RESET_RHI='1';MA0T10_SLAB_EXACT_VIEWPORT='1';MA0T10_ACCEPTANCE_RES_X="$Width";MA0T10_ACCEPTANCE_RES_Y="$Height";MA0T10_SLAB_OUTPUT_ALL=$(if($AllOutputs){'1'}else{'0'});MA0T10_SLAB_REPEAT_RHI=$(if($Repeated){'1'}else{'0'})}
$variables.MA0T10_SLAB_REPEAT_COUNT="$RunCount"
$variables.MA0T10_PLACEMENT_AB_RHI=if($PlacementAB){'1'}else{'0'}
$variables.MA0T10_SENSOR_RESOURCE_DIAGNOSTICS=if($ResourceDiagnostics){'1'}else{'0'}
$variables.MA0T10_SENSOR_RESOURCE_PREVIEW_OFF=if($PreviewOff){'1'}else{'0'}
$variables.MA0T10_SENSOR_RESOURCE_REPORT_DIR=Join-Path $report 'resources'
$previous=@{}
try {
 foreach($key in $variables.Keys){$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$variables[$key],'Process')}
 for($trial=1;$trial -le $Trials;$trial++){
  $out=Join-Path $report "$trial";New-Item -ItemType Directory -Force $out|Out-Null
  $runs=if($Repeated){$RunCount}else{2};$duration=if($Repeated){$RunCount*33+15}else{75}
  $topics=if($AllOutputs){'topic.virtual.sensor.lidar.0,topic.virtual.sensor.camera.0,topic.virtual.sensor.export.0'}else{'topic.virtual.sensor.export.0'}
  $before=@(Get-ChildItem (Join-Path $root 'Saved/Reports/Slab') -Filter '*.json' -ErrorAction SilentlyContinue|Select-Object -ExpandProperty Name)
  $probe=$null;$editor=$null;$samples=[Collections.Generic.List[object]]::new()
  try {
   $args=@("`"$(Join-Path $root 'Tools/Artemis/stomp_probe.mjs')`"",'--url','ws://127.0.0.1:61616','--topics',$topics,'--duration',"$duration",'--timeout',"$($duration+90)",'--slab-runs',"$runs",'--require-contiguous-pcd','true','--metadata-ledger','true','--quiet','true','--output',"`"$(Join-Path $out 'external.json')`"")
   if($PlacementAB){$args+=@('--placement-ledger','true')}
   $probe=Start-Process node -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $out 'probe.log') -RedirectStandardError (Join-Path $out 'probe.err')
   $test=if($Repeated){'MA0T10.SlabReliability.RepeatedRuntime'}else{'MA0T10.SlabScenario.ProductionRuntime'}
   $editorArgs=@("`"$(Join-Path $root 'ma0t10_dt.uproject')`"",'-d3d12','-RenderOffscreen','-unattended','-nosplash','-NoSound','-NoVSync','-ini:Engine:[/Script/LiveCoding.LiveCodingSettings]:bEnabled=False',"-ExecCmds=`"r.VSyncEditor 0,r.VSync 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests $test;Quit`"",'-TestExit="Automation Test Queue Empty"',"-ReportExportPath=`"$(Join-Path $out 'automation')`"","-abslog=`"$(Join-Path $out 'editor.log')`"")
   if($Trace -or $MemoryTrace){$channels=if($MemoryTrace){'cpu,frame,bookmark,memory,metadata,assetmetadata'}else{'cpu,frame,gpu,bookmark,loadtime'};$editorArgs+=@("-trace=$channels","-tracefile=`"$(Join-Path $out 'runtime.utrace')`"")}
   $editor=Start-Process 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $editorArgs -WindowStyle Hidden -PassThru
   $deadline=[DateTime]::UtcNow.AddSeconds($duration+120)
   while(-not $editor.HasExited){$editor.Refresh();$samples.Add([pscustomobject]@{utc=[DateTimeOffset]::UtcNow.ToString('o');privateBytes=$editor.PrivateMemorySize64;workingSet=$editor.WorkingSet64;handles=$editor.HandleCount;threads=$editor.Threads.Count});if([DateTime]::UtcNow -gt $deadline){throw 'Slab test timeout'};Start-Sleep -Seconds 2}
   $editor.WaitForExit()
   $probeDeadline=[DateTime]::UtcNow.AddSeconds(110)
   while(-not $probe.WaitForExit(1000)){if([DateTime]::UtcNow -gt $probeDeadline){throw 'External probe timeout'}}
   Get-ChildItem (Join-Path $root 'Saved/Reports/Slab') -Filter '*.json'|Where-Object {$_.Name -notin $before}|Copy-Item -Destination $out
   if($editor.ExitCode -ne 0){throw "Slab automation failed ($($editor.ExitCode))"}
   $external=Get-Content (Join-Path $out 'external.json') -Raw|ConvertFrom-Json
   if(-not $external.success){throw 'External subscriber validation failed'}
   Write-Output "$Label trial $trial passed"
  }finally{
   $samples|ConvertTo-Json -Depth 4|Set-Content (Join-Path $out 'memory.json') -Encoding utf8
   if($editor -and -not $editor.HasExited){Stop-Process -Id $editor.Id}
   if($probe -and -not $probe.HasExited){Stop-Process -Id $probe.Id}
  }
 }
}finally{foreach($key in $variables.Keys){[Environment]::SetEnvironmentVariable($key,$previous[$key],'Process')}}
