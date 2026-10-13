param([string]$ReportRoot='')
$ErrorActionPreference='Stop'
if(-not $ReportRoot){$ReportRoot=Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved/Reports/SensorResourceLifecycle'}
$runs=[Collections.Generic.List[object]]::new();$failures=[Collections.Generic.List[string]]::new()
foreach($label in @('repeat-pcd','repeat-three')){
 $dir=Join-Path $ReportRoot "$label/1"
 $external=Get-Content (Join-Path $dir 'external.json') -Raw|ConvertFrom-Json
 $externalIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($e in $external.metadataLedger){if($e.valid -and $e.runId){[void]$externalIds.Add($e.runId+'|'+$e.requestId)}}
 $accepted=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 $files=@(Get-ChildItem $dir -Filter '*.json'|Where-Object Name -match '^[a-f0-9-]{36}\.json$')
 if($files.Count-ne 30){$failures.Add("$label has $($files.Count), not 30 run reports")}
 foreach($file in $files){
  $r=Get-Content $file.FullName -Raw|ConvertFrom-Json;$missing=0;$unconfirmed=0
  foreach($a in $r.accepted_requests){$id=$r.run_uuid+'|'+$a.request_id;[void]$accepted.Add($id);if(-not $externalIds.Contains($id)){$missing++};if(([int]$a.flags-band 7)-ne 7){$unconfirmed++}}
  if($missing-or $unconfirmed){$failures.Add("$label $($r.run_uuid) identity mismatch missing=$missing unconfirmed=$unconfirmed")}
  $rate=@{};foreach($a in $r.accepted_requests){$kind=($a.key -split '\|')[0];if(-not $rate.ContainsKey($kind)){$rate[$kind]=0};$rate[$kind]++}
  $perf=$r.average_engine_fps-ge 55-and $r.one_percent_low-ge 45-and $r.engine_frame_p95_ms-le 20
  $rows=[pscustomobject]@{label=$label;run=$r.run_uuid;scenario=$r.scenario_uuid;width=$r.viewport_width;height=$r.viewport_height;fps=$r.average_engine_fps;low=$r.one_percent_low;p95=$r.engine_frame_p95_ms;
    countsByKind=$rate;accepted=$r.accepted_requests.Count;missingExternal=$missing;unconfirmed=$unconfirmed;performancePass=$perf;worker=$r.retention.worker_serial;connection=$r.retention.connection_revision;held=$r.retention.outstanding_bytes}
  $runs.Add($rows)
  if(-not $perf){$failures.Add("$label $($r.run_uuid) performance gate failed")}
  if($r.viewport_width-ne 1920-or $r.viewport_height-ne 1080){$failures.Add("$label actual viewport mismatch")}
 }
 $extra=0;foreach($id in $externalIds){if(-not $accepted.Contains($id)){$extra++}}
 if($extra-or -not $external.success){$failures.Add("$label external mismatch extra=$extra success=$($external.success)")}
 if(@($runs|Where-Object label -eq $label|Select-Object -ExpandProperty worker -Unique).Count-ne 1-or @($runs|Where-Object label -eq $label|Select-Object -ExpandProperty connection -Unique).Count-ne 1){$failures.Add("$label recreated worker or connection")}
}
[pscustomobject]@{generatedUtc=[DateTimeOffset]::UtcNow.ToString('o');passed=$failures.Count-eq 0;failures=$failures;runs=$runs}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $ReportRoot 'repeat-validation.json') -Encoding utf8
$runs|Group-Object label|ForEach-Object{[pscustomobject]@{label=$_.Name;runs=$_.Count;accepted=($_.Group|Measure-Object accepted -Sum).Sum;minimumFps=($_.Group|Measure-Object fps -Minimum).Minimum;minimumLow=($_.Group|Measure-Object low -Minimum).Minimum;maximumP95=($_.Group|Measure-Object p95 -Maximum).Maximum}}|Format-Table
if($failures.Count){throw ($failures-join "`n")}
