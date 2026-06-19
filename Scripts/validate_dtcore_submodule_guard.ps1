param(
    [string]$ProjectRoot = "",
    [string]$ExpectedCommit = "2eec1fee2ef7295d6ad876a4f3dd98d9faa6cdd7",
    [switch]$FailOnViolation,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
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
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $previousErrorActionPreference
        if ($exitCode -ne 0) {
            throw "git $($GitArgs -join ' ') failed with exit code $exitCode`: $($output -join ' ')"
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

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "ProjectRoot not found: $ProjectRoot"
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$submodulePath = "Plugins/DTCore"
$submodulePathBackslash = "Plugins\DTCore"
$submoduleFullPath = Join-Path $ProjectRoot $submodulePathBackslash
$submodulePresent = Test-Path -LiteralPath $submoduleFullPath -PathType Container

$submoduleStatusLines = Get-GitLines -WorkingDirectory $ProjectRoot -GitArgs @("submodule", "status", "--recursive", "--", $submodulePath)
$statusLine = @($submoduleStatusLines | Select-Object -First 1)
$actualCommit = ""
$statusPrefix = ""
if ($statusLine.Count -gt 0 -and [string]$statusLine[0] -match "^(?<prefix>[ +-U]?)(?<commit>[0-9a-fA-F]{40})\s+(?<path>\S+)") {
    $statusPrefix = $Matches["prefix"]
    $actualCommit = $Matches["commit"].ToLowerInvariant()
}

$projectStatusLines = Get-GitLines -WorkingDirectory $ProjectRoot -GitArgs @("status", "--porcelain=v1", "--untracked-files=all")
$dtCoreStatusLines = @(
    $projectStatusLines |
        Where-Object { $_ -match "Plugins[/\\]DTCore" }
)
$stagedDtCoreLines = @(
    $dtCoreStatusLines |
        Where-Object { $_.Length -ge 2 -and $_.Substring(0, 1) -ne " " -and -not $_.StartsWith("?? ") }
)
$unstagedDtCoreLines = @(
    $dtCoreStatusLines |
        Where-Object { $_.Length -ge 2 -and $_.Substring(1, 1) -ne " " }
)
$untrackedDtCoreLines = @(
    $dtCoreStatusLines |
        Where-Object { $_.StartsWith("?? ") }
)

$stagedDtCorePaths = Get-GitLines -WorkingDirectory $ProjectRoot -GitArgs @("diff", "--cached", "--name-only", "--", $submodulePath)
$submoduleWorktreeLines = @()
if ($submodulePresent) {
    $submoduleWorktreeLines = Get-GitLines -WorkingDirectory $submoduleFullPath -GitArgs @("status", "--porcelain=v1", "--untracked-files=all")
}

$expectedCommitMatches = (-not [string]::IsNullOrWhiteSpace($actualCommit) -and $actualCommit -eq $ExpectedCommit.ToLowerInvariant())
$submoduleStatusClean = ([string]::IsNullOrWhiteSpace($statusPrefix))
$dtCoreGitlinkStaged = ($stagedDtCoreLines.Count -gt 0 -or $stagedDtCorePaths.Count -gt 0)
$dtCoreGitlinkModified = ($unstagedDtCoreLines.Count -gt 0)
$dtCoreWorktreeClean = ($submoduleWorktreeLines.Count -eq 0)
$dtCoreInvariantValid = (
    $submodulePresent -and
    $expectedCommitMatches -and
    $submoduleStatusClean -and
    $dtCoreStatusLines.Count -eq 0 -and
    $stagedDtCorePaths.Count -eq 0 -and
    $dtCoreWorktreeClean
)

$violations = [System.Collections.Generic.List[string]]::new()
if (-not $submodulePresent) {
    $violations.Add("Plugins/DTCore is missing.") | Out-Null
}
if (-not $expectedCommitMatches) {
    $violations.Add("Plugins/DTCore commit mismatch. Expected=$ExpectedCommit Actual=$actualCommit") | Out-Null
}
if (-not $submoduleStatusClean) {
    $violations.Add("git submodule status prefix is not clean: '$statusPrefix'") | Out-Null
}
if ($dtCoreGitlinkStaged) {
    $violations.Add("Plugins/DTCore has staged changes.") | Out-Null
}
if ($dtCoreGitlinkModified) {
    $violations.Add("Plugins/DTCore has unstaged changes in the parent repository.") | Out-Null
}
if ($untrackedDtCoreLines.Count -gt 0) {
    $violations.Add("Plugins/DTCore has untracked parent-reported paths.") | Out-Null
}
if (-not $dtCoreWorktreeClean) {
    $violations.Add("Plugins/DTCore submodule worktree is not clean.") | Out-Null
}

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SubmodulePath = $submodulePathBackslash
    SubmoduleFullPath = $submoduleFullPath
    ExpectedCommit = $ExpectedCommit.ToLowerInvariant()
    ActualCommit = $actualCommit
    SubmoduleStatusLine = if ($statusLine.Count -gt 0) { [string]$statusLine[0] } else { "" }
    SubmoduleStatusPrefix = $statusPrefix
    SubmodulePresent = $submodulePresent
    ExpectedCommitMatches = $expectedCommitMatches
    SubmoduleStatusClean = $submoduleStatusClean
    ParentDTCoreStatusLines = $dtCoreStatusLines
    StagedDTCoreLines = $stagedDtCoreLines
    UnstagedDTCoreLines = $unstagedDtCoreLines
    UntrackedDTCoreLines = $untrackedDtCoreLines
    StagedDTCorePaths = $stagedDtCorePaths
    SubmoduleWorktreeLines = $submoduleWorktreeLines
    Violations = @($violations)
    Summary = [PSCustomObject]@{
        DTCoreInvariantValid = $dtCoreInvariantValid
        DTCoreExpectedCommit = $ExpectedCommit.ToLowerInvariant()
        DTCoreActualCommit = $actualCommit
        DTCoreSubmoduleStatus = if ($statusLine.Count -gt 0) { [string]$statusLine[0] } else { "" }
        DTCoreGitlinkStaged = $dtCoreGitlinkStaged
        DTCoreGitlinkModified = $dtCoreGitlinkModified
        DTCoreWorktreeClean = $dtCoreWorktreeClean
        DTCoreStagedPathCount = $stagedDtCorePaths.Count
        DTCoreModifiedPathCount = $unstagedDtCoreLines.Count
        DTCoreUntrackedPathCount = $untrackedDtCoreLines.Count
        ExpectedCommit = $ExpectedCommit.ToLowerInvariant()
        ActualCommit = $actualCommit
        SubmodulePresent = $submodulePresent
        ExpectedCommitMatches = $expectedCommitMatches
        SubmoduleStatusClean = $submoduleStatusClean
        ParentDTCoreStatusLineCount = $dtCoreStatusLines.Count
        StagedDTCoreLineCount = $stagedDtCoreLines.Count
        UnstagedDTCoreLineCount = $unstagedDtCoreLines.Count
        UntrackedDTCoreLineCount = $untrackedDtCoreLines.Count
        StagedDTCorePathCount = $stagedDtCorePaths.Count
        SubmoduleWorktreeLineCount = $submoduleWorktreeLines.Count
        ViolationCount = $violations.Count
        DryRunOnly = $true
        ReadOnly = $true
        ModifiesSubmodule = $false
        StagesDTCore = $false
        DoesNotUpdateSubmodule = $true
        DoesNotStageFiles = $true
        FailOnViolation = [bool]$FailOnViolation
        Boundary = "DTCore is a submodule and must stay pinned to the expected commit with no parent, staged, or submodule worktree changes unless the user explicitly opens DTCore modification scope."
    }
}

if ($FailOnViolation -and -not $dtCoreInvariantValid) {
    throw "DTCore submodule guard failed. Violations=$($violations.Count) Expected=$ExpectedCommit Actual=$actualCommit"
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host "DTCore submodule guard complete."
    Write-Host "Invariant valid: $($report.Summary.DTCoreInvariantValid)"
    Write-Host "Expected commit: $($report.Summary.ExpectedCommit)"
    Write-Host "Actual commit: $($report.Summary.ActualCommit)"
    Write-Host "Submodule present: $($report.Summary.SubmodulePresent)"
    Write-Host "Expected commit matches: $($report.Summary.ExpectedCommitMatches)"
    Write-Host "Submodule status clean: $($report.Summary.SubmoduleStatusClean)"
    Write-Host "Parent DTCore status lines: $($report.Summary.ParentDTCoreStatusLineCount)"
    Write-Host "Staged DTCore paths: $($report.Summary.StagedDTCorePathCount)"
    Write-Host "Submodule worktree lines: $($report.Summary.SubmoduleWorktreeLineCount)"
    Write-Host "Dry run only: $($report.Summary.DryRunOnly)"
    Write-Host "Stages DTCore: $($report.Summary.StagesDTCore)"
    if ($violations.Count -gt 0) {
        Write-Host "Violations:"
        foreach ($violation in $violations) {
            Write-Host "  $violation"
        }
    }
}
