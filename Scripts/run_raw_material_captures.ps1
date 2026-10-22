param(
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.3',
    [string]$RunName='run-final',
    [switch]$Visible
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=Join-Path $root 'Saved/Reports/RAW-CAP-01'
$run=Join-Path $output $RunName
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue){throw 'Close Editor/Live Coding first'}
New-Item -ItemType Directory -Force -Path $run | Out-Null
if(-not(Test-Path "$output/initial-audit.json")){
    & python "$PSScriptRoot/capture_raw_materials.py" backup
    if($LASTEXITCODE -ne 0){throw 'Backup failed'}
}
& python "$PSScriptRoot/capture_raw_materials.py" check_backup
if($LASTEXITCODE -ne 0){throw 'Settings differ from session backup; no Editor launched'}
$beforeX=$env:MA0T10_ACCEPTANCE_RES_X;$beforeY=$env:MA0T10_ACCEPTANCE_RES_Y
try {
    $env:MA0T10_ACCEPTANCE_RES_X='1920';$env:MA0T10_ACCEPTANCE_RES_Y='1080'
    $test='HATCH07.RawPresentationCapturesRhi'
    $commit=((& git -C $root rev-parse HEAD)-join '')
    $hashes=@{}
    Get-ChildItem "$root/Source" -Recurse -File | ForEach-Object {$hashes[$_.FullName.Substring($root.Length+1)]=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}
    @{Commit=$commit;SourceHashes=$hashes;Submodule=((& git -C "$root/Plugins/DTCore" rev-parse HEAD)-join '');Map='/Game/MA0T10/Maps/Tests/ShipHatchTestMap';Resolution='1920x1080';Rhi='D3D12';RayTracing='ProjectDefault';FontPercent=100;RenderOffscreen=(!$Visible);TestFilter=$test} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$run/source-manifest.json"
    $arguments=@("$root/ma0t10_dt.uproject",'/Engine/Maps/Entry','-d3d12','-unattended','-nosplash','-nosound','-nop4',"-ExecCmds=Automation RunTests $test; Quit",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$run/automation","-abslog=$run/automation.log","-RawCaptureOutput=$output","-HatchSourceCommit=$commit")
    if(!$Visible){$arguments+='-RenderOffscreen'}
    $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
    $p=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $quoted -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput "$run/console.log" -RedirectStandardError "$run/stderr.log"
    if($p.ExitCode -ne 0){throw "Capture process exit $($p.ExitCode)"}
    $data=Get-Content "$run/automation/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if(@($data.tests).Count -ne 1 -or $data.tests[0].state -ne 'Success'){throw "Capture test failed: $run"}
    Write-Host "Capture Success: $run"
} finally {
    $env:MA0T10_ACCEPTANCE_RES_X=$beforeX;$env:MA0T10_ACCEPTANCE_RES_Y=$beforeY
    & python "$PSScriptRoot/capture_raw_materials.py" restore
    if($LASTEXITCODE -ne 0){throw 'Restoration audit failed'}
}
