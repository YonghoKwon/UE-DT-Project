param(
    [Parameter(Mandatory=$true)][ValidateSet('MemoryTrendIdle','MemoryTrendDig','MemoryCostOff','MemoryCostOn','MemoryInvestigate','MemoryTraceProbe','Dig','DigIdle')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$ReportRoot,
    [Parameter(Mandatory=$true)][string]$NativeValidator,
    [ValidateRange(1800,3600)][int]$Seconds=1800,
    [ValidateRange(1024,65535)][int]$Port=18626,
    [switch]$WithoutPcd,
    [switch]$MemoryAudit,
    [ValidateRange(600,1200)][int]$InvestigationSeconds=900,
    [ValidateSet('Reference','LidarOff','PcdOff','PreviewOff','Cpu')][string]$MemoryCut='Reference',
    [switch]$MemoryTrace
)
# One owned Editor/loopback pair. No credentials, GPU configuration or user apps are changed.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$investigation=$Mode -in @('MemoryInvestigate','MemoryTraceProbe')
if($investigation -and $MemoryCut -eq 'PcdOff'){$WithoutPcd=$true}
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
if(Test-Path -LiteralPath $ReportRoot){throw 'Use a fresh report directory'}
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole,ShaderCompileWorker -ErrorAction SilentlyContinue){throw 'Other Unreal work is active'}
New-Item -ItemType Directory -Path $ReportRoot -Force|Out-Null
$cancel=Join-Path $ReportRoot 'cancel.request';$probeStop=Join-Path $ReportRoot 'probe.stop'
$environmentLog=Join-Path $ReportRoot 'environment.jsonl'
$runner=$null;$probe=$null;$contaminated=$false;$failure=''
$nvidia=(Get-Command nvidia-smi.exe -ErrorAction Stop).Source
function Environment-Sample {
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $lines=@(& $nvidia --query-gpu=index,name,driver_version,memory.total,memory.used,utilization.gpu,utilization.decoder --format=csv,noheader,nounits)
    if($LASTEXITCODE -ne 0 -or !$lines.Count){throw 'Cannot verify GPU video decoding state'}
    $gpus=@($lines|ForEach-Object{
        $v=@($_ -split ','|ForEach-Object{$_.Trim()})
        if($v.Count -ne 7 -or $v[6] -notmatch '^\d+(\.\d+)?$'){throw 'Unavailable video decoder counter'}
        @{index=[int]$v[0];name=$v[1];driver=$v[2];total_mib=[double]$v[3];used_mib=[double]$v[4];gpu_percent=[double]$v[5];video_decode_percent=[double]$v[6]}
    })
    $row=@{utc=[DateTime]::UtcNow.ToString('o');gpus=$gpus;monitor_wall_ms=$watch.Elapsed.TotalMilliseconds}
    if($MemoryTrace){$file=Join-Path $ReportRoot 'memory.utrace';if(Test-Path -LiteralPath $file){$row.trace_bytes=(Get-Item -LiteralPath $file).Length;if($row.trace_bytes -gt 100GB -or (Get-PSDrive -Name ([IO.Path]::GetPathRoot($ReportRoot).Substring(0,1))).Free -lt 50GB){throw 'Trace size/free-space safety limit reached'}}}
    $loopback=Join-Path $ReportRoot 'loopback.json'
    if(Test-Path -LiteralPath $loopback){try{$data=Get-Content -Raw -Encoding UTF8 -LiteralPath $loopback|ConvertFrom-Json;$row.external_received=$data.sensors;$row.external_invalid=$data.invalid}catch{$row.external_snapshot_pending=$true}}
    $row|ConvertTo-Json -Depth 8 -Compress|Add-Content -Encoding UTF8 -LiteralPath $environmentLog
    return (@($gpus|Where-Object{$_.video_decode_percent -gt 0}).Count -gt 0)
}
try {
    # Browser names/PIDs are recorded; URLs, titles, command lines and credentials are not.
    $processes=@(Get-Process|Where-Object{$_.ProcessName -match 'chrome|msedge|firefox|brave|vlc|mpv|potplayer|Unreal|Shader|LiveCoding'}|ForEach-Object{@{name=$_.ProcessName;pid=$_.Id;cpu_seconds=$_.CPU;working_set_bytes=$_.WorkingSet64}})
    @{commit=(& git -C $root rev-parse HEAD);mode=$Mode;pcd=(!$WithoutPcd);seconds=$Seconds;port=$Port;processes=$processes;note='No browser/media sessions controlled; decoder queried before and throughout run'}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $ReportRoot 'environment-before.json')
    for($i=0;$i -lt 3;$i++){
        if(Environment-Sample){$contaminated=$true;throw 'Video decoding detected before measurement; stopped'}
        if($i -lt 2){Start-Sleep -Seconds 2}
    }
    if(!$WithoutPcd){
        if(!(Test-Path -LiteralPath $NativeValidator)){throw 'Native PCD validator missing'}
        $probeArgs=@((Join-Path $root 'Tools/Artemis/pcd_loopback_probe.py'),'--port',"$Port",'--report',(Join-Path $ReportRoot 'loopback.json'),'--stop-file',$probeStop,'--native-validator',([IO.Path]::GetFullPath($NativeValidator)),'--sample-limit','0')|ForEach-Object{'"'+$_+'"'}
        $probe=Start-Process python -ArgumentList $probeArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportRoot/probe.log" -RedirectStandardError "$ReportRoot/probe-stderr.log"
        $null=$probe.Handle
        $deadline=[DateTime]::UtcNow.AddSeconds(15)
        while(!(Test-Path -LiteralPath "$ReportRoot/loopback.json")){
            if($probe.HasExited -or [DateTime]::UtcNow -gt $deadline){throw 'Loopback fixture did not become ready'}
            Start-Sleep -Milliseconds 200
        }
    }
    $runArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'run_hatch_validation.ps1'),'-Mode',$Mode,'-Sensors','3','-MemoryWarmupSeconds','35','-DigView','1','-ReportRoot',$ReportRoot,'-MemoryCancelFile',$cancel)
    if($Mode -like 'MemoryTrend*'){$runArgs+=@('-MemoryTrendSeconds',"$Seconds",'-MemorySampleSeconds','5')}elseif(!$investigation -and $Mode -notlike 'MemoryCost*'){$runArgs+=@('-MatchedMemorySeconds','90')}
    if($investigation){$runArgs+=@('-InvestigationSeconds',"$InvestigationSeconds",'-MemoryCut',$MemoryCut);if($MemoryCut -eq 'Cpu'){$runArgs+='-CpuReference'}}
    if($MemoryTrace){$runArgs+=@('-MemoryTraceFile',(Join-Path $ReportRoot 'memory.utrace'))}
    if(!$WithoutPcd){$runArgs+=@('-MemoryPcdLoopbackPort',"$Port")}
    if($MemoryAudit){$runArgs+='-MemoryAudit'}
    $quoted=@($runArgs|ForEach-Object{'"'+$_+'"'})
    $runner=Start-Process powershell -ArgumentList $quoted -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportRoot/runner.log" -RedirectStandardError "$ReportRoot/runner-stderr.log"
    $null=$runner.Handle
    while(!$runner.HasExited){
        try{
            if(Environment-Sample){$contaminated=$true;[IO.File]::WriteAllText($cancel,'video decoding detected');Write-Host 'VIDEO DECODE: measurement cancelled; waiting for normal Editor teardown'}
            if($probe -and $probe.HasExited){throw 'Loopback fixture exited while Editor was active'}
        }catch{
            $failure=$_.Exception.Message;[IO.File]::WriteAllText($cancel,'environment verification unavailable')
        }
        Start-Sleep -Seconds 5
    }
    $runner.WaitForExit();$runner.Refresh();if($runner.ExitCode -ne 0){throw "HATCH runner exited $($runner.ExitCode)"}
    if($contaminated){throw 'Video decoding contaminated run; no acceptance claimed'}
    if($failure){throw $failure}
}catch{$failure=$_.Exception.Message}
finally{
    if($probe -and !$probe.HasExited){[IO.File]::WriteAllText($probeStop,'done');if(!$probe.WaitForExit(10000)){$failure+='; loopback fixture failed normal shutdown'}}
    @{Valid=(!$failure);Failure=$failure;Mode=$Mode;Commit=(& git -C $root rev-parse HEAD);VideoDecodeDetected=$contaminated;EditorRunnerExit=$(if($runner){$runner.ExitCode}else{$null});PcdLoopback=(!$WithoutPcd);EnvironmentLog=$environmentLog;Scope='Local credential-free diagnostic; no deployment acceptance'}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $ReportRoot 'serial-manifest.json')
}
if($failure){throw $failure}
Write-Host "Serial memory run complete: $ReportRoot"
