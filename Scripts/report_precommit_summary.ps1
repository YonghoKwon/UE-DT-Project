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
        -Percent 87 `
        -Done "LiDAR/camera schema docs, compatibility notes, fixtures, fixture validator with camera base64/JPEG/byteSize and simulationQuality enum checks, preserved row/col/returnIndex contract, local mock contract validator with camera image invariant checks, schema review policy, server transport contract notes, weak HTTP callback handling, 2xx acceptance tracking, outbound HTTP POST loopback acceptance automation, and exportable contract review report are in place. The payload contract report now includes a server acceptance readiness matrix for endpoint ownership, authentication, retry/timeout, batching/backpressure, response schema, and real judging-server acceptance evidence." `
        -Remaining "Judging server approval, real server acceptance evidence, final endpoint/auth/retry/batching owner decisions, and server-owned response schema tests remain."),
    (New-WorkArea `
        -Name "Local project asset decisions" `
        -Percent 70 `
        -Done "Decision points are reported, unclassified untracked files and staged decision paths are gated, large/sample folders include content summaries, per-decision GitState/CommitReadiness/ReviewQueue/DecisionOwner/DecisionStatus/EvidenceNeeded/EvidenceStatus/EvidenceSatisfied/DecisionChecklist fields are exported, review queues separate ReadyToStage/NeedsOwnerDecision/KeepLocal paths, unresolved owner/evidence metadata is documented and validated, ReadyToStage now requires AcceptedForRepository with complete evidence plus reviewer/date/source evidence, duplicate normalized evidence paths are rejected, an evidence template exporter is available, the evidence workflow and staged decision gate are covered by temp-project automation, runtime config validation inspects the real local project and emits a Game.ini RecommendedDecision, WBP metadata/Git/setup-contract decision reporting is available, large content/sample paths are summarized with risk and RecommendedDecision reports, WBP/runtime-config/large-content decision guards are validated, and local asset/runtime config/WBP/large-content decision report exporters are available." `
        -Remaining "Manual WBP editor-open/binding/PIE acceptance, Game.ini owner acceptance, large content source/license/dependency/storage acceptance, and PixelStreaming sample ownership acceptance remain."),
    (New-WorkArea `
        -Name "Real sensor adapters" `
        -Percent 77 `
        -Done "ROS2/Livox/RealSense placeholders, normalized frame handoff path, replay samples, LiDAR JSON live bridge component, camera JSON live bridge component, generic live JSON payload bridge helper, optional HTTP and UDP JSON live bridge components, HTTP payload automation, local HTTP loopback POST smoke automation, local UDP datagram smoke automation, DTCore WebSocket transaction handler, safe routing guards, WebSocket live sample payload, editor sample/push helpers, transaction routing automation, static adapter-plan validation, exportable WebSocket transaction registration checklist, optional data-table registration evidence automation, editor-only row repair commandlet, DT_TransactionCode row pass evidence, row-based handler parse/process evidence, broker smoke evidence report tooling, WebSocket LiDAR smoke evidence workflow wrapper, brokerless DTCore dispatch automation, opt-in brokerless workflow execution, hardened camera JSON live payload validation with transport/recorder evidence, expanded malformed camera schema rejection coverage, and a consolidated real-sensor adapter readiness report are in place. The readiness report now ranks deployment path candidates and recommends file replay baseline, HTTP JSON live for first local/live ingestion, and DTCore WebSocket when broker ownership is ready." `
        -Remaining "Actual SDK/ROS2/Livox/RealSense connections, completed deployment STOMP/WebSocket broker PIE smoke evidence, HTTP/UDP deployment exposure and credential decisions, final production adapter owner approval, and successful real-frame smoke tests remain."),
    (New-WorkArea `
        -Name "Large point cloud rendering" `
        -Percent 62 `
        -Done "Server payload and preview policies are separated with preview caps, runtime warnings, point-cloud-only clamps, batched ISM AddInstances live preview uploads, procedural CSV high-density preview automation, instanced fallback automation, CSV preview parse/build/load telemetry, generous headless procedural performance-budget automation, exportable CSV preview performance evidence from Unreal automation logs, static preview-policy validation, and a high-density renderer decision report covering CPU fallback, Niagara, custom GPU buffers, and external viewer workflows. The renderer decision report now consumes the same local CSV preview performance evidence, requires 250k-point CPU fallback evidence plus automation success evidence, supports explicit log paths, and an end-to-end wrapper runs the local CPU fallback evidence workflow. CSV evidence reports now record the selected run block, telemetry line numbers, scenario success line numbers, completion line, and same-run validation." `
        -Remaining "GPU/Niagara or another high-density renderer selection, implementation, viewport smoke evidence, and renderer-specific dense-frame performance validation remain."),
    (New-WorkArea `
        -Name "LAZ export" `
        -Percent 49 `
        -Done "Placeholder behavior is explicit, tested as LAS-compatible source export, covered by static placeholder-policy validation, supported by a compression-path decision report, and now has an opt-in external compressor path with missing-compressor guard automation plus a positive external process-contract automation that verifies `{input}`/`{output}` handling and non-empty `.laz` output creation. A local compressor readiness report now scans compressor/reader candidates, records that tool readiness is not readable-output evidence, and is included in the project readiness gate." `
        -Remaining "Accepted compressor/tool selection, readable compressed `.laz` output evidence, native/server workflow decision if external CLI is not enough, and true compressed-output automation remain.")
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
