param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [switch]$Json,
    [switch]$FailOnGeneratedOutput,
    [switch]$FailOnUnclassifiedUntracked,
    [string[]]$FailOnCategory = @()
)

$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "== $Title =="
}

function Get-PathSummary {
    param([string]$FullPath)

    if (-not (Test-Path -LiteralPath $FullPath)) {
        return [PSCustomObject]@{
            State = "missing"
            Kind = "-"
            FileCount = 0
            SizeBytes = 0
            LastWriteTime = $null
        }
    }

    $item = Get-Item -LiteralPath $FullPath
    if ($item.PSIsContainer) {
        $files = @(Get-ChildItem -LiteralPath $FullPath -Recurse -File -ErrorAction SilentlyContinue)
        $sizeBytes = ($files | Measure-Object -Property Length -Sum).Sum
        return [PSCustomObject]@{
            State = "present"
            Kind = "directory"
            FileCount = $files.Count
            SizeBytes = [int64]($sizeBytes)
            LastWriteTime = $item.LastWriteTime
        }
    }

    return [PSCustomObject]@{
        State = "present"
        Kind = "file"
        FileCount = 1
        SizeBytes = [int64]$item.Length
        LastWriteTime = $item.LastWriteTime
    }
}

function Format-Size {
    param([int64]$Bytes)

    if ($Bytes -ge 1GB) {
        return "{0:N1} GB" -f ($Bytes / 1GB)
    }
    if ($Bytes -ge 1MB) {
        return "{0:N1} MB" -f ($Bytes / 1MB)
    }
    if ($Bytes -ge 1KB) {
        return "{0:N1} KB" -f ($Bytes / 1KB)
    }
    return "$Bytes B"
}

function Normalize-RepoPath {
    param([string]$Path)

    return ($Path -replace "\\", "/").TrimEnd("/").ToLowerInvariant()
}

function Test-PathIsUnderDecisionPoint {
    param(
        [string]$RepoPath,
        [string[]]$DecisionPaths
    )

    $normalizedRepoPath = Normalize-RepoPath $RepoPath
    foreach ($decisionPath in $DecisionPaths) {
        $normalizedDecisionPath = Normalize-RepoPath $decisionPath
        if ($normalizedRepoPath -eq $normalizedDecisionPath -or $normalizedRepoPath.StartsWith("$normalizedDecisionPath/")) {
            return $true
        }
    }

    return $false
}

