param([ValidateSet('Fake','Artemis')][string]$Broker='Artemis',[double]$Interval=.05,
    [ValidateRange(1,2)][int]$Lidars=1,[int]$Seconds=300,[string]$ReportDirectory='',
    [string[]]$ServerOptions=@(),[switch]$RecoveryFault,[switch]$OptProbe,[switch]$SwitchIntervals,
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.3')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(!$ReportDirectory){$ReportDirectory=Join-Path $root ('Saved/Reports/PCD-20HZ/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Broker+'-'+$Lidars+'-'+$Interval)}
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$saved=@{};$names=@('MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE','MA0T10_PCD_ONLY','MA0T10_OUTPUT_SENSOR_PAIRS','MA0T10_LAYOUT_API_FIXTURE','MA0T10_PCD_DEMAND_ACCEPTANCE','MA0T10_PCD_JSON_POLICY','MA0T10_STREAM_WARMUP_SECONDS','MA0T10_STREAM_MEASURE_SECONDS','MA0T10_ACCEPTANCE_RES_X','MA0T10_ACCEPTANCE_RES_Y','MA0T10_ACCEPTANCE_LEDGER','MA0T10_ARTEMIS_URL','MA0T10_ARTEMIS_USER','MA0T10_ARTEMIS_PASSWORD','MA0T10_PCD20HZ_NATIVE_INTERVAL')
foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
$saved['MA0T10_PCD20HZ_CASE']=[Environment]::GetEnvironmentVariable('MA0T10_PCD20HZ_CASE')
$server=$null;$editor=$null
try {
    if($RecoveryFault -and $Broker -ne 'Fake'){throw 'RecoveryFault requires the local fake server'}
    if($RecoveryFault -and $Lidars -ne 1){throw 'RecoveryFault uses one explicitly configured PCD sensor for direct UI acceptance'}
    if($RecoveryFault -and $OptProbe){throw 'RecoveryFault and OptProbe have separate acceptance criteria'}
    if($SwitchIntervals -and !$OptProbe){throw 'SwitchIntervals requires OptProbe'}
    if($OptProbe){$env:MA0T10_PCD20HZ_CASE=if($SwitchIntervals){'pcd-opt-switch'}else{'pcd-opt'}}
    elseif($RecoveryFault){$env:MA0T10_PCD20HZ_CASE='overload-resume-native'}else{Remove-Item Env:MA0T10_PCD20HZ_CASE -ErrorAction SilentlyContinue}
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue | Where-Object {!$_.HasExited}){throw 'Close Editor/Live Coding before native acceptance'}
    if(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" | Where-Object {$_.CommandLine -like '*UnrealBuildTool*'}){throw 'Wait for UnrealBuildTool to finish before acceptance'}
    $dllHash=(Get-FileHash "$root/Binaries/Win64/UnrealEditor-ma0t10_dt.dll").Hash
    $sourceHashes=@{};Get-ChildItem "$root/Source" -Recurse -File | ForEach-Object {$sourceHashes[$_.FullName.Substring($root.Length+1)]=(Get-FileHash -LiteralPath $_.FullName).Hash}
    @{Commit=(& git -C $root rev-parse HEAD);DTCore=(& git -C "$root/Plugins/DTCore" rev-parse HEAD);DllHash=$dllHash;SourceHashes=$sourceHashes;Worktree=@(& git -C $root status --porcelain);Broker=$Broker;Seconds=$Seconds;Interval=$Interval;Lidars=$Lidars;StartedUtc=[DateTime]::UtcNow.ToString('o');Rhi='D3D12';Viewport='1920x1080';WarmupSeconds=10} | ConvertTo-Json -Depth 4 | Set-Content "$ReportDirectory/manifest.json" -Encoding UTF8
    if($Broker -eq 'Artemis'){
        if(!(Get-NetTCPConnection -LocalPort 61616 -State Listen -ErrorAction SilentlyContinue)){throw 'Installed Artemis is not listening; start the approved instance without configuration edits'}
        . "$PSScriptRoot/artemis-session.ps1";$session=Get-Pcd20ArtemisSession $root
        $env:MA0T10_ARTEMIS_URL='ws://127.0.0.1:61616';$env:MA0T10_ARTEMIS_USER=$session.UserName;$env:MA0T10_ARTEMIS_PASSWORD=$session.Passcode
    }else{
        if(Get-NetTCPConnection -LocalPort 61618 -State Listen -ErrorAction SilentlyContinue){throw 'Fault port already in use'}
        $server=Start-Process (Get-Command node).Source -ArgumentList (@('"'+(Join-Path $PSScriptRoot 'server.mjs')+'"','--echo')+$ServerOptions) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory/server.jsonl" -RedirectStandardError "$ReportDirectory/server-stderr.log"
        Start-Sleep -Seconds 1
        if($server.HasExited){throw 'Fault server failed'}
        # The map's independent DTCore STOMP bootstrap also reads these values.
        # Keep its auth input complete even though this local TCP harness does
        # not authenticate. Generate the disposable value in memory only.
        $env:MA0T10_ARTEMIS_URL='ws://127.0.0.1:61618';$env:MA0T10_ARTEMIS_USER='local-fault';$env:MA0T10_ARTEMIS_PASSWORD=[Guid]::NewGuid().ToString('N')
    }
    $env:MA0T10_RUN_SENSOR_MAP_STREAM_SMOKE='1';$env:MA0T10_PCD_ONLY='1';$env:MA0T10_OUTPUT_SENSOR_PAIRS="$Lidars"
    $env:MA0T10_LAYOUT_API_FIXTURE='1';$env:MA0T10_PCD_DEMAND_ACCEPTANCE='1';$env:MA0T10_PCD_JSON_POLICY='requested'
    $env:MA0T10_STREAM_WARMUP_SECONDS='10';$env:MA0T10_STREAM_MEASURE_SECONDS="$Seconds"
    $env:MA0T10_ACCEPTANCE_RES_X='1920';$env:MA0T10_ACCEPTANCE_RES_Y='1080';$env:MA0T10_ACCEPTANCE_LEDGER="$ReportDirectory/ledger.json"
    $env:MA0T10_PCD20HZ_NATIVE_INTERVAL=$Interval.ToString([Globalization.CultureInfo]::InvariantCulture)
    & git -C $root archive --format=zip "--output=$ReportDirectory/map.zip" HEAD Content/MA0T10/Maps/SensorTestMap.umap
    if($LASTEXITCODE -ne 0){throw 'Map snapshot failed'}
    Expand-Archive -LiteralPath "$ReportDirectory/map.zip" -DestinationPath "$ReportDirectory/fixture" -Force
    $arguments=@('"'+(Join-Path $root 'ma0t10_dt.uproject')+'"','-d3d12','-unattended','-nosplash','-nop4','-nosound','-windowed','-ResX=1920','-ResY=1080','-NoVSync','-ExecCmds="Automation RunTests MA0T10.SensorV2.Runtime.ContinuousThreeStreamSmoke"','-TestExit="Automation Test Queue Empty"','-ReportExportPath="'+$ReportDirectory+'/automation"','-abslog="'+$ReportDirectory+'/native.log"','-SensorSmokeMapDirectory="'+$ReportDirectory+'/fixture/Content/MA0T10/Maps"')
    $editor=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory/console.log" -RedirectStandardError "$ReportDirectory/stderr.log"
    if(!$editor.WaitForExit(($Seconds+180)*1000)){throw 'Native test exceeded deadline'}
    if((Get-FileHash "$root/Binaries/Win64/UnrealEditor-ma0t10_dt.dll").Hash -ne $dllHash){throw 'DLL changed during test; result invalid'}
    $index=Get-Content "$ReportDirectory/automation/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if(@($index.tests).Count -ne 1 -or $index.tests[0].state -ne 'Success'){throw 'Native acceptance failed; inspect native.log and automation/index.json'}
    Write-Output "Native $Broker interval=$Interval lidars=$Lidars seconds=$Seconds Success: $ReportDirectory"
} finally {
    if($editor -and !$editor.HasExited){$null=$editor.CloseMainWindow();if(!$editor.WaitForExit(10000)){Write-Warning 'Owned Editor did not exit normally; do not rebuild until closed'}}
    if($server -and !$server.HasExited){Stop-Process -Id $server.Id;$server.WaitForExit()}
    if($editor -and $editor.HasExited){$editor.Dispose()}
    if($server -and $server.HasExited){$server.Dispose()}
    foreach($name in $saved.Keys){[Environment]::SetEnvironmentVariable($name,$saved[$name])}
    $session=$null
}
