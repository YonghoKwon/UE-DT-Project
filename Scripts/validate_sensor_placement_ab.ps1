param([Parameter(Mandatory=$true)][string]$ReportDirectory)
$ErrorActionPreference='Stop'
$directory=[IO.Path]::GetFullPath($ReportDirectory)
$external=Get-Content -LiteralPath (Join-Path $directory 'external.json') -Raw|ConvertFrom-Json
if(-not $external.success){throw 'External PCD probe failed'}
$runs=@(Get-ChildItem -LiteralPath $directory -Filter '*.json'|ForEach-Object { $j=Get-Content -LiteralPath $_.FullName -Raw|ConvertFrom-Json; if($j -is [pscustomobject] -and $j.PSObject.Properties.Name -contains 'run_delivery_summary'){$j} })
if($runs.Count -ne 2){throw "Expected two scenario reports, got $($runs.Count)"}
$rows=@();$scenario=$runs[0].scenario_uuid
foreach($run in $runs){
 if($run.scenario_uuid -ne $scenario){throw 'A/B scenario identity changed'}
 $summary=$run.run_delivery_summary
 $sensor=@($summary.sensors|Where-Object sensorId -eq 'SLAB-LIDAR-001')[0]
 if(-not $sensor -or -not $sensor.placement.bookmarkId -or $sensor.placement.bDirectPlacement -or $sensor.placement.bChangedDuringRun){throw 'Missing or changed named start placement'}
 $p=$sensor.placement.worldTransform.translation;$q=$sensor.placement.worldTransform.rotation;$s=$sensor.placement.worldTransform.scale3D
 $expected=[double[]]::new(16)
 $expected[0]=(1-2*($q.y*$q.y+$q.z*$q.z))*$s.x
 $expected[1]=(-2*($q.x*$q.y-$q.z*$q.w))*$s.y
 $expected[2]=(2*($q.x*$q.z+$q.y*$q.w))*$s.z
 $expected[3]=$p.x/100
 $expected[4]=(2*($q.x*$q.y+$q.z*$q.w))*$s.x
 $expected[5]=(-(1-2*($q.x*$q.x+$q.z*$q.z)))*$s.y
 $expected[6]=(2*($q.y*$q.z-$q.x*$q.w))*$s.z
 $expected[7]=$p.y/100
 $expected[8]=(2*($q.x*$q.z-$q.y*$q.w))*$s.x
 $expected[9]=(-2*($q.y*$q.z+$q.x*$q.w))*$s.y
 $expected[10]=(1-2*($q.x*$q.x+$q.y*$q.y))*$s.z
 $expected[11]=$p.z/100
 $expected[15]=1
 $messages=@($external.metadataLedger|Where-Object { $_.runId -eq $run.run_uuid -and $_.sensorId -eq $sensor.sensorId -and $_.placement })
 if($messages.Count -lt 570){throw 'PCD below 19Hz for thirty seconds'}
 foreach($message in $messages){
  if(-not $message.valid){throw 'Invalid external PCD'}
  for($i=0;$i -lt 16;$i++){if([Math]::Abs($message.placement.sensorToWorldM[$i]-$expected[$i]) -gt .00001){throw "PCD acquisition pose differs: run=$($run.run_uuid) frame=$($message.frameId) field=$i"}}
 }
 if($summary.accepted -ne $summary.submitted -or $summary.accepted -ne $summary.receipts -or $summary.deliveryFailures -or $summary.unfinished){throw 'Run delivery ledger mismatch'}
 $accepted=@($run.accepted_requests|ForEach-Object request_id);$received=@{};foreach($m in $messages){$received[$m.requestId]=$true}
 if($accepted.Count -ne $received.Count -or @($accepted|Where-Object {-not $received.ContainsKey($_)}).Count){throw 'Accepted and external request IDs differ'}
 $rows+=[pscustomobject]@{run=$run.run_uuid;scenario=$scenario;bookmark=$sensor.placement.bookmarkId;name=$sensor.placement.bookmarkName;positionCm=$p;pcd=$messages.Count;fps=$run.average_engine_fps;low=$run.one_percent_low;p95=$run.engine_frame_p95_ms}
}
if(@($rows.run|Select-Object -Unique).Count -ne 2 -or @($rows.bookmark|Select-Object -Unique).Count -ne 2){throw 'A/B run/bookmark identities must differ'}
if($rows[0].positionCm.x -eq $rows[1].positionCm.x){throw 'A/B poses are not different'}
$result=[pscustomobject]@{passed=$true;rows=$rows;note='Every external Binary PCD acquisition transform matched the independent start-pose quaternion/translation/scale calculation.'}
$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $directory 'placement-ab-validation.json') -Encoding utf8
$result|ConvertTo-Json -Depth 5
exit 0
