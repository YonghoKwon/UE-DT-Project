param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$ReportRoot,
    [Parameter(Mandatory=$true)][string]$NativeValidator,
    [ValidateSet(30,900,1200,1800,3600,7200)][int]$Seconds=3600,
    [switch]$FinalSoak,
    [switch]$Investigation,
    [switch]$MemoryTrace,
    [switch]$TraceCanary,
    [ValidateSet('none','baseline','crash-off','gpu-stats-off','rdg-events-off','mimalloc','trim-60s')][string]$ExperimentCase='none',
    [ValidateSet('none','baseline','preview-reuse','scene-reuse','readback-reuse','point-leases4','pcd-scratch-reuse')][string]$CodeCase='none',
    [switch]$CameraTagCanary,
    [switch]$DisableProjectTags,
    [switch]$ObserveReceive,
    [switch]$Visible,
    [ValidateRange(1024,65535)][int]$Port=18626
)
# Owned standalone game/loopback only. User apps, broker settings and maps are
# never changed. A30s run qualifies the diagnostic; it is not a soak verdict.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(($Seconds -eq 7200) -ne [bool]$FinalSoak){throw '7200s requires explicit FinalSoak; other diagnostic durations unchanged'}
if($CodeCase -ne 'none' -and $ExperimentCase -ne 'baseline'){throw 'Code experiments require engine controls at baseline'}
if($CameraTagCanary -and $Seconds -ne 30){throw 'Tag registry canary is30s qualification only; never a soak'}
if(($Seconds -in @(900,1200)) -ne [bool]$Investigation){throw '900/1200s require explicit Investigation; ordinary30s/1800..3600s modes remain unchanged'}
if($TraceCanary -and (!$MemoryTrace -or $Seconds -ne 30)){throw 'TraceCanary requires the separate30s memory-trace calibration'}
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
$Executable=(Resolve-Path -LiteralPath $Executable).Path
$NativeValidator=(Resolve-Path -LiteralPath $NativeValidator).Path
if(Test-Path -LiteralPath $ReportRoot){throw 'Use a fresh report directory'}
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole,ShaderCompileWorker,ma0t10_dt,ma0t10_dt-Win64-Shipping -ErrorAction SilentlyContinue){throw 'Other Unreal work is active'}
New-Item -ItemType Directory -Path $ReportRoot | Out-Null
$cancel=Join-Path $ReportRoot 'cancel.request';$stop=Join-Path $ReportRoot 'probe.stop'
$game=$null;$probe=$null;$failure='';$contaminated=$false
$nvidia=(Get-Command nvidia-smi.exe -ErrorAction Stop).Source
function Environment-Sample {
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $lines=@(& $nvidia --query-gpu=index,name,driver_version,memory.total,memory.used,utilization.gpu,utilization.decoder --format=csv,noheader,nounits)
    if($LASTEXITCODE -ne 0 -or !$lines.Count){throw 'Cannot verify video decoding state'}
    $gpus=@($lines|ForEach-Object{
        $v=@($_ -split ','|ForEach-Object{$_.Trim()})
        if($v.Count -ne 7 -or $v[6] -notmatch '^\d+(\.\d+)?$'){throw 'Unavailable video decoder counter'}
        @{index=[int]$v[0];name=$v[1];driver=$v[2];total_mib=[double]$v[3];used_mib=[double]$v[4];gpu_percent=[double]$v[5];video_decode_percent=[double]$v[6]}
    })
    if($MemoryTrace){$trace=Join-Path $ReportRoot 'memory.utrace';if(Test-Path -LiteralPath $trace){if((Get-Item -LiteralPath $trace).Length -gt 100GB -or (Get-PSDrive -Name ([IO.Path]::GetPathRoot($ReportRoot).Substring(0,1))).Free -lt 50GB){throw 'Trace size/free-space safety limit reached'}}}
    @{utc=[DateTime]::UtcNow.ToString('o');gpus=$gpus;monitor_wall_ms=$watch.Elapsed.TotalMilliseconds}|ConvertTo-Json -Depth 5 -Compress|Add-Content -Encoding UTF8 -LiteralPath "$ReportRoot/environment.jsonl"
    return @($gpus|Where-Object{$_.video_decode_percent -gt 0}).Count -gt 0
}
try {
    $processes=@(Get-Process|Where-Object{$_.ProcessName -match 'chrome|msedge|firefox|brave|vlc|mpv|potplayer|Unreal|Shader|LiveCoding'}|ForEach-Object{@{name=$_.ProcessName;pid=$_.Id;working_set_bytes=$_.WorkingSet64}})
    $source=((& git -C $root rev-parse HEAD) -join '')
    @{commit=$source;executable=$Executable;executable_sha256=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash;seconds=$Seconds;experiment_case=$ExperimentCase;processes=$processes;note='Explicit blank loopback credentials; fresh UserDir; no user app/URL/title/command line capture'}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath "$ReportRoot/environment-before.json"
    for($i=0;$i -lt 3;$i++){
        if(Environment-Sample){$contaminated=$true;throw 'Video decoding detected before measurement; stopped'}
        if($i -lt 2){Start-Sleep -Seconds 2}
    }
    $probeArgs=@((Join-Path $root 'Tools/Artemis/pcd_loopback_probe.py'),'--port',"$Port",'--report',"$ReportRoot/loopback.json",'--stop-file',$stop,'--native-validator',$NativeValidator,'--semantic','--sample-limit','0')|ForEach-Object{'"'+$_+'"'}
    $probe=Start-Process python -ArgumentList $probeArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportRoot/probe.log" -RedirectStandardError "$ReportRoot/probe-stderr.log"
    $null=$probe.Handle;$ready=[DateTime]::UtcNow.AddSeconds(15)
    while(!(Test-Path -LiteralPath "$ReportRoot/loopback.json")){
        if($probe.HasExited -or [DateTime]::UtcNow -gt $ready){throw 'Loopback fixture did not become ready'}
        Start-Sleep -Milliseconds 200
    }
    $commands='t.MaxFPS 60,r.VSync 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0'
    $expected=@{}
    switch($ExperimentCase){
        'crash-off' {$expected['r.gpucrash.collectionenable']=0}
        'gpu-stats-off' {$expected['r.GPUStatsEnabled']=0}
        'rdg-events-off' {$expected['r.RDG.Events']=0}
    }
    foreach($key in $expected.Keys){$commands+=",$key $($expected[$key])"}
    $arguments=@('/Game/MA0T10/Maps/Tests/ShipHatchTestMap','-d3d12','-llm','-nosound','-unattended','-nop4','-nosplash','-windowed','-ForceRes','-ResX=1920','-ResY=1080',
        "-ExecCmds=$commands",
        "-UserDir=$ReportRoot/user","-HatchMemoryRuntimeReport=$ReportRoot","-HatchMemoryRuntimeSeconds=$Seconds",'-HatchMemorySampleSeconds=5',
        "-HatchMemoryPcdLoopbackPort=$Port","-HatchMemoryCancelFile=$cancel","-HatchSourceCommit=$source","-abslog=$ReportRoot/game.log")
    if($Investigation){$arguments+='-HatchMemoryInvestigation'}
    if($ObserveReceive){$arguments+='-HatchMemoryObserveReceive'}
    if($FinalSoak){$arguments+='-HatchMemoryFinalValidation'}
    if($MemoryTrace){$arguments+=@('-trace=memory,bookmark',"-tracefile=$ReportRoot/memory.utrace")}
    if($TraceCanary){$arguments+='-HatchMemoryTraceCanary'}
    if($ExperimentCase -ne 'none'){$arguments+='-HatchMemoryExperiment'}
    if($CodeCase -ne 'none'){$arguments+="-Memory26Case=$CodeCase"}
    if($CameraTagCanary){$arguments+='-Memory26CameraTagCanary'}
    if(!$Visible){$arguments+='-RenderOffscreen'}
    if($DisableProjectTags){$arguments+='-NoProjectMemoryTags'}
    if($ExperimentCase -eq 'mimalloc'){$arguments+='-mimalloc'}
    if($ExperimentCase -eq 'trim-60s'){$arguments+='-HatchMemoryTrimSeconds=60'}
    $arguments=@($arguments|ForEach-Object{'"'+$_+'"'})
    $windowStyle=if($Visible){'Normal'}else{'Hidden'}
    $game=Start-Process -FilePath $Executable -ArgumentList $arguments -WindowStyle $windowStyle -PassThru -RedirectStandardOutput "$ReportRoot/console.log" -RedirectStandardError "$ReportRoot/stderr.log"
    $null=$game.Handle;$deadline=[DateTime]::UtcNow.AddSeconds($Seconds+900)
    while(!$game.HasExited){
        try{
            if(Environment-Sample){$contaminated=$true;[IO.File]::WriteAllText($cancel,'video decoding detected')}
            if($probe.HasExited){throw 'Loopback fixture exited during game run'}
            if([DateTime]::UtcNow -gt $deadline){throw 'Package diagnostic deadline exceeded'}
        }catch{$failure=$_.Exception.Message;[IO.File]::WriteAllText($cancel,'environment verification unavailable')}
        Start-Sleep -Seconds 5
    }
    $game.WaitForExit();$game.Refresh()
    if($game.ExitCode -ne 0){throw "Packaged game exited $($game.ExitCode)"}
    if($contaminated){throw 'Video decoding contaminated run; no acceptance claimed'}
    if($failure){throw $failure}
    $last=Get-Content -LiteralPath "$ReportRoot/memory-trend.jsonl" -Tail 1 -Encoding UTF8|ConvertFrom-Json
    if($last.type -ne 'summary' -or !$last.complete){throw 'No complete runtime memory summary'}
    if($ExperimentCase -ne 'none'){
        $experiment=@(Get-Content -LiteralPath "$ReportRoot/experiment.jsonl" -Encoding UTF8|ForEach-Object{$_|ConvertFrom-Json})
        if($experiment[-1].type -ne 'summary' -or !$experiment[-1].complete){throw 'Incomplete experiment sidecar'}
        foreach($entry in $experiment|Where-Object{$_.type -in @('metadata','sample','summary')}){
            if($CodeCase -ne 'none' -and $entry.memory26_case -ne $CodeCase){throw 'Code experiment mode mismatch'}
            foreach($key in $expected.Keys){$value=$entry.cvars.PSObject.Properties[$key];if(!$value -or !$value.Value.present -or $value.Value.value -ne $expected[$key]){throw "Experiment CVar not effective: $key"}}
        }
        if($ExperimentCase -eq 'mimalloc' -and $experiment[0].allocator -notmatch 'mimalloc'){throw 'Requested mimalloc not active'}
        if($ExperimentCase -eq 'trim-60s' -and $Seconds -ge 60 -and $experiment[-1].trim_calls -le 0){throw 'Diagnostic trim never ran'}
        if($ExperimentCase -ne 'trim-60s' -and $experiment[-1].trim_calls -ne 0){throw 'Unexpected allocator trim'}
        $reused=@{'preview-reuse'='preview_array_reuse';'scene-reuse'='tlas_reuse';'readback-reuse'='readback_reuse';'pcd-scratch-reuse'='pcd_scratch_reuse'}
        if($reused.ContainsKey($CodeCase) -and $experiment[-1].resource_counters.PSObject.Properties[$reused[$CodeCase]].Value -le 0){throw 'Requested reuse path was not exercised'}
    }
}catch{$failure=$_.Exception.Message}
finally {
    if($probe -and !$probe.HasExited){[IO.File]::WriteAllText($stop,'done');if(!$probe.WaitForExit(10000)){$failure+='; loopback failed normal shutdown'}}
    try{
        $rx=Get-Content -Raw -Encoding UTF8 "$ReportRoot/loopback.json"|ConvertFrom-Json
        if($rx.invalid -ne 0 -or @($rx.errors).Count -ne 0 -or !$rx.native_validator_enabled -or $rx.validated_frames -le 0){throw 'External full PCD validation failed'}
        if(!$rx.native_records_enabled -or $rx.semantic_validated_frames -ne $rx.validated_frames){throw 'External per-point semantic validation missing'}
        $streams=@($rx.sensors.PSObject.Properties|Where-Object{$_.Value.frames -gt 0})
        if($streams.Count -ne 3 -or $rx.receipts_sent -le 0){throw 'External receiver did not validate and receipt all three sensors'}
        if(!$ObserveReceive -and ($rx.subscriptions -ne 0 -or $rx.messages_sent -ne 0)){throw 'Unrequested internal observation still active'}
        if($ObserveReceive -and ($rx.subscriptions -le 0 -or $rx.messages_sent -le 0)){throw 'Requested internal observation not exercised'}
        @{Pass=$true;ValidatedFrames=$rx.validated_frames;SemanticValidatedFrames=$rx.semantic_validated_frames;NativeRecordsEnabled=$rx.native_records_enabled;Sensors=$streams.Count;Subscriptions=$rx.subscriptions;EchoMessages=$rx.messages_sent;Receipts=$rx.receipts_sent}|ConvertTo-Json|Set-Content -Encoding UTF8 "$ReportRoot/pcd-validation.json"
    }catch{$failure+='; '+$_.Exception.Message}
    @{Valid=(!$failure);Failure=$failure;Commit=((& git -C $root rev-parse HEAD)-join '');VideoDecodeDetected=$contaminated;Seconds=$Seconds;ExperimentCase=$ExperimentCase;CodeCase=$CodeCase;CameraTagCanary=[bool]$CameraTagCanary;GameExit=$(if($game){$game.ExitCode}else{$null});Executable=$Executable;Scope='Packaged local diagnostic; no company acceptance'}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath "$ReportRoot/run-manifest.json"
}
if($failure){throw $failure}
Write-Host "Packaged memory run complete: $ReportRoot"
