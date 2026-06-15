param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Assert-Equal {
    param(
        [object]$Actual,
        [object]$Expected,
        [string]$Label
    )

    if ($Actual -ne $Expected) {
        throw "$Label expected '$Expected' but got '$Actual'"
    }
}

function Assert-True {
    param(
        [bool]$Value,
        [string]$Label
    )

    if (-not $Value) {
        throw "$Label expected true"
    }
}

function Assert-False {
    param(
        [bool]$Value,
        [string]$Label
    )

    if ($Value) {
        throw "$Label expected false"
    }
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Label
    )

    if (-not $Text.Contains($Expected)) {
        throw "$Label expected text '$Expected'"
    }
}

function Invoke-AssetReport {
    param(
        [string]$ProjectRoot,
        [string]$EvidencePath = "",
        [switch]$FailOnStagedDecisionPoints
    )

    $scriptPath = Join-Path $PSScriptRoot "report_local_project_status.ps1"
    if (-not (Test-Path -LiteralPath $scriptPath)) {
        throw "report_local_project_status.ps1 not found: $scriptPath"
    }

    $args = @("-ExecutionPolicy", "Bypass", "-File", $scriptPath, "-ProjectRoot", $ProjectRoot, "-Json")
    if (-not [string]::IsNullOrWhiteSpace($EvidencePath)) {
        $args += @("-EvidencePath", $EvidencePath)
    }
    if ($FailOnStagedDecisionPoints) {
        $args += "-FailOnStagedDecisionPoints"
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & powershell @args 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = @($output) -join "`n"
        Report = if ($exitCode -eq 0) { (($output | Out-String) | ConvertFrom-Json) } else { $null }
    }
}

function Get-DecisionPoint {
    param(
        [object]$Report,
        [string]$Path
    )

    $point = @($Report.DecisionPoints | Where-Object { $_.Path -eq $Path } | Select-Object -First 1)
    if ($point.Count -eq 0) {
        throw "Decision point not found: $Path"
    }
    return $point[0]
}

