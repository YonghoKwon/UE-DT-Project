param([string]$ReportRoot='')
# Synthetic gate fixtures only. These never count as measured performance runs.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$ReportRoot){$ReportRoot=Join-Path $root ('Saved/Reports/perf-cohort-fixtures/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))}
if(Test-Path -LiteralPath $ReportRoot){throw "Refuse to overwrite fixture evidence: $ReportRoot"}
New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null
$results=@()
foreach($case in @('complete','missing-last-B','balanced-incomplete','invalid','regression')){
 $folder=Join-Path $ReportRoot $case;New-Item -ItemType Directory -Path $folder | Out-Null
 for($block=1;$block -le 3;$block++){
  for($i=1;$i -le 4;$i++){
   $label=if($i%2){'A'}else{'B'}
   if($block -eq 3 -and (($case -eq 'missing-last-B' -and $i -eq 4) -or ($case -eq 'balanced-incomplete' -and $i -ge 3))){continue}
   $data=@{valid= !($case -eq 'invalid' -and $block -eq 3 -and $i -eq 4);average_fps=60;engine_frame_p95_ms=16.667;game_thread_mean_ms=$(if($case -eq 'regression' -and $label -eq 'B'){10.3}else{10})}
   $data|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $folder "1pair-visible-block$block-$i-$label.json") -Encoding UTF8
  }
 }
 & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/run_perf_abab.ps1" -SummarizeOnly $folder *> (Join-Path $ReportRoot "$case.log")
 $expected=if($case -eq 'complete'){0}else{1}
 if($LASTEXITCODE -ne $expected){throw "Cohort fixture $case expected exit$expected, actual$LASTEXITCODE"}
 $results+=@{case=$case;expected_exit=$expected;actual_exit=$LASTEXITCODE;pass=$true;synthetic_fixture=$true}
}
$results|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $ReportRoot 'fixture-results.json') -Encoding UTF8
$global:LASTEXITCODE=0
Write-Output '5 synthetic cohort gate fixtures passed; no measured performance claimed.'
