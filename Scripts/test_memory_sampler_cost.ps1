param([Parameter(Mandatory=$true)][string]$ReportRoot)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $ReportRoot){throw 'Use fresh synthetic fixture evidence'}
New-Item -ItemType Directory -Path $ReportRoot -Force|Out-Null
$results=@()
foreach($case in @('complete','gt-regression','source-mismatch','missing-last-B','video-decode','starved-sensor','wrong-backend')){
    $cohort=Join-Path $ReportRoot $case;New-Item -ItemType Directory -Force -Path $cohort|Out-Null
    for($block=1;$block -le 3;$block++){
        for($position=1;$position -le 4;$position++){
            if($case -eq 'missing-last-B' -and $block -eq 3 -and $position -eq 4){continue}
            $on=$position%2 -eq 0;$label=if($on){'on'}else{'off'};$folder=Join-Path $cohort "block$block-$position-$label"
            New-Item -ItemType Directory -Force -Path (Join-Path $folder 'automation')|Out-Null
            $pcd=@{};1..3|ForEach-Object{$pcd['sensor'+$_]=@{input_frames=1200}}
            if($case -eq 'starved-sensor' -and $on){$pcd.sensor3.input_frames=0}
            $backends=@{sensor1=3;sensor2=3;sensor3=3};if($case -eq 'wrong-backend' -and $on){$backends.sensor3=1}
            @{complete=$true;cost_probe=$true;sampler_enabled=$on;llm_enabled=$true;measurement_seconds=60;sample_count=$(if($on){12}else{0});average_fps=60;engine_frame_p95_ms=16.667;game_thread_mean_ms=$(if($case -eq 'gt-regression' -and $on){10.3}else{10});pcd=$pcd;backend_before=$backends;backend_after=$backends}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $folder 'memory-cost.json')
            @{Commit='synthetic';Submodule=$(if($case -eq 'source-mismatch' -and $on){'other'}else{'fixture'});Width=1920;Height=1080;FontPercent=100;MemoryWarmupSeconds=35;MemoryAudit=$true;FullRhiDrain=$false;CpuReference=$false;Visible=$false;SourceHashes=@{fixture='abc'}}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $folder 'source-manifest.json')
            @{Valid=$true;VideoDecodeDetected=($case -eq 'video-decode' -and $on)}|ConvertTo-Json|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $folder 'serial-manifest.json')
            @{tests=@(@{state='Success'})}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $folder 'automation/index.json')
        }
    }
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',"$PSScriptRoot/compare_memory_sampler_cost.ps1",'-ReportRoot',$cohort)|ForEach-Object{'"'+$_+'"'}
    $process=Start-Process powershell -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $ReportRoot "$case.log") -RedirectStandardError (Join-Path $ReportRoot "$case-stderr.log")
    $expected=if($case -eq 'complete'){0}else{1}
    if($process.ExitCode -ne $expected){throw "Synthetic $case expected$expected actual$($process.ExitCode)"}
    $results+=@{case=$case;expected_exit=$expected;actual_exit=$process.ExitCode;pass=$true;synthetic_fixture=$true}
}
@{Valid=$true;Failed=0;Cases=$results;Scope='Synthetic judgement only; no measured performance'}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $ReportRoot 'fixture-results.json')
Write-Output '7 synthetic sampler cohort cases passed; no real performance claimed'
