param([switch]$SkipBuild,[switch]$EnableSlabBroker)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root 'Saved/Reports/DTCoreFinalAcceptance'
$snapshot=Join-Path $report 'committed-map'
New-Item -ItemType Directory -Force -Path $snapshot|Out-Null
& git -C $root archive --format=zip "--output=$(Join-Path $report 'map.zip')" HEAD Content/MA0T10/Maps/SensorTestMap.umap
if($LASTEXITCODE -ne 0){throw 'Committed map archive failed'}
Expand-Archive -LiteralPath (Join-Path $report 'map.zip') -DestinationPath $snapshot -Force
$mapdir=Join-Path $snapshot 'Content/MA0T10/Maps'
$maparg='Saved/Reports/DTCoreFinalAcceptance/committed-map/Content/MA0T10/Maps'
if(-not $SkipBuild){
    & 'C:/Program Files/Epic Games/UE_5.3/Engine/Build/BatchFiles/Build.bat' ma0t10_dtEditor Win64 Development "-Project=$(Join-Path $root 'ma0t10_dt.uproject')" -WaitMutex -NoHotReloadFromIDE
    if($LASTEXITCODE -ne 0){throw 'Build failed'}
}
$env:MA0T10_CAMERA_MARKER_RHI='1';$env:MA0T10_INPUT_ACCEPTANCE_RHI='1';$env:MA0T10_MANIPULATION_RHI='1';$env:MA0T10_WORKSPACE_RHI='1'
$env:MA0T10_SENSOR_FILES_RHI='1';$env:MA0T10_SLAB_RUNTIME='1';$env:MA0T10_SLAB_PORTABLE='1';$env:MA0T10_SLAB_ANALYSIS_RHI='1'
$env:MA0T10_RUN_ARTEMIS_SMOKE='1';$env:MA0T10_ARTEMIS_URL='ws://127.0.0.1:61616';$env:MA0T10_ARTEMIS_RAW_URL='tcp://127.0.0.1:61616'
$env:MA0T10_ARTEMIS_USER='artemis';$env:MA0T10_ARTEMIS_PASSWORD='artemis'
$env:MA0T10_RAW_PROBE='1'
if($EnableSlabBroker){$env:MA0T10_DTCORE_SLAB_BROKER='1'}
$faultProbe=Start-Process node -ArgumentList @("`"$(Join-Path $root 'Tools/Artemis/raw_receipt_probe.mjs')`"",'18617',"`"$(Join-Path $report 'receipt-fault.json')`"") -WindowStyle Hidden -PassThru
try{
    & 'C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' (Join-Path $root 'ma0t10_dt.uproject') -d3d12 -RenderOffscreen -NoSound -NoVSync -unattended -nosplash -nop4 "-SensorSmokeMapDirectory=$maparg" '-ExecCmds=r.VSyncEditor 0,Slate.bAllowThrottling 0,Automation RunTests MA0T10;Quit' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$(Join-Path $report 'full-regression')" "-abslog=$(Join-Path $report 'full-regression.log')"
    if($LASTEXITCODE -ne 0){throw 'Full regression failed; inspect report'}
}finally{
    if($faultProbe -and -not $faultProbe.HasExited){Stop-Process -Id $faultProbe.Id}
    foreach($name in @('MA0T10_RAW_PROBE','MA0T10_DTCORE_SLAB_BROKER','MA0T10_CAMERA_MARKER_RHI','MA0T10_INPUT_ACCEPTANCE_RHI','MA0T10_MANIPULATION_RHI','MA0T10_WORKSPACE_RHI','MA0T10_SENSOR_FILES_RHI','MA0T10_SLAB_RUNTIME','MA0T10_SLAB_PORTABLE','MA0T10_SLAB_ANALYSIS_RHI','MA0T10_RUN_ARTEMIS_SMOKE','MA0T10_ARTEMIS_URL','MA0T10_ARTEMIS_RAW_URL','MA0T10_ARTEMIS_USER','MA0T10_ARTEMIS_PASSWORD')){Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue}
}
