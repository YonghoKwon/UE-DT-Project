param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$SourceRepoRoot = "",
    [string]$EvidencePath = "",
    [switch]$FailOnIncompleteEvidence,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Invoke-JsonScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "Required script not found: $ScriptPath"
    }

    $output = @(& powershell -ExecutionPolicy Bypass -File $ScriptPath @Parameters -Json)
    if ($LASTEXITCODE -ne 0) {
        throw "$ScriptPath failed with exit code ${LASTEXITCODE}: $($output -join ' ')"
    }
    return ($output -join "`n") | ConvertFrom-Json
}

function Test-NonEmptyString {
    param([object]$Value)
    return -not [string]::IsNullOrWhiteSpace([string]$Value)
}

function Test-CompletedStatus {
    param([object]$Value)

    $status = [string]$Value
    return @("Complete", "Recorded", "Passed", "Accepted") -contains $status
}

function Normalize-ProjectPath {
    param([object]$Value)

    return ([string]$Value).Replace("/", "\").TrimStart(".\")
}

function Resolve-EvidencePath {
    param(
        [object]$Value,
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    if (-not (Test-NonEmptyString $Value)) {
        return ""
    }

    $pathText = [string]$Value
    if ([System.IO.Path]::IsPathRooted($pathText)) {
        return $pathText
    }

    $projectCandidate = Join-Path $ProjectRoot $pathText
    if (Test-Path -LiteralPath $projectCandidate -PathType Leaf) {
        return $projectCandidate
    }
    return (Join-Path $SourceRepoRoot $pathText)
}

function Test-EvidenceFilePath {
    param(
        [object]$Value,
        [string]$ProjectRoot,
        [string]$SourceRepoRoot
    )

    $candidate = Resolve-EvidencePath -Value $Value -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot
    return ((Test-NonEmptyString $candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf))
}

function Get-EvidenceItem {
    param(
        [object[]]$Items,
        [string]$Name
    )

    return @($Items | Where-Object { [string]$_.Name -eq $Name } | Select-Object -First 1)
}

function Add-Check {
    param(
        [System.Collections.Generic.List[object]]$Checks,
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $Checks.Add([PSCustomObject]@{
        Name = $Name
        Passed = $Passed
        Detail = $Detail
    }) | Out-Null
}

function Get-MonitorOptionalBindingNames {
    param([string]$SourceRepoRoot)

    $headerPath = Join-Path $SourceRepoRoot "Source\m7at10_dt\M7AT10\UI\VirtualSensorMonitorWidget.h"
    if (-not (Test-Path -LiteralPath $headerPath -PathType Leaf)) {
        throw "VirtualSensorMonitorWidget.h not found: $headerPath"
    }

    $lines = Get-Content -LiteralPath $headerPath
    $names = [System.Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -notmatch "BindWidgetOptional") {
            continue
        }

        for ($next = $index + 1; $next -lt $lines.Count; $next++) {
            $line = $lines[$next].Trim()
            if ($line -match "^TObjectPtr<[^>]+>\s+([A-Za-z_][A-Za-z0-9_]*)\s*;") {
                $names.Add($Matches[1]) | Out-Null
                break
            }
            if ($line -match "^UPROPERTY") {
                break
            }
        }
    }

    return @($names | Select-Object -Unique)
}

if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "ProjectRoot not found: $ProjectRoot"
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($SourceRepoRoot)) {
    $SourceRepoRoot = Split-Path -Parent $PSScriptRoot
}
if (-not (Test-Path -LiteralPath $SourceRepoRoot -PathType Container)) {
    throw "SourceRepoRoot not found: $SourceRepoRoot"
}
$SourceRepoRoot = (Resolve-Path -LiteralPath $SourceRepoRoot).Path

if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $SourceRepoRoot "docs\local_asset_decisions.evidence.json"
}

$wbpRelativePath = "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset"
$wbpPath = Join-Path $ProjectRoot $wbpRelativePath
$preflightScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_preflight_report.ps1"
$decisionScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_decision_report.ps1"

$preflight = Invoke-JsonScript -ScriptPath $preflightScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
}
$decision = Invoke-JsonScript -ScriptPath $decisionScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
}

