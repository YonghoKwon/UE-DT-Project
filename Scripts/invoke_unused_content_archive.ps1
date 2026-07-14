param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$ArchiveRoot = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Execute,
    [switch]$ConfirmReferenceChecks,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-RequiredDirectory {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Label not found: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Convert-ToSizeText {
    param([int64]$Bytes)

    if ($Bytes -ge 1GB) { return "{0:N1} GB" -f ($Bytes / 1GB) }
    if ($Bytes -ge 1MB) { return "{0:N1} MB" -f ($Bytes / 1MB) }
    if ($Bytes -ge 1KB) { return "{0:N1} KB" -f ($Bytes / 1KB) }
    return "$Bytes B"
}

function Test-IsUnderPath {
    param(
        [string]$ChildPath,
        [string]$ParentPath
    )

    $childFull = [System.IO.Path]::GetFullPath($ChildPath).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $parentFull = [System.IO.Path]::GetFullPath($ParentPath).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    return $childFull.StartsWith($parentFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
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

function Convert-ToMarkdownCell {
    param([object]$Value)
    if ($null -eq $Value) { return "" }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

$ProjectRoot = Resolve-RequiredDirectory -Path $ProjectRoot -Label "ProjectRoot"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cleanupPlanScript = Join-Path $scriptRoot "export_large_content_cleanup_plan.ps1"
if (-not (Test-Path -LiteralPath $cleanupPlanScript -PathType Leaf)) {
    throw "export_large_content_cleanup_plan.ps1 not found: $cleanupPlanScript"
}

$planJson = & powershell -ExecutionPolicy Bypass -File $cleanupPlanScript -ProjectRoot $ProjectRoot -Json
if ($LASTEXITCODE -ne 0) {
    throw "Cleanup plan failed with exit code $LASTEXITCODE"
}
$plan = $planJson | ConvertFrom-Json

$effectiveArchiveRoot = $ArchiveRoot
if ([string]::IsNullOrWhiteSpace($effectiveArchiveRoot)) {
    $projectParent = Split-Path -Parent $ProjectRoot
    $projectName = Split-Path -Leaf $ProjectRoot
    $effectiveArchiveRoot = Join-Path $projectParent ("{0}_UnusedContentArchive" -f $projectName)
}

$archiveStamp = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss")
$archiveRunRoot = Join-Path $effectiveArchiveRoot $archiveStamp

if ($Execute -and -not $ConfirmReferenceChecks) {
    throw "Archive execution requires -ConfirmReferenceChecks. Run Unreal reference/dependency checks first."
}
if ($Execute -and [string]::IsNullOrWhiteSpace($ArchiveRoot)) {
    throw "Archive execution requires an explicit -ArchiveRoot outside the project."
}
if ($Execute) {
    $archiveParent = Split-Path -Parent $archiveRunRoot
    if ([string]::IsNullOrWhiteSpace($archiveParent)) {
        throw "ArchiveRoot must resolve to a normal filesystem path."
    }
    New-Item -ItemType Directory -Force -Path $archiveParent | Out-Null
    $resolvedArchiveParent = (Resolve-Path -LiteralPath $archiveParent).Path
    if (Test-IsUnderPath -ChildPath $resolvedArchiveParent -ParentPath $ProjectRoot) {
        throw "ArchiveRoot must be outside the project root to avoid reintroducing large untracked content: $resolvedArchiveParent"
    }
    New-Item -ItemType Directory -Force -Path $archiveRunRoot | Out-Null
    $archiveRunRoot = (Resolve-Path -LiteralPath $archiveRunRoot).Path
}

$actions = [System.Collections.Generic.List[object]]::new()
foreach ($candidate in @($plan.Candidates)) {
    $sourcePath = [string]$candidate.FullPath
    $relativePath = [string]$candidate.Path
    $exists = Test-Path -LiteralPath $sourcePath -PathType Container
    $normalizedRelative = ($relativePath -replace "/", "\").TrimStart("\")
    $destinationPath = Join-Path $archiveRunRoot $normalizedRelative
    $safeSource = $false
    if ($exists) {
        $resolvedSource = (Resolve-Path -LiteralPath $sourcePath).Path
        $safeSource = Test-IsUnderPath -ChildPath $resolvedSource -ParentPath $ProjectRoot
    }

    $status = "PreviewOnly"
    $message = "No filesystem changes requested."
    if ($Execute) {
        if (-not $exists) {
            $status = "SkippedMissing"
            $message = "Source folder does not exist."
        }
        elseif (-not $safeSource) {
            $status = "BlockedUnsafeSource"
            $message = "Source path is not safely under ProjectRoot."
        }
        else {
            $destinationParent = Split-Path -Parent $destinationPath
            New-Item -ItemType Directory -Force -Path $destinationParent | Out-Null
            Move-Item -LiteralPath $sourcePath -Destination $destinationPath
            $status = "Archived"
            $message = "Moved local unused content folder to archive root."
        }
    }

    $actions.Add([PSCustomObject]@{
        Path = $relativePath
        SourcePath = $sourcePath
        DestinationPath = $destinationPath
        Exists = $exists
        SafeSourceUnderProjectRoot = $safeSource
        SizeBytes = [int64]$candidate.SizeBytes
        Size = [string]$candidate.Size
        FileCount = [int]$candidate.FileCount
        ExecuteRequested = [bool]$Execute
        ReferenceChecksConfirmed = [bool]$ConfirmReferenceChecks
        DeletesFiles = $false
        StagesFiles = $false
        ModifiesAssets = $false
        Status = $status
        Message = $message
    }) | Out-Null
}

$archivedActions = @($actions | Where-Object { $_.Status -eq "Archived" })
$previewActions = @($actions | Where-Object { $_.Status -eq "PreviewOnly" })
$blockedActions = @($actions | Where-Object { $_.Status -like "Blocked*" })
$skippedActions = @($actions | Where-Object { $_.Status -like "Skipped*" })
$archivedBytes = [int64](($archivedActions | Measure-Object -Property SizeBytes -Sum).Sum)

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    ArchiveRoot = $effectiveArchiveRoot
    ArchiveRunRoot = $archiveRunRoot
    ExecuteRequested = [bool]$Execute
    PreviewOnly = [bool](-not $Execute)
    ReferenceChecksConfirmed = [bool]$ConfirmReferenceChecks
    DeletesFiles = $false
    StagesFiles = $false
    ModifiesAssets = $false
    CandidateCount = @($actions).Count
    PreviewActionCount = $previewActions.Count
    ArchivedActionCount = $archivedActions.Count
    BlockedActionCount = $blockedActions.Count
    SkippedActionCount = $skippedActions.Count
    ArchivedBytes = $archivedBytes
    ArchivedSize = Convert-ToSizeText -Bytes $archivedBytes
    Actions = @($actions)
    Summary = [PSCustomObject]@{
        CandidateCount = @($actions).Count
        PreviewOnly = [bool](-not $Execute)
        ExecuteRequested = [bool]$Execute
        ReferenceChecksConfirmed = [bool]$ConfirmReferenceChecks
        RequiresExplicitArchiveRootForExecute = $true
        DeletesFiles = $false
        StagesFiles = $false
        ModifiesAssets = $false
        PreviewActionCount = $previewActions.Count
        ArchivedActionCount = $archivedActions.Count
        BlockedActionCount = $blockedActions.Count
        SkippedActionCount = $skippedActions.Count
        ArchivedBytes = $archivedBytes
        ArchivedSize = Convert-ToSizeText -Bytes $archivedBytes
        SafeDefault = (-not $Execute)
        Boundary = "Default mode previews archive actions only. Execution moves local unused folders to an explicit archive root outside the project after reference checks; it never deletes files or stages git changes."
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Unused Content Archive Report") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($report.ProjectRoot)") | Out-Null
$lines.Add("- Archive root: $($report.ArchiveRoot)") | Out-Null
$lines.Add("- Preview only: $($report.PreviewOnly)") | Out-Null
$lines.Add("- Execute requested: $($report.ExecuteRequested)") | Out-Null
$lines.Add("- Reference checks confirmed: $($report.ReferenceChecksConfirmed)") | Out-Null
$lines.Add("- Deletes files: $($report.DeletesFiles)") | Out-Null
$lines.Add("- Stages files: $($report.StagesFiles)") | Out-Null
$lines.Add("- Modifies assets: $($report.ModifiesAssets)") | Out-Null
$lines.Add("- Archived actions: $($report.ArchivedActionCount)") | Out-Null
$lines.Add("- Archived size: $($report.ArchivedSize)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Actions") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Path | Size | Files | Exists | Status | Destination | Message |") | Out-Null
$lines.Add("| --- | ---: | ---: | --- | --- | --- | --- |") | Out-Null
foreach ($action in $report.Actions) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} |" -f `
        (Convert-ToMarkdownCell $action.Path),
        (Convert-ToMarkdownCell $action.Size),
        (Convert-ToMarkdownCell $action.FileCount),
        (Convert-ToMarkdownCell $action.Exists),
        (Convert-ToMarkdownCell $action.Status),
        (Convert-ToMarkdownCell $action.DestinationPath),
        (Convert-ToMarkdownCell $action.Message))) | Out-Null
}

if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-TextFile -Path $MarkdownPath -Lines $lines
}
if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    $lines | ForEach-Object { Write-Host $_ }
}
