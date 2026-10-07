param([string[]]$Modes=@('auth','delay','close','no_receipt','overload','delete'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root 'Saved/Reports/ScenarioDataReliability/faults'
New-Item -ItemType Directory -Force $report | Out-Null
$previous=$env:MA0T10_SCENARIO_FAULT_MODE
foreach($mode in $Modes){
    if($mode -notin @('auth','delay','close','no_receipt','overload','delete')){throw 'Unknown fixture mode'}
    $server=$null
    try{
        $server=Start-Process node -ArgumentList @("`"$(Join-Path $root 'Tools/Artemis/scenario_fault_server.mjs')`"",'--mode',$mode,'--port','18679','--report',"`"$(Join-Path $report ($mode+'.server.json'))`"") -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $report ($mode+'.server.log')) -RedirectStandardError (Join-Path $report ($mode+'.server.err'))
        $limit=[DateTime]::UtcNow.AddSeconds(10)
        while(-not (Test-Path (Join-Path $report ($mode+'.server.json')))){if($server.HasExited -or [DateTime]::UtcNow -gt $limit){throw 'Fixture failed to start'};Start-Sleep -Milliseconds 100}
        $env:MA0T10_SCENARIO_FAULT_MODE=$mode
        & 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' (Join-Path $root 'ma0t10_dt.uproject') -NullRHI -unattended -nosplash -NoSound '-ExecCmds=Automation RunTests MA0T10.SlabReliability.TcpFaultRuntime;Quit' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$(Join-Path $report $mode)" "-abslog=$(Join-Path $report ($mode+'.log'))"
        if($LASTEXITCODE -ne 0){throw "Fault fixture failed: $mode"}
        Write-Output "Fault fixture passed: $mode"
    }finally{
        $env:MA0T10_SCENARIO_FAULT_MODE=$previous
        if($server -and -not $server.HasExited){Stop-Process -Id $server.Id}
    }
}
