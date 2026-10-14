param([string]$ReportDirectory='', [switch]$IncludeTwoLidars, [switch]$SkipCompleted)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(!$ReportDirectory){$ReportDirectory=Join-Path $root ('Saved/Reports/PCD-20HZ/native-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))}
function Invoke-NativeCase([string]$Broker,[double]$Interval,[int]$Lidars,[string]$Directory){
    $index=Join-Path $Directory 'automation/index.json';$manifest=Join-Path $Directory 'manifest.json'
    if($SkipCompleted -and (Test-Path $index) -and (Test-Path $manifest)){
        $result=Get-Content $index -Raw -Encoding UTF8 | ConvertFrom-Json
        $previous=Get-Content $manifest -Raw -Encoding UTF8 | ConvertFrom-Json
        $same=($result.tests[0].state -eq 'Success' -and $previous.Seconds -eq 300 -and $previous.Interval -eq $Interval -and $previous.Lidars -eq $Lidars -and
            $previous.Commit -eq (& git -C $root rev-parse HEAD) -and $previous.DTCore -eq (& git -C "$root/Plugins/DTCore" rev-parse HEAD) -and
            $previous.DllHash -eq (Get-FileHash "$root/Binaries/Win64/UnrealEditor-ma0t10_dt.dll").Hash)
        if($same){
            # Compare the complete current runtime Source set both ways, so added or deleted
            # files also invalidate the result. Assertion-only harness changes (Tests) do not.
            $recorded=@{};foreach($hash in $previous.SourceHashes.PSObject.Properties){if($hash.Name -notlike '*\Tests\*'){$recorded[$hash.Name]=$hash.Value}}
            $current=@{};Get-ChildItem "$root/Source" -Recurse -File | ForEach-Object {$name=$_.FullName.Substring($root.Length+1);if($name -notlike '*\Tests\*'){$current[$name]=(Get-FileHash -LiteralPath $_.FullName).Hash}}
            if($recorded.Count -ne $current.Count){$same=$false}
            else{foreach($name in $current.Keys){if($recorded[$name] -ne $current[$name]){$same=$false;break}}}
        }
        if($same){Write-Output "Retaining accepted case $Broker/$Interval/$Lidars with identical runtime source hashes";return}
    }
    if(Test-Path $index){
        $source=(Resolve-Path -LiteralPath $Directory).Path
        $destination=[IO.Path]::GetFullPath($Directory+'-previous-'+[DateTime]::UtcNow.ToString('HHmmss'))
        $boundary=[IO.Path]::GetFullPath($ReportDirectory).TrimEnd('\')+'\'
        if(!$source.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase) -or !$destination.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase)){throw 'Report move escaped its explicitly selected directory'}
        Move-Item -LiteralPath $source -Destination $destination
    }
    & "$PSScriptRoot/run-native.ps1" -Broker $Broker -Interval $Interval -Lidars $Lidars -Seconds 300 -ReportDirectory $Directory
}
foreach($broker in @('Fake','Artemis')){
    foreach($interval in @(.05,.1,.2)){
        $label=$broker+'-1-'+$interval.ToString([Globalization.CultureInfo]::InvariantCulture)
        Invoke-NativeCase $broker $interval 1 (Join-Path $ReportDirectory $label)
    }
    if($IncludeTwoLidars){Invoke-NativeCase $broker .05 2 (Join-Path $ReportDirectory ($broker+'-2-0.05'))}
}
