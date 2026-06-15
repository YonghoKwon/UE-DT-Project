param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "== $Title =="
}

function Get-GitLines {
    param(
        [string]$WorkingDirectory,
        [string[]]$GitArgs
    )

    Push-Location $WorkingDirectory
    try {
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $output = @(& git.exe @GitArgs 2>&1 | Where-Object { $_ -notmatch "^warning:" })
        $ErrorActionPreference = $previousErrorActionPreference
        if ($LASTEXITCODE -ne 0) {
            throw "git $($GitArgs -join ' ') failed with exit code $LASTEXITCODE"
        }
        return $output
    }
    finally {
        if ($previousErrorActionPreference) {
            $ErrorActionPreference = $previousErrorActionPreference
        }
        Pop-Location
    }
}

function Get-LocalAssetSummary {
    param([string]$ProjectRoot)

    $assetReportScript = Join-Path $script:PSScriptRoot "report_local_project_status.ps1"
    if (-not (Test-Path -LiteralPath $assetReportScript)) {
        return $null
    }

    $jsonText = & powershell -ExecutionPolicy Bypass -File $assetReportScript -ProjectRoot $ProjectRoot -Json
    if ($LASTEXITCODE -ne 0) {
        throw "Local asset report failed with exit code $LASTEXITCODE"
    }

    return $jsonText | ConvertFrom-Json
}

function Get-RepoChangeSummary {
    param([string]$RepoRoot)

    $statusLines = Get-GitLines -WorkingDirectory $RepoRoot -GitArgs @("status", "--porcelain=v1")
    $stagedChanges = @()
    $unstagedChanges = @()
    $untrackedChanges = @()

    foreach ($line in $statusLines) {
        if ([string]::IsNullOrWhiteSpace($line) -or $line.Length -lt 3) {
            continue
        }

        $indexStatus = $line.Substring(0, 1)
        $worktreeStatus = $line.Substring(1, 1)
        $path = $line.Substring(3)

        if ($indexStatus -eq "?" -and $worktreeStatus -eq "?") {
            $untrackedChanges += $path
            continue
        }

        if ($indexStatus -ne " ") {
            $stagedChanges += "$indexStatus`t$path"
        }
        if ($worktreeStatus -ne " ") {
            $unstagedChanges += "$worktreeStatus`t$path"
        }
    }

    return [PSCustomObject]@{
        Staged = $stagedChanges
        Unstaged = $unstagedChanges
        Untracked = $untrackedChanges
    }
}

