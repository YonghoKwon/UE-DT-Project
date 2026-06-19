param(
    [string]$ProjectRoot = "",
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

function Assert-ContainsText {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet)) {
        throw "$Label missing required text '$Pattern' in $Path"
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$requiredFiles = @(
    [PSCustomObject]@{ Label = "LiDAR component header"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h" },
    [PSCustomObject]@{ Label = "LiDAR component implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp" },
    [PSCustomObject]@{ Label = "CSV point cloud preview actor header"; Path = "Source\m7at10_dt\M7AT10\Sensor\CsvPointCloudPreviewActor.h" },
    [PSCustomObject]@{ Label = "Sensor manager implementation"; Path = "Source\m7at10_dt\M7AT10\Sensor\VirtualSensorManager.cpp" },
    [PSCustomObject]@{ Label = "Monitor widget implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp" },
    [PSCustomObject]@{ Label = "Replay automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "Sensor manager automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\SensorManagerAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "CSV point cloud preview automation tests"; Path = "Source\m7at10_dt\M7AT10\Sensor\Tests\CsvPointCloudPreviewAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "Monitor automation tests"; Path = "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "LiDAR payload schema"; Path = "docs\lidar_payload_schema.md" },
    [PSCustomObject]@{ Label = "Editor smoke test document"; Path = "docs\editor_smoke_test.md" },
    [PSCustomObject]@{ Label = "Remaining work document"; Path = "docs\remaining_work.md" },
    [PSCustomObject]@{ Label = "Point cloud renderer acceptance package exporter"; Path = "Scripts\export_point_cloud_renderer_acceptance_package.ps1" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$lidarHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.h"
$lidarCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualLidarSensorComp.cpp"
$csvPreviewHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\CsvPointCloudPreviewActor.h"
$managerCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\VirtualSensorManager.cpp"
$monitorCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp"
$replayTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\LidarReplayAutomationTests.cpp"
$managerTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\SensorManagerAutomationTests.cpp"
$csvPreviewTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\Sensor\Tests\CsvPointCloudPreviewAutomationTests.cpp"
$monitorTests = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp"
$schemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$smokeDoc = Join-Path $ProjectRoot "docs\editor_smoke_test.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$rendererDecisionReportScript = Join-Path $ProjectRoot "Scripts\export_point_cloud_renderer_decision_report.ps1"
$rendererAcceptancePackageScript = Join-Path $ProjectRoot "Scripts\export_point_cloud_renderer_acceptance_package.ps1"
$csvPreviewPerformanceReportScript = Join-Path $ProjectRoot "Scripts\export_csv_preview_performance_report.ps1"
$csvPreviewEvidenceWorkflowScript = Join-Path $ProjectRoot "Scripts\run_csv_preview_performance_evidence.ps1"
$precommitSummaryScript = Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"

Assert-FileExists -Path $rendererDecisionReportScript -Label "Point cloud renderer decision report script"
Assert-FileExists -Path $rendererAcceptancePackageScript -Label "Point cloud renderer acceptance package script"
Assert-FileExists -Path $csvPreviewPerformanceReportScript -Label "CSV preview performance report script"
Assert-FileExists -Path $csvPreviewEvidenceWorkflowScript -Label "CSV preview performance evidence workflow script"
Assert-FileExists -Path $precommitSummaryScript -Label "Pre-commit summary script"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "ServerPayloadStride"; Label = "Server payload stride policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "MaxServerPayloadPoints"; Label = "Server payload max-point policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "PreviewPointStride"; Label = "Preview stride policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "MaxPreviewPoints"; Label = "Preview max-point policy" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "ELidarPointCloudPreviewBackend"; Label = "Preview backend selection enum" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "bAllowExperimentalGpuPreviewBackend"; Label = "Experimental GPU preview opt-in flag" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "GetPreviewBackendName"; Label = "Preview backend status API" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "IsGpuPreviewBackendActive"; Label = "GPU preview active status API" },
    [PSCustomObject]@{ Path = $lidarHeader; Pattern = "GetPerformanceWarning"; Label = "Performance warning API" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "Preview is uncapped"; Label = "Uncapped preview warning" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "FullSpec+MultiHit"; Label = "FullSpec multihit warning" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "FullSpec export-on-scan"; Label = "Export-on-scan warning" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "GPU preview backend is a candidate only; CPU fallback is active"; Label = "GPU preview candidate keeps CPU fallback warning" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "NiagaraCandidateCpuFallback"; Label = "Niagara candidate backend is labeled as CPU fallback" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "CustomGpuCandidateCpuFallback"; Label = "Custom GPU candidate backend is labeled as CPU fallback" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "cpu_instanced_mesh_fallback"; Label = "Payload marks active CPU fallback preview path" },
    [PSCustomObject]@{ Path = $lidarCpp; Pattern = "AddInstances(InstanceTransforms, false, true)"; Label = "Live preview uses batched ISM instance upload" },
    [PSCustomObject]@{ Path = $csvPreviewHeader; Pattern = "ClampMax = `"100000`""; Label = "Procedural CSV batch metadata allows high-density sections" },
    [PSCustomObject]@{ Path = $csvPreviewHeader; Pattern = "LastParseDurationMs"; Label = "CSV preview parse telemetry field" },
    [PSCustomObject]@{ Path = $csvPreviewHeader; Pattern = "LastBuildDurationMs"; Label = "CSV preview build telemetry field" },
    [PSCustomObject]@{ Path = $csvPreviewHeader; Pattern = "LastLoadDurationMs"; Label = "CSV preview total telemetry field" },
    [PSCustomObject]@{ Path = $csvPreviewHeader; Pattern = "GetLastPreviewTelemetryText"; Label = "CSV preview telemetry text accessor" },
    [PSCustomObject]@{ Path = $managerCpp; Pattern = "FMath::Min(LidarComp->MaxPreviewPoints, 3000)"; Label = "PointCloudOnly preview max clamp" },
    [PSCustomObject]@{ Path = $managerCpp; Pattern = "FMath::Max(2, LidarComp->PreviewPointStride)"; Label = "PointCloudOnly preview stride clamp" },
    [PSCustomObject]@{ Path = $monitorCpp; Pattern = "PreviewPoints"; Label = "Monitor exposes preview point count" },
    [PSCustomObject]@{ Path = $monitorCpp; Pattern = "Transport/Warning"; Label = "Monitor exposes warning row" },
    [PSCustomObject]@{ Path = $replayTests; Pattern = "M7AT10.SensorReplay.PerformanceWarningStatus"; Label = "Replay warning automation test" },
    [PSCustomObject]@{ Path = $managerTests; Pattern = "M7AT10.SensorManager.PointCloudOnlyPreservesPayloadPolicy"; Label = "PointCloudOnly policy automation test" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "M7AT10.Sensor.CsvPointCloudPreview.ProceduralHighDensityLoad"; Label = "CSV procedural high-density automation test" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "M7AT10.Sensor.CsvPointCloudPreview.InstancedBatchLoad"; Label = "CSV instanced batch automation test" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "M7AT10.Sensor.CsvPointCloudPreview.ProceduralPerformanceBudget"; Label = "CSV procedural performance budget automation test" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "120000"; Label = "CSV procedural automation uses high-density sample" },
    [PSCustomObject]@{ Path = $csvPreviewTests; Pattern = "250000"; Label = "CSV procedural performance automation uses denser sample" },
    [PSCustomObject]@{ Path = $monitorTests; Pattern = "M7AT10.SensorMonitor.PerformanceWarningStatusText"; Label = "Monitor warning automation test" },
    [PSCustomObject]@{ Path = $schemaDoc; Pattern = 'Server-side judgment should not use `previewPolicy` as measurement truth.'; Label = "Schema documents preview/server split" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "PreviewPointStride = 2"; Label = "Smoke doc preview stride recommendation" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "MaxPreviewPoints = 5000"; Label = "Smoke doc preview max recommendation" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Large Point Cloud Renderer"; Label = "Remaining work tracks high-density renderer" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_point_cloud_renderer_decision_report.ps1"; Label = "Remaining work documents renderer decision report" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "export_csv_preview_performance_report.ps1"; Label = "Remaining work documents CSV preview performance report" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "export_csv_preview_performance_report.ps1"; Label = "Smoke doc documents CSV preview performance report" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "GpuViewportSmokeEvidence"; Label = "Remaining work documents GPU viewport smoke evidence object" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "GpuIntegratedEvidencePending"; Label = "Remaining work documents GPU evidence pending phase" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "GpuEvidenceReady"; Label = "Remaining work documents GPU evidence ready phase" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "GPU/Niagara renderer smoke"; Label = "Smoke doc documents GPU renderer smoke checklist" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "NonBlankPixelCount"; Label = "Smoke doc documents nonblank pixel evidence" },
    [PSCustomObject]@{ Path = $smokeDoc; Pattern = "ObservedFallbackToggle"; Label = "Smoke doc documents fallback toggle evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "Niagara point renderer"; Label = "Renderer report includes Niagara path" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "Custom GPU buffer renderer"; Label = "Renderer report includes custom GPU path" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "External viewer workflow"; Label = "Renderer report includes external viewer path" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "Keep CPU ISM fallback"; Label = "Renderer report keeps CPU fallback" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "RecommendedFirstGpuCandidate"; Label = "Renderer report declares first GPU candidate" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "NiagaraRecommended"; Label = "Renderer report recommends Niagara for first GPU spike" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "RendererDecisionMatrixDeclared"; Label = "Renderer report declares decision matrix" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "DecisionGates"; Label = "Renderer report exports decision gates" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "FirstGpuSpikeCandidate"; Label = "Renderer report marks first GPU spike candidate" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "DecisionBlockers"; Label = "Renderer report tracks candidate blockers" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "GpuSpikeActionPlan"; Label = "Renderer report exports GPU spike action plan" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "GpuViewportSmokeEvidence"; Label = "Renderer report exports GPU viewport smoke evidence object" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "ViewportScreenshotPath"; Label = "Renderer report accepts viewport screenshot evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "NonBlankPixelCount"; Label = "Renderer report accepts nonblank pixel evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "RendererPhase"; Label = "Renderer report separates pre/post GPU phases" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "GpuIntegratedEvidencePending"; Label = "Renderer report represents GPU integrated but evidence pending phase" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "GpuEvidenceReady"; Label = "Renderer report represents GPU evidence ready phase" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "GpuViewportSmokeEvidencePresent"; Label = "Renderer report summarizes GPU viewport smoke evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "GpuFallbackPreservationEvidencePresent"; Label = "Renderer report summarizes fallback preservation evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "GpuDenseFrameEvidencePresent"; Label = "Renderer report summarizes dense-frame evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "ObservedFallbackToggle"; Label = "Renderer report requires fallback toggle observation" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "RequireCsvPerformanceEvidence"; Label = "Renderer report can require local CSV preview performance evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = '[string]$LogPath'; Label = "Renderer report accepts an explicit automation log path" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CpuFallbackPerformanceEvidencePresent"; Label = "Renderer report summarizes CPU fallback performance evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CpuPreviewFallbackEvidencePresent"; Label = "Renderer report separates total CPU preview fallback evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CpuIsmFallbackSmokePresent"; Label = "Renderer report separates ISM smoke evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CpuProceduralDenseEvidencePresent"; Label = "Renderer report separates procedural dense evidence" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "DefaultPreviewBackend"; Label = "Renderer report exposes default preview backend" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "ConfiguredPreviewBackendSource"; Label = "Renderer report exposes configured preview backend source" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CandidatePreviewBackends"; Label = "Renderer report exposes candidate preview backend names" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CpuFallbackPreserved"; Label = "Renderer report exposes CPU fallback preservation flag" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CsvFailureEvidencePresent"; Label = "Renderer report consumes CSV failure evidence gate" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "EvidenceRunStartLine"; Label = "Renderer report forwards CSV evidence run line number" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "TestCompleteLine"; Label = "Renderer report forwards automation completion line number" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "EvidenceLinesWithinRun"; Label = "Renderer report requires CSV evidence inside the selected run block" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CsvEvidenceLinesWithinRun"; Label = "Renderer report summary exposes same-run CSV evidence status" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CsvPreviewPerformanceAutomationEvidencePresent"; Label = "Renderer report separates CSV automation evidence from report generation" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CsvPreviewPerformanceEvidenceMissingReason"; Label = "Renderer report exposes CSV preview performance missing reason" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "ReadyToClaimCsvPreviewPerformance"; Label = "Renderer report exposes CSV preview performance acceptance gate" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "CsvPreviewPerformanceAcceptanceBlocked"; Label = "Renderer report exposes CSV preview performance blocking state" },
    [PSCustomObject]@{ Path = $rendererDecisionReportScript; Pattern = "MaxAcceptedPoints -ge 250000"; Label = "Renderer report treats 250k CSV evidence as dense fallback proof" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "Saved\Reports\PointCloudRendererAcceptance"; Label = "Renderer acceptance package writes the expected report folder" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "export_point_cloud_renderer_decision_report.ps1"; Label = "Renderer acceptance package includes decision report" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "validate_point_cloud_preview_policy.ps1"; Label = "Renderer acceptance package includes preview policy validation" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "export_csv_preview_performance_report.ps1"; Label = "Renderer acceptance package includes CSV performance evidence when available" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "DoesNotIntegrateGpuRenderer = `$true"; Label = "Renderer acceptance package does not claim GPU implementation" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "CreatesNiagaraAssets = `$false"; Label = "Renderer acceptance package does not create Niagara assets" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "RunsGpuViewportSmoke = `$false"; Label = "Renderer acceptance package does not run GPU viewport smoke" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "DoesNotModifyAssets = `$true"; Label = "Renderer acceptance package does not modify assets" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "StagesFiles = `$false"; Label = "Renderer acceptance package does not stage files" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "GpuViewportSmokeEvidencePresent"; Label = "Renderer acceptance package summarizes GPU viewport smoke evidence" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "ReadyToClaimGpuDensePreview"; Label = "Renderer acceptance package preserves GPU dense readiness gate" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "CsvPreviewPerformanceReportShellWritten"; Label = "Renderer acceptance package separates CSV report shell from evidence" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "CsvPreviewPerformanceAutomationEvidencePresent"; Label = "Renderer acceptance package exposes CSV automation evidence state" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "CsvPreviewPerformanceReportIsAcceptanceEvidence"; Label = "Renderer acceptance package says whether CSV report is acceptance evidence" },
    [PSCustomObject]@{ Path = $rendererAcceptancePackageScript; Pattern = "ReadyToClaimCsvPreviewPerformance"; Label = "Renderer acceptance package exposes CSV performance ready gate" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "ProceduralPerformanceBudget"; Label = "CSV preview performance report checks procedural budget scenario" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "250000"; Label = "CSV preview performance report checks dense sample size" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "Saved\Logs\m7at10_dt.log"; Label = "CSV preview performance report reads Unreal automation log" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "RequireAutomationSuccess"; Label = "CSV preview performance report can require automation success evidence" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "TEST COMPLETE\. EXIT CODE"; Label = "CSV preview performance report checks automation exit evidence" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "EvidenceRunStartLine"; Label = "CSV preview performance report records evidence run line number" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "TestCompleteLine"; Label = "CSV preview performance report records completion line number" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "EvidenceLinesWithinRun"; Label = "CSV preview performance report verifies evidence lines are inside the selected run block" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "FailureEvidencePresent"; Label = "CSV preview performance report detects failure evidence inside the selected run block" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "FailureLineCount"; Label = "CSV preview performance report counts failure evidence lines" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = "break"; Label = "CSV preview performance report stops at the selected run completion" },
    [PSCustomObject]@{ Path = $csvPreviewPerformanceReportScript; Pattern = 'Path=\{M7AT10\.Sensor\.CsvPointCloudPreview\.$scenario\}'; Label = "CSV preview performance report matches scenario-specific success lines" },
    [PSCustomObject]@{ Path = $csvPreviewEvidenceWorkflowScript; Pattern = "M7AT10.Sensor.CsvPointCloudPreview"; Label = "CSV evidence workflow runs the dedicated preview automation group" },
    [PSCustomObject]@{ Path = $csvPreviewEvidenceWorkflowScript; Pattern = '[string]$LogPath'; Label = "CSV evidence workflow accepts an explicit automation log path" },
    [PSCustomObject]@{ Path = $csvPreviewEvidenceWorkflowScript; Pattern = "RequireCsvPerformanceEvidence"; Label = "CSV evidence workflow requires renderer decision evidence" },
    [PSCustomObject]@{ Path = $csvPreviewEvidenceWorkflowScript; Pattern = "RequireAutomationSuccess"; Label = "CSV evidence workflow requires automation success evidence" },
    [PSCustomObject]@{ Path = $csvPreviewEvidenceWorkflowScript; Pattern = "CpuFallbackPerformanceEvidencePresent"; Label = "CSV evidence workflow verifies CPU fallback evidence" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "Get-PointCloudRendererDecisionSummary"; Label = "Pre-commit summary includes renderer decision summary helper" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "export_point_cloud_renderer_decision_report.ps1"; Label = "Pre-commit summary consumes renderer decision report" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "RendererPhase"; Label = "Pre-commit summary surfaces renderer phase" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "GpuViewportSmokeEvidencePresent"; Label = "Pre-commit summary surfaces GPU viewport smoke evidence" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "GpuFallbackPreservationEvidencePresent"; Label = "Pre-commit summary surfaces fallback preservation evidence" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "GpuDenseFrameEvidencePresent"; Label = "Pre-commit summary surfaces dense-frame evidence" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ReadyToClaimGpuDensePreview"; Label = "Pre-commit summary avoids claiming GPU dense preview too early" },
    [PSCustomObject]@{ Path = $precommitSummaryScript; Pattern = "ReadyToClaimCsvPreviewPerformance"; Label = "Pre-commit summary avoids claiming CSV preview performance too early" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "CsvPreviewPerformanceReportShellWritten"; Label = "Remaining work documents CSV report shell boundary" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "CsvPreviewPerformanceAutomationEvidencePresent"; Label = "Remaining work documents CSV automation evidence requirement" }
)

foreach ($item in $requiredTexts) {
    Assert-ContainsText -Path $item.Path -Pattern $item.Pattern -Label $item.Label
}

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    CheckedFiles = @($requiredFiles | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Path = Join-Path $ProjectRoot $_.Path
        }
    })
    CheckedContracts = @($requiredTexts | ForEach-Object {
        [PSCustomObject]@{
            Label = $_.Label
            Pattern = $_.Pattern
            Path = $_.Path
        }
    })
    Summary = [PSCustomObject]@{
        RequiredFileCount = $requiredFiles.Count
        RequiredContractCount = $requiredTexts.Count
        ServerPreviewSplitDocumented = $true
        RuntimeWarningsDeclared = $true
        PointCloudOnlyClampDeclared = $true
        BatchedInstanceUploadDeclared = $true
        RendererBackendSelectionDeclared = $true
        GpuPreviewBackendClaimBlocked = $true
        CpuFallbackForGpuCandidatesDeclared = $true
        RendererDecisionReportDeclared = $true
        RendererDecisionMatrixDeclared = $true
        RendererFirstGpuCandidateDeclared = $true
        RendererGpuSpikeActionPlanDeclared = $true
        RendererGpuViewportSmokeEvidenceDeclared = $true
        RendererPhaseGateDeclared = $true
        RendererGpuFallbackEvidenceDeclared = $true
        ProceduralCsvPreviewCoverageDeclared = $true
        ProceduralCsvPreviewTelemetryDeclared = $true
        CsvPreviewPerformanceReportDeclared = $true
        RendererDecisionConsumesCsvPerformanceEvidence = $true
        RendererDecisionSplitsCpuEvidence = $true
        RendererDecisionBackendSourceDeclared = $true
        RendererDecisionCpuFallbackPreservedDeclared = $true
        CsvPreviewPerformanceEvidenceWorkflowDeclared = $true
        CsvPreviewEvidenceLineTrackingDeclared = $true
        CsvPreviewFailureEvidenceGateDeclared = $true
        RendererDecisionExplicitLogPathDeclared = $true
        RendererAcceptancePackageDeclared = $true
        RendererAcceptancePackageNoAssetNoGpuSmokeBoundaryDeclared = $true
        PrecommitRendererSummaryDeclared = $true
        AutomationCoverageDeclared = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Point cloud preview policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Server/preview split documented: $($report.Summary.ServerPreviewSplitDocumented)"
    Write-Host "Runtime warnings declared: $($report.Summary.RuntimeWarningsDeclared)"
    Write-Host "PointCloudOnly clamps declared: $($report.Summary.PointCloudOnlyClampDeclared)"
    Write-Host "Batched instance upload declared: $($report.Summary.BatchedInstanceUploadDeclared)"
    Write-Host "Renderer backend selection declared: $($report.Summary.RendererBackendSelectionDeclared)"
    Write-Host "GPU preview backend claim blocked: $($report.Summary.GpuPreviewBackendClaimBlocked)"
    Write-Host "CPU fallback for GPU candidates declared: $($report.Summary.CpuFallbackForGpuCandidatesDeclared)"
    Write-Host "Renderer decision report declared: $($report.Summary.RendererDecisionReportDeclared)"
    Write-Host "Renderer decision matrix declared: $($report.Summary.RendererDecisionMatrixDeclared)"
    Write-Host "Renderer first GPU candidate declared: $($report.Summary.RendererFirstGpuCandidateDeclared)"
    Write-Host "Renderer GPU spike action plan declared: $($report.Summary.RendererGpuSpikeActionPlanDeclared)"
    Write-Host "Renderer GPU viewport smoke evidence declared: $($report.Summary.RendererGpuViewportSmokeEvidenceDeclared)"
    Write-Host "Renderer phase gate declared: $($report.Summary.RendererPhaseGateDeclared)"
    Write-Host "Renderer GPU fallback evidence declared: $($report.Summary.RendererGpuFallbackEvidenceDeclared)"
    Write-Host "Procedural CSV preview coverage declared: $($report.Summary.ProceduralCsvPreviewCoverageDeclared)"
    Write-Host "Procedural CSV preview telemetry declared: $($report.Summary.ProceduralCsvPreviewTelemetryDeclared)"
    Write-Host "CSV preview performance report declared: $($report.Summary.CsvPreviewPerformanceReportDeclared)"
    Write-Host "Renderer decision consumes CSV performance evidence: $($report.Summary.RendererDecisionConsumesCsvPerformanceEvidence)"
    Write-Host "Renderer decision splits CPU evidence: $($report.Summary.RendererDecisionSplitsCpuEvidence)"
    Write-Host "Renderer decision backend source declared: $($report.Summary.RendererDecisionBackendSourceDeclared)"
    Write-Host "Renderer decision CPU fallback preserved declared: $($report.Summary.RendererDecisionCpuFallbackPreservedDeclared)"
    Write-Host "CSV preview performance evidence workflow declared: $($report.Summary.CsvPreviewPerformanceEvidenceWorkflowDeclared)"
    Write-Host "CSV preview evidence line tracking declared: $($report.Summary.CsvPreviewEvidenceLineTrackingDeclared)"
    Write-Host "CSV preview failure evidence gate declared: $($report.Summary.CsvPreviewFailureEvidenceGateDeclared)"
    Write-Host "Renderer decision explicit log path declared: $($report.Summary.RendererDecisionExplicitLogPathDeclared)"
    Write-Host "Renderer acceptance package declared: $($report.Summary.RendererAcceptancePackageDeclared)"
    Write-Host "Renderer acceptance package no-asset/no-GPU-smoke boundary declared: $($report.Summary.RendererAcceptancePackageNoAssetNoGpuSmokeBoundaryDeclared)"
    Write-Host "Pre-commit renderer summary declared: $($report.Summary.PrecommitRendererSummaryDeclared)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
}
