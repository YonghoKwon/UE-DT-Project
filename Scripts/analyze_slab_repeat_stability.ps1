param([string[]]$Labels=@('baseline-pcd','baseline-three','final-after-pcd','final-after-three','repeat-pcd-no-physics','repeat-three-no-physics'),
 [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$ReportGroup='SlabRepeatRunStability',
 [int]$ExpectedRuns=0,[ValidateRange(1,2)][int]$ExpectedSensorPairs=2,[switch]$RequireSensorMetrics,[switch]$ReportOnly)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root ('Saved/Reports/'+$ReportGroup)
$all=[Collections.Generic.List[object]]::new();$trials=[Collections.Generic.List[object]]::new()
function Median($values){$sorted=@($values|Sort-Object);if(-not $sorted.Count){return $null};if($sorted.Count%2){return $sorted[[int]($sorted.Count/2)]};return ($sorted[$sorted.Count/2-1]+$sorted[$sorted.Count/2])/2}
foreach($label in $Labels){
 $dir=Join-Path $report $label;if(-not (Test-Path $dir)){continue}
 foreach($trial in Get-ChildItem $dir -Directory){
  $externalPath=Join-Path $trial.FullName 'external.json';if(-not(Test-Path $externalPath)){continue}
  $external=Get-Content $externalPath -Raw|ConvertFrom-Json
  $externalIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  foreach($e in $external.metadataLedger){if($e.valid){[void]$externalIds.Add($e.runId+'|'+$e.requestId)}}
  $accepted=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  $missing=0;$notReceipted=0;$sensorPass=$true;$rows=[Collections.Generic.List[object]]::new()
  foreach($file in Get-ChildItem $trial.FullName -Filter '*.json'|Where-Object {$_.Name -match '^[a-f0-9-]{36}\.json$'}){
   $r=Get-Content $file.FullName -Raw|ConvertFrom-Json
   foreach($a in $r.accepted_requests){$key=$r.run_uuid+'|'+$a.request_id;[void]$accepted.Add($key);if(-not $externalIds.Contains($key)){$missing++};if(([int]$a.flags-band 7)-ne 7){$notReceipted++}}
   $sensorRows=@()
   if($RequireSensorMetrics){
    $expected=@($r.expected_sensors);$ids=@($expected.sensor_id|Sort-Object -Unique)
    if($expected.Count-ne 2*$ExpectedSensorPairs-or$ids.Count-ne$expected.Count-or!($r.duration-gt 0)){$sensorPass=$false}
    foreach($s in $expected){
     $pcd=@($r.accepted_requests|Where-Object {($_.key-split '\|')[0]-eq '2'-and($_.key-split '\|')[1]-eq$s.sensor_id}).Count
     $acq=if($r.duration-gt 0){[double]$s.acquisitions/$r.duration}else{0};$pcdHz=if($r.duration-gt 0){$pcd/$r.duration}else{0}
     $valid=$s.running-and(($s.kind-eq 'Camera'-and$acq-ge 29)-or($s.kind-eq 'Lidar'-and$acq-ge 19-and$pcdHz-ge 19))
     if($s.kind-eq 'Lidar'-and$r.compatibility_policy-eq 'requested'){$valid=$valid-and$null-ne$s.compatibility_output.started-and$s.compatibility_output.started-eq 0}
     $sensorPass=$sensorPass-and$valid;$sensorRows+=[pscustomobject]@{sensorId=$s.sensor_id;kind=$s.kind;acquisitionHz=$acq;pcdHz=$pcdHz;pass=$valid}
    }
    if(@($expected|Where-Object kind -eq 'Camera').Count-ne$ExpectedSensorPairs-or@($expected|Where-Object kind -eq 'Lidar').Count-ne$ExpectedSensorPairs){$sensorPass=$false}
   }
   $row=[pscustomobject]@{label=$label;trial=$trial.Name;run=$r.run_uuid;scenario=$r.scenario_uuid;width=$r.viewport_width;height=$r.viewport_height;fps=$r.average_engine_fps;onePercentLow=$r.one_percent_low;p95=$r.engine_frame_p95_ms;worker=$r.retention.worker_serial;connection=$r.retention.connection_revision;heldBytes=$r.retention.outstanding_bytes;sequenceEntries=$r.retention.sequence_entries;completedDiagnostics=$r.retention.completed_runs;knownIds=$r.retention.minimal_known_ids}
   $row|Add-Member -NotePropertyName sensors -NotePropertyValue $sensorRows;$rows.Add($row);$all.Add($row)
  }
  $extra=0;if($accepted.Count){foreach($id in $externalIds){if(-not $accepted.Contains($id)){$extra++}}}
  $complete=$rows.Count-gt 0-and($ExpectedRuns-eq 0-or$rows.Count-eq$ExpectedRuns)
  $fields=@($rows|Where-Object {$null-eq$_.fps-or$null-eq$_.onePercentLow-or$null-eq$_.p95-or$null-eq$_.heldBytes-or$null-eq$_.sequenceEntries}).Count-eq 0
  $trials.Add([pscustomobject]@{label=$label;trial=$trial.Name;runs=$rows.Count;complete=$complete-and$fields;externalSuccess=$external.success;acceptedIdentities=$accepted.Count;missingExternal=$missing;extraExternal=$extra;missingSubmitReceipt=$notReceipted;identityVerified=$accepted.Count-gt 0-and $missing-eq 0-and $extra-eq 0-and $notReceipted-eq 0;perSensorPass=$(if($RequireSensorMetrics){$sensorPass}else{'NotTested'});actual1920=@($rows|Where-Object {$_.width-ne 1920-or $_.height-ne 1080}).Count-eq 0;performancePass=$complete-and$fields-and@($rows|Where-Object {$_.fps-lt 55-or $_.onePercentLow-lt 45-or $_.p95-gt 20}).Count-eq 0;retentionPass=$fields-and@($rows|Where-Object {($_.heldBytes-ne 0)-or($_.sequenceEntries-ne 0)-or($_.completedDiagnostics-gt 20)}).Count-eq 0})
 }
}
$comparison=@()
foreach($kind in @('pcd','three')){
 $before=@($all|Where-Object label -eq "baseline-$kind");$after=@($all|Where-Object label -eq "final-after-$kind")
 if(-not $after.Count){$after=@($all|Where-Object label -eq "after-$kind")}
 if($before.Count-and $after.Count){$bf=Median $before.fps;$af=Median $after.fps;$bp=Median $before.p95;$ap=Median $after.p95;$comparison+=[pscustomobject]@{kind=$kind;beforeFps=$bf;afterFps=$af;beforeP95=$bp;afterP95=$ap;fpsDropPercent=100*($bf-$af)/$bf;p95IncreasePercent=100*($ap-$bp)/$bp;withinFivePercent=$af-ge $bf*.95-and $ap-le $bp*1.05}}
}
[pscustomobject]@{generatedUtc=[DateTimeOffset]::UtcNow.ToString('o');runs=$all;trials=$trials;comparison=$comparison}|ConvertTo-Json -Depth 9|Set-Content (Join-Path $report 'validation-summary.json') -Encoding utf8
$trials|Format-Table label,trial,runs,externalSuccess,identityVerified,performancePass,retentionPass
$comparison|Format-Table kind,beforeFps,afterFps,beforeP95,afterP95,withinFivePercent
if(!$ReportOnly-and($trials.Count-eq 0-or@($trials|Where-Object {!$_.complete-or!$_.externalSuccess-or!$_.identityVerified-or!$_.performancePass-or!$_.retentionPass-or!$_.actual1920-or($RequireSensorMetrics-and!$_.perSensorPass)}).Count-gt 0)){throw 'Slab acceptance failed; inspect validation-summary.json for stage results'}
