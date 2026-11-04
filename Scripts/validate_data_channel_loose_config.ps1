param(
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [Parameter(Mandatory=$true)][string]$ReportRoot
)
$ErrorActionPreference = 'Stop'
$package = [IO.Path]::GetFullPath($PackageRoot)
$report = [IO.Path]::GetFullPath($ReportRoot)
# This acceptance modifies only an explicitly owned staged copy below this worktree's Saved/Reports.
$allowed = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Saved/Reports')).TrimEnd('\') + '\'
if (-not $package.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase) -or
    -not $report.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Use an owned package and report under this worktree Saved/Reports.' }
if (Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(UnrealEditor(-Cmd)?|ma0t10_dt(-Win64-.*)?|DTDeviceTestHost|cl)\.exe$' }) { throw 'External Unreal/build process active.' }
$exe = Join-Path $package 'ma0t10_dt/Binaries/Win64/ma0t10_dt.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Expected Development package executable is absent.' }
New-Item -ItemType Directory -Path $report -Force | Out-Null
$config = Join-Path $package 'ma0t10_dt/Config'
$common = Join-Path $config 'DefaultGame.ini'
$platform = Join-Path $config 'Windows/WindowsGame.ini'
$user = Join-Path $report 'owned-user'
$originals = @{}
foreach ($path in @($common,$platform)) {
    $originals[$path] = if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllBytes($path) } else { $null }
}
$utf8 = [Text.UTF8Encoding]::new($false)
function Write-Fixture([string]$Path,[string]$Marker,[int]$Port) {
    New-Item -ItemType Directory -Path (Split-Path $Path) -Force | Out-Null
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('[/Script/DTCore.DTCoreSettings]')
    $lines.Add(('WebSocketUrl="ws://127.0.0.1:{0}"' -f $Port))
    $lines.Add('!DataChannels=ClearArray')
    $lines.Add(('+DataChannels=(ChannelName="sensor",ChannelClass="/Script/DeviceRuntime.DxDeviceSensorStompChannel",Url="ws://127.0.0.1:{0}",bAutoConnect=False,bUseDefaultCredentials=False)' -f ($Port+1)))
    $lines.Add(('+DataChannels=(ChannelName="sensor-bulk",ChannelClass="/Script/DeviceRuntime.DxDeviceSensorBulkChannel",Url="tcp://127.0.0.1:{0}",bAutoConnect=False,bUseDefaultCredentials=False)' -f ($Port+2)))
    $lines.Add('[/Script/DeviceRuntime.DeviceDataChannelSettings]')
    $lines.Add('!Channels=ClearArray')
    foreach ($name in @('sensor','sensor-bulk','Default')) {
        $destinations = @('camera','lidar','pointcloud') | ForEach-Object {
            '(Alias="{0}",Destination="topic.r55b.{1}.{2}.{0}",bSubscribe=False,Purpose="deployment fixture")' -f $_,$Marker,$name
        }
        $lines.Add(('+Channels=(ChannelName="{0}",Destinations=({1}))' -f $name,($destinations -join ',')))
    }
    [IO.File]::WriteAllLines($Path,$lines,$utf8)
}
$rows = [Collections.Generic.List[object]]::new()
try {
    foreach ($case in @('common','platform','restart-edited-common')) {
        if ($case -eq 'common') {
            Write-Fixture $common 'common' 28713
            if (Test-Path -LiteralPath $platform) { Remove-Item -LiteralPath $platform }
            $marker='common';$port=28713
        } elseif ($case -eq 'platform') {
            Write-Fixture $platform 'platform' 28813
            $marker='platform';$port=28813
        } else {
            Write-Fixture $common 'edited' 28913
            Remove-Item -LiteralPath $platform
            $marker='edited';$port=28913
        }
        $started = [DateTime]::UtcNow
        $args = @('/Engine/Maps/Entry','-NullRHI','-unattended','-nosound','-DeviceChannelConfigProbe',('-UserDir="'+$user+'"'),('-abslog="'+$report+'/'+$case+'.log"'))
        $process = Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(120000)) { Stop-Process -Id $process.Id; throw "$case probe timeout (owned process stopped)." }
        if ($process.ExitCode -ne 0) { throw "$case probe exit $($process.ExitCode)" }
        $snapshots = @(Get-ChildItem -LiteralPath $user -Filter device-channel-config.json -Recurse -File | Where-Object LastWriteTimeUtc -ge $started)
        if ($snapshots.Count -ne 1) { throw "$case expected one fresh config snapshot." }
        $snapshot = Get-Content -LiteralPath $snapshots[0].FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        if (-not $snapshot.registry_ready -or $snapshot.default_url -ne "ws://127.0.0.1:$port") { throw "$case default connection settings mismatch." }
        foreach ($name in @('sensor','sensor-bulk','Default')) {
            $channel = @($snapshot.channels | Where-Object name -eq $name)
            if ($channel.Count -ne 1) { throw "$case missing channel $name" }
            foreach ($alias in @('camera','lidar','pointcloud')) {
                if ($channel[0].$alias -ne "topic.r55b.$marker.$name.$alias") { throw "$case topic alias mismatch: $name/$alias" }
            }
        }
        if (($snapshot.channels | Where-Object name -eq 'sensor').url -ne "ws://127.0.0.1:$($port+1)" -or
            ($snapshot.channels | Where-Object name -eq 'sensor-bulk').url -ne "tcp://127.0.0.1:$($port+2)") { throw "$case named connection settings mismatch." }
        Copy-Item -LiteralPath $snapshots[0].FullName -Destination (Join-Path $report "$case.json")
        Remove-Item -LiteralPath $snapshots[0].FullName
        $rows.Add(@{Case=$case;Pass=$true;Exit=$process.ExitCode;Marker=$marker})
    }
} catch {
    $rows.Add(@{Case=$case;Pass=$false;Failure=$_.Exception.Message})
    throw
} finally {
    foreach ($path in @($common,$platform)) {
        if ($null -ne $originals[$path]) { [IO.File]::WriteAllBytes($path,$originals[$path]) }
        elseif (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    @{Pass=($rows.Count -eq 3 -and @($rows | Where-Object { -not $_.Pass }).Count -eq 0);Cases=$rows.ToArray();RestoredLooseFiles=$true;NullRHI=$true;RealGpuAcceptance=$false} |
        ConvertTo-Json -Depth 6 | Set-Content (Join-Path $report 'manifest.json') -Encoding UTF8
}
