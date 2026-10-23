param(
    [ValidateSet('full-binary','full-compressed','compact-compressed','minimal-compressed','defaults')][string]$Case='full-binary',
    [ValidateRange(30,120)][int]$Seconds=90,
    [int]$Port=18633,
    [switch]$Offscreen,
    [string]$Label=''
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue){throw 'Close Editor/PIE/Live Coding before measurements'}
if(!$Label){$Label=$Case+'-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')}
if($Label -notmatch '^[A-Za-z0-9_.-]+$'){throw 'Invalid label'}
$report=Join-Path $root ('Saved/Reports/PCD-BANDWIDTH-01/measurements/'+$Label)
New-Item -ItemType Directory -Force -Path $report|Out-Null
$stop=Join-Path $report 'stop.flag'
if(Test-Path -LiteralPath $stop){throw 'Use a fresh label'}
$env:MA0T10_ACCEPTANCE_RES_X='1920';$env:MA0T10_ACCEPTANCE_RES_Y='1080'
$probe=$null
try{
    $probeArgs=@((Join-Path $root 'Tools/Artemis/pcd_loopback_probe.py'),'--port',"$Port",'--report',(Join-Path $report 'loopback.json'),'--stop-file',$stop)|ForEach-Object{'"'+$_+'"'}
    $probe=Start-Process python -ArgumentList $probeArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$report/probe.log" -RedirectStandardError "$report/probe-stderr.log"
    $deadline=[DateTime]::UtcNow.AddSeconds(10)
    while(!(Test-Path -LiteralPath "$report/loopback.json")){
        if($probe.HasExited -or [DateTime]::UtcNow -gt $deadline){throw 'Loopback fixture did not start'}
        Start-Sleep -Milliseconds 200
    }
    $args=@("$root/ma0t10_dt.uproject",'/Engine/Maps/Entry','-unattended','-nop4','-nosplash','-NoSound','-windowed','-d3d12','-ResX=1920','-ResY=1080',"-GpuLidarPcdLoopbackPort=$Port","-GpuLidarMeasurementSeconds=$Seconds","-ReportExportPath=$report","-abslog=$report/editor.log",'-TestExit=Automation Test Queue Empty','-ExecCmds=t.MaxFPS 60,r.VSync 0,r.VSyncEditor 0,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10.GpuLidar.ScheduledThree;Quit')
    if($Case -like '*compressed'){$args+='-PcdBandwidthCompressed'}
    if($Case -eq 'compact-compressed'){$args+='-PcdBandwidthProfile=Compact'}
    if($Case -eq 'minimal-compressed'){$args+='-PcdBandwidthProfile=Minimal'}
    if($Offscreen){$args+='-RenderOffscreen'}
    $quoted=@($args|ForEach-Object{'"'+$_+'"'})
    $editor=Start-Process 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $quoted -WindowStyle Hidden -PassThru -RedirectStandardOutput "$report/console.log" -RedirectStandardError "$report/stderr.log"
    $editor.WaitForExit()
    $exitCode=$editor.ExitCode
    $result=Join-Path $report 'measurements/pcd-on/native-three.json'
    if(!(Test-Path -LiteralPath $result)){throw "Measurement missing: $report"}
    $data=Get-Content -LiteralPath $result -Raw -Encoding UTF8|ConvertFrom-Json
    $manifest=[ordered]@{task_id='PCD-BANDWIDTH-01';case=$Case;commit=(& git -C $root rev-parse HEAD);source_diff=(& git -C $root diff --numstat -- Source);seconds=$Seconds;warmup_seconds=10;display=$(if($Offscreen){'offscreen'}else{'visible'});rhi='D3D12';rt='ProjectDefault';engine_exit=$exitCode;automation_valid=$data.valid;measurement=$result;scope='Local credential-free STOMP fixture; no company/external receiver acceptance'}
    $manifest|ConvertTo-Json -Depth 5|Set-Content -LiteralPath "$report/manifest.json" -Encoding UTF8
    Write-Host "Case=$Case valid=$($data.valid) fps=$($data.engine_fps) frame>150ms=$($data.frames_over_150ms) report=$report"
    foreach($sensor in $data.sensors){Write-Host "$($sensor.sensor): scan=$($sensor.hz)Hz sent=$($sensor.pcd.submitted_hz)Hz MiB/s=$($sensor.pcd.mib_per_second) encode p50/p95=$($sensor.pcd.window_encode_p50_ms)/$($sensor.pcd.window_encode_p95_ms)ms"}
}finally{
    if($probe){Set-Content -LiteralPath $stop -Value 'done';if(!$probe.WaitForExit(10000)){throw 'Loopback fixture did not stop normally'}}
    Remove-Item Env:MA0T10_ACCEPTANCE_RES_X,Env:MA0T10_ACCEPTANCE_RES_Y -ErrorAction SilentlyContinue
}
