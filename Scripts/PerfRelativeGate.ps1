# Pure judgement shared by QA-02 and the scheduled LiDAR cohort. No process or file I/O.
function Get-PerfRelativeComparison {
    param([object[]]$A, [object[]]$B, [int]$Expected, [double]$Threshold = 2.0,
          [switch]$RequireAcquisitionRatio, [switch]$RequireGpu)
    function Median([double[]]$Values) {
        $sorted = @($Values | Sort-Object)
        if (!$sorted.Count) { return [double]::NaN }
        $mid = [int][Math]::Floor($sorted.Count / 2)
        if ($sorted.Count % 2) { return $sorted[$mid] }
        return ($sorted[$mid - 1] + $sorted[$mid]) / 2
    }
    $failures = @()
    if ($Expected -lt 2 -or $A.Count -ne $Expected -or $B.Count -ne $Expected) {
        $failures += "incomplete samples A=$($A.Count) B=$($B.Count), expected=$Expected each"
    }
    foreach ($row in @($A) + @($B)) {
        if (!$row.Valid) { $failures += "invalid run: $($row.File)" }
        foreach ($key in 'Fps', 'P95', 'GameThread') {
            $value = [double]$row.$key
            if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0) { $failures += "invalid $key metric: $($row.File)" }
        }
    }
    $fpsA = Median @($A | ForEach-Object Fps); $fpsB = Median @($B | ForEach-Object Fps)
    $p95A = Median @($A | ForEach-Object P95); $p95B = Median @($B | ForEach-Object P95)
    $gtA = Median @($A | ForEach-Object GameThread); $gtB = Median @($B | ForEach-Object GameThread)
    $fpsDrop = if ($fpsA -gt 0) { ($fpsA - $fpsB) / $fpsA * 100 } else { [double]::NaN }
    $p95Rise = if ($p95A -gt 0) { ($p95B - $p95A) / $p95A * 100 } else { [double]::NaN }
    $gtRise = if ($gtA -gt 0) { ($gtB - $gtA) / $gtA * 100 } else { [double]::NaN }
    if ([double]::IsNaN($Threshold) -or [double]::IsInfinity($Threshold) -or $Threshold -lt 0) { $failures += 'invalid relative threshold' }
    if ($fpsDrop -gt $Threshold) { $failures += ('FPS -{0:N3}%' -f $fpsDrop) }
    if ($p95Rise -gt $Threshold) { $failures += ('engine p95 +{0:N3}%' -f $p95Rise) }
    if ($gtRise -gt $Threshold) { $failures += ('game thread +{0:N3}%' -f $gtRise) }
    if ($RequireAcquisitionRatio) {
        # Every candidate run and sensor, including 0Hz. No cohort averaging can hide starvation.
        foreach ($row in $B) {
            $ratios = @($row.AcquisitionRatios)
            if (!$ratios.Count) { $failures += "missing acquisition ratios: $($row.File)" }
            foreach ($ratio in $ratios) {
                if ($null -eq $ratio -or [double]::IsNaN([double]$ratio) -or [double]::IsInfinity([double]$ratio) -or [double]$ratio -lt .99) {
                    $failures += "acquisition/requested Hz ratio below0.99 or invalid: $($row.File)"
                }
            }
        }
    }
    $gpuA=$null; $gpuB=$null; $gpuRise=$null; $rssA=$null; $rssB=$null; $rssRise=$null
    if ($RequireGpu -or $RequireAcquisitionRatio) {
        foreach ($row in @($A)+@($B)) {
            $value=[double]$row.Gpu
            if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0) { $failures += "invalid Gpu metric: $($row.File)" }
        }
        $gpuA=Median @($A | ForEach-Object Gpu); $gpuB=Median @($B | ForEach-Object Gpu)
        if ($gpuA -gt 0) {$gpuRise=($gpuB-$gpuA)/$gpuA*100; if($gpuRise -gt $Threshold){$failures+='GPU mean regression exceeds threshold'}}
    }
    if ($RequireAcquisitionRatio) {
        foreach ($row in @($A)+@($B)) {
            $value=[double]$row.Rss
            if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0) { $failures += "invalid Rss metric: $($row.File)" }
        }
        $rssA=Median @($A | ForEach-Object Rss); $rssB=Median @($B | ForEach-Object Rss)
        if ($rssA -gt 0) {$rssRise=($rssB-$rssA)/$rssA*100; if($rssRise -gt $Threshold){$failures+='RSS regression exceeds threshold'}}
    }
    [PSCustomObject][ordered]@{
        SamplesPerRevision = $A.Count
        FpsA = $fpsA; FpsB = $fpsB; FpsDropPercent = $fpsDrop
        EngineP95A = $p95A; EngineP95B = $p95B; EngineP95RisePercent = $p95Rise
        GameThreadA = $gtA; GameThreadB = $gtB; GameThreadRisePercent = $gtRise
        GpuMeanA=$gpuA; GpuMeanB=$gpuB; GpuRisePercent=$gpuRise
        RssA=$rssA; RssB=$rssB; RssRisePercent=$rssRise
        Pass = ($failures.Count -eq 0); Failures = @($failures)
    }
}
