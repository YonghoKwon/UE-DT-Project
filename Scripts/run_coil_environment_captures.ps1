param(
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.3',
    [string]$TestFilter='MA0T10.CoilLoading.Presentation.EnvironmentsRhi',
    [string]$RunName='run-final'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=Join-Path $root 'Saved/Reports/COIL-CAP-01'
$run=Join-Path $output $RunName
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue){throw 'Close Editor/Live Coding first'}
New-Item -ItemType Directory -Force -Path $run | Out-Null
if(-not(Test-Path "$output/initial-audit.json")){
    & python "$PSScriptRoot/capture_coil_environments.py" backup
    if($LASTEXITCODE -ne 0){throw 'Backup failed'}
}
& python "$PSScriptRoot/capture_coil_environments.py" check_backup
if($LASTEXITCODE -ne 0){throw 'Current settings differ from session backup; no Editor launched'}
& python "$PSScriptRoot/capture_coil_environments.py" prepare
if($LASTEXITCODE -ne 0){throw 'Fixture preparation failed'}
$beforeX=$env:MA0T10_ACCEPTANCE_RES_X;$beforeY=$env:MA0T10_ACCEPTANCE_RES_Y
try {
    $env:MA0T10_ACCEPTANCE_RES_X='1920';$env:MA0T10_ACCEPTANCE_RES_Y='1080'
    $source='Source/ma0t10_dtEditor/Private/CoilLoadingEnvironmentCaptures.cpp'
    @{Commit=((& git -C $root rev-parse HEAD)-join '');TestSourceHash=(Get-FileHash "$root/$source").Hash;Map='/Game/MA0T10/Maps/Tests/CoilLoadingTestMap';Resolution='1920x1080';Rhi='D3D12';RenderOffscreen=$true;TestFilter=$TestFilter} | ConvertTo-Json | Set-Content -Encoding UTF8 "$run/source-manifest.json"
    $arguments=@("$root/ma0t10_dt.uproject",'/Engine/Maps/Entry','-d3d12','-RenderOffscreen','-unattended','-nosplash','-nosound','-nop4',"-ExecCmds=Automation RunTests $TestFilter; Quit",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$run/automation","-abslog=$run/automation.log","-CoilEnvironmentOutput=$output")
    $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
    $p=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $quoted -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput "$run/console.log" -RedirectStandardError "$run/stderr.log"
    if($p.ExitCode -ne 0){throw "Capture process exit $($p.ExitCode)"}
    $data=Get-Content "$run/automation/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if(@($data.tests).Count -eq 0 -or @($data.tests | Where-Object {$_.state -ne 'Success'}).Count){throw "Capture test failed: $run"}
    Write-Host "Capture Success: $run"
} finally {
    $env:MA0T10_ACCEPTANCE_RES_X=$beforeX;$env:MA0T10_ACCEPTANCE_RES_Y=$beforeY
    & python "$PSScriptRoot/capture_coil_environments.py" restore
    if($LASTEXITCODE -ne 0){throw 'Restoration audit failed'}
}
