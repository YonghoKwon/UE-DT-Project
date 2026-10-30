param(
    [Parameter(Mandatory=$true)][string]$RunReport,
    [ValidateRange(30,7200)][int]$MaximumSeconds=600,
    [ValidateRange(1,120)][int]$WaitForOwnerSeconds=60
)
# Diagnostic collector for an existing, normally launched project run. It never
# elevates, enables IFEO/GFlags, changes security, or controls another recorder.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$allowed=[IO.Path]::GetFullPath((Join-Path $root 'Saved/Reports')).TrimEnd('\')+'\'
$run=[IO.Path]::GetFullPath($RunReport)
if(!$run.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'RunReport must be under this project Saved/Reports'}
if(!(Test-Path -LiteralPath $run -PathType Container)){throw 'RunReport must already exist'}
$output=Join-Path $run 'windows-allocation-trace'
if(Test-Path -LiteralPath $output){throw 'Use a fresh run; existing trace evidence is preserved'}
New-Item -ItemType Directory -Path $output|Out-Null
$instance='Memory26h_'+[DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')+'_'+$PID
$trace=Join-Path $output 'virtualalloc.etl'
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal($identity)
$elevated=$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$result=[ordered]@{Status='Preparing';DiagnosticOnly=$true;Elevated=$elevated;Instance=$instance;RunReport=$run;StartUtc=[DateTime]::UtcNow.ToString('o');Calls=@();SettingsChanged=$false;TargetPid=$null;TargetCreationUtc=$null;Trace=$trace;Failure='';EnvironmentOverlap=$false;VideoDecodeDetected=$false;ProcessSessionId=(Get-Process -Id $PID).SessionId;Sessions=@(& query.exe session);GpuAdapters=@(Get-CimInstance Win32_VideoController|Select-Object Name,DriverVersion,Status)}
function Save-Result {
    $result|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $output 'collector.json')
}
Save-Result
if(!$elevated){$result.Status='Blocked';$result.Failure='Administrative ETW rights unavailable; no recording was started. No automatic elevation or settings changes.';Save-Result;exit 2}
$wpr=(Get-Command wpr.exe -ErrorAction Stop).Source
function Invoke-Recorder([string[]]$Arguments) {
    $lines=@(& $wpr @Arguments 2>&1|ForEach-Object{$_.ToString()})
    $code=$LASTEXITCODE
    $result.Calls+=@{Utc=[DateTime]::UtcNow.ToString('o');Arguments=$Arguments;ExitCode=$code;Output=$lines}
    Save-Result
    return $code
}
$started=$false;$complete=$false
try {
    $deadline=[DateTime]::UtcNow.AddSeconds($WaitForOwnerSeconds)
    $ownerPath=Join-Path $run 'owned-process.json'
    while(!(Test-Path -LiteralPath $ownerPath)){
        if([DateTime]::UtcNow -ge $deadline){throw 'Owned process metadata did not arrive'}
        Start-Sleep -Milliseconds 200
    }
    $owner=Get-Content -Raw -Encoding UTF8 -LiteralPath $ownerPath|ConvertFrom-Json
    if($owner.PSObject.Properties['DebuggerPid']){throw 'Use a normal run for ETW; CDB instrumentation must be separate'}
    $target=Get-Process -Id $owner.Pid -ErrorAction Stop
    $created=$target.StartTime.ToUniversalTime()
    $result.TargetPid=$target.Id;$result.TargetCreationUtc=$created.ToString('o')
    if($target.ProcessName -notin @('UnrealEditor','UnrealEditor-Cmd','ma0t10_dt','ma0t10_dt-Win64-Shipping')){throw 'Owned target is not a supported project process'}
    $actual=[IO.Path]::GetFullPath($target.Path);$expected=[IO.Path]::GetFullPath($owner.Executable)
    if(![string]::Equals($actual,$expected,[StringComparison]::OrdinalIgnoreCase)){throw 'Target executable identity mismatch'}
    if((Invoke-Recorder -Arguments @('-start','VirtualAllocation','-filemode','-recordtempto',$output,'-instancename',$instance)) -ne 0){throw 'WPR VirtualAllocation start failed; see exact output'}
    $started=$true;$result.Status='Recording';Save-Result
    $clock=[Diagnostics.Stopwatch]::StartNew()
    while($clock.Elapsed.TotalSeconds -lt $MaximumSeconds){
        $target=Get-Process -Id $owner.Pid -ErrorAction SilentlyContinue
        if(!$target -or $target.StartTime.ToUniversalTime() -ne $created){$complete=$true;break}
        if(Test-Path -LiteralPath (Join-Path $run 'environment-overlap.json')){$result.EnvironmentOverlap=$true;throw 'Environment overlap; trace is diagnostic/rejected'}
        $foreign=@(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole,ShaderCompileWorker,ma0t10_dt,ma0t10_dt-Win64-Shipping -ErrorAction SilentlyContinue|Where-Object{
            if($_.Id -eq $owner.Pid){$false}
            elseif($_.ProcessName -eq 'ShaderCompileWorker'){
                (Get-CimInstance Win32_Process -Filter "ProcessId=$($_.Id)").ParentProcessId -ne $owner.Pid
            }else{$true}
        })
        if($foreign.Count){$result.EnvironmentOverlap=$true;throw 'Foreign Unreal/game process observed; do not use this capture for comparison'}
        $traceBytes=(Get-ChildItem -LiteralPath $output -File -Recurse -Filter '*.etl'|Measure-Object -Property Length -Sum).Sum
        if($traceBytes -gt 16GB){throw 'Diagnostic trace size limit reached (16GiB)'}
        $decoder=@(& nvidia-smi.exe --query-gpu=index,utilization.decoder --format=csv,noheader,nounits)
        if($LASTEXITCODE -ne 0 -or !$decoder.Count){throw 'Cannot verify video decoder state'}
        foreach($line in $decoder){$fields=$line -split ',';if($fields.Count -ne 2 -or $fields[1].Trim() -notmatch '^\d+(\.\d+)?$'){throw 'Unavailable decoder counter'};if([double]$fields[1] -gt 0){$result.VideoDecodeDetected=$true;throw 'Video decoding detected'}}
        @{Utc=[DateTime]::UtcNow.ToString('o');Decoder=$decoder;ForeignCount=$foreign.Count}|ConvertTo-Json -Compress|Add-Content -Encoding UTF8 -LiteralPath (Join-Path $output 'environment.jsonl')
        Start-Sleep -Seconds 5
    }
    $complete=$true
}catch{
    $result.Failure=$_.Exception.Message
    if($result.EnvironmentOverlap -or $result.VideoDecodeDetected){
        [IO.File]::WriteAllText((Join-Path $run 'cancel.request'),'Windows allocation collector rejected environment')
    }
}
finally {
    if($started){
        $stopCode=Invoke-Recorder -Arguments @('-stop',$trace,'-instancename',$instance)
        if($stopCode -ne 0){$result.Failure+='; owned WPR stop failed'}
    }
    $result.Status=if($complete -and !$result.Failure){'Captured'}else{'BlockedOrRejected'}
    $result.EndUtc=[DateTime]::UtcNow.ToString('o')
    $result.Scope='Allocation attribution only. Validate ETW lost events, target PID/creation, exact PDBs, time boundaries and normal-run assertion results before interpreting.'
    Save-Result
}
if($result.Status -ne 'Captured'){exit 2}
exit 0
