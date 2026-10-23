param(
    [ValidateSet('Runtime','Captures','Performance','Manual','Presentation','MemoryControl','Dig','DigCaptures','DigIdle','MemoryTrendDig','MemoryTrendIdle','Feedback','FeedbackCaptures','OrientationCaptures','Orientation','OrientationManual','Industrial','IndustrialManual','Grab','Overlay','Query','Quality','CornerCaptures','Showcase')][string]$Mode='Runtime',
    [ValidateRange(1,3)][int]$Sensors=2,
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.3',
    [string]$ReportRoot='',
    [string]$CornerCaptureDirectory='',
    [string]$CornerReferenceDirectory='',
    [string]$ShowcaseDirectory='',
    [switch]$PresentationCaptures,
    [ValidateRange(1280,3840)][int]$Width=1920,
    [ValidateRange(720,2160)][int]$Height=1080,
    [ValidateRange(10,60)][int]$MemoryWarmupSeconds=35,
    [ValidateSet(100,150)][int]$FontPercent=100,
    [ValidateSet(0,1)][int]$DigView=1,
    [switch]$MemoryAudit,
    [switch]$MemoryDeepReport,
    [switch]$FullRhiDrain=$true,
    [switch]$Visible,
    [switch]$FastCaptures,
    [switch]$WorldTraceReference,
    [switch]$CpuReference,
    [ValidateRange(0,180)][double]$MatchedMemorySeconds=0,
    [ValidateRange(1800,3600)][double]$MemoryTrendSeconds=1800,
    [ValidateRange(5,30)][double]$MemorySampleSeconds=5
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(Get-Process UnrealEditor,UnrealEditor-Cmd,LiveCodingConsole -ErrorAction SilentlyContinue){throw 'Close Editor/PIE before HATCH validation'}
if($PresentationCaptures -and $Mode -ne 'Showcase'){throw 'PresentationCaptures requires Showcase'}
if($WorldTraceReference -and $Mode -ne 'Performance'){throw 'WorldTraceReference is a diagnostic Performance comparison only'}
if($MatchedMemorySeconds -gt 0 -and $Mode -notin @('Dig','DigIdle')){throw 'MatchedMemorySeconds requires Dig or DigIdle'}
$trend=$Mode -in @('MemoryTrendDig','MemoryTrendIdle')
if($trend -and ($MemoryDeepReport -or $CpuReference)){throw 'Default memory trend excludes deep reports and CPU overrides; keep attribution runs separate'}
if($trend -and !$ReportRoot){$ReportRoot=Join-Path $root ('Saved/Reports/MEMORY-TREND-26b/runs/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))}
if(!$ReportRoot){$group=if($Mode -like 'Industrial*' -or $Mode -in @('Grab','Overlay','Query')){'HATCH-06'}elseif($Mode -like 'Orientation*'){'HATCH-05'}elseif($Mode -like 'Feedback*'){'HATCH-04'}elseif($Mode -like 'Dig*'){'HATCH-03'}else{'HATCH-02'};$ReportRoot=Join-Path $root ("Saved/Reports/$group/runs/"+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))}
$ReportRoot=[IO.Path]::GetFullPath($ReportRoot)
if($trend -and (Test-Path -LiteralPath (Join-Path $ReportRoot 'memory-trend.jsonl'))){throw 'Memory trend requires a fresh ReportRoot; preserve previous evidence'}
New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
$sourceCommit=(& git -C $root rev-parse HEAD) -join ''
& {
    $sourceHashes=@{}
    Get-ChildItem -LiteralPath "$root/Source" -File -Recurse | ForEach-Object {$sourceHashes[$_.FullName.Substring($root.Length+1)]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    $sourceHashes['Content/MA0T10/Hatch/Materials/M_HatchCargo.uasset']=(Get-FileHash -LiteralPath "$root/Content/MA0T10/Hatch/Materials/M_HatchCargo.uasset" -Algorithm SHA256).Hash
    if($trend){foreach($relative in @('ma0t10_dt.uproject','Content/MA0T10/Maps/Tests/ShipHatchTestMap.umap')){$sourceHashes[$relative]=(Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash}}
    @{Commit=$sourceCommit; Mode=$Mode; SourceHashes=$sourceHashes; FontPercent=$FontPercent; Width=$Width; Height=$Height; MemoryWarmupSeconds=$MemoryWarmupSeconds; MatchedMemorySeconds=$MatchedMemorySeconds; MemoryAudit=([bool]$MemoryAudit -or $trend); MemoryTrendSeconds=$MemoryTrendSeconds; MemorySampleSeconds=$MemorySampleSeconds; DiagnosticOnly=$trend; FullRhiDrain=([bool]$FullRhiDrain -and !$trend); WorldTraceReference=[bool]$WorldTraceReference; CpuReference=[bool]$CpuReference; FastCaptureOnly=[bool]$FastCaptures; Visible=[bool]$Visible; Submodule=((& git -C "$root/Plugins/DTCore" rev-parse HEAD) -join '')} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$ReportRoot/source-manifest.json" -Encoding UTF8
}
$test=switch($Mode){'Showcase'{'HATCH06.ShowcaseCapturesRhi'} 'CornerCaptures'{'HATCH06.CornerCapturesRhi'} 'Quality'{'HATCH06.ModelQualityRhi'} 'Query'{'HATCH06.SceneQueriesRhi'} 'Overlay'{'HATCH06.OverlayAndModelRhi'} 'Grab'{'HATCH06.GrabPresentationRhi'} 'Industrial'{'HATCH06.AcceptanceRhi'} 'IndustrialManual'{'HATCH06.AcceptanceRhi'} 'OrientationManual'{'MA0T10.Hatch.ViewOrientationRhi'} 'Orientation'{'MA0T10.Hatch.ViewOrientationRhi'} 'OrientationCaptures'{'HATCH05.CapturesRhi'} 'Feedback'{'MA0T10.Hatch.UserFeedbackRhi'} 'FeedbackCaptures'{'HATCH04.CapturesRhi'} 'DigIdle'{'HATCH03.IdleMemoryControlRhi'} 'Dig'{'HATCH03.DigRuntimeRhi'} 'DigCaptures'{'HATCH03.DigCaptureRhi'} 'Captures'{'HATCH01.CaptureRhi'} 'Performance'{'HATCH01.PerformanceRhi'} 'Presentation'{'MA0T10.Hatch.ProductPresentationRhi'} 'MemoryControl'{'HATCH02.IdleMemoryControlRhi'} default{'HATCH01.RuntimeRhi'}}
if($trend){$test=if($Mode -eq 'MemoryTrendDig'){'HATCH26.MemoryTrendDigRhi'}else{'HATCH26.MemoryTrendIdleRhi'}}
$priorX=$env:MA0T10_ACCEPTANCE_RES_X;$priorY=$env:MA0T10_ACCEPTANCE_RES_Y
try {
    $env:MA0T10_ACCEPTANCE_RES_X=[string]$Width;$env:MA0T10_ACCEPTANCE_RES_Y=[string]$Height
    $execPrefix=if($CpuReference){'ma0t10.Lidar.HardwareRayTracing 0, '}else{''}
    $hatchArgs=@("$root/ma0t10_dt.uproject",'/Engine/Maps/Entry','-d3d12','-nosplash','-nosound','-nop4',
        "-ExecCmds=${execPrefix}Automation RunTests $test; Quit",'-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$ReportRoot/automation","-HatchReport=$ReportRoot","-HatchSourceCommit=$sourceCommit","-HatchSensorCount=$Sensors","-HatchMemoryWarmup=$MemoryWarmupSeconds","-HatchFontPercent=$FontPercent","-HatchDigView=$DigView",
        "-abslog=$ReportRoot/automation.log")
    if($Mode -eq 'CornerCaptures' -and $CornerCaptureDirectory){$hatchArgs+="-HatchCornerOutput=$([IO.Path]::GetFullPath($CornerCaptureDirectory))"}
    if($Mode -eq 'CornerCaptures' -and $CornerReferenceDirectory){$hatchArgs+="-HatchCornerReference=$([IO.Path]::GetFullPath($CornerReferenceDirectory))"}
    if($Mode -eq 'Showcase' -and $ShowcaseDirectory){$hatchArgs+="-HatchShowcaseOutput=$([IO.Path]::GetFullPath($ShowcaseDirectory))"}
    if($PresentationCaptures){$hatchArgs+='-HatchPresentationOnly'}
    if($Mode -eq 'OrientationManual'){$hatchArgs+='-HatchOrientationManual'}
    if($Mode -eq 'IndustrialManual'){$hatchArgs+='-Hatch06Manual'}
    if($FastCaptures){if($Mode -ne 'Industrial'){throw 'FastCaptures is only a capture validation mode, not a performance gate'};$hatchArgs+='-Hatch06Fast'}
    if($Mode -in @('Manual','OrientationManual','IndustrialManual')){$hatchArgs+='-HatchManual'}else{$hatchArgs+='-unattended'}
    if($MemoryAudit){$hatchArgs+='-HatchMemoryAudit'}
    if($MemoryDeepReport){if(!$MemoryAudit){throw 'MemoryDeepReport requires MemoryAudit'};$hatchArgs+='-HatchMemoryDeepReport'}
    if(($MemoryAudit -and $Mode -like 'Dig*') -or $trend){$hatchArgs+='-llm'}
    if($trend){$hatchArgs+=@('-HatchMemoryTrend',('-HatchMemoryTrendSeconds='+$MemoryTrendSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)),('-HatchMemorySampleSeconds='+$MemorySampleSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)))}
    if($FullRhiDrain -and !$trend){$hatchArgs+='-HatchFullRhiDrain'}
    if($WorldTraceReference){$hatchArgs+='-HatchWorldTraceReference'}
    if($CpuReference){$hatchArgs+='-HatchCpuReference'}
    if($MatchedMemorySeconds -gt 0){$hatchArgs+="-HatchMemoryWindowSeconds=$MatchedMemorySeconds"}
    if(!$Visible -and $Mode -notin @('Manual','OrientationManual','IndustrialManual')){$hatchArgs+='-RenderOffscreen'}
    $quotedArgs=@($hatchArgs | ForEach-Object {'"'+$_+'"'})
    $process=Start-Process "$EngineRoot/Engine/Binaries/Win64/UnrealEditor.exe" -ArgumentList $quotedArgs -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput "$ReportRoot/console.log" -RedirectStandardError "$ReportRoot/stderr.log"
    if($process.ExitCode -ne 0){throw "HATCH $Mode failed ($($process.ExitCode)): $ReportRoot"}
    $data=Get-Content "$ReportRoot/automation/index.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    if(@($data.tests).Count -ne 1 -or $data.tests[0].state -ne 'Success'){throw "HATCH test did not pass: $ReportRoot"}
    Write-Host "HATCH $Mode Success: $ReportRoot"
} finally {$env:MA0T10_ACCEPTANCE_RES_X=$priorX;$env:MA0T10_ACCEPTANCE_RES_Y=$priorY}
