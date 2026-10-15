param([ValidateSet('Development','Shipping')][string]$Configuration='Development',
    [ValidateSet('Fake','Artemis')][string]$Broker='Fake',[switch]$EditorPie,[switch]$LegacyPolling,[switch]$WorktreeMap,
    [ValidateSet('Pressure','Limited','Healthy')][string]$Case='Pressure',
    [int]$Seconds=90,[double]$Interval=.05,[switch]$SwitchIntervals,[switch]$RequireData,[switch]$Preview,[switch]$LegacyAdmission,[switch]$RequireNoHitch,[switch]$AcceptRateControl,[switch]$KeepDevices,[switch]$LegacyJson,
    [int]$SendBufferBytes=65536,[int]$CpuMask=0,[string]$ReportDirectory='',[string[]]$ServerOptions=@())
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(!$ReportDirectory){$ReportDirectory=Join-Path $root ('Saved/Reports/PCD-OPT/phase2/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Configuration+'-'+$Case)}
if(Test-Path -LiteralPath (Join-Path $ReportDirectory 'result.json')){throw 'Choose a fresh report directory; existing probe results are preserved'}
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$package=Join-Path $root "Saved/Reports/PCD-OPT/phase2/package-$Configuration/Windows"
$name=if($Configuration -eq 'Shipping'){'ma0t10_dt-Win64-Shipping.exe'}else{'ma0t10_dt.exe'}
$exe=Join-Path $package "ma0t10_dt/Binaries/Win64/$name"
if($EditorPie){$package=$root;$exe='C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor.exe'}
if(!(Test-Path -LiteralPath $exe)){throw "Instrumented package is not staged: $Configuration"}
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue | Where-Object {!$_.HasExited}){throw 'Close Editor before packaged hitch measurement'}
if($Broker -eq 'Fake' -and (Get-NetTCPConnection -LocalPort 61618 -State Listen -ErrorAction SilentlyContinue)){throw 'Local fault port already owned'}
$saved=@{};foreach($v in @('MA0T10_ARTEMIS_URL','MA0T10_ARTEMIS_USER','MA0T10_ARTEMIS_PASSWORD','MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE')){$saved[$v]=[Environment]::GetEnvironmentVariable($v)}
$server=$null;$game=$null
try {
    if(!$ServerOptions.Count){$ServerOptions=switch($Case){
        Pressure {@('--pause-after-ms=3000','--pause-ms=6000','--pause-every-ms=10000','--rtt-ms=10')}
        Limited {@('--max-read-mbps=18.5','--rtt-ms=10')}
        Healthy {@()}
    }}
    if($Broker -eq 'Artemis'){
        if(!(Get-NetTCPConnection -LocalPort 61616 -State Listen -ErrorAction SilentlyContinue)){throw 'Approved Artemis instance must already be running'}
        . "$PSScriptRoot/artemis-session.ps1";$session=Get-Pcd20ArtemisSession $root
        $env:MA0T10_ARTEMIS_URL='ws://127.0.0.1:61616';$env:MA0T10_ARTEMIS_USER=$session.UserName;$env:MA0T10_ARTEMIS_PASSWORD=$session.Passcode
    }else{
        $server=Start-Process (Get-Command node).Source -ArgumentList (@(('"'+(Join-Path $PSScriptRoot 'server.mjs')+'"'),'--echo')+$ServerOptions) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory/server.jsonl" -RedirectStandardError "$ReportDirectory/server-stderr.log"
        Start-Sleep -Milliseconds 500
        if($server.HasExited){throw 'Local server failed to start'}
        $env:MA0T10_ARTEMIS_URL='ws://127.0.0.1:61618';$env:MA0T10_ARTEMIS_USER='local-probe';$env:MA0T10_ARTEMIS_PASSWORD=[Guid]::NewGuid().ToString('N')
    }
    $env:MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE='1'
    $probeArguments=@('/Game/MA0T10/Maps/SensorTestMap','-d3d12','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-nosound','-unattended','-nosplash','-log',
        ('-PcdOptReport="'+$ReportDirectory+'"'),"-PcdOptSeconds=$Seconds",('-PcdOptInterval='+$Interval.ToString([Globalization.CultureInfo]::InvariantCulture)),"-SensorTransportSendBufferBytes=$SendBufferBytes",
        ('-UserDir="'+$ReportDirectory+'/user/"'),('-abslog="'+$ReportDirectory+'/game.log"'))
    if($SwitchIntervals){$probeArguments+='-PcdOptSwitch'};if($RequireData){$probeArguments+='-PcdOptRequired'}
    if($Preview){$probeArguments+='-PcdOptPreview'}
    if($LegacyAdmission){$probeArguments+='-PcdOptLegacyAdmission'}
    if($KeepDevices){$probeArguments+='-PcdOptKeepDevices'};if($LegacyJson){$probeArguments+='-PcdOptLegacyJson'}
    if($LegacyPolling){$probeArguments+='-PcdOptLegacyPolling'}
    if($EditorPie){
        $snapshot=Join-Path $ReportDirectory 'map';New-Item -ItemType Directory -Force -Path $snapshot | Out-Null
        if($WorktreeMap){Copy-Item -LiteralPath "$root/Content/MA0T10/Maps/SensorTestMap.umap" -Destination "$snapshot/SensorTestMap.umap"}
        else{& git -C $root archive --format=zip "--output=$ReportDirectory/map.zip" 291b4f9 Content/MA0T10/Maps/SensorTestMap.umap;Expand-Archive -LiteralPath "$ReportDirectory/map.zip" -DestinationPath "$ReportDirectory/map-archive";Copy-Item -LiteralPath "$ReportDirectory/map-archive/Content/MA0T10/Maps/SensorTestMap.umap" -Destination "$snapshot/SensorTestMap.umap"}
        $probeArguments=@(('"'+$root+'/ma0t10_dt.uproject"'))+$probeArguments[1..($probeArguments.Count-1)]
        $probeArguments+=@('-ExecCmds="Automation RunTests MA0T10.SensorStream.PcdOpt.EditorPressure"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$ReportDirectory+'/automation"'),('-SensorSmokeMapDirectory="'+$snapshot+'"'))
        if($RequireNoHitch){$probeArguments+='-PcdOptNoHitch'}
    }
    $fingerprint=@{};Get-ChildItem (Join-Path $root 'Source') -Recurse -File | ForEach-Object {$fingerprint[$_.FullName.Substring($root.Length+1)]=(Get-FileHash -LiteralPath $_.FullName).Hash}
    if($EditorPie){$fingerprint['Binaries/Win64/UnrealEditor-ma0t10_dt.dll']=(Get-FileHash -LiteralPath "$root/Binaries/Win64/UnrealEditor-ma0t10_dt.dll").Hash}
    $fingerprint['Tools/StompFaultServer/run-package.ps1']=(Get-FileHash -LiteralPath $PSCommandPath).Hash
    @{Commit=(git -C $root rev-parse HEAD);DTCore=(git -C "$root/Plugins/DTCore" rev-parse HEAD);Configuration=$Configuration;EditorPie=[bool]$EditorPie;LegacyPolling=[bool]$LegacyPolling;Broker=$Broker;Case=$Case;Seconds=$Seconds;Interval=$Interval;Switch=[bool]$SwitchIntervals;Required=[bool]$RequireData;SendBufferRequested=$SendBufferBytes;CpuMask=$CpuMask;ExeHash=(Get-FileHash -LiteralPath $exe).Hash;CurrentSourceHashes=$fingerprint;ServerOptions=$ServerOptions} | ConvertTo-Json -Depth 4 | Set-Content "$ReportDirectory/manifest.json" -Encoding utf8
    $game=Start-Process $exe -ArgumentList $probeArguments -WorkingDirectory $package -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory/console.log" -RedirectStandardError "$ReportDirectory/stderr.log"
    if($CpuMask){$game.ProcessorAffinity=[IntPtr]$CpuMask}
    $watch=[Diagnostics.Stopwatch]::StartNew();$cpuSamples=[Collections.Generic.List[object]]::new()
    while(!$game.WaitForExit(1000)){
        # Unreal can restore process affinity during startup. Enforce and record
        # the requested mask throughout the run, rather than trusting launch.
        try {
            $game.Refresh();if($game.HasExited){break}
            $affinityBefore=$game.ProcessorAffinity
            if($CpuMask){$game.ProcessorAffinity=[IntPtr]$CpuMask;$game.Refresh()}
            $affinityValue=$game.ProcessorAffinity;if($game.HasExited){break}
            $cpuSamples.Add([PSCustomObject]@{Elapsed=$watch.Elapsed.TotalSeconds;CpuSeconds=$game.TotalProcessorTime.TotalSeconds;WorkingSetBytes=$game.WorkingSet64;Affinity=if($affinityValue){$affinityValue.ToInt64()}else{0};ObservedAffinity=if($affinityBefore){$affinityBefore.ToInt64()}else{0}})
        }catch{if($game.HasExited){break};throw}
        if($watch.Elapsed.TotalSeconds -gt $Seconds+240){throw 'Probe exceeded deadline; inspect owned process before rebuilding'}
    }
    $cpuSamples | Export-Csv -LiteralPath "$ReportDirectory/process.csv" -NoTypeInformation
    if(!(Test-Path "$ReportDirectory/result.json")){throw 'No packaged probe result; inspect game.log'}
    $result=Get-Content "$ReportDirectory/result.json" -Raw -Encoding utf8 | ConvertFrom-Json
    if(!$result.valid){throw "Packaged fixture failed: $($result.reason)"}
    if($RequireNoHitch -and $result.hitchesOver150ms -ne 0){throw 'Game-thread hitch acceptance failed; inspect hitches.csv'}
    if($AcceptRateControl -and ($result.overloads -ne 0 -or $result.suspensions -ne 0 -or $result.maxReceiptGapSeconds -gt 1)){throw 'Adaptive admission acceptance failed: overload/suspension or receipt gap >1s'}
    if($AcceptRateControl -and $Interval -ge .06 -and $result.rateControlSkipped -ne 0){throw 'Spare capacity must not omit any frame'}
    $result | Select-Object configuration,frames,hitchesOver150ms,maxFrameMs,socketWouldBlock,socketSendBufferBytes,suspensions,switchesApplied,switchesRejected,receiptHz,MBps | ConvertTo-Json
}finally{
    if($game -and !$game.HasExited){$null=$game.CloseMainWindow();if(!$game.WaitForExit(10000)){Write-Warning 'Owned game did not exit; no force termination performed'}}
    if($server -and !$server.HasExited){Stop-Process -Id $server.Id;$server.WaitForExit()}
    if($game -and $game.HasExited){$game.Dispose()}
    if($server -and $server.HasExited){$server.Dispose()}
    foreach($v in $saved.Keys){[Environment]::SetEnvironmentVariable($v,$saved[$v])}
}
