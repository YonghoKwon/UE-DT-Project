param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$ReportRoot,
    [Parameter(Mandatory=$true)][string]$NativeValidator,
    [ValidateSet(30,1800,3600)][int]$Seconds=3600,
    [ValidateRange(1024,65535)][int]$Port=18626
)
# Owned standalone game/loopback only. User apps, broker settings and maps are
# never changed. A30s run qualifies the diagnostic; it is not a soak verdict.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
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
    @{utc=[DateTime]::UtcNow.ToString('o');gpus=$gpus;monitor_wall_ms=$watch.Elapsed.TotalMilliseconds}|ConvertTo-Json -Depth 5 -Compress|Add-Content -Encoding UTF8 -LiteralPath "$ReportRoot/environment.jsonl"
    return @($gpus|Where-Object{$_.video_decode_percent -gt 0}).Count -gt 0
}
try {
    $processes=@(Get-Process|Where-Object{$_.ProcessName -match 'chrome|msedge|firefox|brave|vlc|mpv|potplayer|Unreal|Shader|LiveCoding'}|ForEach-Object{@{name=$_.ProcessName;pid=$_.Id;working_set_bytes=$_.WorkingSet64}})
    $source=((& git -C $root rev-parse HEAD) -join '')
    @{commit=$source;executable=$Executable;executable_sha256=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash;seconds=$Seconds;processes=$processes;note='Explicit blank loopback credentials; fresh UserDir; no user app/URL/title/command line capture'}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath "$ReportRoot/environment-before.json"
    for($i=0;$i -lt 3;$i++){
        if(Environment-Sample){$contaminated=$true;throw 'Video decoding detected before measurement; stopped'}
        if($i -lt 2){Start-Sleep -Seconds 2}
    }
    $probeArgs=@((Join-Path $root 'Tools/Artemis/pcd_loopback_probe.py'),'--port',"$Port",'--report',"$ReportRoot/loopback.json",'--stop-file',$stop,'--native-validator',$NativeValidator,'--sample-limit','0')|ForEach-Object{'"'+$_+'"'}
    $probe=Start-Process python -ArgumentList $probeArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportRoot/probe.log" -RedirectStandardError "$ReportRoot/probe-stderr.log"
    $null=$probe.Handle;$ready=[DateTime]::UtcNow.AddSeconds(15)
    while(!(Test-Path -LiteralPath "$ReportRoot/loopback.json")){
        if($probe.HasExited -or [DateTime]::UtcNow -gt $ready){throw 'Loopback fixture did not become ready'}
        Start-Sleep -Milliseconds 200
    }
    $arguments=@('/Game/MA0T10/Maps/Tests/ShipHatchTestMap','-d3d12','-llm','-nosound','-unattended','-nop4','-nosplash','-windowed','-RenderOffscreen','-ForceRes','-ResX=1920','-ResY=1080',
        '-ExecCmds=t.MaxFPS 60,r.VSync 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0',
        "-UserDir=$ReportRoot/user","-HatchMemoryRuntimeReport=$ReportRoot","-HatchMemoryRuntimeSeconds=$Seconds",'-HatchMemorySampleSeconds=5',
        "-HatchMemoryPcdLoopbackPort=$Port","-HatchMemoryCancelFile=$cancel","-HatchSourceCommit=$source","-abslog=$ReportRoot/game.log")|ForEach-Object{'"'+$_+'"'}
    $game=Start-Process -FilePath $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportRoot/console.log" -RedirectStandardError "$ReportRoot/stderr.log"
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
}catch{$failure=$_.Exception.Message}
finally {
    if($probe -and !$probe.HasExited){[IO.File]::WriteAllText($stop,'done');if(!$probe.WaitForExit(10000)){$failure+='; loopback failed normal shutdown'}}
    @{Valid=(!$failure);Failure=$failure;Commit=((& git -C $root rev-parse HEAD)-join '');VideoDecodeDetected=$contaminated;Seconds=$Seconds;GameExit=$(if($game){$game.ExitCode}else{$null});Executable=$Executable;Scope='Packaged local diagnostic; no company acceptance'}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath "$ReportRoot/run-manifest.json"
}
if($failure){throw $failure}
Write-Host "Packaged memory run complete: $ReportRoot"
