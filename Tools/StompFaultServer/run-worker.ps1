param(
    [ValidateSet('healthy','slow','heartbeat','error','reset','observe')][string]$Case='healthy',
    [int]$Seconds=15,[double]$Interval=.05,[ValidateRange(1,2)][int]$Lidars=1,
    [string[]]$ServerOptions=@(),[string]$ReportDirectory='',
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.3'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(!$ReportDirectory){$ReportDirectory=Join-Path $root ('Saved/Reports/PCD-20HZ/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+$Case)}
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$saved=@{}; foreach($name in @('MA0T10_PCD20HZ_CASE','MA0T10_PCD20HZ_SECONDS','MA0T10_PCD20HZ_INTERVAL','MA0T10_PCD20HZ_LIDARS','MA0T10_ARTEMIS_RAW_URL')){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
$server=$null;$editor=$null
try {
    if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue){throw 'Close Editor/Live Coding before socket fault acceptance'}
    if(Get-NetTCPConnection -LocalPort 61618 -State Listen -ErrorAction SilentlyContinue){throw 'Local fault port 61618 is already owned; no existing process will be replaced'}
    $server=Start-Process (Get-Command node).Source -ArgumentList (@('"'+(Join-Path $PSScriptRoot 'server.mjs')+'"','--heartbeat-ms=100','--require-heartbeat-ms=100')+$ServerOptions) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory/server.jsonl" -RedirectStandardError "$ReportDirectory/server-stderr.log"
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    while(!($server.HasExited) -and [DateTime]::UtcNow -lt $deadline){if(Select-String -LiteralPath "$ReportDirectory/server.jsonl" -Pattern 'listening' -SimpleMatch -Quiet){break};Start-Sleep -Milliseconds 100}
    if($server.HasExited -or !(Select-String -LiteralPath "$ReportDirectory/server.jsonl" -Pattern 'listening' -SimpleMatch -Quiet)){throw 'Fault server did not listen'}
    $env:MA0T10_PCD20HZ_CASE=$Case;$env:MA0T10_PCD20HZ_SECONDS="$Seconds";$env:MA0T10_PCD20HZ_INTERVAL=$Interval.ToString([Globalization.CultureInfo]::InvariantCulture);$env:MA0T10_PCD20HZ_LIDARS="$Lidars";$env:MA0T10_ARTEMIS_RAW_URL='tcp://127.0.0.1:61618'
    $arguments=@('"'+(Join-Path $root 'ma0t10_dt.uproject')+'"','/Engine/Maps/Entry','-NullRHI','-unattended','-nosplash','-nop4','-nosound','-ExecCmds="Automation RunTests MA0T10.SensorStream.Pcd20Hz.SocketFaults"','-TestExit="Automation Test Queue Empty"','-ReportExportPath="'+$ReportDirectory+'/automation"','-abslog="'+$ReportDirectory+'/worker.log"')
    $editor=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory/console.log" -RedirectStandardError "$ReportDirectory/stderr.log"
    if(!$editor.WaitForExit(($Seconds+90)*1000)){throw 'Socket test exceeded deadline'}
    $index=Get-Content "$ReportDirectory/automation/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    $tests=@($index.tests)
    if($tests.Count -ne 1 -or $tests[0].state -ne 'Success'){throw 'Socket fault regression failed; inspect automation/index.json and worker.log'}
    Write-Output "Socket case $Case Success: $ReportDirectory"
} finally {
    if($editor -and !$editor.HasExited){$null=$editor.CloseMainWindow();if(!$editor.WaitForExit(5000)){throw 'Owned test Editor did not exit normally; do not rebuild until it is closed'}}
    if($server -and !$server.HasExited){Stop-Process -Id $server.Id; $server.WaitForExit()}
    foreach($name in $saved.Keys){[Environment]::SetEnvironmentVariable($name,$saved[$name])}
}
