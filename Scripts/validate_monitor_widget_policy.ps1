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
    [PSCustomObject]@{ Label = "Monitor widget header"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h" },
    [PSCustomObject]@{ Label = "Monitor widget implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp" },
    [PSCustomObject]@{ Label = "Monitor host header"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.h" },
    [PSCustomObject]@{ Label = "Monitor host implementation"; Path = "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.cpp" },
    [PSCustomObject]@{ Label = "Monitor automation tests"; Path = "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp" },
    [PSCustomObject]@{ Label = "Widget setup document"; Path = "docs\widget_designer_setup.md" },
    [PSCustomObject]@{ Label = "Local asset report document"; Path = "docs\local_asset_report.md" },
    [PSCustomObject]@{ Label = "Remaining work document"; Path = "docs\remaining_work.md" },
    [PSCustomObject]@{ Label = "Local project status report"; Path = "Scripts\report_local_project_status.ps1" },
    [PSCustomObject]@{ Label = "Monitor WBP decision report"; Path = "Scripts\export_monitor_wbp_decision_report.ps1" },
    [PSCustomObject]@{ Label = "Monitor WBP acceptance template"; Path = "Scripts\export_monitor_wbp_acceptance_template.ps1" },
    [PSCustomObject]@{ Label = "Monitor WBP preflight report"; Path = "Scripts\export_monitor_wbp_preflight_report.ps1" },
    [PSCustomObject]@{ Label = "Monitor WBP acceptance evidence validator"; Path = "Scripts\validate_monitor_wbp_acceptance_evidence.ps1" },
    [PSCustomObject]@{ Label = "Monitor WBP acceptance package exporter"; Path = "Scripts\export_monitor_wbp_acceptance_package.ps1" },
    [PSCustomObject]@{ Label = "Monitor WBP editor review prep"; Path = "Scripts\prepare_monitor_wbp_editor_review.ps1" }
)

foreach ($file in $requiredFiles) {
    Assert-FileExists -Path (Join-Path $ProjectRoot $file.Path) -Label $file.Label
}

$widgetHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h"
$widgetCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.cpp"
$hostHeader = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.h"
$hostCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorHostActor.cpp"
$testsCpp = Join-Path $ProjectRoot "Source\m7at10_dt\M7AT10\UI\Tests\VirtualSensorMonitorHostAutomationTests.cpp"
$setupDoc = Join-Path $ProjectRoot "docs\widget_designer_setup.md"
$localAssetDoc = Join-Path $ProjectRoot "docs\local_asset_report.md"
$remainingDoc = Join-Path $ProjectRoot "docs\remaining_work.md"
$assetReportScript = Join-Path $ProjectRoot "Scripts\report_local_project_status.ps1"
$monitorWbpReportScript = Join-Path $ProjectRoot "Scripts\export_monitor_wbp_decision_report.ps1"
$monitorWbpAcceptanceTemplateScript = Join-Path $ProjectRoot "Scripts\export_monitor_wbp_acceptance_template.ps1"
$monitorWbpPreflightScript = Join-Path $ProjectRoot "Scripts\export_monitor_wbp_preflight_report.ps1"
$monitorWbpEvidenceValidatorScript = Join-Path $ProjectRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"
$monitorWbpAcceptancePackageScript = Join-Path $ProjectRoot "Scripts\export_monitor_wbp_acceptance_package.ps1"
$monitorWbpEditorReviewPrepScript = Join-Path $ProjectRoot "Scripts\prepare_monitor_wbp_editor_review.ps1"

$requiredTexts = @(
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "BindWidgetOptional"; Label = "Widget uses optional bindings" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetMonitorTitleText"; Label = "Title text inspection API" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetMonitorStatusText"; Label = "Status text inspection API" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "FVirtualSensorMonitorDisplayData"; Label = "Designer display data struct" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetMonitorDisplayData"; Label = "Designer display data getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "LazExportText"; Label = "Designer LAZ export display row" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetSelectedSensorIdText"; Label = "Designer selected sensor getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetFrameSummaryText"; Label = "Designer frame summary getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetMeasurementSummaryText"; Label = "Designer measurement summary getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetServerPayloadSummaryText"; Label = "Designer server payload summary getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetPreviewPolicySummaryText"; Label = "Designer preview policy summary getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetSlabAnalysisSummaryText"; Label = "Designer slab analysis summary getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetLazExportSummaryText"; Label = "Designer LAZ export summary getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "IsShowingLidar"; Label = "Designer active view getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "HasBoundCamera"; Label = "Designer bound camera getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "HasBoundLidar"; Label = "Designer bound lidar getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetLastManualExportMessage"; Label = "Designer manual export message getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetTransportWarningText"; Label = "Designer warning summary getter" },
    [PSCustomObject]@{ Path = $widgetHeader; Pattern = "GetViewModeSummaryText"; Label = "Designer view mode summary getter" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "ShouldUseNativeFallbackWidget"; Label = "Native fallback detection" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "RebuildWidget"; Label = "Native fallback Slate builder" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "ExportSelectedSensorServerPayload"; Label = "Server payload export control" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "PreviewHitOnlyButton"; Label = "Preview hit-only control binding" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "RefreshLocalCaptureCameraPendingState"; Label = "Camera local capture pending-state helper" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "LocalCaptureCameraAsyncWriteCount"; Label = "Camera async write count is tracked" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "if (LockedData)"; Label = "GPU readback unlock is guarded" },
    [PSCustomObject]@{ Path = $widgetCpp; Pattern = "RowPitchInPixels < Pending.Width"; Label = "GPU readback row pitch is validated" },
    [PSCustomObject]@{ Path = $hostHeader; Pattern = "bUseNativeMonitorWidgetFallback"; Label = "Host fallback setting" },
    [PSCustomObject]@{ Path = $hostCpp; Pattern = "GetEffectiveMonitorWidgetClass"; Label = "Host resolves fallback widget class" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.SensorMonitor.HostNativeFallback"; Label = "Host fallback automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.SensorMonitor.LidarStatusTextContract"; Label = "LiDAR status automation test" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "lidar selected sensor getter exposes sensor id"; Label = "Automation covers Designer sensor getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "lidar display data exposes slab"; Label = "Automation covers Designer display data getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "lidar display data exposes laz export"; Label = "Automation covers Designer LAZ display data getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "monitor laz getter preserves true validation boundary"; Label = "Automation covers LAZ true validation boundary" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "monitor reports bound camera"; Label = "Automation covers bound camera getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "monitor reports bound lidar"; Label = "Automation covers bound lidar getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "server payload export message is populated"; Label = "Automation covers manual export message getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "lidar payload getter exposes server policy"; Label = "Automation covers Designer payload getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "lidar slab getter exposes slab analysis"; Label = "Automation covers Designer slab getter" },
    [PSCustomObject]@{ Path = $testsCpp; Pattern = "M7AT10.SensorMonitor.ServerPayloadExport"; Label = "Server payload export automation test" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "WBP_VirtualSensorMonitor"; Label = "Setup doc names production WBP" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "Optional Bindings"; Label = "Setup doc lists optional bindings" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "GetSlabAnalysisSummaryText"; Label = "Setup doc documents Designer getters" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "FVirtualSensorMonitorDisplayData"; Label = "Setup doc documents display data struct" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "native fallback"; Label = "Setup doc explains native fallback" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "binary Designer widget"; Label = "Local asset doc guards WBP binary" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Production Monitor WBP"; Label = "Remaining work tracks WBP decision" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Commit the WBP only after manual editor verification."; Label = "Remaining work requires manual WBP verification" },
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "WBP_VirtualSensorMonitor.uasset"; Label = "Asset report tracks local WBP path" },
    [PSCustomObject]@{ Path = $assetReportScript; Pattern = "Detected binary monitor WBP asset"; Label = "Asset report emits WBP decision note" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "RecommendedDecision"; Label = "WBP decision report emits recommendation" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "Evidence Draft"; Label = "WBP decision report emits evidence draft" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "Unreal Editor verification"; Label = "WBP decision report keeps manual editor verification required" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "SourceRepoRoot"; Label = "WBP decision report can use separate source repo root" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "report_local_project_status.ps1"; Label = "WBP decision report reuses local decision engine" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "validate_monitor_widget_policy.ps1"; Label = "WBP decision report reuses monitor policy validation" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "DecisionPoint"; Label = "WBP decision report exposes decision point" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "ManualAcceptanceChecklist"; Label = "WBP decision report exposes manual acceptance checklist" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "ReadyToStage"; Label = "WBP decision report exposes ready-to-stage status" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "ReviewQueue"; Label = "WBP decision report exposes review queue" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "CommitReadiness"; Label = "WBP decision report exposes commit readiness" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "MissingEvidenceCount"; Label = "WBP decision report counts missing evidence" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "EvidencePath"; Label = "WBP decision report accepts evidence path" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "FailOnIncompleteEvidence"; Label = "WBP decision report can fail on incomplete evidence" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "ManualEditorVerificationStillRequired"; Label = "WBP decision report exposes manual editor gate" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "AcceptanceTemplate"; Label = "WBP decision report exposes acceptance template summary" },
    [PSCustomObject]@{ Path = $monitorWbpReportScript; Pattern = "AcceptanceTemplateAvailable"; Label = "WBP decision report reports acceptance template availability" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "EvidenceRunId"; Label = "WBP acceptance template includes evidence run id" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "AssetHash"; Label = "WBP acceptance template records asset hash" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "OpenedInEditor"; Label = "WBP acceptance template records editor open evidence" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "CompiledWithoutErrors"; Label = "WBP acceptance template records compile result" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "OptionalBindings"; Label = "WBP acceptance template lists optional bindings" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "Get-MonitorOptionalBindingNames"; Label = "WBP acceptance template extracts optional bindings from C++" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "BoundToExpectedCppName"; Label = "WBP acceptance template records expected C++ binding names" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "MissingOptionalDoesNotCrash"; Label = "WBP acceptance template records optional binding crash check" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "DisplayData visual match"; Label = "WBP acceptance template records DisplayData visual match evidence" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "DisplayDataScreenMatchEvidence"; Label = "WBP acceptance template includes DisplayData manual section" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "GetMonitorDisplayData"; Label = "WBP acceptance template names DisplayData source function" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "bShowingLidar"; Label = "WBP acceptance template records DisplayData mode" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "SelectedSensorText"; Label = "WBP acceptance template includes selected sensor DisplayData row" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "LazExportText"; Label = "WBP acceptance template includes LAZ export DisplayData row" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "GetLazExportSummaryText"; Label = "WBP acceptance template documents LAZ export source getter" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "MatchesDisplayedText"; Label = "WBP acceptance template records visible text matching" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "PieSession"; Label = "WBP acceptance template records PIE session" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "HostActorOrLevelBlueprintBinding"; Label = "WBP acceptance template records monitor binding path" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "ExportedPayloadPath"; Label = "WBP acceptance template records exported payload evidence" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "IsShowingLidar/HasBoundCamera/HasBoundLidar"; Label = "WBP acceptance template records state helper smoke check" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "GetLastManualExportMessage is recorded"; Label = "WBP acceptance template records manual export message smoke check" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "Production WBP acceptance"; Label = "WBP acceptance template includes owner acceptance evidence" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "ManualAcceptanceSections"; Label = "WBP acceptance template exports manual acceptance sections" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "EditorOpenEvidence"; Label = "WBP acceptance template includes editor-open section" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "LidarStatusPanelEvidence"; Label = "WBP acceptance template includes LiDAR status panel section" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "ReadyToStageMonitorWbpAsset"; Label = "WBP acceptance template separates asset presence from stage readiness" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "MonitorWbpAssetStageAllowed"; Label = "WBP acceptance template exposes stage allowed flag" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "StagesWbp"; Label = "WBP acceptance template declares it does not stage WBP" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptanceTemplateScript; Pattern = "ModifiesAssets"; Label = "WBP acceptance template declares read-only asset behavior" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "ReadyForManualEditorReview"; Label = "WBP preflight reports manual editor readiness" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "DirectBinaryPatchSupported"; Label = "WBP preflight rejects direct binary patching" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "EditorMediatedAssetEditRequired"; Label = "WBP preflight requires editor-mediated asset edits" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "CodexCanModifyNativeBindings"; Label = "WBP preflight states Codex can modify native bindings" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "RequiresBackupBeforeAssetEdit"; Label = "WBP preflight requires backup before asset edit" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "BlockedPreflightCheckCount"; Label = "WBP preflight reports blocked checks" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "ModifiesAssets"; Label = "WBP preflight declares read-only asset behavior" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "StagesWbp"; Label = "WBP preflight declares no staging" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "not WBP acceptance"; Label = "WBP preflight preserves acceptance boundary" },
    [PSCustomObject]@{ Path = $monitorWbpPreflightScript; Pattern = "export_unused_content_archive_evidence.ps1"; Label = "WBP preflight considers post-archive evidence" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "ReadyToStageCandidate"; Label = "WBP evidence validator reports ready-to-stage candidate only" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "ManualAcceptanceSectionsPresent"; Label = "WBP evidence validator reports manual acceptance section presence" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "AcceptedManualAcceptanceSectionCount"; Label = "WBP evidence validator counts accepted manual acceptance sections" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "ReadyToStageMonitorWbpAsset"; Label = "WBP evidence validator reports monitor WBP stage readiness" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "MonitorWbpManualAcceptanceComplete"; Label = "WBP evidence validator reports manual acceptance completion" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "FailOnIncompleteEvidence"; Label = "WBP evidence validator supports incomplete evidence fail gate" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "Get-MonitorOptionalBindingNames"; Label = "WBP evidence validator extracts optional bindings from C++" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "Present optional bindings use expected C++ names"; Label = "WBP evidence validator checks expected C++ binding names" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "DisplayData visual match"; Label = "WBP evidence validator reads DisplayData visual match evidence" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "DisplayDataScreenMatchEvidence"; Label = "WBP evidence validator requires DisplayData manual section" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "LazExportText"; Label = "WBP evidence validator requires LAZ export DisplayData row" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "DisplayData screenshot or log path exists"; Label = "WBP evidence validator accepts DisplayData screenshot or log evidence" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "Required DisplayData rows match visible WBP text"; Label = "WBP evidence validator checks required DisplayData rows" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "Optional DisplayData rows match or are explicitly unavailable"; Label = "WBP evidence validator checks optional DisplayData rows" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "AssetHash"; Label = "WBP evidence validator checks asset hash" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "EditorLogPath"; Label = "WBP evidence validator checks editor log path" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "ScreenshotPath"; Label = "WBP evidence validator checks screenshot paths" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "ExportedPayloadPath"; Label = "WBP evidence validator checks exported payload path" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "DryRunOnly = `$true"; Label = "WBP evidence validator declares dry-run behavior" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "StagesWbp = `$false"; Label = "WBP evidence validator declares no staging" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "ModifiesAssets = `$false"; Label = "WBP evidence validator declares no asset modification" },
    [PSCustomObject]@{ Path = $monitorWbpEvidenceValidatorScript; Pattern = "never stages the binary asset"; Label = "WBP evidence validator preserves staging boundary" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "Saved\Reports\MonitorWbpAcceptance"; Label = "WBP acceptance package writes local Saved report bundle" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "monitor_wbp_acceptance.evidence.json"; Label = "WBP acceptance package creates focused evidence draft" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "validate_monitor_wbp_acceptance_evidence.ps1"; Label = "WBP acceptance package runs evidence validator" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "ReadyForManualEditorReview"; Label = "WBP acceptance package exposes manual editor readiness" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "WbpAcceptanceEvidenceComplete"; Label = "WBP acceptance package exposes evidence completeness" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "MonitorWbpAssetStageAllowed"; Label = "WBP acceptance package exposes stage allowed flag" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "ReadyToStageMonitorWbpAsset"; Label = "WBP acceptance package exposes WBP ready-to-stage gate" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "MonitorWbpManualAcceptanceComplete"; Label = "WBP acceptance package exposes manual acceptance completion" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "WbpDirectBinaryPatchSupported"; Label = "WBP acceptance package exposes direct patch boundary" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "EditorMediatedAssetEditRequired"; Label = "WBP acceptance package exposes editor-mediated edit requirement" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "CodexCanModifyNativeBindings"; Label = "WBP acceptance package exposes native binding edit path" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "StagesFiles = `$false"; Label = "WBP acceptance package declares no staging" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "ModifiesAssets = `$false"; Label = "WBP acceptance package declares no asset modification" },
    [PSCustomObject]@{ Path = $monitorWbpAcceptancePackageScript; Pattern = "never modifies assets, never stages files"; Label = "WBP acceptance package preserves manual acceptance boundary" },
    [PSCustomObject]@{ Path = $monitorWbpEditorReviewPrepScript; Pattern = "Saved\Backups\MonitorWbp"; Label = "WBP editor review prep writes backup under Saved" },
    [PSCustomObject]@{ Path = $monitorWbpEditorReviewPrepScript; Pattern = "PreEditAssetHash"; Label = "WBP editor review prep records pre-edit hash" },
    [PSCustomObject]@{ Path = $monitorWbpEditorReviewPrepScript; Pattern = "BackupHashMatchesPreEditHash"; Label = "WBP editor review prep verifies backup hash" },
    [PSCustomObject]@{ Path = $monitorWbpEditorReviewPrepScript; Pattern = "export_monitor_wbp_acceptance_package.ps1"; Label = "WBP editor review prep creates acceptance package" },
    [PSCustomObject]@{ Path = $monitorWbpEditorReviewPrepScript; Pattern = "ModifiesAssets = `$false"; Label = "WBP editor review prep declares no asset edits" },
    [PSCustomObject]@{ Path = $monitorWbpEditorReviewPrepScript; Pattern = "StagesWbp = `$false"; Label = "WBP editor review prep declares no WBP staging" },
    [PSCustomObject]@{ Path = $monitorWbpEditorReviewPrepScript; Pattern = "WritesSavedOnly"; Label = "WBP editor review prep is Saved-only" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "WbpDecisionSummary"; Label = "Pre-commit summary exports WBP decision summary" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "WbpPreflightSummary"; Label = "Pre-commit summary exports WBP preflight summary" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "WbpAcceptanceEvidenceSummary"; Label = "Pre-commit summary exports WBP acceptance evidence summary" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "MissingAcceptanceItems"; Label = "Pre-commit summary lists missing WBP acceptance items" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "UnexpectedWbpStaged"; Label = "Pre-commit summary flags unexpectedly staged WBP" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "MustRemainUntracked"; Label = "Pre-commit summary marks WBP that must remain untracked" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "StagingBlocked"; Label = "Pre-commit summary reports WBP staging block" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "ReadyToStage"; Label = "Pre-commit summary reports WBP ready-to-stage state" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "Monitor WBP stays untracked"; Label = "Pre-commit summary preserves WBP untracked boundary" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "AcceptanceTemplateAvailable"; Label = "Pre-commit summary reports WBP acceptance template availability" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "ReadyToStageMonitorWbpAsset"; Label = "Pre-commit summary reports WBP ready-to-stage gate" },
    [PSCustomObject]@{ Path = (Join-Path $ProjectRoot "Scripts\report_precommit_summary.ps1"); Pattern = "MonitorWbpManualAcceptanceComplete"; Label = "Pre-commit summary reports manual acceptance completion" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "export_monitor_wbp_decision_report.ps1"; Label = "Local asset doc documents WBP decision report" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "export_monitor_wbp_acceptance_template.ps1"; Label = "Local asset doc documents WBP acceptance template" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "export_monitor_wbp_acceptance_package.ps1"; Label = "Local asset doc documents WBP acceptance package" },
    [PSCustomObject]@{ Path = $localAssetDoc; Pattern = "prepare_monitor_wbp_editor_review.ps1"; Label = "Local asset doc documents WBP editor review prep" },
    [PSCustomObject]@{ Path = $setupDoc; Pattern = "prepare_monitor_wbp_editor_review.ps1"; Label = "Setup doc documents WBP editor review prep" },
    [PSCustomObject]@{ Path = $remainingDoc; Pattern = "Monitor WBP decision report"; Label = "Remaining work tracks WBP decision report" }
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
        OptionalBindingsDeclared = $true
        NativeFallbackDeclared = $true
        CameraCapturePendingStateGuarded = $true
        LocalWbpDecisionGuardDeclared = $true
        MonitorWbpDecisionReportUsesAssetDecisionEngine = $true
        MonitorWbpManualAcceptanceChecklistDeclared = $true
        MonitorWbpEvidencePathDeclared = $true
        MonitorWbpIncompleteEvidenceFailGateDeclared = $true
        MonitorWbpAcceptanceTemplateDeclared = $true
        MonitorWbpPreflightDeclared = $true
        MonitorWbpAcceptanceEvidenceValidatorDeclared = $true
        MonitorWbpAcceptancePackageDeclared = $true
        MonitorWbpManualAcceptanceSectionsDeclared = $true
        MonitorWbpStageGateDeclared = $true
        AutomationCoverageDeclared = $true
        ManualEditorVerificationStillRequired = $true
        Valid = $true
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 5
}
else {
    Write-Host "Monitor widget policy is internally consistent."
    Write-Host "Required files: $($report.Summary.RequiredFileCount)"
    Write-Host "Required contract checks: $($report.Summary.RequiredContractCount)"
    Write-Host "Optional bindings declared: $($report.Summary.OptionalBindingsDeclared)"
    Write-Host "Native fallback declared: $($report.Summary.NativeFallbackDeclared)"
    Write-Host "Camera capture pending state guarded: $($report.Summary.CameraCapturePendingStateGuarded)"
    Write-Host "Local WBP decision guard declared: $($report.Summary.LocalWbpDecisionGuardDeclared)"
    Write-Host "Monitor WBP decision report uses asset decision engine: $($report.Summary.MonitorWbpDecisionReportUsesAssetDecisionEngine)"
    Write-Host "Monitor WBP manual acceptance checklist declared: $($report.Summary.MonitorWbpManualAcceptanceChecklistDeclared)"
    Write-Host "Monitor WBP evidence path declared: $($report.Summary.MonitorWbpEvidencePathDeclared)"
    Write-Host "Monitor WBP incomplete evidence fail gate declared: $($report.Summary.MonitorWbpIncompleteEvidenceFailGateDeclared)"
    Write-Host "Monitor WBP acceptance template declared: $($report.Summary.MonitorWbpAcceptanceTemplateDeclared)"
    Write-Host "Monitor WBP preflight declared: $($report.Summary.MonitorWbpPreflightDeclared)"
    Write-Host "Monitor WBP acceptance evidence validator declared: $($report.Summary.MonitorWbpAcceptanceEvidenceValidatorDeclared)"
    Write-Host "Monitor WBP acceptance package declared: $($report.Summary.MonitorWbpAcceptancePackageDeclared)"
    Write-Host "Automation coverage declared: $($report.Summary.AutomationCoverageDeclared)"
    Write-Host "Manual editor verification still required: $($report.Summary.ManualEditorVerificationStillRequired)"
}
