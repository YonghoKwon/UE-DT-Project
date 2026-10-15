param([string]$ReportDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(!$ReportDirectory){$ReportDirectory=Join-Path $root ('Saved/Reports/PCD-20HZ/faults-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))}
$cases=@(
    @{Name='healthy';Seconds=10;Options=@()},
    @{Name='slow';Seconds=18;Options=@('--pause-after-ms=1000','--pause-ms=8000')},
    @{Name='slow';Folder='slow-long';Seconds=30;Options=@('--heartbeat-ms=5000','--require-heartbeat-ms=5000','--pause-after-ms=1000','--pause-ms=20000')},
    @{Name='heartbeat';Seconds=8;Options=@()},
    @{Name='silent';Seconds=8;Options=@('--silent-after-ms=500')},
    @{Name='error';Seconds=10;Options=@('--error-after-ms=1500')},
    @{Name='reset';Seconds=10;Options=@('--close-after-ms=1500')},
    @{Name='overload-resume';Seconds=45;Options=@('--heartbeat-ms=5000','--require-heartbeat-ms=5000','--pause-after-ms=1000','--pause-ms=20000')}
)
foreach($case in $cases){$folder=if($case.Folder){$case.Folder}else{$case.Name};& "$PSScriptRoot/run-worker.ps1" -Case $case.Name -Seconds $case.Seconds -ServerOptions $case.Options -ReportDirectory (Join-Path $ReportDirectory $folder)}
# Exercise publisher admission over an actual socket, in addition to the raw
# worker faults above. The separate PCD-OPT acceptance run lasts five minutes.
& "$PSScriptRoot/run-package.ps1" -EditorPie -Case Limited -Seconds 60 -SendBufferBytes 16777216 -KeepDevices -AcceptRateControl -RequireNoHitch -ReportDirectory (Join-Path $ReportDirectory 'bandwidth-admission')