$evidenceFilePresent = Test-Path -LiteralPath $EvidencePath -PathType Leaf
$rawEvidence = $null
if ($evidenceFilePresent) {
    $rawEvidence = Get-Content -LiteralPath $EvidencePath -Raw | ConvertFrom-Json
}

$evidenceRecord = $null
if ($rawEvidence) {
    if ($rawEvidence.PSObject.Properties.Name -contains "Decisions") {
        $evidenceRecord = @($rawEvidence.Decisions | Where-Object { (Normalize-ProjectPath $_.Path) -eq $wbpRelativePath } | Select-Object -First 1)
    }
    elseif ($rawEvidence.PSObject.Properties.Name -contains "Path" -and (Normalize-ProjectPath $rawEvidence.Path) -eq $wbpRelativePath) {
        $evidenceRecord = $rawEvidence
    }
    elseif ($rawEvidence.PSObject.Properties.Name -contains "LocalAssetDecisionEvidenceDraft" -and (Normalize-ProjectPath $rawEvidence.LocalAssetDecisionEvidenceDraft.Path) -eq $wbpRelativePath) {
        $evidenceRecord = $rawEvidence.LocalAssetDecisionEvidenceDraft
    }
}

$templateEvidenceItems = @()
if ($rawEvidence -and $rawEvidence.PSObject.Properties.Name -contains "RequiredEvidence") {
    $templateEvidenceItems = @($rawEvidence.RequiredEvidence)
}
$manualAcceptanceSections = if ($rawEvidence -and $rawEvidence.PSObject.Properties.Name -contains "ManualAcceptanceSections") { $rawEvidence.ManualAcceptanceSections } else { $null }
$decisionEvidenceItems = if ($evidenceRecord -and $evidenceRecord.PSObject.Properties.Name -contains "Evidence") { @($evidenceRecord.Evidence) } else { @() }

$requiredManualAcceptanceSectionNames = @(
    "EditorOpenEvidence",
    "WidgetBindingEvidence",
    "PieSmokeEvidence",
    "SensorSelectionEvidence",
    "LidarStatusPanelEvidence",
    "SlabAnalysisPanelEvidence",
    "DisplayDataScreenMatchEvidence",
    "NoCrashEvidence",
    "OwnerAcceptance"
)
$requiredNames = @(
    "Editor open verification",
    "Optional binding check",
    "PIE smoke result",
    "DisplayData visual match",
    "Production WBP acceptance"
)
$requiredDisplayDataRows = @(
    "TitleText",
    "SelectedSensorText",
    "FrameText",
    "MeasurementText",
    "ServerPayloadText",
    "PreviewText",
    "SlabText",
    "LazExportText",
    "WarningText",
    "ViewModeText"
)
$requiredOptionalBindings = @(Get-MonitorOptionalBindingNames -SourceRepoRoot $SourceRepoRoot)
$acceptedPieStatuses = @("Passed", "Accepted", "UnavailableAccepted", "ExplicitlyUnavailable")

$checks = [System.Collections.Generic.List[object]]::new()
Add-Check -Checks $checks -Name "Evidence file present" -Passed $evidenceFilePresent -Detail $EvidencePath
Add-Check -Checks $checks -Name "WBP asset present" -Passed (Test-Path -LiteralPath $wbpPath -PathType Leaf) -Detail $wbpPath
Add-Check -Checks $checks -Name "Evidence record exists" -Passed ($null -ne $evidenceRecord) -Detail $wbpRelativePath
Add-Check -Checks $checks -Name "Asset hash algorithm is SHA256" -Passed ($evidenceFilePresent -and (
        [string]$rawEvidence.AssetHashAlgorithm -eq "SHA256" -or
        [string]$rawEvidence.LocalAssetDecisionEvidenceDraft.AssetHashAlgorithm -eq "SHA256"
    )) -Detail "AssetHashAlgorithm"
Add-Check -Checks $checks -Name "WBP asset hash matches" -Passed ($evidenceFilePresent -and (Test-NonEmptyString $preflight.Summary.AssetHash) -and (
        [string]$rawEvidence.AssetHash -eq [string]$preflight.Summary.AssetHash -or
        [string]$rawEvidence.LocalAssetDecisionEvidenceDraft.AssetHash -eq [string]$preflight.Summary.AssetHash
    )) -Detail "CurrentHash=$($preflight.Summary.AssetHash)"
