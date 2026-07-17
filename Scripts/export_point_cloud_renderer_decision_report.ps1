param(
    [string]$ProjectRoot = "",
    [string]$LocalProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$LogPath = "",
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
        [string]$LocalProjectRoot,
        [string]$LogPath
    )

    $csvPerformanceScript = Join-Path $ProjectRoot "Scripts\export_csv_preview_performance_report.ps1"
    if (-not (Test-Path -LiteralPath $csvPerformanceScript -PathType Leaf)) {
        return [PSCustomObject]@{
            Present = $false
            Reason = "CSV preview performance report script is missing."
        }
    }

    $resolvedLocalRoot = Resolve-OptionalDirectory -Path $LocalProjectRoot
    $resolvedLogPath = $LogPath

    if ([string]::IsNullOrWhiteSpace($resolvedLogPath) -and [string]::IsNullOrWhiteSpace($resolvedLocalRoot)) {
        return [PSCustomObject]@{
            Present = $false
            Reason = "Local project root was not found: $LocalProjectRoot"
        }
    }

    if ([string]::IsNullOrWhiteSpace($resolvedLogPath)) {
        $resolvedLogPath = Join-Path $resolvedLocalRoot "Saved\Logs\ma0t10_dt.log"
    }
    if (-not (Test-Path -LiteralPath $resolvedLogPath -PathType Leaf)) {
        return [PSCustomObject]@{
            Present = $false
            LocalProjectRoot = $resolvedLocalRoot
            LogPath = $resolvedLogPath
            Reason = "Unreal automation log is missing. Run Automation RunTests MA0T10.Sensor.CsvPointCloudPreview first, or pass -LogPath to a log that contains that run."
        }
    }
    $resolvedLogPath = (Resolve-Path -LiteralPath $resolvedLogPath).Path

    try {
        $csvLocalRootArg = $resolvedLocalRoot
        if ([string]::IsNullOrWhiteSpace($csvLocalRootArg)) {
            $csvLocalRootArg = $LocalProjectRoot
        }
        $csvArgs = @(
            "-ProjectRoot", $ProjectRoot,
            "-LocalProjectRoot", $csvLocalRootArg,
            "-LogPath", $resolvedLogPath,
            "-RequireAutomationSuccess",
            "-Json"
        )
        $scriptOutput = @(& powershell -ExecutionPolicy Bypass -File $csvPerformanceScript @csvArgs 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "CSV preview performance report exited with code ${LASTEXITCODE}: $($scriptOutput -join ' ')"
        }
        $jsonText = $scriptOutput -join "`n"
        $report = $jsonText | ConvertFrom-Json
        $metricsByScenario = @{}
        foreach ($metric in @($report.Metrics)) {
            $metricsByScenario[$metric.Scenario] = $metric
        }
        $instancedMetric = $metricsByScenario["InstancedBatchLoad"]
        $proceduralDenseMetric = $metricsByScenario["ProceduralHighDensityLoad"]
        $proceduralBudgetMetric = $metricsByScenario["ProceduralPerformanceBudget"]
        return [PSCustomObject]@{
            Present = [bool]$report.Summary.Valid
            LocalProjectRoot = $resolvedLocalRoot
            LogPath = $resolvedLogPath
            ScenarioCount = [int]$report.Summary.ScenarioCount
            MaxAcceptedPoints = [int]$report.Summary.MaxAcceptedPoints
            MaxTotalLoadMs = [double]$report.Summary.MaxTotalLoadMs
            ProceduralPerformanceBudgetMs = [double]$report.Summary.ProceduralPerformanceBudgetMs
            InstancedSmokeAcceptedPoints = if ($instancedMetric) { [int]$instancedMetric.AcceptedPoints } else { 0 }
            InstancedSmokeRenderMode = if ($instancedMetric) { [string]$instancedMetric.RenderMode } else { "" }
            ProceduralDenseAcceptedPoints = if ($proceduralDenseMetric) { [int]$proceduralDenseMetric.AcceptedPoints } else { 0 }
            ProceduralDenseRenderMode = if ($proceduralDenseMetric) { [string]$proceduralDenseMetric.RenderMode } else { "" }
            ProceduralBudgetAcceptedPoints = if ($proceduralBudgetMetric) { [int]$proceduralBudgetMetric.AcceptedPoints } else { 0 }
            ProceduralBudgetRenderMode = if ($proceduralBudgetMetric) { [string]$proceduralBudgetMetric.RenderMode } else { "" }
            RequiredScenariosPresent = [bool]$report.Summary.RequiredScenariosPresent
            AutomationSuccessEvidencePresent = [bool]$report.Summary.AutomationSuccessEvidencePresent
            SuccessfulTestCompletionCount = [int]$report.Summary.SuccessfulTestCompletionCount
            TestCompleteExitCodeZero = [bool]$report.Summary.TestCompleteExitCodeZero
            FailureEvidencePresent = [bool]$report.Summary.FailureEvidencePresent
            FailureLineCount = [int]$report.Summary.FailureLineCount
            EvidenceRunStartLine = [int]$report.Summary.EvidenceRunStartLine
            TestCompleteLine = [int]$report.Summary.TestCompleteLine
            EvidenceLinesWithinRun = [bool]$report.Summary.EvidenceLinesWithinRun
            Reason = "CSV preview performance telemetry was read from the local automation log."
        }
    }
    catch {
        return [PSCustomObject]@{
            Present = $false
            LocalProjectRoot = $resolvedLocalRoot
            LogPath = $resolvedLogPath
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
    $lines += "- CSV performance automation evidence present: $($Report.Summary.CsvPreviewPerformanceAutomationEvidencePresent)"
    $lines += "- CSV performance evidence missing reason: $($Report.Summary.CsvPreviewPerformanceEvidenceMissingReason)"
    $lines += "- Ready to claim CSV preview performance: $($Report.Summary.ReadyToClaimCsvPreviewPerformance)"
    $lines += "- CPU preview fallback evidence present: $($Report.Summary.CpuPreviewFallbackEvidencePresent)"
    $lines += "- CPU ISM fallback smoke present: $($Report.Summary.CpuIsmFallbackSmokePresent)"
    $lines += "- CPU procedural dense evidence present: $($Report.Summary.CpuProceduralDenseEvidencePresent)"
    $lines += "- Default preview backend: $($Report.Summary.DefaultPreviewBackend)"
    $lines += "- Configured preview backend source: $($Report.Summary.ConfiguredPreviewBackendSource)"
    $lines += "- Preview backend selection declared: $($Report.Summary.PreviewBackendSelectionDeclared)"
    $lines += "- GPU preview backend claim blocked: $($Report.Summary.GpuPreviewBackendClaimBlocked)"
    $lines += "- CPU fallback forced for GPU candidates: $($Report.Summary.CpuFallbackForcedForGpuCandidates)"
    $lines += "- CPU fallback preserved: $($Report.Summary.CpuFallbackPreserved)"
    $lines += "- CSV max accepted points: $($Report.Summary.CsvPreviewMaxAcceptedPoints)"
    $lines += "- CSV failure evidence present: $($Report.Summary.CsvFailureEvidencePresent)"
    $lines += "- CSV evidence lines within run: $($Report.Summary.CsvEvidenceLinesWithinRun)"
    $lines += "- GPU renderer integrated: $($Report.Summary.GpuRendererIntegrated)"
    $lines += "- Renderer phase: $($Report.Summary.RendererPhase)"
    $lines += "- Recommended first GPU candidate: $($Report.Summary.RecommendedFirstGpuCandidate)"
    $lines += "- GPU viewport smoke evidence present: $($Report.Summary.GpuViewportSmokeEvidencePresent)"
    $lines += "- GPU fallback preservation evidence present: $($Report.Summary.GpuFallbackPreservationEvidencePresent)"
    $lines += "- GPU dense-frame evidence present: $($Report.Summary.GpuDenseFrameEvidencePresent)"
    $lines += "- Renderer decision matrix declared: $($Report.Summary.RendererDecisionMatrixDeclared)"
    $lines += "- Recommended next decision: $($Report.Summary.RecommendedNextDecision)"
    $lines += ""
    $lines += "## Candidate Renderers"
    $lines += ""
    $lines += "| Rank | Option | Runtime shape | Pros | Risks | Recommended decision | First GPU spike | Decision blockers |"
    $lines += "| ---: | --- | --- | --- | --- | --- | --- | --- |"
    foreach ($option in $Report.CandidateRenderers) {
        $lines += "| $($option.Rank) | $(Convert-ToMarkdownCell $option.Option) | $(Convert-ToMarkdownCell $option.RuntimeShape) | $(Convert-ToMarkdownCell ($option.Pros -join '; ')) | $(Convert-ToMarkdownCell ($option.Risks -join '; ')) | $(Convert-ToMarkdownCell $option.RecommendedDecision) | $($option.FirstGpuSpikeCandidate) | $(Convert-ToMarkdownCell ($option.DecisionBlockers -join '; ')) |"
    }
    $lines += ""
    $lines += "## GPU Spike Action Plan"
    $lines += ""
    $lines += "| Priority | Step | Owner needed | Status | Required evidence | Blockers | Next action |"
    $lines += "| ---: | --- | --- | --- | --- | --- | --- |"
    foreach ($item in $Report.GpuSpikeActionPlan) {
        $lines += "| $($item.Priority) | $(Convert-ToMarkdownCell $item.Step) | $(Convert-ToMarkdownCell $item.OwnerNeeded) | $(Convert-ToMarkdownCell $item.Status) | $(Convert-ToMarkdownCell ($item.RequiredEvidence -join '; ')) | $(Convert-ToMarkdownCell ($item.Blockers -join '; ')) | $(Convert-ToMarkdownCell $item.NextAction) |"
    }
    $lines += ""
    $lines += "## GPU Viewport Smoke Evidence"
    $lines += ""
    $lines += "- Screenshot path: ``$($Report.GpuViewportSmokeEvidence.ViewportScreenshotPath)``"
    $lines += "- Screenshot bytes: $($Report.GpuViewportSmokeEvidence.ViewportScreenshotBytes)"
    $lines += "- Nonblank pixel count: $($Report.GpuViewportSmokeEvidence.NonBlankPixelCount)"
    $lines += "- Point count: $($Report.GpuViewportSmokeEvidence.PointCount)"
    $lines += "- Map name: ``$($Report.GpuViewportSmokeEvidence.MapName)``"
    $lines += "- Sensor id: ``$($Report.GpuViewportSmokeEvidence.SensorId)``"
    $lines += "- Renderer name: ``$($Report.GpuViewportSmokeEvidence.RendererName)``"
    $lines += "- Operator: ``$($Report.GpuViewportSmokeEvidence.Operator)``"
    $lines += "- Dense frame no stall observed: $($Report.GpuViewportSmokeEvidence.ObservedDenseFrameNoStall)"
    $lines += "- Fallback toggle observed: $($Report.GpuViewportSmokeEvidence.ObservedFallbackToggle)"
    $lines += "- Evidence present: $($Report.GpuViewportSmokeEvidence.EvidencePresent)"
    $lines += "- Missing evidence fields: $(@($Report.GpuViewportSmokeEvidence.MissingEvidenceFields) -join ', ')"
    $lines += ""
    $lines += "## Acceptance Evidence Needed"
    $lines += ""
    foreach ($item in $Report.AcceptanceEvidenceNeeded) {
        $lines += "- $item"
    }
    $lines += ""
    $lines += "## Decision Gates"
    $lines += ""
    foreach ($gate in $Report.DecisionGates) {
        $lines += "- $($gate.Name): $($gate.Status) - $($gate.Evidence)"
    }
    $lines += ""
    $lines += "## CSV Preview Performance Evidence"
    $lines += ""
    if ($Report.CsvPreviewPerformanceEvidence.Present) {
        $lines += "- Local project root: ``$($Report.CsvPreviewPerformanceEvidence.LocalProjectRoot)``"
        $lines += "- Log path: ``$($Report.CsvPreviewPerformanceEvidence.LogPath)``"
        $lines += "- Scenario count: $($Report.CsvPreviewPerformanceEvidence.ScenarioCount)"
        $lines += "- Max accepted points: $($Report.CsvPreviewPerformanceEvidence.MaxAcceptedPoints)"
        $lines += "- Instanced smoke accepted points: $($Report.CsvPreviewPerformanceEvidence.InstancedSmokeAcceptedPoints)"
        $lines += "- Procedural dense accepted points: $($Report.CsvPreviewPerformanceEvidence.ProceduralDenseAcceptedPoints)"
        $lines += "- Procedural budget accepted points: $($Report.CsvPreviewPerformanceEvidence.ProceduralBudgetAcceptedPoints)"
        $lines += "- Max total load ms: $($Report.CsvPreviewPerformanceEvidence.MaxTotalLoadMs)"
        $lines += "- Procedural performance budget ms: $($Report.CsvPreviewPerformanceEvidence.ProceduralPerformanceBudgetMs)"
        $lines += "- Automation success evidence present: $($Report.CsvPreviewPerformanceEvidence.AutomationSuccessEvidencePresent)"
        $lines += "- Successful test completion count: $($Report.CsvPreviewPerformanceEvidence.SuccessfulTestCompletionCount)"
        $lines += "- Test complete exit code zero: $($Report.CsvPreviewPerformanceEvidence.TestCompleteExitCodeZero)"
        $lines += "- Failure evidence present: $($Report.CsvPreviewPerformanceEvidence.FailureEvidencePresent)"
        $lines += "- Failure line count: $($Report.CsvPreviewPerformanceEvidence.FailureLineCount)"
        $lines += "- Evidence run start line: $($Report.CsvPreviewPerformanceEvidence.EvidenceRunStartLine)"
        $lines += "- Test complete line: $($Report.CsvPreviewPerformanceEvidence.TestCompleteLine)"
        $lines += "- Evidence lines within run: $($Report.CsvPreviewPerformanceEvidence.EvidenceLinesWithinRun)"
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
$csvPreviewPerformanceEvidence = Get-CsvPreviewPerformanceEvidence -ProjectRoot $ProjectRoot -LocalProjectRoot $LocalProjectRoot -LogPath $LogPath

$lidarHeader = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\VirtualLidarScanComponent.h"
$lidarCpp = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\VirtualLidarScanComponent.cpp"
$managerCpp = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\VirtualSensorCoordinator.cpp"
$monitorCpp = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\UI\VirtualSensorMonitorPanelWidget.cpp"
$replayTests = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\Tests\LidarReplayAutomationTests.cpp"
$managerTests = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\Tests\SensorManagerAutomationTests.cpp"
$schemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"

foreach ($file in @(
    [PSCustomObject]@{ Path = $lidarHeader; Label = "LiDAR component header" },
    [PSCustomObject]@{ Path = $lidarCpp; Label = "LiDAR component implementation" },
    [PSCustomObject]@{ Path = $managerCpp; Label = "Sensor manager implementation" },
    [PSCustomObject]@{ Path = $monitorCpp; Label = "Monitor widget implementation" },
    [PSCustomObject]@{ Path = $replayTests; Label = "Replay automation tests" },
    [PSCustomObject]@{ Path = $managerTests; Label = "Sensor manager automation tests" },
    [PSCustomObject]@{ Path = $schemaDoc; Label = "LiDAR payload schema" },
    [PSCustomObject]@{ Path = $smokeDoc; Label = "Editor smoke test document" }
)) {
    Assert-FileExists -Path $file.Path -Label $file.Label
}

$serverPreviewSplitDocumented = Test-ContainsText -Path $schemaDoc -Pattern 'Server-side judgment should not use `previewPolicy` as measurement truth.'
$previewCapsDeclared = (Test-ContainsText -Path $lidarHeader -Pattern "PreviewPointStride") -and
    (Test-ContainsText -Path $lidarHeader -Pattern "MaxPreviewPoints") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "Preview is uncapped")
$pointCloudOnlyClampDeclared = (Test-ContainsText -Path $managerCpp -Pattern "FMath::Min(LidarComp->MaxPreviewPoints, 3000)") -and
    (Test-ContainsText -Path $managerCpp -Pattern "FMath::Max(2, LidarComp->PreviewPointStride)")
$visualizationCpp = Join-Path $ProjectRoot "Source\ma0t10_dt\MA0T10\Sensor\VirtualLidarVisualizationComponent.cpp"
$niagaraAsset = Join-Path $ProjectRoot "Content\MA0T10\Sensor\VFX\NS_VirtualLidarPointCloud.uasset"
$batchedInstanceUploadDeclared = Test-ContainsText -Path $lidarCpp -Pattern "BatchUpdateInstancesTransforms"
$previewBackendSelectionDeclared = (Test-ContainsText -Path $visualizationCpp -Pattern "NiagaraPointCloudSystem") -and
    (Test-ContainsText -Path $visualizationCpp -Pattern "SetNiagaraArrayPosition") -and
    (Test-ContainsText -Path $visualizationCpp -Pattern "SetNiagaraArrayColor")
$gpuPreviewBackendClaimBlocked = $false
$cpuFallbackForcedForGpuCandidates = (Test-ContainsText -Path $visualizationCpp -Pattern "CPU ISM") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "RefreshPointCloudPreview") -and
    (Test-ContainsText -Path $lidarCpp -Pattern "BatchUpdateInstancesTransforms")
$automationCoverageDeclared = (Test-ContainsText -Path $replayTests -Pattern "MA0T10.SensorReplay.PerformanceWarningStatus") -and
    (Test-ContainsText -Path $managerTests -Pattern "MA0T10.SensorManager.PointCloudOnlyPreservesPayloadPolicy")
$gpuRendererIntegrated = (Test-ContainsText -Path $visualizationCpp -Pattern "UNiagaraComponent") -and
    (Test-ContainsText -Path $visualizationCpp -Pattern "SetNiagaraArrayPosition") -and
    (Test-Path -LiteralPath $niagaraAsset -PathType Leaf)

$candidateRenderers = @(
    [PSCustomObject]@{
        Rank = 2
        Option = "Keep CPU ISM fallback"
        RuntimeShape = "Use current `UInstancedStaticMeshComponent` preview with stride/max caps and batched `AddInstances`."
        Pros = @("Already compiles", "Simple editor fallback", "Works without plugin decisions")
        Risks = @("Still CPU/update-bound for very dense scans", "Full refresh can stall on very large frames")
        RecommendedDecision = "Keep as fallback even after a GPU renderer is added"
        FirstGpuSpikeCandidate = $false
        DecisionBlockers = @("Not enough for final dense interactive preview by itself")
    },
    [PSCustomObject]@{
        Rank = 1
        Option = "Niagara point renderer"
        RuntimeShape = "Feed downsampled or full point buffers into a Niagara system for point sprites."
        Pros = @("UE-native GPU visualization path", "Good fit for interactive editor preview", "Can support color/semantic attributes")
        Risks = @("Requires Niagara asset and data-interface design", "Needs screenshot/pixel smoke testing", "Packaging/settings ownership")
        RecommendedDecision = "Preferred first GPU renderer candidate"
        FirstGpuSpikeCandidate = $true
        DecisionBlockers = @("Niagara asset ownership", "Data interface/buffer design", "Desktop and viewport smoke evidence")
    },
    [PSCustomObject]@{
        Rank = 3
        Option = "Custom GPU buffer renderer"
        RuntimeShape = "Upload point data to render buffers and draw points with a custom component or render proxy."
        Pros = @("Highest control", "Best ceiling for dense scans", "Can avoid Niagara asset coupling")
        Risks = @("More engine-level code", "Higher maintenance", "Requires deeper render-thread testing")
        RecommendedDecision = "Use only if Niagara cannot meet density or attribute requirements"
        FirstGpuSpikeCandidate = $false
        DecisionBlockers = @("Render-thread implementation ownership", "Longer validation loop", "Packaged build risk")
    },
    [PSCustomObject]@{
        Rank = 4
        Option = "External viewer workflow"
        RuntimeShape = "Keep Unreal preview downsampled and inspect dense clouds through exported LAS/PCD/JSONL in a dedicated viewer."
        Pros = @("Lowest Unreal runtime risk", "Good for offline QA", "Avoids editor stalls")
        Risks = @("Not an in-editor dense view", "Extra workflow step", "Harder for operator dashboards")
        RecommendedDecision = "Use for archival/debug workflows, not as the main operator preview"
        FirstGpuSpikeCandidate = $false
        DecisionBlockers = @("Does not satisfy in-editor/operator dense preview requirement")
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

$gpuSpikeActionPlan = @(
    [PSCustomObject]@{
        Priority = 1
        Step = "Select Niagara point renderer spike"
        OwnerNeeded = "Rendering/Blueprint owner"
        Status = "Recommended"
        RequiredEvidence = @("Niagara asset ownership decision", "Data interface or buffer-feed approach", "Target point budget and frame-time budget")
        Blockers = @("No Niagara asset owner recorded", "No buffer/data-interface design selected")
        NextAction = "Create a short spike design note before adding assets or runtime code."
    },
    [PSCustomObject]@{
        Priority = 2
        Step = "Preserve CPU fallback"
        OwnerNeeded = "DT-Project runtime owner"
        Status = "Required"
        RequiredEvidence = @("CPU ISM fallback smoke remains available", "Preview stride/max policy still applies", "Server payload count remains independent from preview count")
        Blockers = @("GPU renderer must not replace the only preview path", "No regression evidence after GPU integration")
        NextAction = "Keep CPU preview code paths and add fallback-preservation checks when the GPU path lands."
    },
    [PSCustomObject]@{
        Priority = 3
        Step = "Collect viewport smoke evidence"
        OwnerNeeded = "QA/editor operator"
        Status = "Missing"
        RequiredEvidence = @("Desktop screenshot or viewport capture", "Nonblank point pixels", "Selected map and sensor id", "Point count and preview policy", "No overlap/clipping with monitor UI")
        Blockers = @("Requires Unreal Editor viewport run", "Requires visual/pixel evidence")
        NextAction = "After the Niagara spike is integrated, capture desktop and viewport smoke evidence before claiming dense preview readiness."
    },
    [PSCustomObject]@{
        Priority = 4
        Step = "Validate dense-frame behavior"
        OwnerNeeded = "Performance owner"
        Status = "Missing"
        RequiredEvidence = @("Dense frame point count", "Frame-time or load-time budget", "No editor stall observation", "CSV CPU fallback comparison")
        Blockers = @("No renderer-specific dense-frame telemetry", "No accepted performance budget")
        NextAction = "Run dense-frame evidence after the GPU path exists and compare it against the current CPU fallback reports."
    }
)

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$cpuIsmFallbackSmokePresent = $csvPreviewPerformanceEvidence.Present -and
    ($csvPreviewPerformanceEvidence.InstancedSmokeRenderMode -eq "InstancedMesh") -and
    ($csvPreviewPerformanceEvidence.InstancedSmokeAcceptedPoints -ge 128) -and
    $csvPreviewPerformanceEvidence.AutomationSuccessEvidencePresent -and
    $csvPreviewPerformanceEvidence.EvidenceLinesWithinRun -and
    (-not $csvPreviewPerformanceEvidence.FailureEvidencePresent)
$cpuProceduralDenseEvidencePresent = $csvPreviewPerformanceEvidence.Present -and
    ($csvPreviewPerformanceEvidence.ProceduralDenseRenderMode -eq "ProceduralMesh") -and
    ($csvPreviewPerformanceEvidence.ProceduralDenseAcceptedPoints -ge 120000) -and
    ($csvPreviewPerformanceEvidence.ProceduralBudgetRenderMode -eq "ProceduralMesh") -and
    ($csvPreviewPerformanceEvidence.ProceduralBudgetAcceptedPoints -ge 250000) -and
    $csvPreviewPerformanceEvidence.AutomationSuccessEvidencePresent -and
    $csvPreviewPerformanceEvidence.EvidenceLinesWithinRun -and
    (-not $csvPreviewPerformanceEvidence.FailureEvidencePresent)
$cpuPreviewFallbackEvidencePresent = $cpuIsmFallbackSmokePresent -and $cpuProceduralDenseEvidencePresent
$cpuFallbackPerformanceEvidencePresent = $csvPreviewPerformanceEvidence.Present -and
    $csvPreviewPerformanceEvidence.RequiredScenariosPresent -and
    $csvPreviewPerformanceEvidence.AutomationSuccessEvidencePresent -and
    $csvPreviewPerformanceEvidence.EvidenceLinesWithinRun -and
    (-not $csvPreviewPerformanceEvidence.FailureEvidencePresent) -and
    ($csvPreviewPerformanceEvidence.MaxAcceptedPoints -ge 250000)

function Get-MissingGpuViewportSmokeFields {
    $missing = @()
    if ([string]::IsNullOrWhiteSpace($ViewportScreenshotPath)) { $missing += "ViewportScreenshotPath" }
    elseif (-not (Test-Path -LiteralPath $ViewportScreenshotPath -PathType Leaf)) { $missing += "ViewportScreenshotPathExists" }
    if ($ViewportScreenshotBytes -le 0) { $missing += "ViewportScreenshotBytes" }
    if ($NonBlankPixelCount -le 0) { $missing += "NonBlankPixelCount" }
    if ($GpuSmokePointCount -le 0) { $missing += "GpuSmokePointCount" }
    if ([string]::IsNullOrWhiteSpace($GpuSmokeMapName)) { $missing += "GpuSmokeMapName" }
    if ([string]::IsNullOrWhiteSpace($GpuSmokeSensorId)) { $missing += "GpuSmokeSensorId" }
    if ([string]::IsNullOrWhiteSpace($GpuSmokeRendererName)) { $missing += "GpuSmokeRendererName" }
    if ([string]::IsNullOrWhiteSpace($GpuSmokeOperator)) { $missing += "GpuSmokeOperator" }
    if ([string]::IsNullOrWhiteSpace($GpuSmokeNotes)) { $missing += "GpuSmokeNotes" }
    if (-not $ObservedDenseFrameNoStall) { $missing += "ObservedDenseFrameNoStall" }
    if (-not $ObservedFallbackToggle) { $missing += "ObservedFallbackToggle" }
    return $missing
}

$missingGpuViewportSmokeFields = @(Get-MissingGpuViewportSmokeFields)
$gpuViewportSmokeEvidencePresent = ($missingGpuViewportSmokeFields.Count -eq 0)
$gpuFallbackPreservationEvidencePresent = ($ObservedFallbackToggle -and $cpuIsmFallbackSmokePresent)
$gpuDenseFrameEvidencePresent = ($ObservedDenseFrameNoStall -and $GpuSmokePointCount -ge 21600)
$rendererPhase = if (-not $gpuRendererIntegrated) {
    "PreGpuSpike"
}
elseif ($gpuViewportSmokeEvidencePresent -and $gpuFallbackPreservationEvidencePresent -and $gpuDenseFrameEvidencePresent) {
    "GpuEvidenceReady"
}
else {
    "GpuIntegratedEvidencePending"
}

$decisionGates = @(
    [PSCustomObject]@{
        Name = "CPU preview fallback evidence"
        Status = if ($cpuPreviewFallbackEvidencePresent) { "Ready" } else { "Missing" }
        Evidence = "Requires same-run CSV telemetry, automation success, no failure lines, ISM smoke, and procedural dense evidence."
    },
    [PSCustomObject]@{
        Name = "CPU ISM fallback smoke"
        Status = if ($cpuIsmFallbackSmokePresent) { "Ready" } else { "Missing" }
        Evidence = "Requires the InstancedBatchLoad scenario to use InstancedMesh with at least 128 accepted points."
    },
    [PSCustomObject]@{
        Name = "CPU procedural dense evidence"
        Status = if ($cpuProceduralDenseEvidencePresent) { "Ready" } else { "Missing" }
        Evidence = "Requires ProceduralMesh dense scenarios with at least 120000 and 250000 accepted points."
    },
    [PSCustomObject]@{
        Name = "First GPU spike selection"
        Status = "NiagaraRecommended"
        Evidence = "Niagara is ranked first because it is UE-native and lower risk than custom render-thread work."
    },
    [PSCustomObject]@{
        Name = "GPU implementation"
        Status = if ($gpuRendererIntegrated) { "Integrated" } else { "Open" }
        Evidence = "Requires Niagara or custom GPU renderer code/assets plus viewport smoke evidence."
    },
    [PSCustomObject]@{
        Name = "GPU viewport smoke evidence"
        Status = if ($gpuViewportSmokeEvidencePresent) { "Ready" } elseif ($gpuRendererIntegrated) { "Missing" } else { "RequiredAfterGpuIntegration" }
        Evidence = "Requires screenshot path, nonblank pixel evidence, map/sensor/renderer identity, operator notes, dense-frame no-stall observation, and fallback toggle observation."
    },
    [PSCustomObject]@{
        Name = "Fallback preservation"
        Status = if ($gpuFallbackPreservationEvidencePresent) { "Ready" } elseif ($gpuRendererIntegrated) { "Missing" } else { "RequiredAfterGpuIntegration" }
        Evidence = "CPU ISM fallback must remain available and be observed after any GPU renderer is added."
    },
    [PSCustomObject]@{
        Name = "GPU dense-frame evidence"
        Status = if ($gpuDenseFrameEvidencePresent) { "Ready" } elseif ($gpuRendererIntegrated) { "Missing" } else { "RequiredAfterGpuIntegration" }
        Evidence = "Requires a renderer-specific dense frame point count and no-stall observation."
    }
)
$gpuSpikeViewportSmokeRequired = [bool](@($gpuSpikeActionPlan | Where-Object { $_.Step -eq "Collect viewport smoke evidence" -and @($_.RequiredEvidence) -contains "Nonblank point pixels" }).Count -gt 0)
$gpuSpikeFallbackPreservationRequired = [bool](@($gpuSpikeActionPlan | Where-Object { $_.Step -eq "Preserve CPU fallback" -and @($_.RequiredEvidence) -contains "CPU ISM fallback smoke remains available" }).Count -gt 0)
$gpuSpikeDenseFrameEvidenceRequired = [bool](@($gpuSpikeActionPlan | Where-Object { $_.Step -eq "Validate dense-frame behavior" -and @($_.RequiredEvidence) -contains "Dense frame point count" }).Count -gt 0)
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
        NiagaraAsset = $niagaraAsset
    }
    CsvPreviewPerformanceEvidence = $csvPreviewPerformanceEvidence
    CandidateRenderers = $candidateRenderers
    GpuSpikeActionPlan = $gpuSpikeActionPlan
    GpuViewportSmokeEvidence = [PSCustomObject]@{
        ViewportScreenshotPath = $ViewportScreenshotPath
        ViewportScreenshotBytes = $ViewportScreenshotBytes
        NonBlankPixelCount = $NonBlankPixelCount
        PointCount = $GpuSmokePointCount
        MapName = $GpuSmokeMapName
        SensorId = $GpuSmokeSensorId
        RendererName = $GpuSmokeRendererName
        Operator = $GpuSmokeOperator
        Notes = $GpuSmokeNotes
        ObservedDenseFrameNoStall = [bool]$ObservedDenseFrameNoStall
        ObservedFallbackToggle = [bool]$ObservedFallbackToggle
        EvidencePresent = [bool]$gpuViewportSmokeEvidencePresent
        MissingEvidenceFields = $missingGpuViewportSmokeFields
    }
    AcceptanceEvidenceNeeded = $acceptanceEvidence
    DecisionGates = $decisionGates
    Summary = [PSCustomObject]@{
        ServerPreviewSplitDocumented = $serverPreviewSplitDocumented
        PreviewCapsDeclared = $previewCapsDeclared
        PointCloudOnlyClampDeclared = $pointCloudOnlyClampDeclared
        BatchedInstanceUploadDeclared = $batchedInstanceUploadDeclared
        DefaultPreviewBackend = "CpuInstancedMesh"
        ConfiguredPreviewBackendSource = "UVirtualLidarScanComponent::PreviewBackend"
        CandidatePreviewBackends = @("NiagaraGpuSprites", "CpuInstancedMeshFallback")
        PreviewBackendSelectionDeclared = $previewBackendSelectionDeclared
        GpuPreviewBackendClaimBlocked = $gpuPreviewBackendClaimBlocked
        CpuFallbackForcedForGpuCandidates = $cpuFallbackForcedForGpuCandidates
        CpuFallbackPreserved = $cpuFallbackForcedForGpuCandidates
        AutomationCoverageDeclared = $automationCoverageDeclared
        CsvPerformanceEvidencePresent = [bool]$csvPreviewPerformanceEvidence.Present
        CsvPreviewPerformanceAutomationEvidencePresent = [bool]$csvPreviewPerformanceEvidence.Present
        CsvPreviewPerformanceEvidenceMissingReason = if ($csvPreviewPerformanceEvidence.Present) { "" } else { [string]$csvPreviewPerformanceEvidence.Reason }
        CsvPreviewPerformanceRequiresAutomationLog = (-not [bool]$csvPreviewPerformanceEvidence.Present)
        ReadyToClaimCsvPreviewPerformance = [bool]$csvPreviewPerformanceEvidence.Present
        CsvPreviewPerformanceAcceptanceBlocked = (-not [bool]$csvPreviewPerformanceEvidence.Present)
        CsvPreviewPerformanceAcceptanceBlockers = if ($csvPreviewPerformanceEvidence.Present) { @() } else { @("CSV preview automation log evidence is missing or invalid.") }
        CpuFallbackPerformanceEvidencePresent = [bool]$cpuFallbackPerformanceEvidencePresent
        CpuPreviewFallbackEvidencePresent = [bool]$cpuPreviewFallbackEvidencePresent
        CpuIsmFallbackSmokePresent = [bool]$cpuIsmFallbackSmokePresent
        CpuProceduralDenseEvidencePresent = [bool]$cpuProceduralDenseEvidencePresent
        CsvPreviewMaxAcceptedPoints = if ($csvPreviewPerformanceEvidence.Present) { [int]$csvPreviewPerformanceEvidence.MaxAcceptedPoints } else { 0 }
        CsvFailureEvidencePresent = if ($csvPreviewPerformanceEvidence.Present) { [bool]$csvPreviewPerformanceEvidence.FailureEvidencePresent } else { $false }
        CsvFailureLineCount = if ($csvPreviewPerformanceEvidence.Present) { [int]$csvPreviewPerformanceEvidence.FailureLineCount } else { 0 }
        CsvEvidenceLinesWithinRun = if ($csvPreviewPerformanceEvidence.Present) { [bool]$csvPreviewPerformanceEvidence.EvidenceLinesWithinRun } else { $false }
        CsvEvidenceRunStartLine = if ($csvPreviewPerformanceEvidence.Present) { [int]$csvPreviewPerformanceEvidence.EvidenceRunStartLine } else { 0 }
        CsvEvidenceTestCompleteLine = if ($csvPreviewPerformanceEvidence.Present) { [int]$csvPreviewPerformanceEvidence.TestCompleteLine } else { 0 }
        GpuRendererIntegrated = $gpuRendererIntegrated
        RendererPhase = $rendererPhase
        GpuViewportSmokeEvidencePresent = [bool]$gpuViewportSmokeEvidencePresent
        GpuViewportSmokeMissingEvidenceFieldCount = [int]$missingGpuViewportSmokeFields.Count
        GpuViewportSmokeMissingEvidenceFields = $missingGpuViewportSmokeFields
        GpuFallbackPreservationEvidencePresent = [bool]$gpuFallbackPreservationEvidencePresent
        GpuDenseFrameEvidencePresent = [bool]$gpuDenseFrameEvidencePresent
        RendererDecisionMatrixDeclared = $true
        RecommendedFirstGpuCandidate = "Niagara point renderer"
        GpuSpikeActionPlanDeclared = $true
        GpuSpikeActionPlanItemCount = $gpuSpikeActionPlan.Count
        GpuSpikeViewportSmokeRequired = $gpuSpikeViewportSmokeRequired
        GpuSpikeFallbackPreservationRequired = $gpuSpikeFallbackPreservationRequired
        GpuSpikeDenseFrameEvidenceRequired = $gpuSpikeDenseFrameEvidenceRequired
        DecisionGateCount = $decisionGates.Count
        CandidateRendererCount = $candidateRenderers.Count
        AcceptanceEvidenceCount = $acceptanceEvidence.Count
        RecommendedNextDecision = "Use the integrated Niagara point renderer, preserve CPU ISM fallback, and collect desktop/viewport smoke plus 21,600-point FullSpec evidence."
        Valid = ($serverPreviewSplitDocumented -and $previewCapsDeclared -and $pointCloudOnlyClampDeclared -and $batchedInstanceUploadDeclared -and $previewBackendSelectionDeclared -and (-not $gpuPreviewBackendClaimBlocked) -and $cpuFallbackForcedForGpuCandidates -and $automationCoverageDeclared -and $gpuSpikeViewportSmokeRequired -and $gpuSpikeFallbackPreservationRequired -and $gpuSpikeDenseFrameEvidenceRequired -and (-not $gpuRendererIntegrated -or ($gpuViewportSmokeEvidencePresent -and $gpuFallbackPreservationEvidencePresent -and $gpuDenseFrameEvidencePresent)) -and (-not $RequireCsvPerformanceEvidence -or $cpuPreviewFallbackEvidencePresent))
    }
}

if (-not $report.Summary.Valid) {
    throw "Point cloud renderer decision report invariants failed. ServerPreviewSplit=$serverPreviewSplitDocumented PreviewCaps=$previewCapsDeclared PointCloudOnlyClamp=$pointCloudOnlyClampDeclared BatchedUpload=$batchedInstanceUploadDeclared PreviewBackend=$previewBackendSelectionDeclared GpuClaimBlocked=$gpuPreviewBackendClaimBlocked CpuFallbackForGpuCandidates=$cpuFallbackForcedForGpuCandidates Automation=$automationCoverageDeclared CpuPreviewFallbackEvidence=$cpuPreviewFallbackEvidencePresent GpuIntegrated=$gpuRendererIntegrated"
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
    Write-Host "CSV performance automation evidence present: $($report.Summary.CsvPreviewPerformanceAutomationEvidencePresent)"
    Write-Host "CSV performance evidence missing reason: $($report.Summary.CsvPreviewPerformanceEvidenceMissingReason)"
    Write-Host "Ready to claim CSV preview performance: $($report.Summary.ReadyToClaimCsvPreviewPerformance)"
    Write-Host "CPU preview fallback evidence present: $($report.Summary.CpuPreviewFallbackEvidencePresent)"
    Write-Host "CPU ISM fallback smoke present: $($report.Summary.CpuIsmFallbackSmokePresent)"
    Write-Host "CPU procedural dense evidence present: $($report.Summary.CpuProceduralDenseEvidencePresent)"
    Write-Host "Default preview backend: $($report.Summary.DefaultPreviewBackend)"
    Write-Host "Configured preview backend source: $($report.Summary.ConfiguredPreviewBackendSource)"
    Write-Host "Preview backend selection declared: $($report.Summary.PreviewBackendSelectionDeclared)"
    Write-Host "GPU preview backend claim blocked: $($report.Summary.GpuPreviewBackendClaimBlocked)"
    Write-Host "CPU fallback forced for GPU candidates: $($report.Summary.CpuFallbackForcedForGpuCandidates)"
    Write-Host "CPU fallback preserved: $($report.Summary.CpuFallbackPreserved)"
    Write-Host "CSV max accepted points: $($report.Summary.CsvPreviewMaxAcceptedPoints)"
    Write-Host "GPU renderer integrated: $($report.Summary.GpuRendererIntegrated)"
    Write-Host "Renderer phase: $($report.Summary.RendererPhase)"
    Write-Host "Recommended first GPU candidate: $($report.Summary.RecommendedFirstGpuCandidate)"
    Write-Host "GPU viewport smoke evidence present: $($report.Summary.GpuViewportSmokeEvidencePresent)"
    Write-Host "GPU viewport smoke missing evidence fields: $($report.Summary.GpuViewportSmokeMissingEvidenceFieldCount)"
    Write-Host "GPU fallback preservation evidence present: $($report.Summary.GpuFallbackPreservationEvidencePresent)"
    Write-Host "GPU dense-frame evidence present: $($report.Summary.GpuDenseFrameEvidencePresent)"
    Write-Host "GPU spike action-plan items: $($report.Summary.GpuSpikeActionPlanItemCount)"
    Write-Host "GPU spike viewport smoke required: $($report.Summary.GpuSpikeViewportSmokeRequired)"
    Write-Host "GPU spike fallback preservation required: $($report.Summary.GpuSpikeFallbackPreservationRequired)"
    Write-Host "GPU spike dense-frame evidence required: $($report.Summary.GpuSpikeDenseFrameEvidenceRequired)"
    Write-Host "Decision gates: $($report.Summary.DecisionGateCount)"
    Write-Host "Recommended next decision: $($report.Summary.RecommendedNextDecision)"
}