function Get-DecisionPointNote {
    param(
        [string]$RelativePath,
        [string]$FullPath
    )

    $normalizedPath = Normalize-RepoPath $RelativePath
    if (-not (Test-Path -LiteralPath $FullPath)) {
        return ""
    }

    if ($normalizedPath -eq "content/m7at10/ui/wbp_virtualsensormonitor.uasset") {
        return "Detected binary monitor WBP asset. Open in Unreal Editor, verify optional bindings, and run PIE smoke before staging."
    }

    if ($normalizedPath -ne "config/game.ini") {
        return ""
    }

    $lines = @(Get-Content -LiteralPath $FullPath)
    $hasRuntimeOverride = $false
    $nonEmptyValues = @()
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed -eq "[DTCoreRuntimeOverride]") {
            $hasRuntimeOverride = $true
            continue
        }
        if (-not $hasRuntimeOverride -or [string]::IsNullOrWhiteSpace($trimmed) -or -not $trimmed.Contains("=")) {
            continue
        }

        $parts = $trimmed.Split("=", 2)
        if ($parts.Count -eq 2 -and -not [string]::IsNullOrWhiteSpace($parts[1])) {
            $nonEmptyValues += $parts[0]
        }
    }

    if (-not $hasRuntimeOverride) {
        return "Config/Game.ini exists but does not contain [DTCoreRuntimeOverride]; inspect manually before staging."
    }
    if ($nonEmptyValues.Count -eq 0) {
        return "Detected empty [DTCoreRuntimeOverride]. Treat as local-only runtime override unless shared endpoint defaults are explicitly required."
    }

    return "Detected non-empty [DTCoreRuntimeOverride] values: $($nonEmptyValues -join ', '). Review for endpoint or credential leakage before staging."
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
Push-Location $ProjectRoot
try {
    $uproject = Get-ChildItem -LiteralPath $ProjectRoot -Filter *.uproject -File | Select-Object -First 1

    $pathsToCheck = @(
        [PSCustomObject]@{
            Path = "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset"
            Category = "ReviewCandidate"
            Recommendation = "Open in editor and commit only if this is the intended production monitor WBP."
        },
        [PSCustomObject]@{
            Path = "Config\Game.ini"
            Category = "ReviewCandidate"
            Recommendation = "Diff manually; commit only intentional project setting changes."
        },
        [PSCustomObject]@{
            Path = "Content\ChemicalPlantEnv"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit asset/vendor decision; otherwise keep out of this code change."
        },
        [PSCustomObject]@{
            Path = "Content\Materials"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit asset/material decision."
        },
        [PSCustomObject]@{
            Path = "Content\Mega_Crane"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit crane asset decision."
        },
        [PSCustomObject]@{
            Path = "Content\Meshes"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit mesh asset decision."
        },
        [PSCustomObject]@{
            Path = "Content\Textures"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit texture asset decision."
        },
        [PSCustomObject]@{
            Path = "Samples\PixelStreaming"
            Category = "SampleOrThirdParty"
            Recommendation = "Keep untracked unless Pixel Streaming samples are intentionally added to the project."
        },
        [PSCustomObject]@{
            Path = "Windows.zip"
            Category = "GeneratedOutput"
            Recommendation = "Do not commit packaged output archives."
        },
        [PSCustomObject]@{
            Path = "Windows"
            Category = "GeneratedOutput"
            Recommendation = "Do not commit packaged output directories."
        },
        [PSCustomObject]@{
            Path = "launcher.config.json"
            Category = "GeneratedOrLocalConfig"
            Recommendation = "Keep untracked unless a launcher workflow explicitly requires it."
        }
    )

    $decisionRelativePaths = @($pathsToCheck | ForEach-Object { $_.Path })
    $untrackedGitPaths = @(
        git status --porcelain --untracked-files=all |
            Where-Object { $_.StartsWith("?? ") } |
            ForEach-Object { $_.Substring(3).Trim() }
    )
    $unclassifiedUntrackedPaths = @(
        $untrackedGitPaths |
            Where-Object { -not (Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths $decisionRelativePaths) } |
            Sort-Object
    )

    $decisionPoints = @()
    $presentCount = 0
    $generatedCount = 0
    $presentCategoryCounts = @{}
    foreach ($entry in $pathsToCheck) {
        $relativePath = $entry.Path
        $fullPath = Join-Path $ProjectRoot $relativePath
        $summary = Get-PathSummary -FullPath $fullPath
        $decisionNote = Get-DecisionPointNote -RelativePath $relativePath -FullPath $fullPath
        if ($summary.State -eq "present") {
            ++$presentCount
            if ($entry.Category -like "Generated*") {
                ++$generatedCount
            }
            if (-not $presentCategoryCounts.ContainsKey($entry.Category)) {
                $presentCategoryCounts[$entry.Category] = 0
            }
            ++$presentCategoryCounts[$entry.Category]
        }

        $decisionPoints += [PSCustomObject]@{
            Path = $relativePath
            State = $summary.State
            Kind = $summary.Kind
            FileCount = $summary.FileCount
            SizeBytes = $summary.SizeBytes
            Size = Format-Size $summary.SizeBytes
            LastWriteTime = $summary.LastWriteTime
            Category = $entry.Category
            Recommendation = $entry.Recommendation
            DetectedNote = $decisionNote
        }
    }

    $report = [PSCustomObject]@{
        ProjectRoot = $ProjectRoot
        UProject = if ($uproject) { $uproject.Name } else { $null }
        GitBranch = (git branch --show-current)
        RecentCommits = @(git log --oneline -3)
        GitStatus = @(git status --short)
        UntrackedGitPaths = $untrackedGitPaths
        UnclassifiedUntrackedPaths = $unclassifiedUntrackedPaths
        Submodules = @(git submodule status --recursive)
        DecisionPoints = $decisionPoints
        Summary = [PSCustomObject]@{
            PresentDecisionPoints = $presentCount
            GeneratedOrLocalOutputItemsPresent = $generatedCount
            HasGeneratedOutput = ($generatedCount -gt 0)
            UnclassifiedUntrackedCount = $unclassifiedUntrackedPaths.Count
            HasUnclassifiedUntracked = ($unclassifiedUntrackedPaths.Count -gt 0)
            PresentCategoryCounts = [PSCustomObject]$presentCategoryCounts
            DefaultAction = "Do not stage these paths until each item has an explicit content or packaging decision."
        }
        SuggestedChecks = [PSCustomObject]@{
            Build = "& 'C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat' m7at10_dtEditor Win64 Development '$ProjectRoot\m7at10_dt.uproject' -WaitMutex -NoHotReloadFromIDE"
            Smoke = "powershell -ExecutionPolicy Bypass -File '.\Scripts\run_smoke_tests.ps1' -SkipBuild"
        }
    }

    if ($Json) {
        $report | ConvertTo-Json -Depth 5
    }
    else {
        Write-Section "Project"
        Write-Host "Root: $($report.ProjectRoot)"
        if ($report.UProject) {
            Write-Host "UProject: $($report.UProject)"
        }

        Write-Section "Git"
        Write-Host $report.GitBranch
        $report.RecentCommits | ForEach-Object { Write-Host $_ }
        $report.GitStatus | ForEach-Object { Write-Host $_ }

        Write-Section "Submodules"
        $report.Submodules | ForEach-Object { Write-Host $_ }

        Write-Section "Local asset decision points"
        foreach ($point in $report.DecisionPoints) {
            Write-Host "$($point.Path)"
            Write-Host "  state: $($point.State) $($point.Kind), files=$($point.FileCount), size=$($point.Size)"
            if ($point.LastWriteTime) {
                Write-Host "  lastWriteTime: $($point.LastWriteTime)"
            }
            Write-Host "  category: $($point.Category)"
            Write-Host "  recommendation: $($point.Recommendation)"
            if (-not [string]::IsNullOrWhiteSpace($point.DetectedNote)) {
                Write-Host "  detected: $($point.DetectedNote)"
            }
        }

        Write-Section "Asset decision summary"
        Write-Host "Present decision points: $($report.Summary.PresentDecisionPoints)"
        Write-Host "Generated/local-output items present: $($report.Summary.GeneratedOrLocalOutputItemsPresent)"
        Write-Host "Unclassified untracked paths: $($report.Summary.UnclassifiedUntrackedCount)"
        foreach ($category in ($presentCategoryCounts.Keys | Sort-Object)) {
            Write-Host "Present $category items: $($presentCategoryCounts[$category])"
        }
        Write-Host "Default action: $($report.Summary.DefaultAction)"

        if ($unclassifiedUntrackedPaths.Count -gt 0) {
            Write-Section "Unclassified untracked paths"
            $unclassifiedUntrackedPaths | ForEach-Object { Write-Host $_ }
            Write-Host "Recommendation: classify these paths in Scripts/report_local_project_status.ps1 before staging or committing."
        }

        Write-Section "Suggested next checks"
        Write-Host "Build: $($report.SuggestedChecks.Build)"
        Write-Host "Smoke: $($report.SuggestedChecks.Smoke)"
    }

    if ($FailOnGeneratedOutput -and $generatedCount -gt 0) {
        throw "Generated or local-output items are present. Remove them or run without -FailOnGeneratedOutput after explicitly accepting the local state."
    }
    if ($FailOnUnclassifiedUntracked -and $unclassifiedUntrackedPaths.Count -gt 0) {
        $previewPaths = @($unclassifiedUntrackedPaths | Select-Object -First 10) -join ", "
        throw "Unclassified untracked paths are present: $previewPaths. Classify them in Scripts/report_local_project_status.ps1 or run without -FailOnUnclassifiedUntracked after explicitly accepting the local state."
    }
    if ($FailOnCategory.Count -gt 0) {
        $categoriesToFail = @(
            $FailOnCategory |
                ForEach-Object { $_ -split "," } |
                ForEach-Object { $_.Trim() } |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        )
        $matchedCategories = @($categoriesToFail | Where-Object { $presentCategoryCounts.ContainsKey($_) })
        if ($matchedCategories.Count -gt 0) {
            $details = @($matchedCategories | ForEach-Object { "$_=$($presentCategoryCounts[$_])" }) -join ", "
            throw "Requested local asset categories are present: $details. Remove them or run without -FailOnCategory after explicitly accepting the local state."
        }
    }
}
finally {
    Pop-Location
}
