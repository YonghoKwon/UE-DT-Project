param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$SourceRepoRoot = "",
    [string]$OutputRoot = "",
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

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
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

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "Saved\Reports\MonitorWbpAcceptance"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
if (-not $OutputRoot.StartsWith((Join-Path $ProjectRoot "Saved"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be under ProjectRoot\Saved: $OutputRoot"
}

$evidencePath = Join-Path $OutputRoot "monitor_wbp_acceptance.evidence.json"
$packageScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_acceptance_package.ps1"
$validatorScript = Join-Path $SourceRepoRoot "Scripts\validate_monitor_wbp_acceptance_evidence.ps1"
$postEditHashScript = Join-Path $SourceRepoRoot "Scripts\export_monitor_wbp_post_edit_hash_report.ps1"
$hashUpdaterScript = Join-Path $SourceRepoRoot "Scripts\update_monitor_wbp_acceptance_hash_evidence.ps1"
$manualEvidenceUpdaterScript = Join-Path $SourceRepoRoot "Scripts\update_monitor_wbp_manual_evidence_paths.ps1"
$manualAcceptanceUpdaterScript = Join-Path $SourceRepoRoot "Scripts\update_monitor_wbp_manual_acceptance_sections.ps1"

$package = Invoke-JsonScript -ScriptPath $packageScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    OutputRoot = $OutputRoot
}
$validation = Invoke-JsonScript -ScriptPath $validatorScript -Parameters @{
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    EvidencePath = $evidencePath
}

$jsonPath = Join-Path $OutputRoot "monitor_wbp_acceptance_runbook.json"
$markdownPath = Join-Path $OutputRoot "monitor_wbp_acceptance_runbook.md"
$commands = @(
    [PSCustomObject]@{
        Step = 1
        Name = "Refresh acceptance package"
        Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}"' -f $packageScript, $ProjectRoot, $SourceRepoRoot)
    },
    [PSCustomObject]@{
        Step = 2
        Name = "Open WBP in Unreal Editor"
        Command = "Open $ProjectRoot\ma0t10_dt.uproject in Unreal Editor 5.3, then open Content/MA0T10/UI/WBP_VirtualSensorMonitor and compile/save through the editor."
    },
    [PSCustomObject]@{
        Step = 3
        Name = "Export post-edit hash report"
        Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}"' -f $postEditHashScript, $ProjectRoot, $SourceRepoRoot, $evidencePath)
    },
    [PSCustomObject]@{
        Step = 4
        Name = "Copy hash evidence"
        Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}"' -f $hashUpdaterScript, $ProjectRoot, $SourceRepoRoot, $evidencePath)
    },
    [PSCustomObject]@{
        Step = 5
        Name = "Merge Editor/PIE evidence paths"
        Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -EvidenceRunId "<run-id>" -Operator "<name>" -VerifiedAt "<yyyy-mm-ddThh:mm:ss>" -MapName "<map>" -PieSession "<session>" -EditorLogPath "<path>" -PieLogPath "<path>" -Json' -f $manualEvidenceUpdaterScript, $ProjectRoot, $SourceRepoRoot, $evidencePath)
    },
    [PSCustomObject]@{
        Step = 6
        Name = "Mark manual acceptance sections"
        Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -EditorOpenEvidencePath "<path>" -PieSmokeEvidencePath "<path>" -DisplayDataScreenMatchEvidencePath "<path>" -Json' -f $manualAcceptanceUpdaterScript, $ProjectRoot, $SourceRepoRoot, $evidencePath)
    },
    [PSCustomObject]@{
        Step = 7
        Name = "Validate acceptance evidence"
        Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -Json' -f $validatorScript, $ProjectRoot, $SourceRepoRoot, $evidencePath)
    },
    [PSCustomObject]@{
        Step = 8
        Name = "Strict gate before staging"
        Command = ('powershell -ExecutionPolicy Bypass -File "{0}" -ProjectRoot "{1}" -SourceRepoRoot "{2}" -EvidencePath "{3}" -FailOnIncompleteEvidence' -f $validatorScript, $ProjectRoot, $SourceRepoRoot, $evidencePath)
    }
)

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SourceRepoRoot = $SourceRepoRoot
    OutputRoot = $OutputRoot
    EvidencePath = $evidencePath
    JsonPath = $jsonPath
    MarkdownPath = $markdownPath
    DryRunOnly = $true
    ModifiesAssets = $false
    StagesFiles = $false
    StagesWbp = $false
    PackageSummary = $package.Summary
    ValidationSummary = $validation.Summary
    Commands = $commands
    Summary = [PSCustomObject]@{
        RunbookCreated = $true
        CommandCount = $commands.Count
        WbpAcceptanceEvidenceComplete = [bool]$validation.Summary.Complete
        MissingEvidenceActionCount = [int]$validation.Summary.MissingEvidenceActionCount
        ReadyToStageMonitorWbpAsset = [bool]$validation.Summary.ReadyToStageMonitorWbpAsset
        MonitorWbpManualAcceptanceComplete = [bool]$validation.Summary.MonitorWbpManualAcceptanceComplete
        Boundary = "This runbook is a Saved/Reports guide for manual Editor/PIE WBP acceptance. It does not modify assets, stage files, or accept the binary WBP."
        Valid = $true
    }
}

$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Monitor WBP Acceptance Runbook") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Evidence file: $($report.EvidencePath)") | Out-Null
$lines.Add("- Acceptance complete: $($report.Summary.WbpAcceptanceEvidenceComplete)") | Out-Null
$lines.Add("- Missing evidence action count: $($report.Summary.MissingEvidenceActionCount)") | Out-Null
$lines.Add("- Ready to stage WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)") | Out-Null
$lines.Add("- Manual acceptance complete: $($report.Summary.MonitorWbpManualAcceptanceComplete)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Execution Steps") | Out-Null
$lines.Add("") | Out-Null
foreach ($command in $commands) {
    $lines.Add("### $($command.Step). $($command.Name)") | Out-Null
    $lines.Add("") | Out-Null
    if ($command.Command -match "^powershell ") {
        $lines.Add('```powershell') | Out-Null
        $lines.Add($command.Command) | Out-Null
        $lines.Add('```') | Out-Null
    }
    else {
        $lines.Add($command.Command) | Out-Null
    }
    $lines.Add("") | Out-Null
}
$lines.Add("Boundary: $($report.Summary.Boundary)") | Out-Null
Write-TextFile -Path $markdownPath -Lines $lines

if ($Json) {
    $report | ConvertTo-Json -Depth 10
}
else {
    Write-Host "Monitor WBP acceptance runbook exported."
    Write-Host "JSON: $jsonPath"
    Write-Host "Markdown: $markdownPath"
    Write-Host "Command count: $($report.Summary.CommandCount)"
    Write-Host "Ready to stage WBP asset: $($report.Summary.ReadyToStageMonitorWbpAsset)"
    Write-Host "Modifies assets: $($report.ModifiesAssets)"
    Write-Host "Stages files: $($report.StagesFiles)"
}
