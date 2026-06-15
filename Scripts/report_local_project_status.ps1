param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [switch]$Json,
    [switch]$FailOnGeneratedOutput,
    [switch]$FailOnUnclassifiedUntracked,
    [switch]$FailOnStagedDecisionPoints,
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

function Get-DirectoryContentSummary {
    param([string]$FullPath)

    if (-not (Test-Path -LiteralPath $FullPath)) {
        return $null
    }

    $item = Get-Item -LiteralPath $FullPath
    if (-not $item.PSIsContainer) {
        return $null
    }

    $files = @(Get-ChildItem -LiteralPath $FullPath -Recurse -File -ErrorAction SilentlyContinue)
    $extensionCounts = @(
        $files |
            Group-Object Extension |
            Sort-Object Count -Descending |
            Select-Object -First 8 |
            ForEach-Object {
                [PSCustomObject]@{
                    Extension = if ([string]::IsNullOrWhiteSpace($_.Name)) { "(none)" } else { $_.Name }
                    Count = $_.Count
                }
            }
    )
    $largestFiles = @(
        $files |
            Sort-Object Length -Descending |
            Select-Object -First 5 |
            ForEach-Object {
                [PSCustomObject]@{
                    Path = $_.FullName.Substring($FullPath.Length).TrimStart("\", "/")
                    SizeBytes = [int64]$_.Length
                    Size = Format-Size ([int64]$_.Length)
                }
            }
    )

    return [PSCustomObject]@{
        ExtensionCounts = $extensionCounts
        LargestFiles = $largestFiles
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

function Test-PathIsAllowedCommitCandidate {
    param([string]$RepoPath)

    $normalizedRepoPath = Normalize-RepoPath $RepoPath
    $extension = [System.IO.Path]::GetExtension($normalizedRepoPath).ToLowerInvariant()
    if ($normalizedRepoPath.StartsWith("source/") -and $extension -in @(".h", ".cpp", ".cs")) {
        return $true
    }
    if ($normalizedRepoPath.StartsWith("scripts/") -and $extension -eq ".ps1") {
        return $true
    }
    if ($normalizedRepoPath.StartsWith("docs/") -and $extension -eq ".md") {
        return $true
    }
    if ($normalizedRepoPath.StartsWith("samples/payload_fixtures/") -and $extension -eq ".json") {
        return $true
    }
    if ($normalizedRepoPath.StartsWith("samples/websocket/") -and $extension -eq ".json") {
        return $true
    }
    if ($normalizedRepoPath.StartsWith("samples/") -and $extension -in @(".csv", ".jsonl")) {
        return $true
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

function Get-DecisionChecklist {
    param(
        [string]$RelativePath,
        [string]$Category
    )

    $normalizedPath = Normalize-RepoPath $RelativePath
    if ($normalizedPath -eq "content/m7at10/ui/wbp_virtualsensormonitor.uasset") {
        return @(
            "Open the widget in Unreal Editor.",
            "Verify optional bindings against docs/widget_designer_setup.md.",
            "Run a PIE smoke test in the intended map.",
            "Confirm camera/LiDAR switching and status text update correctly.",
            "Commit only after accepting the binary asset as the production monitor WBP."
        )
    }

    if ($normalizedPath -eq "config/game.ini") {
        return @(
            "Inspect Config/Game.ini diff manually.",
            "Confirm there are no endpoint, credential, token, or environment-specific values.",
            "Decide whether blank DTCore runtime overrides are local-only or shared project defaults.",
            "Run Scripts/validate_runtime_config_policy.ps1 before staging."
        )
    }

    if ($Category -eq "LargeContentCandidate") {
        return @(
            "Identify asset source, license, and intended project ownership.",
            "Confirm whether maps, WBP assets, or production workflows depend on this folder.",
            "Review size, extension counts, and largest files.",
            "Decide storage/versioning strategy before staging.",
            "Run Scripts/validate_large_content_decision_policy.ps1 after the decision."
        )
    }

    if ($Category -eq "SampleOrThirdParty") {
        return @(
            "Confirm this sample or third-party folder is intentionally owned by the project.",
            "Check license and redistribution terms.",
            "Prefer documenting setup steps instead of committing copied sample content when possible.",
            "Run Scripts/validate_large_content_decision_policy.ps1 after the decision."
        )
    }

    if ($Category -like "Generated*") {
        return @(
            "Keep generated packaging output out of normal source commits.",
            "Remove or ignore this path unless a packaging workflow explicitly requires it.",
            "Run report_local_project_status.ps1 -FailOnGeneratedOutput for a strict clean check."
        )
    }

    return @(
        "Inspect the path manually.",
        "Document why it belongs in the repository before staging."
    )
}

function Get-CommitReadiness {
    param(
        [string]$State,
        [string]$Category
    )

    if ($State -ne "present") {
        return "NotPresent"
    }
    if ($Category -like "Generated*") {
        return "DoNotCommitGeneratedOutput"
    }

    return "BlockedByManualDecision"
}

function Get-ReviewQueue {
    param(
        [string]$CommitReadiness,
        [string]$Category
    )

    if ($CommitReadiness -eq "NotPresent") {
        return "NotPresent"
    }
    if ($CommitReadiness -eq "DoNotCommitGeneratedOutput") {
        return "KeepLocal"
    }
    if ($Category -eq "ReviewCandidate" -or $Category -eq "LargeContentCandidate" -or $Category -eq "SampleOrThirdParty") {
        return "NeedsOwnerDecision"
    }

    return "NeedsOwnerDecision"
}

function Get-DecisionPointGitState {
    param(
        [string]$RelativePath,
        [string[]]$UntrackedGitPaths,
        [string[]]$StagedGitPaths,
        [string[]]$UnstagedGitPaths
    )

    $normalizedPath = Normalize-RepoPath $RelativePath
    $hasUntracked = @($UntrackedGitPaths | Where-Object { Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths @($RelativePath) }).Count -gt 0
    if ($hasUntracked) {
        return "Untracked"
    }

    $hasStaged = @($StagedGitPaths | Where-Object { Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths @($RelativePath) }).Count -gt 0
    if ($hasStaged) {
        return "Staged"
    }

    $hasUnstaged = @($UnstagedGitPaths | Where-Object { Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths @($RelativePath) }).Count -gt 0
    if ($hasUnstaged) {
        return "TrackedModified"
    }

    return "CleanOrIgnored"
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
            DecisionOwner = "ProjectOwnerRequired"
            DecisionStatus = "EvidencePending"
            EvidenceNeeded = @("Editor open verification", "Optional binding check", "PIE smoke result", "Production WBP acceptance")
        },
        [PSCustomObject]@{
            Path = "Config\Game.ini"
            Category = "ReviewCandidate"
            Recommendation = "Diff manually; commit only intentional project setting changes."
            DecisionOwner = "ConfigOwnerRequired"
            DecisionStatus = "PendingOwnerDecision"
            EvidenceNeeded = @("Manual diff review", "No endpoint or credential values", "Shared-defaults decision", "Runtime config policy pass")
        },
        [PSCustomObject]@{
            Path = "Content\ChemicalPlantEnv"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit asset/vendor decision; otherwise keep out of this code change."
            DecisionOwner = "AssetOwnerRequired"
            DecisionStatus = "PendingOwnerDecision"
            EvidenceNeeded = @("Asset source", "License/redistribution approval", "Map dependency evidence", "Size/storage acceptance")
        },
        [PSCustomObject]@{
            Path = "Content\Materials"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit asset/material decision."
            DecisionOwner = "AssetOwnerRequired"
            DecisionStatus = "PendingOwnerDecision"
            EvidenceNeeded = @("Asset source", "License/redistribution approval", "Map or WBP dependency evidence", "Size/storage acceptance")
        },
        [PSCustomObject]@{
            Path = "Content\Mega_Crane"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit crane asset decision."
            DecisionOwner = "AssetOwnerRequired"
            DecisionStatus = "PendingOwnerDecision"
            EvidenceNeeded = @("Asset source", "License/redistribution approval", "Branch scope decision", "Size/storage acceptance")
        },
        [PSCustomObject]@{
            Path = "Content\Meshes"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit mesh asset decision."
            DecisionOwner = "AssetOwnerRequired"
            DecisionStatus = "PendingOwnerDecision"
            EvidenceNeeded = @("Asset source", "License/redistribution approval", "Map or WBP dependency evidence", "Size/storage acceptance")
        },
        [PSCustomObject]@{
            Path = "Content\Textures"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit texture asset decision."
            DecisionOwner = "AssetOwnerRequired"
            DecisionStatus = "PendingOwnerDecision"
            EvidenceNeeded = @("Asset source", "License/redistribution approval", "Map or WBP dependency evidence", "Size/storage acceptance")
        },
        [PSCustomObject]@{
            Path = "Samples\PixelStreaming"
            Category = "SampleOrThirdParty"
            Recommendation = "Keep untracked unless Pixel Streaming samples are intentionally added to the project."
            DecisionOwner = "ProjectOwnerRequired"
            DecisionStatus = "PendingOwnerDecision"
            EvidenceNeeded = @("Project ownership decision", "License/redistribution approval", "Setup documentation alternative considered")
        },
        [PSCustomObject]@{
            Path = "Windows.zip"
            Category = "GeneratedOutput"
            Recommendation = "Do not commit packaged output archives."
            DecisionOwner = "PackagingOwnerRequired"
            DecisionStatus = "DoNotCommitGeneratedOutput"
            EvidenceNeeded = @("Remove or keep ignored as generated output")
        },
        [PSCustomObject]@{
            Path = "Windows"
            Category = "GeneratedOutput"
            Recommendation = "Do not commit packaged output directories."
            DecisionOwner = "PackagingOwnerRequired"
            DecisionStatus = "DoNotCommitGeneratedOutput"
            EvidenceNeeded = @("Remove or keep ignored as generated output")
        },
        [PSCustomObject]@{
            Path = "launcher.config.json"
            Category = "GeneratedOrLocalConfig"
            Recommendation = "Keep untracked unless a launcher workflow explicitly requires it."
            DecisionOwner = "PackagingOwnerRequired"
            DecisionStatus = "DoNotCommitGeneratedOutput"
            EvidenceNeeded = @("Launcher workflow ownership decision", "No environment-specific values")
        }
    )

    $decisionRelativePaths = @($pathsToCheck | ForEach-Object { $_.Path })
    $gitStatusLines = @(git status --porcelain=v1 --untracked-files=all)
    $untrackedGitPaths = @(
        $gitStatusLines |
            Where-Object { $_.StartsWith("?? ") } |
            ForEach-Object { $_.Substring(3).Trim() }
    )
    $unclassifiedUntrackedPaths = @(
        $untrackedGitPaths |
            Where-Object { -not (Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths $decisionRelativePaths) } |
            Where-Object { -not (Test-PathIsAllowedCommitCandidate -RepoPath $_) } |
            Sort-Object
    )
    $allowedCommitCandidatePaths = @(
        $untrackedGitPaths |
            Where-Object { -not (Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths $decisionRelativePaths) } |
            Where-Object { Test-PathIsAllowedCommitCandidate -RepoPath $_ } |
            Sort-Object
    )
    $stagedGitPaths = @(
        git diff --cached --name-only |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            ForEach-Object { $_.Trim() }
    )
    $unstagedGitPaths = @(
        $gitStatusLines |
            Where-Object { -not $_.StartsWith("?? ") -and $_.Length -ge 4 -and $_.Substring(1, 1) -ne " " } |
            ForEach-Object { $_.Substring(3).Trim() }
    )
    $stagedDecisionPaths = @(
        $stagedGitPaths |
            Where-Object { Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths $decisionRelativePaths } |
            Sort-Object
    )

    $decisionPoints = @()
    $presentCount = 0
    $generatedCount = 0
    $presentCategoryCounts = @{}
    $contentSummaryCategories = @("LargeContentCandidate", "SampleOrThirdParty")
    foreach ($entry in $pathsToCheck) {
        $relativePath = $entry.Path
        $fullPath = Join-Path $ProjectRoot $relativePath
        $summary = Get-PathSummary -FullPath $fullPath
        $contentSummary = if ($contentSummaryCategories -contains $entry.Category) { Get-DirectoryContentSummary -FullPath $fullPath } else { $null }
        $decisionNote = Get-DecisionPointNote -RelativePath $relativePath -FullPath $fullPath
        $commitReadiness = Get-CommitReadiness -State $summary.State -Category $entry.Category
        $reviewQueue = Get-ReviewQueue -CommitReadiness $commitReadiness -Category $entry.Category
        $decisionChecklist = Get-DecisionChecklist -RelativePath $relativePath -Category $entry.Category
        $gitState = Get-DecisionPointGitState -RelativePath $relativePath -UntrackedGitPaths $untrackedGitPaths -StagedGitPaths $stagedGitPaths -UnstagedGitPaths $unstagedGitPaths
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
            DecisionOwner = $entry.DecisionOwner
            DecisionStatus = $entry.DecisionStatus
            EvidenceNeeded = $entry.EvidenceNeeded
            DetectedNote = $decisionNote
            GitState = $gitState
            CommitReadiness = $commitReadiness
            ReviewQueue = $reviewQueue
            DecisionChecklist = $decisionChecklist
            ContentSummary = $contentSummary
        }
    }

    $report = [PSCustomObject]@{
        ProjectRoot = $ProjectRoot
        UProject = if ($uproject) { $uproject.Name } else { $null }
        GitBranch = (git branch --show-current)
        RecentCommits = @(git log --oneline -3)
        GitStatus = @(git status --short)
        UntrackedGitPaths = $untrackedGitPaths
        AllowedCommitCandidatePaths = $allowedCommitCandidatePaths
        UnclassifiedUntrackedPaths = $unclassifiedUntrackedPaths
        StagedGitPaths = $stagedGitPaths
        StagedDecisionPaths = $stagedDecisionPaths
        Submodules = @(git submodule status --recursive)
        DecisionPoints = $decisionPoints
        Summary = [PSCustomObject]@{
            PresentDecisionPoints = $presentCount
            GeneratedOrLocalOutputItemsPresent = $generatedCount
            HasGeneratedOutput = ($generatedCount -gt 0)
            AllowedCommitCandidateCount = $allowedCommitCandidatePaths.Count
            UnclassifiedUntrackedCount = $unclassifiedUntrackedPaths.Count
            HasUnclassifiedUntracked = ($unclassifiedUntrackedPaths.Count -gt 0)
            StagedDecisionPointCount = $stagedDecisionPaths.Count
            HasStagedDecisionPoints = ($stagedDecisionPaths.Count -gt 0)
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
            Write-Host "  gitState: $($point.GitState)"
            Write-Host "  commitReadiness: $($point.CommitReadiness)"
            Write-Host "  reviewQueue: $($point.ReviewQueue)"
            Write-Host "  recommendation: $($point.Recommendation)"
            Write-Host "  decisionOwner: $($point.DecisionOwner)"
            Write-Host "  decisionStatus: $($point.DecisionStatus)"
            if ($point.EvidenceNeeded.Count -gt 0) {
                Write-Host "  evidence needed:"
                foreach ($evidence in $point.EvidenceNeeded) {
                    Write-Host "    - $evidence"
                }
            }
            if (-not [string]::IsNullOrWhiteSpace($point.DetectedNote)) {
                Write-Host "  detected: $($point.DetectedNote)"
            }
            if ($point.DecisionChecklist.Count -gt 0) {
                Write-Host "  decision checklist:"
                foreach ($check in $point.DecisionChecklist) {
                    Write-Host "    - $check"
                }
            }
            if ($point.ContentSummary) {
                $extensions = @($point.ContentSummary.ExtensionCounts | ForEach-Object { "$($_.Extension)=$($_.Count)" }) -join ", "
                if (-not [string]::IsNullOrWhiteSpace($extensions)) {
                    Write-Host "  extensions: $extensions"
                }
                if ($point.ContentSummary.LargestFiles.Count -gt 0) {
                    Write-Host "  largest files:"
                    foreach ($largestFile in $point.ContentSummary.LargestFiles) {
                        Write-Host "    $($largestFile.Size) $($largestFile.Path)"
                    }
                }
            }
        }

        Write-Section "Asset decision summary"
        Write-Host "Present decision points: $($report.Summary.PresentDecisionPoints)"
        Write-Host "Generated/local-output items present: $($report.Summary.GeneratedOrLocalOutputItemsPresent)"
        Write-Host "Allowed code/doc commit candidates: $($report.Summary.AllowedCommitCandidateCount)"
        Write-Host "Unclassified untracked paths: $($report.Summary.UnclassifiedUntrackedCount)"
        Write-Host "Staged decision-point paths: $($report.Summary.StagedDecisionPointCount)"
        foreach ($category in ($presentCategoryCounts.Keys | Sort-Object)) {
            Write-Host "Present $category items: $($presentCategoryCounts[$category])"
        }
        Write-Host "Default action: $($report.Summary.DefaultAction)"

        if ($allowedCommitCandidatePaths.Count -gt 0) {
            Write-Section "Allowed code/doc commit candidates"
            $allowedCommitCandidatePaths | ForEach-Object { Write-Host $_ }
            Write-Host "Recommendation: review and stage these only when they belong to the current code/documentation change."
        }

        if ($unclassifiedUntrackedPaths.Count -gt 0) {
            Write-Section "Unclassified untracked paths"
            $unclassifiedUntrackedPaths | ForEach-Object { Write-Host $_ }
            Write-Host "Recommendation: classify these paths in Scripts/report_local_project_status.ps1 before staging or committing."
        }

        if ($stagedDecisionPaths.Count -gt 0) {
            Write-Section "Staged decision-point paths"
            $stagedDecisionPaths | ForEach-Object { Write-Host $_ }
            Write-Host "Recommendation: unstage these paths unless each one has an explicit content or packaging decision."
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
    if ($FailOnStagedDecisionPoints -and $stagedDecisionPaths.Count -gt 0) {
        $previewPaths = @($stagedDecisionPaths | Select-Object -First 10) -join ", "
        throw "Decision-point paths are staged: $previewPaths. Unstage them or run without -FailOnStagedDecisionPoints after explicitly accepting the content decision."
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