function New-EvidenceFile {
    param(
        [string]$Path,
        [object[]]$Decisions
    )

    $document = [PSCustomObject]@{
        Schema = "LocalAssetDecisionEvidenceV1"
        GeneratedAt = (Get-Date).ToString("s")
        Decisions = $Decisions
    }

    $document | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function New-DecisionEvidence {
    param(
        [object]$Point,
        [string]$DecisionStatus,
        [string[]]$IncompleteEvidenceNames = @(),
        [bool]$IncludeAcceptance = $true,
        [string]$PathOverride = ""
    )

    $evidenceItems = @(
        @($Point.EvidenceNeeded) |
            ForEach-Object {
                [PSCustomObject]@{
                    Name = $_
                    Status = if ($IncompleteEvidenceNames -contains $_) { "Pending" } else { "Complete" }
                    Source = if ($IncompleteEvidenceNames -contains $_) { "" } else { "automated workflow validation" }
                    Note = ""
                }
            }
    )

    return [PSCustomObject]@{
        Path = if ([string]::IsNullOrWhiteSpace($PathOverride)) { $Point.Path } else { $PathOverride }
        DecisionOwner = $Point.DecisionOwner
        DecisionStatus = $DecisionStatus
        AcceptedBy = if ($IncludeAcceptance) { "Automated Evidence Workflow Check" } else { "" }
        AcceptedAt = if ($IncludeAcceptance) { "2026-06-16" } else { "" }
        EvidenceSource = "automated workflow validation"
        Notes = ""
        Evidence = $evidenceItems
    }
}

function New-TempProject {
    param([string]$Root)

    New-Item -ItemType Directory -Path $Root | Out-Null
    Push-Location $Root
    try {
        git init | Out-Null
        git config user.email "dt-evidence-workflow@example.local" | Out-Null
        git config user.name "DT Evidence Workflow" | Out-Null
        git config core.autocrlf false | Out-Null
        "dummy" | Set-Content -LiteralPath "m7at10_dt.uproject" -Encoding UTF8
        git add -- "m7at10_dt.uproject" | Out-Null
        git commit -m "Initial temp project" | Out-Null

        New-Item -ItemType Directory -Path "Config" | Out-Null
        @"
[DTCoreRuntimeOverride]
ServerUrl=
"@ | Set-Content -LiteralPath "Config\Game.ini" -Encoding UTF8

        New-Item -ItemType Directory -Path "Content\M7AT10\UI" -Force | Out-Null
        "dummy widget asset" | Set-Content -LiteralPath "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset" -Encoding UTF8

        New-Item -ItemType Directory -Path "Content\Materials" -Force | Out-Null
        "dummy material asset" | Set-Content -LiteralPath "Content\Materials\M_Test.uasset" -Encoding UTF8

        New-Item -ItemType Directory -Path "Windows" | Out-Null
        "packaged output" | Set-Content -LiteralPath "Windows\dummy.txt" -Encoding UTF8
        "packaged archive" | Set-Content -LiteralPath "Windows.zip" -Encoding UTF8
        "{}" | Set-Content -LiteralPath "launcher.config.json" -Encoding UTF8
    }
    finally {
        Pop-Location
    }
}

function Invoke-StagedGate {
    param(
        [string]$ProjectRoot,
        [string]$EvidencePath = ""
    )

    return Invoke-AssetReport -ProjectRoot $ProjectRoot -EvidencePath $EvidencePath -FailOnStagedDecisionPoints
}

$ResolvedProjectRoot = if (Test-Path -LiteralPath $ProjectRoot) { (Resolve-Path -LiteralPath $ProjectRoot).Path } else { $ProjectRoot }
$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("dt-local-asset-evidence-" + [System.Guid]::NewGuid().ToString("N"))
$results = [System.Collections.Generic.List[object]]::new()

try {
    New-TempProject -Root $tempDir

    $baseline = Invoke-AssetReport -ProjectRoot $tempDir
    if ($baseline.ExitCode -ne 0) {
        throw "Baseline report failed: $($baseline.Output)"
    }

    $configPoint = Get-DecisionPoint -Report $baseline.Report -Path "Config\Game.ini"
    Assert-Equal -Actual $configPoint.EvidenceStatus -Expected "NoEvidenceRecord" -Label "No-evidence status"
    Assert-Equal -Actual $configPoint.EvidenceReviewStatus -Expected "NoEvidenceRecord" -Label "No-evidence review status"
    Assert-False -Value ([bool]$configPoint.EvidenceSatisfied) -Label "No-evidence satisfaction"
    Assert-False -Value ([bool]$configPoint.EvidenceComplete) -Label "No-evidence completeness"
    Assert-Equal -Actual $configPoint.CommitReadiness -Expected "BlockedByManualDecision" -Label "No-evidence readiness"
    Assert-Equal -Actual $configPoint.ReviewQueue -Expected "NeedsOwnerDecision" -Label "No-evidence queue"
    Assert-Equal -Actual @($configPoint.MissingEvidence).Count -Expected @($configPoint.EvidenceNeeded).Count -Label "No-evidence missing evidence count"
    $results.Add([PSCustomObject]@{ Case = "NoEvidenceRecord"; Path = $configPoint.Path; ReviewQueue = $configPoint.ReviewQueue; EvidenceStatus = $configPoint.EvidenceStatus }) | Out-Null

    $widgetPoint = Get-DecisionPoint -Report $baseline.Report -Path "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset"
    Assert-Equal -Actual $widgetPoint.DecisionStatus -Expected "EvidencePending" -Label "WBP default status"
    Assert-Equal -Actual $widgetPoint.ReviewQueue -Expected "NeedsOwnerDecision" -Label "WBP default queue"
    $results.Add([PSCustomObject]@{ Case = "EvidencePendingDefault"; Path = $widgetPoint.Path; ReviewQueue = $widgetPoint.ReviewQueue; EvidenceStatus = $widgetPoint.EvidenceStatus }) | Out-Null

    foreach ($generatedPath in @("Windows.zip", "Windows", "launcher.config.json")) {
        $generatedPoint = Get-DecisionPoint -Report $baseline.Report -Path $generatedPath
        Assert-Equal -Actual $generatedPoint.EvidenceStatus -Expected "GeneratedOutput" -Label "$generatedPath generated evidence status"
        Assert-Equal -Actual $generatedPoint.CommitReadiness -Expected "DoNotCommitGeneratedOutput" -Label "$generatedPath generated readiness"
        Assert-Equal -Actual $generatedPoint.ReviewQueue -Expected "KeepLocal" -Label "$generatedPath generated queue"
    }
    $results.Add([PSCustomObject]@{ Case = "GeneratedOutput"; Path = "Windows.zip/Windows/launcher.config.json"; ReviewQueue = "KeepLocal"; EvidenceStatus = "GeneratedOutput" }) | Out-Null

    $incompletePath = Join-Path $tempDir "incomplete.evidence.json"
    New-EvidenceFile -Path $incompletePath -Decisions @(
        (New-DecisionEvidence -Point $configPoint -DecisionStatus "AcceptedForRepository" -IncompleteEvidenceNames @("Runtime config policy pass"))
    )
    $incomplete = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $incompletePath
    $incompletePoint = Get-DecisionPoint -Report $incomplete.Report -Path $configPoint.Path
    Assert-Equal -Actual $incompletePoint.DecisionStatus -Expected "AcceptedForRepository" -Label "Incomplete accepted status"
    Assert-Equal -Actual $incompletePoint.EvidenceStatus -Expected "AcceptedButEvidenceIncomplete" -Label "Incomplete evidence status"
    Assert-False -Value ([bool]$incompletePoint.EvidenceSatisfied) -Label "Incomplete evidence satisfaction"
    Assert-Equal -Actual $incompletePoint.CommitReadiness -Expected "BlockedByManualDecision" -Label "Incomplete evidence readiness"
    Assert-Equal -Actual $incompletePoint.ReviewQueue -Expected "NeedsOwnerDecision" -Label "Incomplete evidence queue"
    Assert-True -Value (@($incompletePoint.MissingEvidence) -contains "Runtime config policy pass") -Label "Incomplete evidence reports missing required item"
    $results.Add([PSCustomObject]@{ Case = "AcceptedButEvidenceIncomplete"; Path = $incompletePoint.Path; ReviewQueue = $incompletePoint.ReviewQueue; EvidenceStatus = $incompletePoint.EvidenceStatus }) | Out-Null

    $blankAcceptancePath = Join-Path $tempDir "blank-acceptance.evidence.json"
    New-EvidenceFile -Path $blankAcceptancePath -Decisions @(
        (New-DecisionEvidence -Point $configPoint -DecisionStatus "AcceptedForRepository" -IncludeAcceptance:$false)
    )
    $blankAcceptance = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $blankAcceptancePath
    $blankAcceptancePoint = Get-DecisionPoint -Report $blankAcceptance.Report -Path $configPoint.Path
    Assert-Equal -Actual $blankAcceptancePoint.EvidenceStatus -Expected "AcceptedButEvidenceIncomplete" -Label "Blank acceptance evidence status"
    Assert-False -Value ([bool]$blankAcceptancePoint.EvidenceSatisfied) -Label "Blank acceptance evidence satisfaction"
    Assert-Equal -Actual $blankAcceptancePoint.ReviewQueue -Expected "NeedsOwnerDecision" -Label "Blank acceptance queue"
    $results.Add([PSCustomObject]@{ Case = "BlankAcceptance"; Path = $blankAcceptancePoint.Path; ReviewQueue = $blankAcceptancePoint.ReviewQueue; EvidenceStatus = $blankAcceptancePoint.EvidenceStatus }) | Out-Null

    $pendingPath = Join-Path $tempDir "pending.evidence.json"
    New-EvidenceFile -Path $pendingPath -Decisions @(
        (New-DecisionEvidence -Point $configPoint -DecisionStatus "PendingOwnerDecision")
    )
    $pending = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $pendingPath
    $pendingPoint = Get-DecisionPoint -Report $pending.Report -Path $configPoint.Path
    Assert-Equal -Actual $pendingPoint.DecisionStatus -Expected "PendingOwnerDecision" -Label "Pending status"
    Assert-Equal -Actual $pendingPoint.ReviewQueue -Expected "NeedsOwnerDecision" -Label "Pending queue"
    $results.Add([PSCustomObject]@{ Case = "PendingOwnerDecisionWithCompleteEvidence"; Path = $pendingPoint.Path; ReviewQueue = $pendingPoint.ReviewQueue; EvidenceStatus = $pendingPoint.EvidenceStatus }) | Out-Null

    $slashPath = Join-Path $tempDir "slash-path.evidence.json"
    New-EvidenceFile -Path $slashPath -Decisions @(
        (New-DecisionEvidence -Point $configPoint -DecisionStatus "AcceptedForRepository" -PathOverride "config/game.ini")
    )
    $slash = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $slashPath
    $slashPoint = Get-DecisionPoint -Report $slash.Report -Path $configPoint.Path
    Assert-Equal -Actual $slashPoint.ReviewQueue -Expected "ReadyToStage" -Label "Normalized evidence path queue"
    Assert-Equal -Actual $slashPoint.EvidenceStatus -Expected "ReadyEvidenceAccepted" -Label "Normalized evidence path status"
    $results.Add([PSCustomObject]@{ Case = "NormalizedEvidencePath"; Path = "config/game.ini"; ReviewQueue = $slashPoint.ReviewQueue; EvidenceStatus = $slashPoint.EvidenceStatus }) | Out-Null

    $keepLocalPath = Join-Path $tempDir "keep-local.evidence.json"
    New-EvidenceFile -Path $keepLocalPath -Decisions @(
        (New-DecisionEvidence -Point $configPoint -DecisionStatus "KeepLocal" -IncompleteEvidenceNames @("Manual diff review") -IncludeAcceptance:$false)
    )
    $keepLocal = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $keepLocalPath
    $keepLocalPoint = Get-DecisionPoint -Report $keepLocal.Report -Path $configPoint.Path
    Assert-Equal -Actual $keepLocalPoint.EvidenceStatus -Expected "KeepLocalByDecision" -Label "KeepLocal evidence status"
    Assert-Equal -Actual $keepLocalPoint.CommitReadiness -Expected "KeepLocalByDecision" -Label "KeepLocal readiness"
    Assert-Equal -Actual $keepLocalPoint.ReviewQueue -Expected "KeepLocal" -Label "KeepLocal queue"
    $results.Add([PSCustomObject]@{ Case = "KeepLocalByDecision"; Path = $keepLocalPoint.Path; ReviewQueue = $keepLocalPoint.ReviewQueue; EvidenceStatus = $keepLocalPoint.EvidenceStatus }) | Out-Null

    $generatedOverridePath = Join-Path $tempDir "generated-override.evidence.json"
    $windowsZipPoint = Get-DecisionPoint -Report $baseline.Report -Path "Windows.zip"
    New-EvidenceFile -Path $generatedOverridePath -Decisions @(
        (New-DecisionEvidence -Point $windowsZipPoint -DecisionStatus "AcceptedForRepository")
    )
    $generatedOverride = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $generatedOverridePath
    $generatedOverridePoint = Get-DecisionPoint -Report $generatedOverride.Report -Path "Windows.zip"
    Assert-Equal -Actual $generatedOverridePoint.EvidenceStatus -Expected "GeneratedOutput" -Label "Generated override evidence status"
    Assert-Equal -Actual $generatedOverridePoint.CommitReadiness -Expected "DoNotCommitGeneratedOutput" -Label "Generated override readiness"
    Assert-Equal -Actual $generatedOverridePoint.ReviewQueue -Expected "KeepLocal" -Label "Generated override queue"
    $results.Add([PSCustomObject]@{ Case = "GeneratedOverrideIgnored"; Path = $generatedOverridePoint.Path; ReviewQueue = $generatedOverridePoint.ReviewQueue; EvidenceStatus = $generatedOverridePoint.EvidenceStatus }) | Out-Null

    $completePath = Join-Path $tempDir "complete.evidence.json"
    New-EvidenceFile -Path $completePath -Decisions @(
        (New-DecisionEvidence -Point $configPoint -DecisionStatus "AcceptedForRepository")
    )
    $complete = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $completePath
    $completePoint = Get-DecisionPoint -Report $complete.Report -Path $configPoint.Path
    Assert-Equal -Actual $completePoint.DecisionStatus -Expected "AcceptedForRepository" -Label "Complete accepted status"
    Assert-Equal -Actual $completePoint.EvidenceStatus -Expected "ReadyEvidenceAccepted" -Label "Complete evidence status"
    Assert-True -Value ([bool]$completePoint.EvidenceSatisfied) -Label "Complete evidence satisfaction"
    Assert-Equal -Actual $completePoint.CommitReadiness -Expected "ReadyWithEvidence" -Label "Complete readiness"
    Assert-Equal -Actual $completePoint.ReviewQueue -Expected "ReadyToStage" -Label "Complete queue"
    $results.Add([PSCustomObject]@{ Case = "ReadyEvidenceAccepted"; Path = $completePoint.Path; ReviewQueue = $completePoint.ReviewQueue; EvidenceStatus = $completePoint.EvidenceStatus }) | Out-Null

    Push-Location $tempDir
    try {
        git add -- "Config/Game.ini" | Out-Null
        $blockedGate = Invoke-StagedGate -ProjectRoot $tempDir
        Assert-True -Value ($blockedGate.ExitCode -ne 0) -Label "Blocked staged decision path exits nonzero"
        Assert-Contains -Text $blockedGate.Output -Expected "without ReadyToStage evidence" -Label "Blocked staged decision path message"

        $readyGate = Invoke-StagedGate -ProjectRoot $tempDir -EvidencePath $completePath
        Assert-Equal -Actual $readyGate.ExitCode -Expected 0 -Label "Ready staged decision path gate"
        Assert-Equal -Actual $readyGate.Report.Summary.StagedBlockedDecisionPointCount -Expected 0 -Label "Ready staged blocked count"

        git add -- "Content/M7AT10/UI/WBP_VirtualSensorMonitor.uasset" | Out-Null
        $mixedGate = Invoke-AssetReport -ProjectRoot $tempDir -EvidencePath $completePath
        Assert-Equal -Actual $mixedGate.Report.Summary.StagedDecisionPointCount -Expected 2 -Label "Mixed staged decision count"
        Assert-Equal -Actual $mixedGate.Report.Summary.StagedBlockedDecisionPointCount -Expected 1 -Label "Mixed staged blocked count"
        Assert-True -Value (@($mixedGate.Report.StagedBlockedDecisionPaths) -contains "Content/M7AT10/UI/WBP_VirtualSensorMonitor.uasset") -Label "Mixed staged blocked path"
    }
    finally {
        Pop-Location
    }
    $results.Add([PSCustomObject]@{ Case = "StagedDecisionGate"; Path = "Config/Game.ini + WBP"; ReviewQueue = "Ready path passes; blocked path fails"; EvidenceStatus = "verified" }) | Out-Null
}
finally {
    if (Test-Path -LiteralPath $tempDir) {
        Remove-Item -LiteralPath $tempDir -Recurse -Force
    }
}

$summary = [PSCustomObject]@{
    ReferenceProjectRoot = $ResolvedProjectRoot
    TempProjectRoot = $tempDir
    Cases = @($results)
}

if ($Json) {
    $summary | ConvertTo-Json -Depth 5
    return
}

Write-Host "Local asset decision evidence workflow is valid."
Write-Host "Cases: $($summary.Cases.Count)"
foreach ($case in $summary.Cases) {
    Write-Host "$($case.Case): $($case.Path) -> $($case.ReviewQueue) / $($case.EvidenceStatus)"
}