Add-Check -Checks $checks -Name "Decision accepted for repository" -Passed ($evidenceRecord -and [string]$evidenceRecord.DecisionStatus -eq "AcceptedForRepository") -Detail $(if ($evidenceRecord) { [string]$evidenceRecord.DecisionStatus } else { "" })
Add-Check -Checks $checks -Name "Accepted metadata complete" -Passed ($evidenceRecord -and (Test-NonEmptyString $evidenceRecord.AcceptedBy) -and (Test-NonEmptyString $evidenceRecord.AcceptedAt) -and (Test-NonEmptyString $evidenceRecord.EvidenceSource)) -Detail "AcceptedBy/AcceptedAt/EvidenceSource"
Add-Check -Checks $checks -Name "Manual acceptance sections present" -Passed ($null -ne $manualAcceptanceSections) -Detail "ManualAcceptanceSections"

$acceptedManualAcceptanceSectionCount = 0
if ($manualAcceptanceSections) {
    foreach ($sectionName in $requiredManualAcceptanceSectionNames) {
        $sectionPresent = ($manualAcceptanceSections.PSObject.Properties.Name -contains $sectionName)
        Add-Check -Checks $checks -Name "$sectionName section present" -Passed $sectionPresent -Detail $sectionName
        if ($sectionPresent) {
            $section = $manualAcceptanceSections.$sectionName
            $accepted = ([bool]$section.Present -and [bool]$section.Accepted -and (Test-NonEmptyString $section.EvidencePath) -and (Test-EvidenceFilePath -Value $section.EvidencePath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot))
            if ($accepted) {
                $acceptedManualAcceptanceSectionCount++
            }
            Add-Check -Checks $checks -Name "$sectionName section accepted with evidence file" -Passed $accepted -Detail "Present=$($section.Present) Accepted=$($section.Accepted) EvidencePath=$($section.EvidencePath)"
        }
    }
}

foreach ($requiredName in $requiredNames) {
    $decisionItem = Get-EvidenceItem -Items $decisionEvidenceItems -Name $requiredName
    $templateItem = Get-EvidenceItem -Items $templateEvidenceItems -Name $requiredName
    $decisionComplete = ($decisionItem.Count -gt 0 -and (Test-CompletedStatus $decisionItem[0].Status) -and (Test-NonEmptyString $decisionItem[0].Source))
    $templateComplete = ($templateItem.Count -gt 0 -and (Test-CompletedStatus $templateItem[0].Status))
    Add-Check -Checks $checks -Name "$requiredName complete" -Passed ($decisionComplete -or $templateComplete) -Detail "DecisionEvidence=$decisionComplete TemplateEvidence=$templateComplete"
}

foreach ($templateItem in $templateEvidenceItems) {
    Add-Check -Checks $checks -Name "$($templateItem.Name) common evidence metadata" -Passed (
        (Test-NonEmptyString $templateItem.EvidenceRunId) -and
        ((Test-NonEmptyString $templateItem.Operator) -or (Test-NonEmptyString $templateItem.AcceptedBy)) -and
        ((Test-NonEmptyString $templateItem.VerifiedAt) -or (Test-NonEmptyString $templateItem.AcceptedAt)) -and
        (Test-NonEmptyString $templateItem.Notes)
    ) -Detail "EvidenceRunId plus operator/accepted-by plus verified/accepted-at plus notes"
}

$editorEvidence = Get-EvidenceItem -Items $templateEvidenceItems -Name "Editor open verification"
if ($editorEvidence.Count -gt 0) {
    $editor = $editorEvidence[0]
    Add-Check -Checks $checks -Name "Editor log or screenshot path exists" -Passed (
        (Test-EvidenceFilePath -Value $editor.EditorLogPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot) -or
        (Test-EvidenceFilePath -Value $editor.ScreenshotPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot)
    ) -Detail "EditorLogPath=$($editor.EditorLogPath) ScreenshotPath=$($editor.ScreenshotPath)"
    Add-Check -Checks $checks -Name "Editor opened and compiled" -Passed ([bool]$editor.OpenedInEditor -and [bool]$editor.CompiledWithoutErrors) -Detail "OpenedInEditor=$($editor.OpenedInEditor) CompiledWithoutErrors=$($editor.CompiledWithoutErrors)"
}

