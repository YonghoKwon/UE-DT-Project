param([Parameter(Mandatory=$true)][string]$ReportRoot)
# Same judgement as QA-02; this cohort isolates the sampler in a3-LiDAR PCD fixture.
$ErrorActionPreference='Stop'
. "$PSScriptRoot/PerfRelativeGate.ps1"
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
$a=@();$b=@();$signature=$null;$runs=@();$invalid=@()
for($block=1;$block -le 3;$block++){
    for($position=1;$position -le 4;$position++){
        $on=($position%2 -eq 0);$label=if($on){'on'}else{'off'}
        $folder=Join-Path $ReportRoot "block$block-$position-$label"
        $data=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $folder 'memory-cost.json')|ConvertFrom-Json
        $manifest=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $folder 'source-manifest.json')|ConvertFrom-Json
        $serial=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $folder 'serial-manifest.json')|ConvertFrom-Json
        $automation=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $folder 'automation/index.json')|ConvertFrom-Json
        $hashes=($manifest.SourceHashes.PSObject.Properties|Sort-Object Name|ForEach-Object{$_.Name+'='+$_.Value}) -join "`n"
        $key=@($manifest.Commit,$manifest.Submodule,$manifest.Width,$manifest.Height,$manifest.FontPercent,$manifest.MemoryWarmupSeconds,$manifest.MemoryAudit,$manifest.FullRhiDrain,$manifest.CpuReference,$manifest.Visible,$hashes) -join '|'
        if($null -eq $signature){$signature=$key}elseif($signature -cne $key){$invalid+='condition/source mismatch: '+$folder}
        $success=(@($automation.tests).Count -eq 1 -and $automation.tests[0].state -eq 'Success')
        $valid=$serial.Valid -and !$serial.VideoDecodeDetected -and $success -and $data.complete -and $data.cost_probe -and ($data.sampler_enabled -eq $on) -and $data.llm_enabled -and $data.measurement_seconds -ge 60 -and $data.measurement_seconds -le 62
        if($on){$valid=$valid -and $data.sample_count -ge 10}else{$valid=$valid -and $data.sample_count -eq 0}
        $sensors=@($data.pcd.PSObject.Properties)
        if($sensors.Count -ne 3){$valid=$false}
        foreach($backends in @($data.backend_before,$data.backend_after)){if(@($backends.PSObject.Properties).Count -ne 3 -or @($backends.PSObject.Properties|Where-Object{$_.Value -ne 3}).Count){$valid=$false}}
        $ratios=@($sensors|ForEach-Object{$_.Value.input_frames/[double]$data.measurement_seconds/20})
        # Include every sensor, including0Hz. Lower acquisition must not make B cheaper.
        if($on -and @($ratios|Where-Object{[double]::IsNaN($_) -or $_ -lt .99}).Count){$valid=$false}
        $row=[pscustomobject]@{Valid=[bool]$valid;File=$folder;Fps=[double]$data.average_fps;P95=[double]$data.engine_frame_p95_ms;GameThread=[double]$data.game_thread_mean_ms;AcquisitionRatios=$ratios}
        if($on){$b+= $row}else{$a+= $row};$runs+= $row
    }
}
$comparison=Get-PerfRelativeComparison -A $a -B $b -Expected 6 -Threshold 2.0
if($invalid.Count){$comparison.Pass=$false;$comparison.Failures+= $invalid}
$result=[ordered]@{Valid=[bool]$comparison.Pass;Pass=[bool]$comparison.Pass;Commit=$manifest.Commit;ThresholdPercent=2.0;Scope='Sampler off/on ABABx3,6 samples each;3HWRT LiDAR+PCD,LLM enabled in both; not project-wide PERF01 acceptance';Comparison=$comparison;Runs=$runs}
$result|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $ReportRoot 'comparison.json')
$comparison|Format-List
if(!$comparison.Pass){exit 1}
