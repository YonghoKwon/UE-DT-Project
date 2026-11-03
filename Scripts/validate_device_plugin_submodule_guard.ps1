param(
    [string]$ProjectRoot = "",
    # Defaults to the pin recorded in Scripts/device_plugin_pin.txt once the plugin is connected.
    [string]$ExpectedCommit = "",
    # Before DEVICE-PLUGIN-01 stage 2 connects Plugins/DTDevice, "not connected" is a valid state.
    [switch]$RequireConnected,
    [switch]$FailOnViolation,
    [switch]$Json
)

# DEVICE-PLUGIN-01: same fail-closed policy as validate_dtcore_submodule_guard.ps1 for the
# DTDevice plugin submodule. Read-only: never updates, stages or edits the submodule.
$ErrorActionPreference = "Stop"

function Get-GitLines {
    param([string]$WorkingDirectory, [string[]]$GitArgs)
    Push-Location $WorkingDirectory
    try {
        $previous = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $output = @(& git.exe @GitArgs 2>&1 | Where-Object { $_ -notmatch "^warning:" })
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $previous
        if ($exitCode -ne 0) { throw "git $($GitArgs -join ' ') failed with exit code $exitCode`: $($output -join ' ')" }
        return $output
    }
    finally { Pop-Location }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) { throw "ProjectRoot not found: $ProjectRoot" }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$submodulePath = "Plugins/DTDevice"
$submoduleFullPath = Join-Path $ProjectRoot "Plugins\DTDevice"
$pinFile = Join-Path $ProjectRoot "Scripts\device_plugin_pin.txt"
if ([string]::IsNullOrWhiteSpace($ExpectedCommit) -and (Test-Path -LiteralPath $pinFile -PathType Leaf)) {
    $ExpectedCommit = ((Get-Content -LiteralPath $pinFile -Raw) -split "\s+" | Where-Object { $_ -match "^[0-9a-fA-F]{40}$" } | Select-Object -First 1)
}
$ExpectedCommit = ([string]$ExpectedCommit).ToLowerInvariant()

# Registered in .gitmodules = connected. Until then only stray files under the path are violations.
$gitmodules = Join-Path $ProjectRoot ".gitmodules"
$registered = (Test-Path -LiteralPath $gitmodules -PathType Leaf) -and ((Get-Content -LiteralPath $gitmodules -Raw) -match "path\s*=\s*Plugins/DTDevice\s")
$submodulePresent = Test-Path -LiteralPath $submoduleFullPath -PathType Container

$statusLine = ""; $statusPrefix = ""; $actualCommit = ""
if ($registered) {
    $statusLine = [string](@(Get-GitLines -WorkingDirectory $ProjectRoot -GitArgs @("submodule", "status", "--", $submodulePath)) | Select-Object -First 1)
    if ($statusLine -match "^(?<prefix>[ +-U]?)(?<commit>[0-9a-fA-F]{40})\s+(?<path>\S+)") {
        $statusPrefix = $Matches["prefix"]; $actualCommit = $Matches["commit"].ToLowerInvariant()
    }
}
$parentLines = @(Get-GitLines -WorkingDirectory $ProjectRoot -GitArgs @("status", "--porcelain=v1", "--untracked-files=all") | Where-Object { $_ -match "Plugins[/\\]DTDevice" })
$stagedPaths = @(Get-GitLines -WorkingDirectory $ProjectRoot -GitArgs @("diff", "--cached", "--name-only", "--", $submodulePath))
$worktreeLines = @()
if ($registered -and $submodulePresent -and (Test-Path -LiteralPath (Join-Path $submoduleFullPath ".git"))) {
    $worktreeLines = @(Get-GitLines -WorkingDirectory $submoduleFullPath -GitArgs @("status", "--porcelain=v1", "--untracked-files=all"))
}

$violations = [System.Collections.Generic.List[string]]::new()
if ($registered) {
    if (-not $submodulePresent) { $violations.Add("Plugins/DTDevice is registered but missing.") | Out-Null }
    if ([string]::IsNullOrWhiteSpace($ExpectedCommit)) { $violations.Add("No expected DTDevice commit (Scripts/device_plugin_pin.txt).") | Out-Null }
    elseif ($actualCommit -ne $ExpectedCommit) { $violations.Add("Plugins/DTDevice commit mismatch. Expected=$ExpectedCommit Actual=$actualCommit") | Out-Null }
    if (-not [string]::IsNullOrWhiteSpace($statusPrefix)) { $violations.Add("git submodule status prefix is not clean: '$statusPrefix'") | Out-Null }
    if ($worktreeLines.Count -gt 0) { $violations.Add("Plugins/DTDevice submodule worktree is not clean.") | Out-Null }
}
elseif ($RequireConnected) {
    $violations.Add("Plugins/DTDevice is not connected as a submodule.") | Out-Null
}
elseif ($submodulePresent) {
    # A copied plugin folder must not stand in for the pinned submodule.
    $violations.Add("Plugins/DTDevice exists but is not a registered submodule.") | Out-Null
}
if ($stagedPaths.Count -gt 0) { $violations.Add("Plugins/DTDevice has staged changes.") | Out-Null }
if ($parentLines.Count -gt 0) { $violations.Add("Plugins/DTDevice has parent-reported changes.") | Out-Null }

$valid = ($violations.Count -eq 0)
$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    SubmodulePath = $submodulePath
    Connected = $registered
    SubmodulePresent = $submodulePresent
    ExpectedCommit = $ExpectedCommit
    ActualCommit = $actualCommit
    SubmoduleStatusLine = $statusLine
    ParentStatusLines = $parentLines
    StagedPaths = $stagedPaths
    SubmoduleWorktreeLines = $worktreeLines
    Violations = @($violations)
    Summary = [PSCustomObject]@{
        DTDeviceInvariantValid = $valid
        Connected = $registered
        RequireConnected = [bool]$RequireConnected
        ViolationCount = $violations.Count
        ReadOnly = $true
        Boundary = "DTDevice is consumed as a pinned submodule; the host never stages its gitlink or edits it in place unless plugin development scope is explicitly opened."
    }
}

if ($FailOnViolation -and -not $valid) {
    throw "DTDevice submodule guard failed. Violations=$($violations.Count) Expected=$ExpectedCommit Actual=$actualCommit"
}
if ($Json) { $report | ConvertTo-Json -Depth 6 }
else {
    Write-Host "DTDevice submodule guard complete."
    Write-Host "Invariant valid: $valid"
    Write-Host "Connected: $registered"
    Write-Host "Expected commit: $ExpectedCommit"
    Write-Host "Actual commit: $actualCommit"
    foreach ($v in $violations) { Write-Host "  $v" }
}