$optionalEvidence = Get-EvidenceItem -Items $templateEvidenceItems -Name "Optional binding check"
if ($optionalEvidence.Count -gt 0) {
    $optional = $optionalEvidence[0]
    $observedNames = @($optional.OptionalBindings | ForEach-Object { [string]$_.Name })
    $missingNames = @($requiredOptionalBindings | Where-Object { $observedNames -notcontains $_ })
    $unexpectedRequiredFailures = @($optional.UnexpectedRequiredBindingFailures | Where-Object { Test-NonEmptyString $_ })
    $unsafeMissingOptional = @($optional.OptionalBindings | Where-Object { -not [bool]$_.Present -and -not [bool]$_.MissingOptionalDoesNotCrash })
    $wrongCppNameBindings = @($optional.OptionalBindings | Where-Object { [bool]$_.Present -and ($_.PSObject.Properties.Name -contains "BoundToExpectedCppName") -and -not [bool]$_.BoundToExpectedCppName })
    Add-Check -Checks $checks -Name "Optional binding list complete" -Passed ($missingNames.Count -eq 0) -Detail "MissingNames=$($missingNames -join ', ')"
    Add-Check -Checks $checks -Name "No unexpected required binding failures" -Passed ($unexpectedRequiredFailures.Count -eq 0) -Detail "UnexpectedRequiredBindingFailures=$($unexpectedRequiredFailures.Count)"
    Add-Check -Checks $checks -Name "Missing optional bindings are crash-safe" -Passed ($unsafeMissingOptional.Count -eq 0) -Detail "UnsafeMissingOptional=$(@($unsafeMissingOptional | Select-Object -ExpandProperty Name) -join ', ')"
    Add-Check -Checks $checks -Name "Present optional bindings use expected C++ names" -Passed ($wrongCppNameBindings.Count -eq 0) -Detail "WrongCppNameBindings=$(@($wrongCppNameBindings | Select-Object -ExpandProperty Name) -join ', ')"
}

$pieEvidence = Get-EvidenceItem -Items $templateEvidenceItems -Name "PIE smoke result"
if ($pieEvidence.Count -gt 0) {
    $pie = $pieEvidence[0]
    $pieChecks = @($pie.Checks)
    $badPieChecks = @($pieChecks | Where-Object { $acceptedPieStatuses -notcontains [string]$_.Status })
    Add-Check -Checks $checks -Name "PIE metadata complete" -Passed (
        (Test-NonEmptyString $pie.MapName) -and
        (Test-NonEmptyString $pie.PieSession) -and
        (Test-NonEmptyString $pie.HostActorOrLevelBlueprintBinding) -and
        (Test-NonEmptyString $pie.SensorManagerBinding) -and
        (Test-NonEmptyString $pie.LogPath)
    ) -Detail "MapName/PieSession/HostActorOrLevelBlueprintBinding/SensorManagerBinding/LogPath"
    Add-Check -Checks $checks -Name "PIE log path exists" -Passed (Test-EvidenceFilePath -Value $pie.LogPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot) -Detail [string]$pie.LogPath
    Add-Check -Checks $checks -Name "PIE optional screenshot or exported payload path exists" -Passed (
        (Test-EvidenceFilePath -Value $pie.ScreenshotPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot) -or
        (Test-EvidenceFilePath -Value $pie.ExportedPayloadPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot)
    ) -Detail "ScreenshotPath=$($pie.ScreenshotPath) ExportedPayloadPath=$($pie.ExportedPayloadPath)"
    Add-Check -Checks $checks -Name "PIE checks are passed or explicitly accepted unavailable" -Passed ($pieChecks.Count -gt 0 -and $badPieChecks.Count -eq 0) -Detail "BadPieChecks=$($badPieChecks.Count)"
}

