param(
    [string]$ProjectRoot = "",
    [string]$LocalProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [switch]$RequireCsvPerformanceEvidence,
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Assert-FileExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found: $Path"
    }
}

function Test-ContainsText {
    param(
        [string]$Path,
        [string]$Pattern
    )

    return [bool](Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet)
}

function Resolve-OptionalDirectory {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return ""
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Get-CsvPreviewPerformanceEvidence {
    param(
        [string]$ProjectRoot,
        [string]$LocalProjectRoot
    )

    $csvPerformanceScript = Join-Path $ProjectRoot "Scripts\export_csv_preview_performance_report.ps1"
    if (-not (Test-Path -LiteralPath $csvPerformanceScript -PathType Leaf)) {
        return [PSCustomObject]@{
            Present = $false
            Reason = "CSV preview performance report script is missing."
        }
    }

    $resolvedLocalRoot = Resolve-OptionalDirectory -Path $LocalProjectRoot
    if ([string]::IsNullOrWhiteSpace($resolvedLocalRoot)) {
        return [PSCustomObject]@{
            Present = $false
            Reason = "Local project root was not found: $LocalProjectRoot"
        }
    }

    $logPath = Join-Path $resolvedLocalRoot "Saved\Logs\m7at10_dt.log"
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        return [PSCustomObject]@{
            Present = $false
            LocalProjectRoot = $resolvedLocalRoot
            LogPath = $logPath
            Reason = "Unreal automation log is missing. Run Automation RunTests M7AT10.Sensor.CsvPointCloudPreview first."
        }
    }

    try {
        $scriptOutput = @(& powershell -ExecutionPolicy Bypass -File $csvPerformanceScript -ProjectRoot $ProjectRoot -LocalProjectRoot $resolvedLocalRoot -LogPath $logPath -RequireAutomationSuccess -Json 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "CSV preview performance report exited with code ${LASTEXITCODE}: $($scriptOutput -join ' ')"
        }
        $jsonText = $scriptOutput -join "`n"
        $report = $jsonText | ConvertFrom-Json
        return [PSCustomObject]@{
            Present = [bool]$report.Summary.Valid
            LocalProjectRoot = $resolvedLocalRoot
            LogPath = $logPath
            ScenarioCount = [int]$report.Summary.ScenarioCount
            MaxAcceptedPoints = [int]$report.Summary.MaxAcceptedPoints
            MaxTotalLoadMs = [double]$report.Summary.MaxTotalLoadMs
            ProceduralPerformanceBudgetMs = [double]$report.Summary.ProceduralPerformanceBudgetMs
            RequiredScenariosPresent = [bool]$report.Summary.RequiredScenariosPresent
            AutomationSuccessEvidencePresent = [bool]$report.Summary.AutomationSuccessEvidencePresent
            SuccessfulTestCompletionCount = [int]$report.Summary.SuccessfulTestCompletionCount
            TestCompleteExitCodeZero = [bool]$report.Summary.TestCompleteExitCodeZero
            Reason = "CSV preview performance telemetry was read from the local automation log."
        }
    }
    catch {
        return [PSCustomObject]@{
            Present = $false
            LocalProjectRoot = $resolvedLocalRoot
            LogPath = $logPath
            Reason = $_.Exception.Message
        }
    }
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

function Convert-ToMarkdownCell {
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Write-MarkdownReport {
    param(
        [object]$Report,
        [string]$Path
    )

    $lines = @()
    $lines += "# Point Cloud Renderer Decision Report"
    $lines += ""
    $lines += "Generated UTC: $($Report.GeneratedUtc)"
    $lines += ""
    $lines += "## Current Preview State"
    $lines += ""
    $lines += "- Server/preview split documented: $($Report.Summary.ServerPreviewSplitDocumented)"
    $lines += "- Preview caps declared: $($Report.Summary.PreviewCapsDeclared)"
    $lines += "- PointCloudOnly clamps declared: $($Report.Summary.PointCloudOnlyClampDeclared)"
    $lines += "- Batched ISM upload declared: $($Report.Summary.BatchedInstanceUploadDeclared)"
    $lines += "- CSV performance evidence present: $($Report.Summary.CsvPerformanceEvidencePresent)"
    $lines += "- CPU fallback performance evidence present: $($Report.Summary.CpuFallbackPerformanceEvidencePresent)"
    $lines += "- CSV max accepted points: $($Report.Summary.CsvPreviewMaxAcceptedPoints)"
    $lines += "- GPU renderer integrated: $($Report.Summary.GpuRendererIntegrated)"
    $lines += "- Recommended next decision: $($Report.Summary.RecommendedNextDecision)"
    $lines += ""
    $lines += "## Candidate Renderers"
    $lines += ""
    $lines += "| Option | Runtime shape | Pros | Risks | Recommended decision |"
    $lines += "| --- | --- | --- | --- | --- |"
    foreach ($option in $Report.CandidateRenderers) {
        $lines += "| $(Convert-ToMarkdownCell $option.Option) | $(Convert-ToMarkdownCell $option.RuntimeShape) | $(Convert-ToMarkdownCell ($option.Pros -join '; ')) | $(Convert-ToMarkdownCell ($option.Risks -join '; ')) | $(Convert-ToMarkdownCell $option.RecommendedDecision) |"
    }
    $lines += ""
    $lines += "## Acceptance Evidence Needed"
    $lines += ""
    foreach ($item in $Report.AcceptanceEvidenceNeeded) {
        $lines += "- $item"
    }
    $lines += ""
    $lines += "## CSV Preview Performance Evidence"
    $lines += ""
    if ($Report.CsvPreviewPerformanceEvidence.Present) {
        $lines += "- Local project root: ``$($Report.CsvPreviewPerformanceEvidence.LocalProjectRoot)``"
        $lines += "- Log path: ``$($Report.CsvPreviewPerformanceEvidence.LogPath)``"
        $lines += "- Scenario count: $($Report.CsvPreviewPerformanceEvidence.ScenarioCount)"
        $lines += "- Max accepted points: $($Report.CsvPreviewPerformanceEvidence.MaxAcceptedPoints)"
        $lines += "- Max total load ms: $($Report.CsvPreviewPerformanceEvidence.MaxTotalLoadMs)"
        $lines += "- Procedural performance budget ms: $($Report.CsvPreviewPerformanceEvidence.ProceduralPerformanceBudgetMs)"
        $lines += "- Automation success evidence present: $($Report.CsvPreviewPerformanceEvidence.AutomationSuccessEvidencePresent)"
        $lines += "- Successful test completion count: $($Report.CsvPreviewPerformanceEvidence.SuccessfulTestCompletionCount)"
        $lines += "- Test complete exit code zero: $($Report.CsvPreviewPerformanceEvidence.TestCompleteExitCodeZero)"
    }
    else {
        $lines += "- Missing: $($Report.CsvPreviewPerformanceEvidence.Reason)"
    }
    $lines += ""
    $lines += "## Notes"
    $lines += ""
    $lines += "Keep the current CPU/ISM preview caps as the fallback path even if a GPU renderer is added."

    Write-TextFile -Path $Path -Lines $lines
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$csvPreviewPerformanceEvidence = Get-CsvPreviewPerformanceEvidence -ProjectRoot $ProjectRoot -LocalProjectRoot $LocalProjectRoot

$lidarHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h"
$lidarCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp"
$managerCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualSensorManager.cpp"
$monitorCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp"
$replayTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp"
$managerTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\SensorManagerAutomationTests.cpp"
$schemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"

foreach ($file in @(
    [PSCustomObject]@{ Path = $lidarHeader; Label = "LiDAR component header" },
    [PSCustomObject]@{ Path = $lidarCpp; Label = "LiDAR component implementation" },
    [PSCustomObject]@{ Path = $managerCpp; Label = "Sensor manager implementation" },
    [PSCustomObject]@{ Path = $monitorCpp; Label = "Monitor widget implementation" },
    [PSCustomObject]@{ Path = $replayTests; Label = "Replay automation tests" },
    [PSCustomObject]@{ Path = $managerTests; Label = "Sensor manager automation tests" },
    [PSCustomObject]@{ Path = $schemaDoc; Label = "LiDAR payload schema" },
    [PSCustomObject]@{ Path = $smokeDoc; Label = "Editor smoke test document" },
    [PSCustomObject]@{ Path = $remainingDoc; Label = "Remaining work document" }
)) {
    Assert-FileExists -Path $file.Path -Label $file.Label
}

$serverPreviewSplitDocumented = Test-ContainsText -Path $schemaDoc -Pattern 'Server-side judgment should not use `previewPolicy` as measurement truth.'
$previewCapsDeclared = (Test-ContainsText -Path $lidarHeader -Pattern "PreviewPointStride") -and
    (Test-ContainsText -Path $lidarHeader -Pattern "MaxPreviewPoints") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "Preview is uncapped")
$pointCloudOnlyClampDeclared = (Test-ContainsText -Path $managerCpp -Pattern "FMath::Min(LidarComp->MaxPreviewPoints, 3000)") -and
    (Test-ContainsText -Path $managerCpp -Pattern "FMath::Max(2, LidarComp->PreviewPointStride)")
$batchedInstanceUploadDeclared = Test-ContainsText -Path $lidarCpp -Pattern "AddInstances(InstanceTransforms, false, true)"
$automationCoverageDeclared = (Test-ContainsText -Path $replayTests -Pattern "M7AT10.SensorReplay.PerformanceWarningStatus") -and
    (Test-ContainsText -Path $managerTests -Pattern "M7AT10.SensorManager.PointCloudOnlyPreservesPayloadPolicy")
$gpuRendererIntegrated = (Test-ContainsText -Path $lidarCpp -Pattern "UNiagaraComponent") -or
    (Test-ContainsText -Path $lidarCpp -Pattern "FRDGBuffer") -or
    (Test-ContainsText -Path $remainingDoc -Pattern "GPU renderer integrated")

$candidateRenderers = @(
    [PSCustomObject]@{
        Option = "Keep CPU ISM fallback"
        RuntimeShape = "Use current `UInstancedStaticMeshComponent` preview with stride/max caps and batched `AddInstances`."
        Pros = @("Already compiles", "Simple editor fallback", "Works without plugin decisions")
        Risks = @("Still CPU/update-bound for very dense scans", "Full refresh can stall on very large frames")
        RecommendedDecision = "Keep as fallback even after a GPU renderer is added"
    },
    [PSCustomObject]@{
        Option = "Niagara point renderer"
        RuntimeShape = "Feed downsampled or full point buffers into a Niagara system for point sprites."
        Pros = @("UE-native GPU visualization path", "Good fit for interactive editor preview", "Can support color/semantic attributes")
        Risks = @("Requires Niagara asset and data-interface design", "Needs screenshot/pixel smoke testing", "Packaging/settings ownership")
        RecommendedDecision = "Preferred first GPU renderer candidate"
    },
    [PSCustomObject]@{
        Option = "Custom GPU buffer renderer"
        RuntimeShape = "Upload point data to render buffers and draw points with a custom component or render proxy."
        Pros = @("Highest control", "Best ceiling for dense scans", "Can avoid Niagara asset coupling")
        Risks = @("More engine-level code", "Higher maintenance", "Requires deeper render-thread testing")
        RecommendedDecision = "Use only if Niagara cannot meet density or attribute requirements"
    },
    [PSCustomObject]@{
        Option = "External viewer workflow"
        RuntimeShape = "Keep Unreal preview downsampled and inspect dense clouds through exported LAS/PCD/JSONL in a dedicated viewer."
        Pros = @("Lowest Unreal runtime risk", "Good for offline QA", "Avoids editor stalls")
        Risks = @("Not an in-editor dense view", "Extra workflow step", "Harder for operator dashboards")
        RecommendedDecision = "Use for archival/debug workflows, not as the main operator preview"
    }
)

$acceptanceEvidence = @(
    "Renderer option selected with owner and rationale",
    "Target preview density and frame-time budget documented",
    "Server payload point count remains independent from preview point count",
    "PointCloudOnly mode still preserves collision/trace behavior",
    "Desktop and viewport smoke evidence shows nonblank rendered points",
    "Automation or manual test records that dense frames avoid editor stalls"
)

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$cpuFallbackPerformanceEvidencePresent = $csvPreviewPerformanceEvidence.Present -and
    $csvPreviewPerformanceEvidence.RequiredScenariosPresent -and
    $csvPreviewPerformanceEvidence.AutomationSuccessEvidencePresent -and
    ($csvPreviewPerformanceEvidence.MaxAcceptedPoints -ge 250000)
$report = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    ProjectRoot = $ProjectRoot
    CurrentEvidence = [PSCustomObject]@{
        LidarHeader = $lidarHeader
        LidarImplementation = $lidarCpp
        SensorManagerImplementation = $managerCpp
        MonitorImplementation = $monitorCpp
        ReplayTests = $replayTests
        SensorManagerTests = $managerTests
        SchemaDoc = $schemaDoc
        SmokeDoc = $smokeDoc
        RemainingWorkDoc = $remainingDoc
    }
    CsvPreviewPerformanceEvidence = $csvPreviewPerformanceEvidence
    CandidateRenderers = $candidateRenderers
    AcceptanceEvidenceNeeded = $acceptanceEvidence
    Summary = [PSCustomObject]@{
        ServerPreviewSplitDocumented = $serverPreviewSplitDocumented
        PreviewCapsDeclared = $previewCapsDeclared
        PointCloudOnlyClampDeclared = $pointCloudOnlyClampDeclared
        BatchedInstanceUploadDeclared = $batchedInstanceUploadDeclared
        AutomationCoverageDeclared = $automationCoverageDeclared
        CsvPerformanceEvidencePresent = [bool]$csvPreviewPerformanceEvidence.Present
        CpuFallbackPerformanceEvidencePresent = [bool]$cpuFallbackPerformanceEvidencePresent
        CsvPreviewMaxAcceptedPoints = if ($csvPreviewPerformanceEvidence.Present) { [int]$csvPreviewPerformanceEvidence.MaxAcceptedPoints } else { 0 }
        GpuRendererIntegrated = $gpuRendererIntegrated
        CandidateRendererCount = $candidateRenderers.Count
        AcceptanceEvidenceCount = $acceptanceEvidence.Count
        RecommendedNextDecision = "Choose Niagara, CustomGpuBuffer, or ExternalViewer workflow while keeping CPU ISM as fallback."
        Valid = ($serverPreviewSplitDocumented -and $previewCapsDeclared -and $pointCloudOnlyClampDeclared -and $batchedInstanceUploadDeclared -and $automationCoverageDeclared -and -not $gpuRendererIntegrated -and (-not $RequireCsvPerformanceEvidence -or $cpuFallbackPerformanceEvidencePresent))
    }
}

if (-not $report.Summary.Valid) {
    throw "Point cloud renderer decision report invariants failed. ServerPreviewSplit=$serverPreviewSplitDocumented PreviewCaps=$previewCapsDeclared PointCloudOnlyClamp=$pointCloudOnlyClampDeclared BatchedUpload=$batchedInstanceUploadDeclared Automation=$automationCoverageDeclared CsvPerfEvidence=$cpuFallbackPerformanceEvidencePresent GpuIntegrated=$gpuRendererIntegrated"
}

if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}
if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-MarkdownReport -Report $report -Path $MarkdownPath
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Point cloud renderer decision report is ready."
    Write-Host "Project root: $ProjectRoot"
    Write-Host "Candidate renderers: $($report.Summary.CandidateRendererCount)"
    Write-Host "Acceptance evidence items: $($report.Summary.AcceptanceEvidenceCount)"
    Write-Host "Batched instance upload: $($report.Summary.BatchedInstanceUploadDeclared)"
    Write-Host "CSV performance evidence present: $($report.Summary.CsvPerformanceEvidencePresent)"
    Write-Host "CPU fallback performance evidence present: $($report.Summary.CpuFallbackPerformanceEvidencePresent)"
    Write-Host "CSV max accepted points: $($report.Summary.CsvPreviewMaxAcceptedPoints)"
    Write-Host "GPU renderer integrated: $($report.Summary.GpuRendererIntegrated)"
    Write-Host "Recommended next decision: $($report.Summary.RecommendedNextDecision)"
}
