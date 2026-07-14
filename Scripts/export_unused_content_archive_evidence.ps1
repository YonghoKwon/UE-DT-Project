param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [string]$ArchiveRoot = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Convert-ToSizeText {
    param([int64]$Bytes)

    if ($Bytes -ge 1GB) { return "{0:N1} GB" -f ($Bytes / 1GB) }
    if ($Bytes -ge 1MB) { return "{0:N1} MB" -f ($Bytes / 1MB) }
    if ($Bytes -ge 1KB) { return "{0:N1} KB" -f ($Bytes / 1KB) }
    return "$Bytes B"
}

function Get-DirectorySizeBytes {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return [int64]0
    }

    $sum = (Get-ChildItem -LiteralPath $Path -Recurse -Force -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    return [int64]$sum
}

function Convert-ToMarkdownCell {
    param([object]$Value)
    if ($null -eq $Value) { return "" }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
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

function Get-GitPorcelain {
    param([string]$WorkingDirectory)

    if (-not (Test-Path -LiteralPath (Join-Path $WorkingDirectory ".git"))) {
        return @()
    }

    Push-Location $WorkingDirectory
    try {
        $output = @(& git.exe status --porcelain=v1 --untracked-files=all 2>$null)
        if ($LASTEXITCODE -ne 0) {
            return @()
        }
        return $output
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "ProjectRoot not found: $ProjectRoot"
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ([string]::IsNullOrWhiteSpace($ArchiveRoot)) {
    $projectParent = Split-Path -Parent $ProjectRoot
    $projectName = Split-Path -Leaf $ProjectRoot
    $ArchiveRoot = Join-Path $projectParent ("{0}_UnusedContentArchive" -f $projectName)
}

$knownUnusedPaths = @(
    "Content\ChemicalPlantEnv",
    "Content\Mega_Crane",
    "Content\Materials",
    "Content\Meshes",
    "Content\Textures"
)

$archiveRootExists = Test-Path -LiteralPath $ArchiveRoot -PathType Container
$archiveRootFullPath = if ($archiveRootExists) { (Resolve-Path -LiteralPath $ArchiveRoot).Path } else { [System.IO.Path]::GetFullPath($ArchiveRoot) }
$projectFullPath = [System.IO.Path]::GetFullPath($ProjectRoot)
$archiveRootOutsideProject = -not $archiveRootFullPath.StartsWith($projectFullPath.TrimEnd("\") + "\", [System.StringComparison]::OrdinalIgnoreCase)
$archiveRuns = @()
if ($archiveRootExists) {
    $archiveRuns = @(
        Get-ChildItem -LiteralPath $ArchiveRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
    )
}
$latestArchiveRun = if ($archiveRuns.Count -gt 0) { $archiveRuns[0].FullName } else { "" }

$pathEvidence = @()
foreach ($relativePath in $knownUnusedPaths) {
    $projectPath = Join-Path $ProjectRoot $relativePath
    $presentInProject = Test-Path -LiteralPath $projectPath -PathType Container
    $archiveMatches = @()
    if ($archiveRootExists) {
        $archiveMatches = @(
            foreach ($run in $archiveRuns) {
                $candidateArchivePath = Join-Path $run.FullName $relativePath
                if (Test-Path -LiteralPath $candidateArchivePath -PathType Container) {
                    $candidateArchivePath
                }
            }
        )
    }
    $latestArchivedPath = if ($archiveMatches.Count -gt 0) { [string]$archiveMatches[0] } else { "" }
    $archived = -not [string]::IsNullOrWhiteSpace($latestArchivedPath)
    $sizeBytes = if ($archived) { Get-DirectorySizeBytes -Path $latestArchivedPath } else { [int64]0 }

    $pathEvidence += [PSCustomObject]@{
        Path = $relativePath
        ProjectPath = $projectPath
        PresentInProject = $presentInProject
        Archived = $archived
        LatestArchivedPath = $latestArchivedPath
        ArchivedSizeBytes = $sizeBytes
        ArchivedSize = Convert-ToSizeText -Bytes $sizeBytes
        EvidenceStatus = if ((-not $presentInProject) -and $archived) { "ArchivedOutsideProject" } elseif ($presentInProject) { "StillPresentInProject" } else { "AbsentWithoutArchiveEvidence" }
    }
}

$presentInProjectCount = @($pathEvidence | Where-Object { $_.PresentInProject }).Count
$archivedCount = @($pathEvidence | Where-Object { $_.Archived }).Count
$missingArchiveEvidenceCount = @($pathEvidence | Where-Object { -not $_.PresentInProject -and -not $_.Archived }).Count
$archivedBytes = [int64](($pathEvidence | Measure-Object -Property ArchivedSizeBytes -Sum).Sum)
$projectGitLines = @(Get-GitPorcelain -WorkingDirectory $ProjectRoot)
$archiveRootGitLines = if ($archiveRootExists) { @(Get-GitPorcelain -WorkingDirectory $ArchiveRoot) } else { @() }
$archiveRootStagedLines = @($archiveRootGitLines | Where-Object { $_ -match "^[MADRCU]" })
$dtCoreTouchedLines = @($projectGitLines | Where-Object { $_ -match "Plugins[/\\]DTCore" })

$report = [PSCustomObject]@{
    SchemaVersion = 1
    GeneratedAt = (Get-Date).ToString("s")
    ProjectRoot = $ProjectRoot
    ArchiveRoot = $ArchiveRoot
    ArchiveRootExists = $archiveRootExists
    ArchiveRootOutsideProject = $archiveRootOutsideProject
    LatestArchiveRun = $latestArchiveRun
    KnownUnusedPathCount = $knownUnusedPaths.Count
    PresentInProjectCount = $presentInProjectCount
    ArchivedCount = $archivedCount
    MissingArchiveEvidenceCount = $missingArchiveEvidenceCount
    ArchivedBytes = $archivedBytes
    ArchivedSize = Convert-ToSizeText -Bytes $archivedBytes
    ArchiveRootGitState = if ($archiveRootGitLines.Count -gt 0) { "GitWorktreeWithChanges" } elseif (Test-Path -LiteralPath (Join-Path $ArchiveRoot ".git")) { "GitWorktreeClean" } else { "NotGitWorktree" }
    ArchiveRootStagedFileCount = $archiveRootStagedLines.Count
    DTCoreTouchedFileCount = $dtCoreTouchedLines.Count
    DeletesFiles = $false
    StagesFiles = $false
    ModifiesAssets = $false
    PathEvidence = $pathEvidence
    Summary = [PSCustomObject]@{
        KnownUnusedPathCount = $knownUnusedPaths.Count
        PresentInProjectCount = $presentInProjectCount
        ArchivedCount = $archivedCount
        MissingArchiveEvidenceCount = $missingArchiveEvidenceCount
        ArchiveRootExists = $archiveRootExists
        ArchiveRootOutsideProject = $archiveRootOutsideProject
        LatestArchiveRun = $latestArchiveRun
        ArchivedBytes = $archivedBytes
        ArchivedSize = Convert-ToSizeText -Bytes $archivedBytes
        ArchiveRootGitState = if ($archiveRootGitLines.Count -gt 0) { "GitWorktreeWithChanges" } elseif (Test-Path -LiteralPath (Join-Path $ArchiveRoot ".git")) { "GitWorktreeClean" } else { "NotGitWorktree" }
        ArchiveRootStagedFileCount = $archiveRootStagedLines.Count
        DTCoreTouchedFileCount = $dtCoreTouchedLines.Count
        ArchiveEvidenceComplete = ($presentInProjectCount -eq 0 -and $archivedCount -eq $knownUnusedPaths.Count -and $missingArchiveEvidenceCount -eq 0 -and $archiveRootOutsideProject -and $archiveRootStagedLines.Count -eq 0 -and $dtCoreTouchedLines.Count -eq 0)
        DeletesFiles = $false
        StagesFiles = $false
        ModifiesAssets = $false
        Boundary = "Archive evidence proves local unused Content folders were moved outside the Unreal project after reference checks; it is not repository acceptance, deletion approval, or permission to stage archived asset folders."
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Unused Content Archive Evidence") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- Generated: $($report.GeneratedAt)") | Out-Null
$lines.Add("- Project: $($report.ProjectRoot)") | Out-Null
$lines.Add("- Archive root: $($report.ArchiveRoot)") | Out-Null
$lines.Add("- Archive root exists: $($report.ArchiveRootExists)") | Out-Null
$lines.Add("- Archive root outside project: $($report.ArchiveRootOutsideProject)") | Out-Null
$lines.Add("- Latest archive run: $($report.LatestArchiveRun)") | Out-Null
$lines.Add("- Known unused paths: $($report.KnownUnusedPathCount)") | Out-Null
$lines.Add("- Present in project: $($report.PresentInProjectCount)") | Out-Null
$lines.Add("- Archived count: $($report.ArchivedCount)") | Out-Null
$lines.Add("- Missing archive evidence: $($report.MissingArchiveEvidenceCount)") | Out-Null
$lines.Add("- Archive evidence complete: $($report.Summary.ArchiveEvidenceComplete)") | Out-Null
$lines.Add("- Archived size: $($report.ArchivedSize)") | Out-Null
$lines.Add("- Archive root git state: $($report.ArchiveRootGitState)") | Out-Null
$lines.Add("- Archive root staged file count: $($report.ArchiveRootStagedFileCount)") | Out-Null
$lines.Add("- DTCore touched file count: $($report.DTCoreTouchedFileCount)") | Out-Null
$lines.Add("- Deletes files: $($report.DeletesFiles)") | Out-Null
$lines.Add("- Stages files: $($report.StagesFiles)") | Out-Null
$lines.Add("- Modifies assets: $($report.ModifiesAssets)") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Path | Present in project | Archived | Archived size | Latest archived path | Status |") | Out-Null
$lines.Add("| --- | --- | --- | ---: | --- | --- |") | Out-Null
foreach ($item in $report.PathEvidence) {
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} |" -f `
        (Convert-ToMarkdownCell $item.Path),
        (Convert-ToMarkdownCell $item.PresentInProject),
        (Convert-ToMarkdownCell $item.Archived),
        (Convert-ToMarkdownCell $item.ArchivedSize),
        (Convert-ToMarkdownCell $item.LatestArchivedPath),
        (Convert-ToMarkdownCell $item.EvidenceStatus))) | Out-Null
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