$displayEvidence = Get-EvidenceItem -Items $templateEvidenceItems -Name "DisplayData visual match"
if ($displayEvidence.Count -gt 0) {
    $display = $displayEvidence[0]
    $displayRows = @($display.DisplayRows)
    $observedDisplayNames = @($displayRows | ForEach-Object { [string]$_.FieldName })
    $missingDisplayRows = @($requiredDisplayDataRows | Where-Object { $observedDisplayNames -notcontains $_ })
    $badRequiredRows = @($displayRows | Where-Object {
            [bool]$_.Required -and (
                -not (Test-NonEmptyString $_.ObservedValue) -or
                -not (Test-NonEmptyString $_.TextBlockName) -or
                -not [bool]$_.VisibleInWbp -or
                -not [bool]$_.MatchesDisplayedText
            )
        })
    $badOptionalRows = @($displayRows | Where-Object {
            -not [bool]$_.Required -and
            -not [bool]$_.ExplicitlyUnavailable -and (
                -not (Test-NonEmptyString $_.ObservedValue) -or
                -not (Test-NonEmptyString $_.TextBlockName) -or
                -not [bool]$_.VisibleInWbp -or
                -not [bool]$_.MatchesDisplayedText
            )
        })

    Add-Check -Checks $checks -Name "DisplayData evidence metadata complete" -Passed (
        (Test-NonEmptyString $display.MapName) -and
        (Test-NonEmptyString $display.PieSession) -and
        (Test-NonEmptyString $display.SourceFunction) -and
        [string]$display.SourceFunction -eq "GetMonitorDisplayData" -and
        ($display.PSObject.Properties.Name -contains "bShowingLidar") -and
        (Test-NonEmptyString $display.SensorMode)
    ) -Detail "MapName/PieSession/SourceFunction/bShowingLidar/SensorMode"
    Add-Check -Checks $checks -Name "DisplayData screenshot or log path exists" -Passed (
        (Test-EvidenceFilePath -Value $display.ScreenshotPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot) -or
        (Test-EvidenceFilePath -Value $display.EditorLogPath -ProjectRoot $ProjectRoot -SourceRepoRoot $SourceRepoRoot)
    ) -Detail "ScreenshotPath=$($display.ScreenshotPath) EditorLogPath=$($display.EditorLogPath)"
    Add-Check -Checks $checks -Name "DisplayData row list complete" -Passed ($missingDisplayRows.Count -eq 0) -Detail "MissingRows=$($missingDisplayRows -join ', ')"
    Add-Check -Checks $checks -Name "Required DisplayData rows match visible WBP text" -Passed ($badRequiredRows.Count -eq 0) -Detail "BadRequiredRows=$(@($badRequiredRows | Select-Object -ExpandProperty FieldName) -join ', ')"
    Add-Check -Checks $checks -Name "Optional DisplayData rows match or are explicitly unavailable" -Passed ($badOptionalRows.Count -eq 0) -Detail "BadOptionalRows=$(@($badOptionalRows | Select-Object -ExpandProperty FieldName) -join ', ')"
}

$productionEvidence = Get-EvidenceItem -Items $templateEvidenceItems -Name "Production WBP acceptance"
if ($productionEvidence.Count -gt 0) {
    $production = $productionEvidence[0]
    Add-Check -Checks $checks -Name "Production acceptance metadata complete" -Passed (
        [string]$production.DecisionStatus -eq "AcceptedForRepository" -and
        (Test-NonEmptyString $production.AcceptedBy) -and
        (Test-NonEmptyString $production.AcceptedAt) -and
        (Test-NonEmptyString $production.EvidenceSource) -and
        (Test-NonEmptyString $production.OwnerDecisionNote)
    ) -Detail "DecisionStatus/AcceptedBy/AcceptedAt/EvidenceSource/OwnerDecisionNote"
}

