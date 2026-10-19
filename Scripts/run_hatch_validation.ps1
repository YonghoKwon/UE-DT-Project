param(
    [ValidateSet('Runtime','Captures','Performance','Manual')][string]$Mode='Runtime',
    [ValidateRange(1,2)][int]$Sensors=2,
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.3',
    [string]$ReportRoot='',
    [switch]$Visible
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue){throw 'Close Editor/PIE before HATCH validation'}
if(!$ReportRoot){$ReportRoot=Join-Path $root ('Saved/Reports/HATCH-01/runs/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))}
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
$test=switch($Mode){'Captures'{'HATCH01.CaptureRhi'} 'Performance'{'HATCH01.PerformanceRhi'} default{'HATCH01.RuntimeRhi'}}
$priorX=$env:MA0T10_ACCEPTANCE_RES_X;$priorY=$env:MA0T10_ACCEPTANCE_RES_Y
try {
    $env:MA0T10_ACCEPTANCE_RES_X='1920';$env:MA0T10_ACCEPTANCE_RES_Y='1080'
    $hatchArgs=@("$root/ma0t10_dt.uproject",'/Engine/Maps/Entry','-d3d12','-nosplash','-nosound','-nop4',
        "-ExecCmds=Automation RunTests $test; Quit",'-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$ReportRoot/automation","-HatchReport=$ReportRoot","-HatchSensorCount=$Sensors",
        "-abslog=$ReportRoot/automation.log")
    if($Mode -eq 'Manual'){$hatchArgs+='-HatchManual'}else{$hatchArgs+='-unattended'}
    if(!$Visible -and $Mode -ne 'Manual'){$hatchArgs+='-RenderOffscreen'}
    $quotedArgs=@($hatchArgs | ForEach-Object {'"'+$_+'"'})
    $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $quotedArgs -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput "$ReportRoot/console.log" -RedirectStandardError "$ReportRoot/stderr.log"
    if($process.ExitCode -ne 0){throw "HATCH $Mode failed ($($process.ExitCode)): $ReportRoot"}
    $data=Get-Content "$ReportRoot/automation/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if(@($data.tests).Count -ne 1 -or $data.tests[0].state -ne 'Success'){throw "HATCH test did not pass: $ReportRoot"}
    Write-Host "HATCH $Mode Success: $ReportRoot"
} finally {$env:MA0T10_ACCEPTANCE_RES_X=$priorX;$env:MA0T10_ACCEPTANCE_RES_Y=$priorY}
