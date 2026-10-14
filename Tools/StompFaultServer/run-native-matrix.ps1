param([string]$ReportDirectory='', [switch]$IncludeTwoLidars)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(!$ReportDirectory){$ReportDirectory=Join-Path $root ('Saved/Reports/PCD-20HZ/native-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))}
foreach($broker in @('Fake','Artemis')){
    foreach($interval in @(.05,.1,.2)){
        $label=$broker+'-1-'+$interval.ToString([Globalization.CultureInfo]::InvariantCulture)
        & "$PSScriptRoot/run-native.ps1" -Broker $broker -Interval $interval -Lidars 1 -Seconds 300 -ReportDirectory (Join-Path $ReportDirectory $label)
    }
    if($IncludeTwoLidars){& "$PSScriptRoot/run-native.ps1" -Broker $broker -Interval .05 -Lidars 2 -Seconds 300 -ReportDirectory (Join-Path $ReportDirectory ($broker+'-2-0.05'))}
}