$failedChecks = @($checks | Where-Object { -not [bool]$_.Passed })
$readyToStageCandidate = ($failedChecks.Count -eq 0 -and $evidenceFilePresent -and -not [bool]$preflight.Summary.WbpStaged)
$monitorWbpManualAcceptanceComplete = ($acceptedManualAcceptanceSectionCount -eq $requiredManualAcceptanceSectionNames.Count -and $readyToStageCandidate)

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $EvidencePath
    EvidenceFilePresent = $evidenceFilePresent
    WbpRelativePath = $wbpRelativePath
    WbpPath = $wbpPath
    CurrentAssetHash = [string]$preflight.Summary.AssetHash
    DryRunOnly = $true
    ModifiesAssets = $false
    StagesWbp = $false
    Checks = @($checks)
    FailedChecks = $failedChecks
    Summary = [PSCustomObject]@{
        EvidenceFilePresent = $evidenceFilePresent
        EvidenceRecordPresent = ($null -ne $evidenceRecord)
        RequiredEvidenceCount = $requiredNames.Count
        RequiredDisplayDataRowCount = $requiredDisplayDataRows.Count
        ManualAcceptanceSectionCount = $requiredManualAcceptanceSectionNames.Count
        ManualAcceptanceSectionsPresent = ($null -ne $manualAcceptanceSections)
        AcceptedManualAcceptanceSectionCount = $acceptedManualAcceptanceSectionCount
        ManualAcceptanceSections = $requiredManualAcceptanceSectionNames
        MonitorWbpAssetPresent = [bool](Test-Path -LiteralPath $wbpPath -PathType Leaf)
        MonitorWbpAssetTracked = ([bool](Test-Path -LiteralPath $wbpPath -PathType Leaf) -and -not [bool]$preflight.Summary.WbpUntracked)
        MonitorWbpAssetStageAllowed = $readyToStageCandidate
        ReadyToStageMonitorWbpAsset = $readyToStageCandidate
        EditorManualAcceptancePresent = ($acceptedManualAcceptanceSectionCount -gt 0)
        MonitorWbpManualAcceptanceComplete = $monitorWbpManualAcceptanceComplete
        PassedCheckCount = @($checks | Where-Object { [bool]$_.Passed }).Count
        FailedCheckCount = $failedChecks.Count
        ReadyToStageCandidate = $readyToStageCandidate
        WbpStaged = [bool]$preflight.Summary.WbpStaged
        DecisionReportReadyToStage = [bool]$decision.Summary.ReadyToStage
        DryRunOnly = $true
        ModifiesAssets = $false
        StagesWbp = $false
        FailOnIncompleteEvidence = [bool]$FailOnIncompleteEvidence
        Boundary = "WBP evidence validation is read-only. It can mark a stage candidate only after completed evidence, matching hash, files, and owner acceptance are present; it never stages the binary asset."
    }
}

if ($FailOnIncompleteEvidence -and -not $readyToStageCandidate) {
    throw "Monitor WBP acceptance evidence is incomplete. FailedCheckCount=$($report.Summary.FailedCheckCount) EvidencePath=$EvidencePath"
}

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP acceptance evidence validation complete."
    Write-Host "Evidence file present: $($report.Summary.EvidenceFilePresent)"
    Write-Host "Evidence record present: $($report.Summary.EvidenceRecordPresent)"
    Write-Host "Passed checks: $($report.Summary.PassedCheckCount)"
    Write-Host "Failed checks: $($report.Summary.FailedCheckCount)"
    Write-Host "Manual acceptance sections present: $($report.Summary.ManualAcceptanceSectionsPresent)"
    Write-Host "Accepted manual acceptance sections: $($report.Summary.AcceptedManualAcceptanceSectionCount)/$($report.Summary.ManualAcceptanceSectionCount)"
    Write-Host "Monitor WBP asset present: $($report.Summary.MonitorWbpAssetPresent)"
    Write-Host "Monitor WBP asset tracked: $($report.Summary.MonitorWbpAssetTracked)"
    Write-Host "Monitor WBP asset stage allowed: $($report.Summary.MonitorWbpAssetStageAllowed)"
    Write-Host "Ready to stage monitor WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)"
    Write-Host "Editor manual acceptance present: $($report.Summary.EditorManualAcceptancePresent)"
    Write-Host "Monitor WBP manual acceptance complete: $($report.Summary.MonitorWbpManualAcceptanceComplete)"
    Write-Host "Ready to stage candidate: $($report.Summary.ReadyToStageCandidate)"
    Write-Host "Dry run only: $($report.Summary.DryRunOnly)"
    Write-Host "Modifies assets: $($report.Summary.ModifiesAssets)"
    Write-Host "Stages WBP: $($report.Summary.StagesWbp)"
    if ($failedChecks.Count -gt 0) {
        Write-Host "Failed checks:"
        foreach ($check in $failedChecks) {
            Write-Host "  $($check.Name): $($check.Detail)"
        }
    }
}
