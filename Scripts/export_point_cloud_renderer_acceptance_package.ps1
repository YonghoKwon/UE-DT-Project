param(
    [string]$ProjectRoot = "",
    [string]$LocalProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$LogPath = "",
    [string]$OutputRoot = "",
    [string]$ViewportScreenshotPath = "",
    [int64]$ViewportScreenshotBytes = -1,
    [int64]$NonBlankPixelCount = -1,
    [int64]$GpuSmokePointCount = -1,
    [string]$GpuSmokeMapName = "",
    [string]$GpuSmokeSensorId = "",
    [string]$GpuSmokeRendererName = "",
    [string]$GpuSmokeOperator = "",
    [string]$GpuSmokeNotes = "",
    [switch]$ObservedDenseFrameNoStall,
    [switch]$ObservedFallbackToggle,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    $Value | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [string[]]$Arguments
    )

    $output = @(& powershell -ExecutionPolicy Bypass -File $ScriptPath @Arguments -Json 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Get-OptionalLogPath {
    param(
        [string]$LocalProjectRoot,
        [string]$LogPath
    )

    if (-not [string]::IsNullOrWhiteSpace($LogPath) -and (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $LogPath).Path
    }
    if (-not (Test-Path -LiteralPath $LocalProjectRoot -PathType Container)) {
        return ""
    }

    $candidate = Join-Path $LocalProjectRoot "Saved\Logs\m7at10_dt.log"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return (Resolve-Path -LiteralPath $candidate).Path
    }
    return ""
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (Test-Path -LiteralPath $LocalProjectRoot -PathType Container) {
    $LocalProjectRoot = (Resolve-Path -LiteralPath $LocalProjectRoot).Path
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\PointCloudRendererAcceptance"
}
if (-not (Test-Path -LiteralPath $OutputRoot)) {
    New-Item -ItemType Directory -Path $OutputRoot | Out-Null
}
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$decisionScript = Join-Path $ProjectRoot "Scripts\export_point_cloud_renderer_decision_report.ps1"
$policyScript = Join-Path $ProjectRoot "Scripts\validate_point_cloud_preview_policy.ps1"
$csvPerformanceScript = Join-Path $ProjectRoot "Scripts\export_csv_preview_performance_report.ps1"

foreach ($requiredScript in @($decisionScript, $policyScript, $csvPerformanceScript)) {
    if (-not (Test-Path -LiteralPath $requiredScript -PathType Leaf)) {
        throw "Required script not found: $requiredScript"
    }
}

$decisionJsonPath = Join-Path $OutputRoot "point_cloud_renderer_decision.json"
$decisionMarkdownPath = Join-Path $OutputRoot "point_cloud_renderer_decision.md"
$decisionArgs = @(
    "-ProjectRoot", $ProjectRoot,
    "-LocalProjectRoot", $LocalProjectRoot,
    "-MarkdownPath", $decisionMarkdownPath,
    "-JsonPath", $decisionJsonPath
)
if (-not [string]::IsNullOrWhiteSpace($LogPath)) {
    $decisionArgs += @("-LogPath", $LogPath)
}
else {
    $decisionArgs += @("-LogPath", (Join-Path $OutputRoot "missing_csv_preview_performance.log"))
}
if (-not [string]::IsNullOrWhiteSpace($ViewportScreenshotPath)) {
    $decisionArgs += @("-ViewportScreenshotPath", $ViewportScreenshotPath)
}
if ($ViewportScreenshotBytes -ge 0) {
    $decisionArgs += @("-ViewportScreenshotBytes", ([string]$ViewportScreenshotBytes))
}
if ($NonBlankPixelCount -ge 0) {
    $decisionArgs += @("-NonBlankPixelCount", ([string]$NonBlankPixelCount))
}
if ($GpuSmokePointCount -ge 0) {
    $decisionArgs += @("-GpuSmokePointCount", ([string]$GpuSmokePointCount))
}
foreach ($pair in @(
    @{ Name = "GpuSmokeMapName"; Value = $GpuSmokeMapName },
    @{ Name = "GpuSmokeSensorId"; Value = $GpuSmokeSensorId },
    @{ Name = "GpuSmokeRendererName"; Value = $GpuSmokeRendererName },
    @{ Name = "GpuSmokeOperator"; Value = $GpuSmokeOperator },
    @{ Name = "GpuSmokeNotes"; Value = $GpuSmokeNotes }
)) {
    if (-not [string]::IsNullOrWhiteSpace($pair.Value)) {
        $decisionArgs += @("-$($pair.Name)", $pair.Value)
    }
}
if ($ObservedDenseFrameNoStall) {
    $decisionArgs += "-ObservedDenseFrameNoStall"
}
if ($ObservedFallbackToggle) {
    $decisionArgs += "-ObservedFallbackToggle"
}
$decisionReport = Invoke-JsonScript -ScriptPath $decisionScript -Arguments $decisionArgs

$policyReport = Invoke-JsonScript -ScriptPath $policyScript -Arguments @("-ProjectRoot", $ProjectRoot)
$policyJsonPath = Join-Path $OutputRoot "point_cloud_preview_policy.json"
Write-JsonFile -Path $policyJsonPath -Value $policyReport

$csvLogPath = Get-OptionalLogPath -LocalProjectRoot $LocalProjectRoot -LogPath $LogPath
$csvPerformanceReport = $null
$csvJsonPath = Join-Path $OutputRoot "csv_preview_performance.json"
$csvMarkdownPath = Join-Path $OutputRoot "csv_preview_performance.md"
if ([string]::IsNullOrWhiteSpace($csvLogPath)) {
    $csvPerformanceReport = [PSCustomObject]@{
        Present = $false
        Reason = "CSV preview performance log was not found. Run the CSV preview automation workflow, then rerun this package with -LogPath."
        ExpectedLogPath = if (Test-Path -LiteralPath $LocalProjectRoot -PathType Container) { Join-Path $LocalProjectRoot "Saved\Logs\m7at10_dt.log" } else { "" }
    }
    Write-JsonFile -Path $csvJsonPath -Value $csvPerformanceReport
    Write-TextFile -Path $csvMarkdownPath -Lines @(
        "# CSV Preview Performance Evidence",
        "",
        "CSV preview performance evidence is missing.",
        "",
        "Run:",
        "",
        '```powershell',
        'powershell -ExecutionPolicy Bypass -File ".\Scripts\run_csv_preview_performance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"',
        'powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_acceptance_package.ps1" -LogPath "C:\Unreal Projects\m7at10_dt\Saved\Logs\m7at10_dt.log"',
        '```'
    )
}
else {
    try {
        $csvPerformanceReport = Invoke-JsonScript -ScriptPath $csvPerformanceScript -Arguments @(
            "-ProjectRoot", $ProjectRoot,
            "-LocalProjectRoot", $LocalProjectRoot,
            "-LogPath", $csvLogPath,
            "-MarkdownPath", $csvMarkdownPath,
            "-JsonPath", $csvJsonPath,
            "-RequireAutomationSuccess"
        )
    }
    catch {
        $csvPerformanceReport = [PSCustomObject]@{
            Present = $false
            LogPath = $csvLogPath
            Reason = $_.Exception.Message
        }
        Write-JsonFile -Path $csvJsonPath -Value $csvPerformanceReport
        Write-TextFile -Path $csvMarkdownPath -Lines @(
            "# CSV Preview Performance Evidence",
            "",
            "CSV preview performance evidence was not accepted from the selected log.",
            "",
            "Log path: ``$csvLogPath``",
            "",
            "Reason: $($csvPerformanceReport.Reason)"
        )
    }
}

$readyToClaimGpuDensePreview = (
    [bool]$decisionReport.Summary.GpuRendererIntegrated -and
    [bool]$decisionReport.Summary.GpuViewportSmokeEvidencePresent -and
    [bool]$decisionReport.Summary.GpuFallbackPreservationEvidencePresent -and
    [bool]$decisionReport.Summary.GpuDenseFrameEvidencePresent
)

$manifest = [PSCustomObject]@{
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("o")
    ProjectRoot = $ProjectRoot
    LocalProjectRoot = $LocalProjectRoot
    OutputRoot = $OutputRoot
    Artifacts = [PSCustomObject]@{
        RendererDecisionJson = $decisionJsonPath
        RendererDecisionMarkdown = $decisionMarkdownPath
        PreviewPolicyJson = $policyJsonPath
        CsvPreviewPerformanceJson = $csvJsonPath
        CsvPreviewPerformanceMarkdown = $csvMarkdownPath
    }
    Summary = [PSCustomObject]@{
        PackageCreated = $true
        RendererDecisionValid = [bool]$decisionReport.Summary.Valid
        PreviewPolicyValid = [bool]$policyReport.Summary.Valid
        CsvPreviewPerformanceEvidencePresent = [bool]$decisionReport.Summary.CsvPerformanceEvidencePresent
        CpuPreviewFallbackEvidencePresent = [bool]$decisionReport.Summary.CpuPreviewFallbackEvidencePresent
        CpuIsmFallbackSmokePresent = [bool]$decisionReport.Summary.CpuIsmFallbackSmokePresent
        CpuProceduralDenseEvidencePresent = [bool]$decisionReport.Summary.CpuProceduralDenseEvidencePresent
        DefaultPreviewBackend = [string]$decisionReport.Summary.DefaultPreviewBackend
        ConfiguredPreviewBackendSource = [string]$decisionReport.Summary.ConfiguredPreviewBackendSource
        CandidatePreviewBackends = @($decisionReport.Summary.CandidatePreviewBackends)
        CpuFallbackPreserved = [bool]$decisionReport.Summary.CpuFallbackPreserved
        GpuRendererIntegrated = [bool]$decisionReport.Summary.GpuRendererIntegrated
        RendererPhase = [string]$decisionReport.Summary.RendererPhase
        GpuViewportSmokeEvidencePresent = [bool]$decisionReport.Summary.GpuViewportSmokeEvidencePresent
        GpuFallbackPreservationEvidencePresent = [bool]$decisionReport.Summary.GpuFallbackPreservationEvidencePresent
        GpuDenseFrameEvidencePresent = [bool]$decisionReport.Summary.GpuDenseFrameEvidencePresent
        ReadyToClaimGpuDensePreview = $readyToClaimGpuDensePreview
        CsvPreviewPerformanceReportWritten = (Test-Path -LiteralPath $csvJsonPath -PathType Leaf)
        CsvPreviewPerformanceReportValid = if ($csvPerformanceReport.Summary) { [bool]$csvPerformanceReport.Summary.Valid } else { [bool]$csvPerformanceReport.Present }
        DryRunOnly = $true
        DoesNotIntegrateGpuRenderer = $true
        CreatesNiagaraAssets = $false
        RunsEditor = $false
        RunsGpuViewportSmoke = $false
        DoesNotModifyAssets = $true
        StagesFiles = $false
        Valid = ([bool]$decisionReport.Summary.Valid -and [bool]$policyReport.Summary.Valid)
        Boundary = "This package records renderer readiness evidence only. It does not implement a GPU/Niagara renderer, create screenshots, modify assets, or stage files."
    }
    ManualEvidenceStillNeeded = @(
        "Run CSV preview automation and rerun this package with -LogPath when CPU fallback performance evidence is needed.",
        "Implement or enable the selected GPU/Niagara renderer candidate.",
        "Capture a nonblank dense viewport screenshot and rerun this package with GPU smoke parameters.",
        "Record fallback toggle and dense-frame no-stall observations before claiming GPU dense preview readiness."
    )
    FollowUpCommands = @(
        'powershell -ExecutionPolicy Bypass -File ".\Scripts\run_csv_preview_performance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"',
        'powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -LogPath "C:\Unreal Projects\m7at10_dt\Saved\Logs\m7at10_dt.log"',
        'powershell -ExecutionPolicy Bypass -File ".\Scripts\export_point_cloud_renderer_acceptance_package.ps1" -ViewportScreenshotPath "C:\path\to\gpu_viewport.png" -ViewportScreenshotBytes 123456 -NonBlankPixelCount 1000 -GpuSmokePointCount 120000 -GpuSmokeMapName "TestMap" -GpuSmokeSensorId "Lidar01" -GpuSmokeRendererName "Niagara point renderer" -GpuSmokeOperator "name" -GpuSmokeNotes "dense frame viewport smoke" -ObservedDenseFrameNoStall -ObservedFallbackToggle'
    )
}

$manifestJsonPath = Join-Path $OutputRoot "point_cloud_renderer_acceptance_manifest.json"
$readmePath = Join-Path $OutputRoot "README.md"
Write-JsonFile -Path $manifestJsonPath -Value $manifest
Write-TextFile -Path $readmePath -Lines @(
    "# Point Cloud Renderer Acceptance Package",
    "",
    "Generated UTC: $($manifest.GeneratedUtc)",
    "",
    "## Summary",
    "",
    "- Renderer decision valid: $($manifest.Summary.RendererDecisionValid)",
    "- Preview policy valid: $($manifest.Summary.PreviewPolicyValid)",
    "- CSV preview performance evidence present: $($manifest.Summary.CsvPreviewPerformanceEvidencePresent)",
    "- CPU preview fallback evidence present: $($manifest.Summary.CpuPreviewFallbackEvidencePresent)",
    "- CPU ISM fallback smoke present: $($manifest.Summary.CpuIsmFallbackSmokePresent)",
    "- CPU procedural dense evidence present: $($manifest.Summary.CpuProceduralDenseEvidencePresent)",
    "- Default preview backend: $($manifest.Summary.DefaultPreviewBackend)",
    "- Configured preview backend source: $($manifest.Summary.ConfiguredPreviewBackendSource)",
    "- CPU fallback preserved: $($manifest.Summary.CpuFallbackPreserved)",
    "- GPU renderer integrated: $($manifest.Summary.GpuRendererIntegrated)",
    "- Renderer phase: $($manifest.Summary.RendererPhase)",
    "- GPU viewport smoke evidence present: $($manifest.Summary.GpuViewportSmokeEvidencePresent)",
    "- GPU fallback preservation evidence present: $($manifest.Summary.GpuFallbackPreservationEvidencePresent)",
    "- GPU dense-frame evidence present: $($manifest.Summary.GpuDenseFrameEvidencePresent)",
    "- Ready to claim GPU dense preview: $($manifest.Summary.ReadyToClaimGpuDensePreview)",
    "- Boundary: $($manifest.Summary.Boundary)",
    "",
    "## Artifacts",
    "",
    '- `point_cloud_renderer_decision.json` / `point_cloud_renderer_decision.md`',
    '- `point_cloud_preview_policy.json`',
    '- `csv_preview_performance.json` / `csv_preview_performance.md`',
    '- `point_cloud_renderer_acceptance_manifest.json`',
    "",
    "## Follow-up Commands",
    "",
    '```powershell',
    $manifest.FollowUpCommands[0],
    $manifest.FollowUpCommands[1],
    $manifest.FollowUpCommands[2],
    '```'
)

if ($Json) {
    $manifest | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Point cloud renderer acceptance package written: $OutputRoot"
    Write-Host "Renderer decision valid: $($manifest.Summary.RendererDecisionValid)"
    Write-Host "Preview policy valid: $($manifest.Summary.PreviewPolicyValid)"
    Write-Host "CSV preview performance evidence present: $($manifest.Summary.CsvPreviewPerformanceEvidencePresent)"
    Write-Host "Default preview backend: $($manifest.Summary.DefaultPreviewBackend)"
    Write-Host "CPU fallback preserved: $($manifest.Summary.CpuFallbackPreserved)"
    Write-Host "GPU renderer integrated: $($manifest.Summary.GpuRendererIntegrated)"
    Write-Host "Ready to claim GPU dense preview: $($manifest.Summary.ReadyToClaimGpuDensePreview)"
    Write-Host "Boundary: $($manifest.Summary.Boundary)"
}
