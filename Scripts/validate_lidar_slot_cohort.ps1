param([string]$RunFolder='', [string]$ReportRoot='', [switch]$SelfTest)
$ErrorActionPreference='Stop'
# Additional, stricter wall-clock safeguard. It never depends on engine FPS.
# The two acquisition-boundary frames are the same allowance as legacy.
function Get-LidarRequestSlotVerdict([double]$RequestedHz,[double]$Seconds,[double]$Completed) {
    $valid=$RequestedHz -gt 0 -and $Seconds -gt 0 -and $Completed -ge 0
    foreach($value in @($RequestedHz,$Seconds,$Completed)) {
        $valid=$valid -and ![double]::IsNaN($value) -and ![double]::IsInfinity($value)
    }
    $valid=$valid -and $Completed -eq [Math]::Floor($Completed)
    $minimum=if($valid){[Math]::Max(0,[Math]::Ceiling($RequestedHz*$Seconds-1.e-9)-2)}else{0}
    [pscustomobject]@{Pass=($valid -and $Completed -ge $minimum);Valid=$valid;Minimum=$minimum;Completed=$Completed;Headroom=($Completed-$minimum)}
}
if($SelfTest) {
    foreach($case in @(
        @{Hz=20;Seconds=90;Count=1798;Expected=$true},
        @{Hz=20;Seconds=90.01;Count=1799;Expected=$true},
        @{Hz=20;Seconds=90.01;Count=1798;Expected=$false},
        @{Hz=20;Seconds=90;Count=1782;Expected=$false},
        @{Hz=20;Seconds=90;Count=0;Expected=$false},
        @{Hz=0;Seconds=90;Count=1800;Expected=$false},
        @{Hz=20;Seconds=0;Count=1800;Expected=$false},
        @{Hz=[double]::NaN;Seconds=90;Count=1800;Expected=$false}
    )) {
        $v=Get-LidarRequestSlotVerdict $case.Hz $case.Seconds $case.Count
        if($v.Pass -ne $case.Expected){throw 'Wall-clock slot fixture failed'}
    }
    Write-Output '8 pure wall-clock slot safeguards passed; no build or measurement.'
    if(!$RunFolder){exit 0}
}
if(!$RunFolder){throw 'RunFolder is required unless -SelfTest'}
if(!$ReportRoot){$ReportRoot=Join-Path $RunFolder 'slot-guard'}
New-Item -ItemType Directory -Force -Path $ReportRoot|Out-Null
$rows=@();$failures=@()
foreach($file in Get-ChildItem -LiteralPath $RunFolder -Filter '*-B.json' -File) {
    $data=Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8|ConvertFrom-Json
    if(!$data.valid -or $data.measurement_seconds -ne 90 -or @($data.sensors).Count -ne 3){$failures+="Invalid90s candidate: $($file.Name)";continue}
    $legacyMinimum=[Math]::Floor([Math]::Round($data.engine_counter_fps*$data.seconds)/3)-2
    foreach($sensor in $data.sensors) {
        $v=Get-LidarRequestSlotVerdict $sensor.requested_hz $data.seconds $sensor.frame_count
        $rows+=[pscustomobject]@{Run=$file.BaseName;Sensor=$sensor.sensor;RequestedHz=$sensor.requested_hz;Seconds=$data.seconds;Completed=$sensor.frame_count;Minimum=$v.Minimum;Headroom=$v.Headroom;Pass=$v.Pass;LegacyMinimum=$legacyMinimum;AtLeastLegacy=($v.Minimum -ge $legacyMinimum)}
        if(!$v.Pass){$failures+="Requested-slot shortfall: $($file.Name) / $($sensor.sensor)"}
        if($v.Minimum -lt $legacyMinimum){$failures+="Non-relaxation proof failed: $($file.Name)"}
    }
}
if($rows.Count -ne 18){$failures+="Incomplete candidate cohort: $($rows.Count)/18 sensor runs"}
$result=[pscustomobject]@{Pass=($failures.Count -eq 0);Rule='completed >= ceil(requested Hz * wall seconds)-2; additionally99percent/19.9Hz/.995 and ABAB2percent remain';RunFolder=$RunFolder;Rows=$rows;Failures=$failures;Legacy='unchanged reference assertions; proof under recorded capped60FPS conditions'}
$result|ConvertTo-Json -Depth 8|Set-Content (Join-Path $ReportRoot 'manifest.json') -Encoding UTF8
Write-Output "Requested-slot safeguard PASS=$($result.Pass); $($rows.Count) sensor runs"
if(!$result.Pass){exit 1}
