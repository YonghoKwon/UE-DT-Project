param([string]$ReportRoot='',[string[]]$Labels=@('repeat-pcd','repeat-three'))
$ErrorActionPreference='Stop'
if(-not $ReportRoot){$ReportRoot=Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved/Reports/SensorResourceLifecycle'}
$rows=[Collections.Generic.List[object]]::new()
$violations=[Collections.Generic.List[string]]::new()
foreach($label in $Labels){
 $dir=Join-Path $ReportRoot $label
 $files=@(Get-ChildItem (Join-Path $dir 'resources') -Filter '*.json' -ErrorAction SilentlyContinue)
 if(-not $files.Count){$violations.Add("$label has no resource evidence");continue}
 foreach($file in $files){
  $s=Get-Content -LiteralPath $file.FullName -Raw|ConvertFrom-Json
  foreach($owner in $s.owners){
   $rows.Add([pscustomobject]@{label=$label;run=$s.run_uuid;repetition=$s.repetition;phase=$s.phase;utc=$s.utc;owner=$owner.owner;
    counts=$owner.counts;ownedCpuBytes=$owner.owned_cpu_bytes;sharedCpuBytes=$owner.shared_cpu_bytes;estimatedGpuBytes=$owner.estimated_gpu_bytes;
    fixtureAuditEntries=$s.fixture_audit_entries;fixtureAuditCpuBytes=$s.fixture_audit_cpu_bytes;raw=$s.raw_transport})
  }
 }
}
foreach($r in $rows){
 if($r.phase -eq 'drained'){
  if($r.raw.outstanding_bytes -ne 0 -or $r.raw.active_runs -ne 0 -or $r.raw.retiring_runs -ne 0){$violations.Add("$($r.label)/$($r.repetition): raw run not returned")}
  if($r.raw.completed_runs -gt 20){$violations.Add("$($r.label)/$($r.repetition): closed run diagnostics exceeded 20")}
  if($r.owner -eq 'slab-results' -and ($r.counts.results -gt 20 -or $r.counts.writers -gt 2 -or $r.counts.requests -gt 65536)){$violations.Add("$($r.label)/$($r.repetition): result/ledger/writer bound exceeded")}
 }
 if($r.phase -eq 'sensors-stopped' -and $r.owner -like '*/camera'){
  foreach($key in @('pending_readbacks','pending_encodes','active_encodes','global_encode_jobs','completed_reorder','encode_order','acquisition_snapshots','slab_contexts','readbacks_in_flight')){
   if($r.counts.$key -ne 0){$violations.Add("$($r.label): stopped camera retained $key=$($r.counts.$key)")}
  }
 }
}
[pscustomobject]@{generatedUtc=[DateTimeOffset]::UtcNow.ToString('o');resourceBoundsPassed=$violations.Count -eq 0;violations=$violations;
 byteSemantics='Shared observations are not additive across owners; GPU sizes are unpadded estimates. Process private bytes alone do not establish a leak.';
 rows=@($rows|Sort-Object label,repetition,phase,owner)}|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $ReportRoot 'resource-summary.json') -Encoding utf8
$rows|Where-Object {$_.owner -eq 'slab-results' -and $_.repetition -in @(1,10,20,21,25,30)}|Select-Object label,repetition,phase,@{n='results';e={$_.counts.results}},@{n='ledger';e={$_.counts.requests}},@{n='audit';e={$_.fixtureAuditEntries}}|Format-Table
if($violations.Count){throw ($violations -join "`n")}
