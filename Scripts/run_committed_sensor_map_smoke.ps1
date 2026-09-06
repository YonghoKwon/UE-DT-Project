param(
    [string]$Commit = 'HEAD',
    [ValidateSet('MA0T10.EditorSmoke.MapSensorComposition', 'MA0T10')]
    [string]$TestGroup = 'MA0T10.EditorSmoke.MapSensorComposition'
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Hash = & git -C $Root rev-parse --verify "$Commit^{commit}"
if ($LASTEXITCODE -ne 0 -or $Hash -notmatch '^[a-f0-9]{40}$') { throw 'Invalid commit' }
$ReportRoot = Join-Path $Root 'Saved/Reports'
$RunRoot = Join-Path $ReportRoot ('committed-map-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
$Archive = Join-Path $RunRoot 'map.zip'
& git -C $Root archive --format=zip "--output=$Archive" $Hash 'Content/MA0T10/Maps/SensorTestMap.umap'
if ($LASTEXITCODE -ne 0) { throw 'Could not archive committed SensorTestMap' }
Expand-Archive -LiteralPath $Archive -DestinationPath $RunRoot
$MapDirectory = Join-Path $RunRoot 'Content/MA0T10/Maps'
$Log = Join-Path $RunRoot 'smoke.log'
Write-Host "Testing commit $Hash; working SensorTestMap is not modified."
& 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
    (Join-Path $Root 'ma0t10_dt.uproject') -NullRHI -unattended -nosplash `
    "-SensorSmokeMapDirectory=$MapDirectory" `
    "-ExecCmds=Automation RunTests $TestGroup; Quit" `
    '-TestExit=Automation Test Queue Empty' "-abslog=$Log"
if ($LASTEXITCODE -ne 0) { throw "Committed-map smoke failed; see $Log" }
if (-not (Select-String -LiteralPath $Log -SimpleMatch 'Result={Success} Name={MapSensorComposition}' -Quiet)) {
    throw "No successful map test found; see $Log"
}
Write-Host "Committed-map smoke passed; log: $Log"
