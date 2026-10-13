param([switch]$SkipPackage)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root 'Saved/Reports/DTCoreFinalAcceptance'
$engine='C:/Program Files/Epic Games/UE_5.3/Engine'
$fixture=Join-Path $root 'Saved/FinalAcceptanceCompatHost'
if(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Close Editor processes before final build validation'}
foreach($entry in @(
    @{target='ma0t10_dtEditor';config='Development';project=(Join-Path $root 'ma0t10_dt.uproject');label='project-editor'},
    @{target='ma0t10_dt';config='Shipping';project=(Join-Path $root 'ma0t10_dt.uproject');label='project-shipping'},
    @{target='DTCoreCompatHostEditor';config='Development';project=(Join-Path $fixture 'DTCoreCompatHost.uproject');label='isolated-editor'},
    @{target='DTCoreCompatHost';config='Shipping';project=(Join-Path $fixture 'DTCoreCompatHost.uproject');label='isolated-shipping'})){
    & "$engine/Build/BatchFiles/Build.bat" $entry.target Win64 $entry.config "-Project=$($entry.project)" -WaitMutex -NoHotReloadFromIDE 2>&1|Tee-Object -FilePath (Join-Path $report "$($entry.label).log")
    if($LASTEXITCODE -ne 0){throw "Build failed: $($entry.label)"}
}
$assetLog=Join-Path $report 'assets.log'
$assetArgs=@("`"$(Join-Path $root 'ma0t10_dt.uproject')`"",'-NullRHI','-unattended','-nosplash',"-ExecutePythonScript=`"$(Join-Path $root 'Scripts/validate_dtcore_integration_assets.py')`"","-abslog=`"$assetLog`"")
$assetProcess=Start-Process "$engine/Binaries/Win64/UnrealEditor-Cmd.exe" -ArgumentList $assetArgs -WindowStyle Hidden -PassThru -Wait
if($assetProcess.ExitCode -ne 0 -or -not (Select-String -LiteralPath $assetLog -SimpleMatch 'DTCORE_ASSETS_PASSED' -Quiet)){throw 'In-memory Blueprint compatibility check failed or did not execute'}
if(-not $SkipPackage){
    & "$engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$(Join-Path $root 'ma0t10_dt.uproject')" -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor -cook -stage -pak -archive "-archivedirectory=$(Join-Path $root 'Saved/DTCoreFinalAcceptancePackage')" -unattended -utf8output 2>&1|Tee-Object -FilePath (Join-Path $report 'development-package.log')
    if($LASTEXITCODE -ne 0){throw 'Development package failed'}
}
Write-Output 'Final build matrix passed; independent plugin copy used by minimum host.'
