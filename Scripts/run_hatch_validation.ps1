param(
    [ValidateSet('Runtime','Captures','Performance','Manual','Presentation','MemoryControl','Dig','DigCaptures','DigIdle','Feedback','FeedbackCaptures')][string]$Mode='Runtime',
    [ValidateRange(1,3)][int]$Sensors=2,
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.3',
    [string]$ReportRoot='',
    [ValidateRange(1280,3840)][int]$Width=1920,
    [ValidateRange(720,2160)][int]$Height=1080,
    [ValidateRange(10,60)][int]$MemoryWarmupSeconds=35,
    [ValidateSet(100,150)][int]$FontPercent=100,
    [ValidateSet(0,1)][int]$DigView=1,
    [switch]$MemoryAudit,
    [switch]$FullRhiDrain,
    [switch]$Visible
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue){throw 'Close Editor/PIE before HATCH validation'}
if(!$ReportRoot){$group=if($Mode -like 'Feedback*'){'HATCH-04'}elseif($Mode -like 'Dig*'){'HATCH-03'}else{'HATCH-02'};$ReportRoot=Join-Path $root ("Saved/Reports/$group/runs/"+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))}
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
$test=switch($Mode){'Feedback'{'MA0T10.Hatch.UserFeedbackRhi'} 'FeedbackCaptures'{'HATCH04.CapturesRhi'} 'DigIdle'{'HATCH03.IdleMemoryControlRhi'} 'Dig'{'HATCH03.DigRuntimeRhi'} 'DigCaptures'{'HATCH03.DigCaptureRhi'} 'Captures'{'HATCH01.CaptureRhi'} 'Performance'{'HATCH01.PerformanceRhi'} 'Presentation'{'MA0T10.Hatch.ProductPresentationRhi'} 'MemoryControl'{'HATCH02.IdleMemoryControlRhi'} default{'HATCH01.RuntimeRhi'}}
$priorX=$env:MA0T10_ACCEPTANCE_RES_X;$priorY=$env:MA0T10_ACCEPTANCE_RES_Y
try {
    $env:MA0T10_ACCEPTANCE_RES_X=[string]$Width;$env:MA0T10_ACCEPTANCE_RES_Y=[string]$Height
    $hatchArgs=@("$root/ma0t10_dt.uproject",'/Engine/Maps/Entry','-d3d12','-nosplash','-nosound','-nop4',
        "-ExecCmds=Automation RunTests $test; Quit",'-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$ReportRoot/automation","-HatchReport=$ReportRoot","-HatchSensorCount=$Sensors","-HatchMemoryWarmup=$MemoryWarmupSeconds","-HatchFontPercent=$FontPercent","-HatchDigView=$DigView",
        "-abslog=$ReportRoot/automation.log")
    if($Mode -eq 'Manual'){$hatchArgs+='-HatchManual'}else{$hatchArgs+='-unattended'}
    if($MemoryAudit){$hatchArgs+='-HatchMemoryAudit'}
    if($MemoryAudit -and $Mode -like 'Dig*'){$hatchArgs+='-llm'}
    if($FullRhiDrain){$hatchArgs+='-HatchFullRhiDrain'}
    if(!$Visible -and $Mode -ne 'Manual'){$hatchArgs+='-RenderOffscreen'}
    $quotedArgs=@($hatchArgs | ForEach-Object {'"'+$_+'"'})
    $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $quotedArgs -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput "$ReportRoot/console.log" -RedirectStandardError "$ReportRoot/stderr.log"
    if($process.ExitCode -ne 0){throw "HATCH $Mode failed ($($process.ExitCode)): $ReportRoot"}
    $data=Get-Content "$ReportRoot/automation/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if(@($data.tests).Count -ne 1 -or $data.tests[0].state -ne 'Success'){throw "HATCH test did not pass: $ReportRoot"}
    Write-Host "HATCH $Mode Success: $ReportRoot"
} finally {$env:MA0T10_ACCEPTANCE_RES_X=$priorX;$env:MA0T10_ACCEPTANCE_RES_Y=$priorY}
