param([string[]]$Labels=@('baseline-pcd','baseline-three','final-after-pcd','final-after-three','repeat-pcd-no-physics','repeat-three-no-physics'),
 [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$ReportGroup='SlabRepeatRunStability')
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
  $missing=0;$notReceipted=0;$rows=[Collections.Generic.List[object]]::new()
  foreach($file in Get-ChildItem $trial.FullName -Filter '*.json'|Where-Object {$_.Name -match '^[a-f0-9-]{36}\.json$'}){
   $r=Get-Content $file.FullName -Raw|ConvertFrom-Json
   foreach($a in $r.accepted_requests){$key=$r.run_uuid+'|'+$a.request_id;[void]$accepted.Add($key);if(-not $externalIds.Contains($key)){$missing++};if(([int]$a.flags-band 7)-ne 7){$notReceipted++}}
   $row=[pscustomobject]@{label=$label;trial=$trial.Name;run=$r.run_uuid;scenario=$r.scenario_uuid;width=$r.viewport_width;height=$r.viewport_height;fps=$r.average_engine_fps;onePercentLow=$r.one_percent_low;p95=$r.engine_frame_p95_ms;worker=$r.retention.worker_serial;connection=$r.retention.connection_revision;heldBytes=$r.retention.outstanding_bytes;sequenceEntries=$r.retention.sequence_entries;completedDiagnostics=$r.retention.completed_runs;knownIds=$r.retention.minimal_known_ids}
   $rows.Add($row);$all.Add($row)
  }
  $extra=0;if($accepted.Count){foreach($id in $externalIds){if(-not $accepted.Contains($id)){$extra++}}}
  $trials.Add([pscustomobject]@{label=$label;trial=$trial.Name;runs=$rows.Count;externalSuccess=$external.success;acceptedIdentities=$accepted.Count;missingExternal=$missing;extraExternal=$extra;missingSubmitReceipt=$notReceipted;identityVerified=$accepted.Count-gt 0-and $missing-eq 0-and $extra-eq 0-and $notReceipted-eq 0;actual1920=@($rows|Where-Object {$_.width-ne 1920-or $_.height-ne 1080}).Count-eq 0;performancePass=@($rows|Where-Object {$_.fps-lt 55-or $_.onePercentLow-lt 45-or $_.p95-gt 20}).Count-eq 0;retentionPass=@($rows|Where-Object {($null-ne $_.heldBytes-and $_.heldBytes-ne 0)-or ($null-ne $_.sequenceEntries-and $_.sequenceEntries-ne 0)-or ($null-ne $_.completedDiagnostics-and $_.completedDiagnostics-gt 20)}).Count-eq 0})
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
