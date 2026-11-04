param([string]$Baseline='7aefff54',[Parameter(Mandatory=$true)][string]$ReportRoot)
$ErrorActionPreference='Stop';$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'prepare_naming_perf_archive.ps1')
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
if(Test-Path $ReportRoot){throw 'Use a fresh report directory'}
New-Item -ItemType Directory -Path $ReportRoot|Out-Null
$target=Join-Path $ReportRoot 'owned'
git -C $root archive --format=zip "--output=$ReportRoot/host.zip" $Baseline Source
if($LASTEXITCODE){throw 'Host archive failed'}
Expand-Archive "$ReportRoot/host.zip" $target
$entry=(git -C $root ls-tree $Baseline -- Plugins/DTDevice)-join ''
if($entry -notmatch '160000 commit ([0-9a-f]{40})'){throw 'Baseline plugin absent'}
$pin=$Matches[1]
git -C "$root/Plugins/DTDevice" archive --format=zip "--output=$ReportRoot/device.zip" $pin Source DTDevice.uplugin
if($LASTEXITCODE){throw 'Device archive failed'}
Expand-Archive "$ReportRoot/device.zip" "$target/Plugins/DTDevice"
$instrument='Source/ma0t10_dtEditor/Private/Perf01AcquisitionTests.cpp'
Copy-Item "$root/$instrument" "$target/$instrument" -Force
$before=@{}
Get-ChildItem "$target/Source","$target/Plugins/DTDevice/Source" -Recurse -File|ForEach-Object {$before[$_.FullName]=(Get-FileHash $_.FullName).Hash}
$prepared=Prepare-NamingPerfArchive -Target $target -InstrumentPath "$target/$instrument"
if(-not $prepared.LegacyNaming -or @($prepared.MemberAliases).Count -lt 6){throw 'Legacy naming aliases not installed'}
foreach($file in $before.Keys){if((Get-FileHash $file).Hash -ne $before[$file]){throw "Existing source changed: $file"}}
$forward="$target/Plugins/DTDevice/Source/DeviceRuntime/Public/Device/Sensor/VirtualLidarScanComp.h"
if(-not (Test-Path $forward) -or -not((Get-Content $forward -Raw) -match 'using UVirtualLidarScanComp = UVirtualLidarScanComponent;')){throw 'Class forward alias absent'}
if(-not(Test-Path "$target/Source/ma0t10_dt/MA0T10/UI/Common/VirtualSensorToolWorkspaceSubsystem.h")){throw 'Moved host include alias absent'}
@{Pass=$true;Baseline=$Baseline;Plugin=$pin;ProductionSourcesUnchanged=$true;InstrumentBytesIdentical=$true;NoUnrealExecution=$true;Preparation=$prepared}|ConvertTo-Json -Depth 8|Set-Content "$ReportRoot/manifest.json" -Encoding UTF8
Write-Output 'Naming perf preparation PASS; original source bytes unchanged'
