param(
 [Parameter(Mandatory=$true)][string]$Executable,
 [Parameter(Mandatory=$true)][string]$ReportRoot,
 [ValidateSet('ShipHatchTestMap','SlabScenarioValidationMap','CoilLoadingTestMap')][string]$Map='ShipHatchTestMap',
 [ValidateSet(30,3600)][int]$Seconds=3600,
 [switch]$AllLidarOff,
 [switch]$ForceHardware
)
$ErrorActionPreference='Stop';$root=Split-Path -Parent $PSScriptRoot
$Executable=(Resolve-Path -LiteralPath $Executable).Path;$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
if(Test-Path -LiteralPath $ReportRoot){throw 'Fresh report directory required'}
if($AllLidarOff -and $ForceHardware){throw 'Conflicting all-off/hardware diagnostic conditions'}
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole,ShaderCompileWorker,ma0t10_dt,ma0t10_dt-Win64-Shipping -ErrorAction SilentlyContinue){throw 'Other Unreal work active'}
New-Item -ItemType Directory -Path $ReportRoot|Out-Null
$game=$null;$monitor=$null;$failure='';$video=$false;$clock=$false
function EnvironmentSample{
 $line=& nvidia-smi --query-gpu=name,driver_version,utilization.gpu,utilization.decoder,memory.used --format=csv,noheader,nounits
 $parts=@($line -split ','|ForEach-Object{$_.Trim()})
 if($LASTEXITCODE -ne 0 -or $parts.Count -ne 5 -or $parts[3] -notmatch '^\d+$'){throw 'Decoder counter unavailable'}
 @{Utc=[DateTime]::UtcNow.ToString('o');Gpu=$parts}|ConvertTo-Json -Compress|Add-Content -Encoding UTF8 "$ReportRoot/environment.jsonl"
 if([int]$parts[3] -gt 0){$script:video=$true;throw 'Video decoding detected; stop without retry'}
}
try{
 EnvironmentSample
 $before=@{SourceCommit=((& git -C $root rev-parse HEAD)-join '');Executable=$Executable;ExecutableSHA256=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash;Map=$Map;Seconds=$Seconds;AllLidarOff=[bool]$AllLidarOff;Processes=@(Get-Process|Where-Object{$_.ProcessName -match 'chrome|msedge|firefox|vlc|mpv|Unreal|Shader|LiveCoding'}|ForEach-Object{@{Name=$_.ProcessName;Pid=$_.Id}});Scope='Map defaults and native automatic policy; no forced backend; no map/save write; no trace/debug flags'}
 $before|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$ReportRoot/environment-before.json"
 $args=@("/Game/MA0T10/Maps/Tests/$Map",'-d3d12','-llm','-NoProjectMemoryTags','-nosound','-unattended','-nop4','-nosplash','-windowed','-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',
  '-ExecCmds=t.MaxFPS 60,r.VSync 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0',"-UserDir=$ReportRoot/user","-LidarMapMemoryReport=$ReportRoot","-LidarMapMemorySeconds=$Seconds","-LidarMapMemoryCancel=$ReportRoot/cancel.request","-abslog=$ReportRoot/game.log")
 if($AllLidarOff){$args+='-LidarMapMemoryAllOff'}
 if($ForceHardware){$args+='-LidarMapMemoryHardware'}
 $game=Start-Process -FilePath $Executable -ArgumentList @($args|ForEach-Object{'"'+$_+'"'}) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportRoot/console.log" -RedirectStandardError "$ReportRoot/stderr.log";$null=$game.Handle
 @{Pid=$game.Id;Executable=$Executable;Utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -Encoding UTF8 "$ReportRoot/owned-process.json"
 $m=@("$root/Scripts/record_process_counters.py",'--pid',"$($game.Id)",'--output',"$ReportRoot/os-counters.jsonl",'--stop-file',"$ReportRoot/os.stop",'--interval','5')
 $monitor=Start-Process python -ArgumentList @($m|ForEach-Object{'"'+$_+'"'}) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportRoot/os-monitor.log" -RedirectStandardError "$ReportRoot/os-monitor-stderr.log";$null=$monitor.Handle
 $deadline=[DateTime]::UtcNow.AddSeconds($Seconds+600)
 while(!$game.HasExited){
  try{
   EnvironmentSample
   if($monitor.HasExited){throw 'OS counter monitor exited early'}
   if([DateTime]::UtcNow -gt $deadline){throw 'Map measurement deadline exceeded'}
   if(!$clock -and (Test-Path -LiteralPath "$ReportRoot/memory-trend.jsonl")){
    $curve=@(Get-Content -Encoding UTF8 "$ReportRoot/memory-trend.jsonl"|ForEach-Object{$_|ConvertFrom-Json}|Where-Object{$_.type -eq 'memory_curve'}|Select-Object -First 1)
    if($curve.Count){@{FirstCurveObservedUtc=[DateTime]::UtcNow.ToString('o');FirstCurveSeconds=$curve[0].seconds;PollingUncertaintySeconds=5}|ConvertTo-Json|Set-Content -Encoding UTF8 "$ReportRoot/measurement-clock.json";$clock=$true}
   }
  }catch{$failure=$_.Exception.Message;[IO.File]::WriteAllText("$ReportRoot/cancel.request",$failure)}
  Start-Sleep -Seconds 5
 }
 $game.WaitForExit();$game.Refresh();if($game.ExitCode -ne 0){throw "Game exited $($game.ExitCode)"};if($failure){throw $failure}
 $last=Get-Content -Encoding UTF8 "$ReportRoot/memory-trend.jsonl" -Tail 1|ConvertFrom-Json
 if($last.type -ne 'summary' -or !$last.complete){throw 'No complete map footer'}
}catch{$failure=$_.Exception.Message}
finally{
 if($game -and !$game.HasExited){[IO.File]::WriteAllText("$ReportRoot/cancel.request",'launcher failed; orderly owned process shutdown requested');if(!$game.WaitForExit(15000)){$failure+='; owned game did not exit after cancel'}}
 [IO.File]::WriteAllText("$ReportRoot/os.stop",'done');if($monitor){[void]$monitor.WaitForExit(10000)}
 @{Valid=(!$failure);Failure=$failure;VideoDecodeDetected=$video;Map=$Map;AllLidarOff=[bool]$AllLidarOff;ForceHardware=[bool]$ForceHardware;Seconds=$Seconds;GameExit=$(if($game){$game.ExitCode}else{$null});Executable=$Executable;NativeSelection='automatic validated driver/configuration policy; optional sensorHardware diagnostic, never forceNative bypass';Scope='Packaged additional map observation; OS private Commit and private Working Set kept distinct'}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$ReportRoot/run-manifest.json"
}
if($failure){throw $failure}
if($Seconds -eq 3600){& python "$root/Scripts/analyze_lidar_map_memory.py" $ReportRoot;if($LASTEXITCODE -ne 0){throw 'Map/OS analysis failed'}}
