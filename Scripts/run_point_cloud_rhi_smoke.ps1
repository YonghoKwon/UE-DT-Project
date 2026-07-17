param(
    [string]$ProjectPath = "C:\Unreal Projects\ma0t10_dt\ma0t10_dt.uproject",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ProjectPath)) {
    throw "Project file not found: $ProjectPath"
}

$ProjectRoot = Split-Path -Parent $ProjectPath
$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$ReportRoot = Join-Path $ProjectRoot "Saved\Reports"
$LogPath = Join-Path $ReportRoot "point_cloud_rhi_smoke.log"
$ScreenshotPath = Join-Path $ReportRoot "point_cloud_rhi_smoke.png"
$JsonPath = Join-Path $ReportRoot "point_cloud_rhi_smoke.json"
$MarkdownPath = Join-Path $ReportRoot "point_cloud_rhi_smoke.md"

New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
Remove-Item -LiteralPath $LogPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $ScreenshotPath -Force -ErrorAction SilentlyContinue

if (-not $SkipBuild) {
    & $BuildBat "ma0t10_dtEditor" "Win64" "Development" $ProjectPath "-WaitMutex" "-NoHotReloadFromIDE"
    if ($LASTEXITCODE -ne 0) {
        throw "Editor build failed with exit code $LASTEXITCODE"
    }
}

$Arguments = @(
    $ProjectPath,
    "-Windowed",
    "-Unattended",
    "-NoSplash",
    "-NoSound",
    "-ResX=1920",
    "-ResY=1080",
    "-ExecCmds=Automation RunTests MA0T10.SensorV2.Runtime.FeatureSmoke; Quit",
    "-TestExit=Automation Test Queue Empty",
    "-abslog=$LogPath"
)

& $EditorCmd @Arguments
$EditorExitCode = $LASTEXITCODE

if (-not (Test-Path -LiteralPath $LogPath)) {
    throw "RHI smoke log was not created: $LogPath"
}

$Log = Get-Content -LiteralPath $LogPath -Raw
$UsedNullRhi = $Log -match "NullRHI"
$UsedD3d12 = $Log -match "D3D12RHI|DirectX 12|D3D12 Adapter"
$HasEnsure = $Log -match "Ensure condition failed"
$TestPassed = $Log -match "MA0T10\.SensorV2\.Runtime\.FeatureSmoke.*Success|Test Completed\. Result=\{Success\}.*FeatureSmoke"
$HasHitProof = $Log -notmatch "automatic LiDAR scan produces renderable hit points.*Error"
$HasInstanceProof = $Log -notmatch "point-cloud ISM has visible instances.*Error"
$HasNiagaraProof = $Log -notmatch "forced Niagara reports active rendering or an explicit error.*Error|automatic renderer selects live Niagara or CPU fallback.*Error|automatic renderer keeps visible hit points.*Error"
$ScreenshotBytes = if (Test-Path -LiteralPath $ScreenshotPath) { (Get-Item -LiteralPath $ScreenshotPath).Length } else { 0 }
$HasScreenshot = $ScreenshotBytes -gt 4096
$Passed = $EditorExitCode -eq 0 -and $TestPassed -and -not $UsedNullRhi -and $UsedD3d12 -and -not $HasEnsure -and $HasHitProof -and $HasInstanceProof -and $HasNiagaraProof -and $HasScreenshot

$Result = [ordered]@{
    GeneratedUtc = [DateTime]::UtcNow.ToString("o")
    Map = "/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"
    Test = "MA0T10.SensorV2.Runtime.FeatureSmoke"
    Resolution = "1920x1080"
    EditorExitCode = $EditorExitCode
    D3D12Detected = $UsedD3d12
    NullRhiDetected = $UsedNullRhi
    EnsureDetected = $HasEnsure
    RenderableHitsVerified = $HasHitProof
    CpuInstancesVerified = $HasInstanceProof
    RendererPolicyVerified = $HasNiagaraProof
    ScreenshotBytes = $ScreenshotBytes
    ScreenshotPath = $ScreenshotPath
    Passed = $Passed
    LogPath = $LogPath
}

$Result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $JsonPath -Encoding UTF8

$Markdown = @"
# Sensor V2 point-cloud RHI smoke

- Result: **$(if ($Passed) { "PASS" } else { "FAIL" })**
- Map: ``$($Result.Map)``
- Test: ``$($Result.Test)``
- Resolution: $($Result.Resolution)
- D3D12 detected: $($Result.D3D12Detected)
- NullRHI detected: $($Result.NullRhiDetected)
- Ensure detected: $($Result.EnsureDetected)
- Renderable LiDAR hits verified: $($Result.RenderableHitsVerified)
- CPU fallback instances verified: $($Result.CpuInstancesVerified)
- Niagara/CPU policy verified: $($Result.RendererPolicyVerified)
- Screenshot bytes: $($Result.ScreenshotBytes)
- Screenshot: ``$ScreenshotPath``
- Editor exit code: $($Result.EditorExitCode)
- Log: ``$LogPath``

The runtime test starts PIE, waits for a LiDAR frame containing real hits, enables the world point cloud,
forces the CPU fallback, verifies that its ISM contains visible instances, and verifies that an instance
uses a measured world-space point. This complements the Niagara workload benchmark; it does not replace
a human visual-quality review of sprite size and color.
"@
$Markdown | Set-Content -LiteralPath $MarkdownPath -Encoding UTF8

if (-not $Passed) {
    throw "Point-cloud RHI smoke failed. See $MarkdownPath and $LogPath"
}

Write-Host "Point-cloud RHI smoke passed."
Write-Host $MarkdownPath
