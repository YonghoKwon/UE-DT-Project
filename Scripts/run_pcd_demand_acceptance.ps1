param([ValidateSet('Compare','Repeat','Endurance')][string]$Phase='Compare')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$group='PcdDemandPlacementAcceptance'
$report=Join-Path $root ('Saved/Reports/'+$group)
$backup=Join-Path $report 'personal-save-backup'
New-Item -ItemType Directory -Force $report|Out-Null
function Restore-PersonalUi {
 foreach($name in @('MA0T10_SensorToolAppearance_v1.sav','MA0T10_SensorToolWorkspace_v1.sav','MA0T10_VirtualSensorUI_v6.sav')){
  $file=Join-Path $backup $name
  if(Test-Path -LiteralPath $file){Copy-Item -LiteralPath $file -Destination (Join-Path $root ('Saved/SaveGames/'+$name)) -Force}
 }
}
try {
 if($Phase-eq 'Repeat'){
  Restore-PersonalUi
  & (Join-Path $PSScriptRoot 'run_slab_repeat_stability.ps1') -Label 'requested-repeat-final' -Repeated -PcdDemandFixture -SensorPairs 2 -JsonPolicy requested -RunCount 10 -ReportGroup $group -ResourceDiagnostics -Width 1920 -Height 1080
 }elseif($Phase-eq 'Endurance'){
  Restore-PersonalUi
  & (Join-Path $PSScriptRoot 'run_sensor_map_stream_rhi_smoke.ps1') -SkipBuild -PointCloudOnly -PcdDemandFixture -JsonPolicy requested -SensorPairs 2 -ClientWidth 1920 -ClientHeight 1080 -MeasurementSeconds 600 -ReportLabel pcd_demand_endurance
 }else{
  $cases=@();foreach($trial in 1..3){$order=if($trial-eq 2){@('requested','legacy-full')}else{@('legacy-full','requested')};foreach($policy in $order){$cases+=@{Policy=$policy;Pairs=2;Trial=$trial}}}
  foreach($policy in @('legacy-full','requested')){$cases+=@{Policy=$policy;Pairs=1;Trial=1}}
  $rows=@()
  foreach($case in $cases){
   Restore-PersonalUi
   $label='pcd_demand_compare_'+$case.Policy.Replace('-','_')+'_'+$case.Pairs+'pair_'+$case.Trial
   try{& (Join-Path $PSScriptRoot 'run_sensor_map_stream_rhi_smoke.ps1') -SkipBuild -PointCloudOnly -PcdDemandFixture -JsonPolicy $case.Policy -SensorPairs $case.Pairs -ClientWidth 1920 -ClientHeight 1080 -MeasurementSeconds 60 -ReportLabel $label}
   catch{Write-Warning "$label failed: $($_.Exception.Message)"}
   $path=Join-Path $root ('Saved/Reports/'+$label+'.json')
   if(!(Test-Path -LiteralPath $path)){throw "Missing acceptance evidence: $label"}
   $r=Get-Content -Raw -LiteralPath $path|ConvertFrom-Json
   $rows+=[pscustomobject]@{label=$label;policy=$case.Policy;pairs=$case.Pairs;trial=$case.Trial;valid=$r.Valid;fps=$r.averageFps;low=$r.onePercentLowFps;p95=$r.p95FrameMs;perSensor=$r.perSensorWindow;acquisition=$r.acquisitionWindow;window=$r.fullWindow}
   $rows|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $report 'policy-comparison.json') -Encoding utf8
  }
  $selected=@($rows|Where-Object policy -eq 'requested')
  if($selected.Count-ne 4-or@($selected|Where-Object {!$_.valid}).Count){throw 'Requested-policy acceptance failed; legacy controls and all failures retained'}
 }
}finally{Restore-PersonalUi}