function New-WorkArea {
    param(
        [string]$Name,
        [int]$Percent,
        [string]$Done,
        [string]$Remaining
    )

    return [PSCustomObject]@{
        Name = $Name
        Percent = $Percent
        Done = $Done
        Remaining = $Remaining
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Get-Location).Path).Path
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$workAreas = @(
    (New-WorkArea `
        -Name "Virtual sensor baseline" `
        -Percent 88 `
        -Done "LiDAR/camera payload, replay, preserved LiDAR grid coords, slab analysis, monitor fallback, monitor camera capture pending-state guards, monitor policy validation, smoke/readiness scripts are implemented." `
        -Remaining "Full editor PIE validation and production map/WBP verification remain."),
    (New-WorkArea `
        -Name "Server payload contract" `
        -Percent 80 `
        -Done "LiDAR/camera schema docs, compatibility notes, fixtures, fixture validator, preserved row/col/returnIndex contract, local mock contract validator, schema review policy, server transport contract notes, weak HTTP callback handling, 2xx acceptance tracking, and exportable contract review report are in place." `
        -Remaining "Judging server approval, real server acceptance evidence, and final endpoint/auth/retry/batching decisions remain."),
    (New-WorkArea `
        -Name "Local project asset decisions" `
        -Percent 45 `
        -Done "Decision points are reported, unclassified untracked files and staged decision paths are gated, large/sample folders include content summaries, WBP/runtime-config/large-content decision guards are validated, and a local asset decision report exporter is available." `
        -Remaining "WBP, Game.ini, large content folders, and PixelStreaming sample ownership decisions remain."),
    (New-WorkArea `
        -Name "Real sensor adapters" `
        -Percent 59 `
        -Done "ROS2/Livox/RealSense placeholders, normalized frame handoff path, replay samples, JSON live bridge component, DTCore WebSocket transaction handler, safe routing guards, WebSocket live sample payload, editor sample/push helpers, transaction routing automation, static adapter-plan validation, exportable WebSocket transaction registration checklist, optional data-table registration evidence automation, editor-only row repair commandlet, DT_TransactionCode row pass evidence, row-based handler parse/process evidence, broker smoke evidence report tooling, WebSocket LiDAR smoke evidence workflow wrapper, brokerless DTCore dispatch automation, and opt-in brokerless workflow execution are in place." `
        -Remaining "Actual SDK/ROS2/Livox/RealSense connections, completed deployment STOMP/WebSocket broker PIE smoke evidence, optional HTTP/UDP bridge wiring, and successful real-frame smoke tests remain."),
    (New-WorkArea `
        -Name "Large point cloud rendering" `
        -Percent 30 `
        -Done "Server payload and preview policies are separated with preview caps, runtime warnings, point-cloud-only clamps, and static preview-policy validation." `
        -Remaining "GPU/Niagara or another high-density renderer decision and implementation remain."),
    (New-WorkArea `
        -Name "LAZ export" `
        -Percent 25 `
        -Done "Placeholder behavior is explicit, tested as LAS-compatible source export, and covered by static placeholder-policy validation." `
        -Remaining "True LAZ compression decision, integration, and validation remain.")
)

$overallPercent = [int][Math]::Round((($workAreas | Measure-Object -Property Percent -Average).Average), 0)
$changeSummary = Get-RepoChangeSummary -RepoRoot $repoRoot
$staged = $changeSummary.Staged
$unstaged = $changeSummary.Unstaged
$untracked = $changeSummary.Untracked
$branch = (Get-GitLines -WorkingDirectory $repoRoot -GitArgs @("branch", "--show-current") | Select-Object -First 1)
$recentCommit = (Get-GitLines -WorkingDirectory $repoRoot -GitArgs @("log", "--oneline", "-1") | Select-Object -First 1)
$localAssetReport = Get-LocalAssetSummary -ProjectRoot $ProjectRoot

$report = [PSCustomObject]@{
    RepositoryRoot = $repoRoot
    LocalProjectRoot = $ProjectRoot
    Branch = $branch
    RecentCommit = $recentCommit
    OverallPercent = $overallPercent
    WorkAreas = $workAreas
    StagedChanges = $staged
    UnstagedChanges = $unstaged
    UntrackedChanges = $untracked
    LocalAssetSummary = if ($localAssetReport) { $localAssetReport.Summary } else { $null }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 6
    exit 0
}

Write-Section "Progress"
Write-Host "Overall progress: $overallPercent%"
foreach ($area in $workAreas) {
    Write-Host "$($area.Percent)% - $($area.Name)"
    Write-Host "  done: $($area.Done)"
    Write-Host "  remaining: $($area.Remaining)"
}

Write-Section "Git"
Write-Host "Branch: $branch"
Write-Host "Latest commit: $recentCommit"

Write-Section "Commit candidates"
if ($staged.Count -gt 0) {
    Write-Host "Staged:"
    $staged | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host "Staged: none"
}

if ($unstaged.Count -gt 0) {
    Write-Host "Unstaged:"
    $unstaged | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host "Unstaged: none"
}

if ($untracked.Count -gt 0) {
    Write-Host "Untracked:"
    $untracked | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host "Untracked: none"
}

if ($localAssetReport) {
    Write-Section "Local asset decisions"
    Write-Host "Present decision points: $($localAssetReport.Summary.PresentDecisionPoints)"
    Write-Host "Generated/local-output items present: $($localAssetReport.Summary.GeneratedOrLocalOutputItemsPresent)"
    Write-Host "Unclassified untracked paths: $($localAssetReport.Summary.UnclassifiedUntrackedCount)"
}
