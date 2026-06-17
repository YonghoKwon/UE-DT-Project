param(
    [string]$ProjectRoot = "",
    [string]$LocalProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$LogPath = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$RequireAutomationSuccess,
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

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    $Value | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Convert-ToMarkdownCell {
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Get-ScenarioName {
    param([string]$CsvPath)

    if ($CsvPath -match "procedural_performance_budget\.csv") {
        return "ProceduralPerformanceBudget"
    }
    if ($CsvPath -match "procedural_high_density\.csv") {
        return "ProceduralHighDensityLoad"
    }
    if ($CsvPath -match "instanced_batch\.csv") {
        return "InstancedBatchLoad"
    }
    return "Unknown"
}

function Convert-ToPreviewMetric {
    param(
        [System.Text.RegularExpressions.Match]$Match,
        [int]$LineNumber
    )

    $csvPath = $Match.Groups["path"].Value.Trim()
    $scenario = Get-ScenarioName -CsvPath $csvPath

    return [PSCustomObject]@{
        Scenario = $scenario
        EvidenceLine = $LineNumber
        CsvPath = $csvPath
        LoadedPoints = [int]$Match.Groups["points"].Value
        RenderMode = $Match.Groups["mode"].Value
        TelemetryMode = $Match.Groups["tmode"].Value
        InputLines = [int]$Match.Groups["lines"].Value
        AcceptedPoints = [int]$Match.Groups["accepted"].Value
        Sections = [int]$Match.Groups["sections"].Value
        Instances = [int]$Match.Groups["instances"].Value
        ParseMs = [double]$Match.Groups["parse"].Value
        BuildMs = [double]$Match.Groups["build"].Value
        TotalMs = [double]$Match.Groups["total"].Value
        Status = $Match.Groups["status"].Value
    }
}

function Write-MarkdownReport {
    param(
        [object]$Report,
        [string]$Path
    )

    $lines = @()
    $lines += "# CSV Preview Performance Report"
    $lines += ""
    $lines += "Generated UTC: $($Report.GeneratedUtc)"
    $lines += ""
    $lines += ("Project root: ``{0}``" -f $Report.ProjectRoot)
    $lines += ("Local project root: ``{0}``" -f $Report.LocalProjectRoot)
    $lines += ("Log path: ``{0}``" -f $Report.LogPath)
    $lines += ""
    $lines += "## Summary"
    $lines += ""
    $lines += "- Required scenarios present: $($Report.Summary.RequiredScenariosPresent)"
    $lines += "- Max accepted points: $($Report.Summary.MaxAcceptedPoints)"
    $lines += "- Max total load ms: $($Report.Summary.MaxTotalLoadMs)"
    $lines += "- Procedural performance budget ms: $($Report.Summary.ProceduralPerformanceBudgetMs)"
    $lines += "- Automation success evidence present: $($Report.Summary.AutomationSuccessEvidencePresent)"
    $lines += "- Successful test completion count: $($Report.Summary.SuccessfulTestCompletionCount)"
    $lines += "- Test complete exit code zero: $($Report.Summary.TestCompleteExitCodeZero)"
    $lines += "- Evidence run start line: $($Report.Summary.EvidenceRunStartLine)"
    $lines += "- Test complete line: $($Report.Summary.TestCompleteLine)"
    $lines += "- Evidence lines within run: $($Report.Summary.EvidenceLinesWithinRun)"
    $lines += "- Valid: $($Report.Summary.Valid)"
    $lines += ""
    $lines += "## Metrics"
    $lines += ""
    $lines += "| Scenario | Log line | Mode | Loaded | Accepted | Lines | Sections | Instances | Parse ms | Build ms | Total ms | Status |"
    $lines += "| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |"
    foreach ($metric in $Report.Metrics) {
        $lines += "| $(Convert-ToMarkdownCell $metric.Scenario) | $($metric.EvidenceLine) | $(Convert-ToMarkdownCell $metric.RenderMode) | $($metric.LoadedPoints) | $($metric.AcceptedPoints) | $($metric.InputLines) | $($metric.Sections) | $($metric.Instances) | $($metric.ParseMs) | $($metric.BuildMs) | $($metric.TotalMs) | $(Convert-ToMarkdownCell $metric.Status) |"
    }
    $lines += ""
    $lines += "## Automation Success Evidence"
    $lines += ""
    $lines += "| Scenario | Log line |"
    $lines += "| --- | ---: |"
    foreach ($item in $Report.AutomationSuccessEvidence) {
        $lines += "| $(Convert-ToMarkdownCell $item.Scenario) | $($item.Line) |"
    }
    $lines += ""
    $lines += "## Notes"
    $lines += ""
    $lines += "This report reads the latest CSV preview telemetry emitted by headless automation logs."
    $lines += "It is evidence for the current CPU preview fallback, not proof that a GPU/Niagara renderer has been integrated."

    Write-TextFile -Path $Path -Lines $lines
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (Test-Path -LiteralPath $LocalProjectRoot -PathType Container) {
    $LocalProjectRoot = (Resolve-Path -LiteralPath $LocalProjectRoot).Path
}
elseif ([string]::IsNullOrWhiteSpace($LogPath)) {
    throw "Local project root not found: $LocalProjectRoot"
}

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $LogPath = Join-Path $LocalProjectRoot "Saved\Logs\m7at10_dt.log"
}
$LogPath = (Resolve-Path -LiteralPath $LogPath).Path
Assert-FileExists -Path $LogPath -Label "Unreal automation log"

$logLines = @(Get-Content -LiteralPath $LogPath)
$logText = $logLines -join "`n"
$pattern = "\[CsvPointCloudPreview\] Loaded (?<points>\d+) points from (?<path>.+?) mode=(?<mode>\w+).*?telemetry=mode=(?<tmode>\w+) lines=(?<lines>\d+) accepted=(?<accepted>\d+) sections=(?<sections>\d+) instances=(?<instances>\d+) parseMs=(?<parse>[0-9.]+) buildMs=(?<build>[0-9.]+) totalMs=(?<total>[0-9.]+) status=(?<status>\w+)"

$evidenceRunStartLine = 0
for ($lineIndex = 0; $lineIndex -lt $logLines.Count; ++$lineIndex) {
    $line = $logLines[$lineIndex]
    if ($line -match "M7AT10\.Sensor\.CsvPointCloudPreview" -and
        ($line -match "Automation RunTests" -or $line -match "RunTests=" -or $line -match "Found \d+ automation tests")) {
        $evidenceRunStartLine = $lineIndex + 1
    }
}

$metricsByScenario = @{}
for ($lineIndex = 0; $lineIndex -lt $logLines.Count; ++$lineIndex) {
    $lineNumber = $lineIndex + 1
    if ($evidenceRunStartLine -gt 0 -and $lineNumber -lt $evidenceRunStartLine) {
        continue
    }

    $match = [regex]::Match($logLines[$lineIndex], $pattern)
    if (-not $match.Success) {
        continue
    }

    $metric = Convert-ToPreviewMetric -Match $match -LineNumber $lineNumber
    if ($metric.Scenario -ne "Unknown") {
        $metricsByScenario[$metric.Scenario] = $metric
    }
}

if ($metricsByScenario.Count -eq 0) {
    throw "No CSV preview telemetry entries were found in $LogPath. Run Automation RunTests M7AT10.Sensor.CsvPointCloudPreview first."
}

$requiredScenarios = @(
    "InstancedBatchLoad",
    "ProceduralHighDensityLoad",
    "ProceduralPerformanceBudget"
)

$missingScenarios = @($requiredScenarios | Where-Object { -not $metricsByScenario.ContainsKey($_) })
if ($missingScenarios.Count -gt 0) {
    throw "Missing CSV preview telemetry scenarios in ${LogPath}: $($missingScenarios -join ', ')"
}

$metrics = @($requiredScenarios | ForEach-Object { $metricsByScenario[$_] })
$proceduralBudget = $metricsByScenario["ProceduralPerformanceBudget"]
$successEvidenceByScenario = @{}
$testCompleteLine = 0
for ($lineIndex = 0; $lineIndex -lt $logLines.Count; ++$lineIndex) {
    $lineNumber = $lineIndex + 1
    if ($evidenceRunStartLine -gt 0 -and $lineNumber -lt $evidenceRunStartLine) {
        continue
    }

    $line = $logLines[$lineIndex]
    foreach ($scenario in $requiredScenarios) {
        if ($line -match "Test Completed\. Result=\{Success\}.*Path=\{M7AT10\.Sensor\.CsvPointCloudPreview\.$scenario\}") {
            $successEvidenceByScenario[$scenario] = [PSCustomObject]@{
                Scenario = $scenario
                Line = $lineNumber
            }
        }
    }
    if ($line -match "TEST COMPLETE\. EXIT CODE:\s*0") {
        $testCompleteLine = $lineNumber
    }
}
$automationSuccessEvidence = @($requiredScenarios | Where-Object { $successEvidenceByScenario.ContainsKey($_) } | ForEach-Object { $successEvidenceByScenario[$_] })
$successfulTestCompletionCount = $automationSuccessEvidence.Count
$testCompleteExitCodeZero = ($testCompleteLine -gt 0)
$automationSuccessEvidencePresent = ($successfulTestCompletionCount -eq $requiredScenarios.Count) -and $testCompleteExitCodeZero
$evidenceLinesWithinRun = ($evidenceRunStartLine -gt 0) -and ($testCompleteLine -gt $evidenceRunStartLine)
foreach ($metric in $metrics) {
    $evidenceLinesWithinRun = $evidenceLinesWithinRun -and ($metric.EvidenceLine -gt $evidenceRunStartLine) -and ($metric.EvidenceLine -lt $testCompleteLine)
}
foreach ($success in $automationSuccessEvidence) {
    $evidenceLinesWithinRun = $evidenceLinesWithinRun -and ($success.Line -gt $evidenceRunStartLine) -and ($success.Line -lt $testCompleteLine)
}

$invariants = @(
    [PSCustomObject]@{ Label = "instanced scenario uses InstancedMesh"; Passed = ($metricsByScenario["InstancedBatchLoad"].RenderMode -eq "InstancedMesh") },
    [PSCustomObject]@{ Label = "instanced scenario accepted 128 points"; Passed = ($metricsByScenario["InstancedBatchLoad"].AcceptedPoints -eq 128) },
    [PSCustomObject]@{ Label = "procedural high-density accepted 120000 points"; Passed = ($metricsByScenario["ProceduralHighDensityLoad"].AcceptedPoints -eq 120000) },
    [PSCustomObject]@{ Label = "procedural performance accepted 250000 points"; Passed = ($proceduralBudget.AcceptedPoints -eq 250000) },
    [PSCustomObject]@{ Label = "procedural performance split into 5 sections"; Passed = ($proceduralBudget.Sections -eq 5) },
    [PSCustomObject]@{ Label = "procedural performance status loaded"; Passed = ($proceduralBudget.Status -eq "Loaded") },
    [PSCustomObject]@{ Label = "procedural performance stayed below 60000ms guard"; Passed = ($proceduralBudget.TotalMs -lt 60000.0) }
)

$failedInvariants = @($invariants | Where-Object { -not $_.Passed })
if ($failedInvariants.Count -gt 0) {
    throw "CSV preview performance report invariants failed: $((@($failedInvariants | ForEach-Object { $_.Label })) -join '; ')"
}
if ($RequireAutomationSuccess -and -not $automationSuccessEvidencePresent) {
    throw "CSV preview performance report found telemetry, but automation success evidence was missing. SuccessCount=$successfulTestCompletionCount TestCompleteExitCodeZero=$testCompleteExitCodeZero"
}
if ($RequireAutomationSuccess -and -not $evidenceLinesWithinRun) {
    throw "CSV preview performance report found telemetry and success lines, but they were not all inside the selected CSV preview automation run block. StartLine=$evidenceRunStartLine TestCompleteLine=$testCompleteLine"
}

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$report = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    ProjectRoot = $ProjectRoot
    LocalProjectRoot = $LocalProjectRoot
    LogPath = $LogPath
    Metrics = $metrics
    AutomationSuccessEvidence = $automationSuccessEvidence
    Invariants = $invariants
    Summary = [PSCustomObject]@{
        RequiredScenariosPresent = ($missingScenarios.Count -eq 0)
        ScenarioCount = $metrics.Count
        MaxAcceptedPoints = [int](($metrics | Measure-Object -Property AcceptedPoints -Maximum).Maximum)
        MaxTotalLoadMs = [double](($metrics | Measure-Object -Property TotalMs -Maximum).Maximum)
        ProceduralPerformanceBudgetMs = [double]$proceduralBudget.TotalMs
        AutomationSuccessEvidencePresent = [bool]$automationSuccessEvidencePresent
        SuccessfulTestCompletionCount = [int]$successfulTestCompletionCount
        TestCompleteExitCodeZero = [bool]$testCompleteExitCodeZero
        EvidenceRunStartLine = [int]$evidenceRunStartLine
        TestCompleteLine = [int]$testCompleteLine
        EvidenceLinesWithinRun = [bool]$evidenceLinesWithinRun
        Valid = $true
    }
}

if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    Write-JsonFile -Path $JsonPath -Value $report
}
if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-MarkdownReport -Report $report -Path $MarkdownPath
}

if ($Json) {
    $report | ConvertTo-Json -Depth 6
}
else {
    Write-Host "CSV preview performance report is ready."
    Write-Host "Log path: $LogPath"
    Write-Host "Scenarios: $($report.Summary.ScenarioCount)"
    Write-Host "Max accepted points: $($report.Summary.MaxAcceptedPoints)"
    Write-Host "Max total load ms: $($report.Summary.MaxTotalLoadMs)"
    Write-Host "Procedural performance budget ms: $($report.Summary.ProceduralPerformanceBudgetMs)"
    Write-Host "Automation success evidence present: $($report.Summary.AutomationSuccessEvidencePresent)"
    Write-Host "Successful test completion count: $($report.Summary.SuccessfulTestCompletionCount)"
    Write-Host "Test complete exit code zero: $($report.Summary.TestCompleteExitCodeZero)"
    Write-Host "Evidence run start line: $($report.Summary.EvidenceRunStartLine)"
    Write-Host "Test complete line: $($report.Summary.TestCompleteLine)"
    Write-Host "Evidence lines within run: $($report.Summary.EvidenceLinesWithinRun)"
}
