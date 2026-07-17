param(
    [string]$ProjectRoot = "C:\Unreal Projects\ma0t10_dt",
    [switch]$Json,
    [switch]$FailOnGeneratedOutput,
    [switch]$FailOnUnclassifiedUntracked,
    [switch]$FailOnStagedDecisionPoints,
    [string[]]$FailOnCategory = @(),
    [string]$EvidencePath = ""
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
    if ($normalizedRepoPath -eq "docs/local_asset_decisions.evidence.json") {
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

    if ($normalizedPath -eq "content/ma0t10/ui/wbp_virtualsensormonitor.uasset") {
        return "Detected binary monitor WBP asset. Open in Unreal Editor, verify optional bindings, and run PIE smoke before staging."
    }

    if ($normalizedPath -ne "config/game.ini") {
        return ""
    }

    $lines = @(Get-Content -LiteralPath $FullPath)
    $hasRuntimeOverride = $false
    $inRuntimeOverride = $false
    $nonEmptyValues = @()
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $inRuntimeOverride = ($trimmed -eq "[DTCoreRuntimeOverride]")
            if ($inRuntimeOverride) {
                $hasRuntimeOverride = $true
            }
            continue
        }
        if (-not $inRuntimeOverride -or [string]::IsNullOrWhiteSpace($trimmed) -or -not $trimmed.Contains("=")) {
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

function Test-EmptyRuntimeOverrideConfig {
    param(
        [string]$RelativePath,
        [string]$FullPath
    )

    if ((Normalize-RepoPath $RelativePath) -ne "config/game.ini") {
        return $false
    }
    if (-not (Test-Path -LiteralPath $FullPath -PathType Leaf)) {
        return $false
    }

    $lines = @(Get-Content -LiteralPath $FullPath)
    $hasRuntimeOverride = $false
    $inRuntimeOverride = $false
    $nonEmptyValues = @()
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $inRuntimeOverride = ($trimmed -eq "[DTCoreRuntimeOverride]")
            if ($inRuntimeOverride) {
                $hasRuntimeOverride = $true
            }
            continue
        }
        if (-not $inRuntimeOverride -or [string]::IsNullOrWhiteSpace($trimmed) -or -not $trimmed.Contains("=")) {
            continue
        }

        $parts = $trimmed.Split("=", 2)
        if ($parts.Count -eq 2 -and -not [string]::IsNullOrWhiteSpace($parts[1])) {
            $nonEmptyValues += $parts[0]
        }
    }

    return ($hasRuntimeOverride -and $nonEmptyValues.Count -eq 0)
}

function Get-DecisionChecklist {
    param(
        [string]$RelativePath,
        [string]$Category
    )

    $normalizedPath = Normalize-RepoPath $RelativePath
    if ($normalizedPath -in @(
        "content/ma0t10/ui/wbp_virtualsensormonitor.uasset",
        "content/ma0t10/ui/wbp_virtualsensorsettings.uasset",
        "content/ma0t10/ui/wbp_virtualsensorcaptureexport.uasset"
    )) {
        return @(
            "Open the widget in Unreal Editor.",
            "Verify the native parent and optional bindings against docs/sensor_test_map_setup.ko.md.",
            "Run a PIE smoke test in the intended map.",
            "Confirm the panel-specific sensor controls and status update correctly.",
            "Commit only after accepting the binary asset as a repository-owned WBP."
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

function Test-UnusedLocalContentPath {
    param([string]$RelativePath)

    $normalizedPath = Normalize-RepoPath $RelativePath
    return $normalizedPath -in @(
        "content/chemicalplantenv",
        "content/materials",
        "content/mega_crane",
        "content/meshes",
        "content/textures"
    )
}

function Get-PropertyValue {
    param(
        [object]$Object,
        [string]$Name
    )

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Resolve-DecisionEvidencePath {
    param(
        [string]$ProjectRoot,
        [string]$EvidencePath
    )

    if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
        return Join-Path $ProjectRoot "docs\local_asset_decisions.evidence.json"
    }
    if ([System.IO.Path]::IsPathRooted($EvidencePath)) {
        return $EvidencePath
    }
    return Join-Path $ProjectRoot $EvidencePath
}

function Import-DecisionEvidence {
    param(
        [string]$ProjectRoot,
        [string]$EvidencePath
    )

    $resolvedPath = Resolve-DecisionEvidencePath -ProjectRoot $ProjectRoot -EvidencePath $EvidencePath
    $decisionsByPath = @{}
    if (-not (Test-Path -LiteralPath $resolvedPath)) {
        return [PSCustomObject]@{
            Path = $resolvedPath
            Exists = $false
            DecisionsByPath = $decisionsByPath
        }
    }

    $document = Get-Content -LiteralPath $resolvedPath -Raw | ConvertFrom-Json
    foreach ($decision in @($document.Decisions)) {
        $decisionPath = [string](Get-PropertyValue -Object $decision -Name "Path")
        if ([string]::IsNullOrWhiteSpace($decisionPath)) {
            continue
        }
        $normalizedDecisionPath = Normalize-RepoPath $decisionPath
        if ($decisionsByPath.ContainsKey($normalizedDecisionPath)) {
            throw "Duplicate decision evidence path after normalization: $decisionPath"
        }
        $decisionsByPath[$normalizedDecisionPath] = $decision
    }

    return [PSCustomObject]@{
        Path = $resolvedPath
        Exists = $true
        DecisionsByPath = $decisionsByPath
    }
}

function Get-EvidenceItemStatus {
    param(
        [object[]]$EvidenceItems,
        [string]$EvidenceName
    )

    foreach ($item in @($EvidenceItems)) {
        $name = [string](Get-PropertyValue -Object $item -Name "Name")
        if ($name -ne $EvidenceName) {
            continue
        }

        return [string](Get-PropertyValue -Object $item -Name "Status")
    }

    return ""
}

function Get-EvidenceItemSource {
    param(
        [object[]]$EvidenceItems,
        [string]$EvidenceName
    )

    foreach ($item in @($EvidenceItems)) {
        $name = [string](Get-PropertyValue -Object $item -Name "Name")
        if ($name -ne $EvidenceName) {
            continue
        }

        return [string](Get-PropertyValue -Object $item -Name "Source")
    }

    return ""
}

function Get-DecisionEvidenceReview {
    param(
        [object]$Entry,
        [object]$EvidenceRecord
    )

    if ($Entry.Category -like "Generated*") {
        return [PSCustomObject]@{
            EvidenceReviewStatus = "GeneratedOutput"
            EvidenceComplete = $false
            MissingEvidence = @()
            EvidenceRecordFound = $false
            EvidenceAcceptedBy = ""
            EvidenceAcceptedAt = ""
            EvidenceAcceptedSource = ""
            EffectiveDecisionStatus = $Entry.DecisionStatus
            CommitReadinessOverride = "DoNotCommitGeneratedOutput"
        }
    }

    if ($null -eq $EvidenceRecord) {
        if ($Entry.DecisionStatus -eq "KeepLocal") {
            return [PSCustomObject]@{
                EvidenceReviewStatus = "KeepLocalByDefault"
                EvidenceComplete = $false
                MissingEvidence = @()
                EvidenceRecordFound = $false
                EvidenceAcceptedBy = ""
                EvidenceAcceptedAt = ""
                EvidenceAcceptedSource = ""
                EffectiveDecisionStatus = $Entry.DecisionStatus
                CommitReadinessOverride = "KeepLocalByDecision"
            }
        }

        return [PSCustomObject]@{
            EvidenceReviewStatus = "NoEvidenceRecord"
            EvidenceComplete = $false
            MissingEvidence = @($Entry.EvidenceNeeded)
            EvidenceRecordFound = $false
            EvidenceAcceptedBy = ""
            EvidenceAcceptedAt = ""
            EvidenceAcceptedSource = ""
            EffectiveDecisionStatus = $Entry.DecisionStatus
            CommitReadinessOverride = ""
        }
    }

    $decisionStatus = [string](Get-PropertyValue -Object $EvidenceRecord -Name "DecisionStatus")
    if ([string]::IsNullOrWhiteSpace($decisionStatus)) {
        $decisionStatus = $Entry.DecisionStatus
    }

    $evidenceItems = @((Get-PropertyValue -Object $EvidenceRecord -Name "Evidence"))
    $missingEvidence = @()
    foreach ($evidenceName in @($Entry.EvidenceNeeded)) {
        $status = Get-EvidenceItemStatus -EvidenceItems $evidenceItems -EvidenceName $evidenceName
        $source = Get-EvidenceItemSource -EvidenceItems $evidenceItems -EvidenceName $evidenceName
        if ($status -ne "Complete" -or [string]::IsNullOrWhiteSpace($source)) {
            $missingEvidence += $evidenceName
        }
    }

    $acceptedBy = [string](Get-PropertyValue -Object $EvidenceRecord -Name "AcceptedBy")
    $acceptedAt = [string](Get-PropertyValue -Object $EvidenceRecord -Name "AcceptedAt")
    $acceptedSource = [string](Get-PropertyValue -Object $EvidenceRecord -Name "EvidenceSource")
    $hasAcceptance = -not [string]::IsNullOrWhiteSpace($acceptedBy) -and -not [string]::IsNullOrWhiteSpace($acceptedAt) -and -not [string]::IsNullOrWhiteSpace($acceptedSource)
    $evidenceComplete = $missingEvidence.Count -eq 0 -and $hasAcceptance

    $reviewStatus = "EvidencePending"
    $commitOverride = ""
    if ($decisionStatus -eq "KeepLocal") {
        $reviewStatus = "KeepLocalByDecision"
        $commitOverride = "KeepLocalByDecision"
    }
    elseif ($decisionStatus -eq "AcceptedForRepository" -and $evidenceComplete) {
        $reviewStatus = "ReadyEvidenceAccepted"
        $commitOverride = "ReadyWithEvidence"
    }
    elseif ($decisionStatus -eq "AcceptedForRepository") {
        $reviewStatus = "AcceptedButEvidenceIncomplete"
    }
    elseif ($missingEvidence.Count -lt @($Entry.EvidenceNeeded).Count) {
        $reviewStatus = "PartialEvidence"
    }

    return [PSCustomObject]@{
        EvidenceReviewStatus = $reviewStatus
        EvidenceComplete = $evidenceComplete
        MissingEvidence = $missingEvidence
        EvidenceRecordFound = $true
        EvidenceAcceptedBy = $acceptedBy
        EvidenceAcceptedAt = $acceptedAt
        EvidenceAcceptedSource = $acceptedSource
        EffectiveDecisionStatus = $decisionStatus
        CommitReadinessOverride = $commitOverride
    }
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
    if ($CommitReadiness -eq "ReadyWithEvidence") {
        return "ReadyToStage"
    }
    if ($CommitReadiness -eq "DoNotCommitGeneratedOutput") {
        return "KeepLocal"
    }
    if ($CommitReadiness -eq "KeepLocalByDecision") {
        return "KeepLocal"
    }
    if ($Category -eq "ReviewCandidate" -or $Category -eq "LargeContentCandidate" -or $Category -eq "SampleOrThirdParty") {
        return "NeedsOwnerDecision"
    }

    return "NeedsOwnerDecision"
}

function Get-ReviewPriority {
    param(
        [string]$RelativePath,
        [string]$Category,
        [int64]$SizeBytes
    )

    $normalizedPath = Normalize-RepoPath $RelativePath
    if ($normalizedPath -eq "content/ma0t10/ui/wbp_virtualsensormonitor.uasset") {
        return 10
    }
    if ($normalizedPath -eq "config/game.ini") {
        return 20
    }
    if ($Category -eq "LargeContentCandidate" -and $SizeBytes -ge 1GB) {
        return 30
    }
    if ($Category -eq "LargeContentCandidate") {
        return 40
    }
    if ($Category -eq "SampleOrThirdParty") {
        return 50
    }
    if ($Category -like "Generated*") {
        return 90
    }
    return 60
}

function Get-BlockingReason {
    param(
        [string]$CommitReadiness,
        [string]$DecisionStatus,
        [string]$EvidenceStatus,
        [object[]]$MissingEvidence
    )

    if ($CommitReadiness -eq "NotPresent") {
        return "Path is not present in the local project."
    }
    if ($CommitReadiness -eq "ReadyWithEvidence") {
        return ""
    }
    if ($CommitReadiness -eq "DoNotCommitGeneratedOutput") {
        return "Generated or local-output path should not be committed."
    }
    if ($CommitReadiness -eq "KeepLocalByDecision") {
        return "Owner decision says this path should remain local."
    }
    if ($DecisionStatus -ne "AcceptedForRepository") {
        return "DecisionStatus is $DecisionStatus; AcceptedForRepository is required before staging."
    }
    if ($EvidenceStatus -ne "ReadyEvidenceAccepted") {
        $missing = @($MissingEvidence)
        if ($missing.Count -gt 0) {
            return "Evidence is incomplete: $($missing -join ', ')."
        }
        return "Evidence is not fully accepted."
    }
    return "Manual decision is still blocking this path."
}

function Get-NextReviewAction {
    param(
        [string]$RelativePath,
        [string]$Category,
        [string]$ReviewQueue,
        [string]$CommitReadiness
    )

    $normalizedPath = Normalize-RepoPath $RelativePath
    if ($ReviewQueue -eq "ReadyToStage") {
        return "Stage this path only with its accepted evidence record."
    }
    if ($ReviewQueue -eq "KeepLocal") {
        return "Keep this path out of source commits; remove it from staging if it appears there."
    }
    if ($normalizedPath -eq "content/ma0t10/ui/wbp_virtualsensormonitor.uasset") {
        return "Open the widget in Unreal Editor, verify optional bindings and PIE behavior, then record AcceptedForRepository evidence only if it is the production WBP."
    }
    if ($normalizedPath -eq "config/game.ini") {
        return "Inspect the diff for endpoint/credential leakage and decide whether these runtime defaults are shared project settings or local-only overrides."
    }
    if ($Category -eq "LargeContentCandidate") {
        if (Test-UnusedLocalContentPath -RelativePath $RelativePath) {
            return "Unused local asset content; keep out of source control and remove from the local project when no map or workflow references remain."
        }
        return "Confirm source/license, map or WBP dependency, storage/versioning strategy, and owner acceptance before considering repository inclusion."
    }
    if ($Category -eq "SampleOrThirdParty") {
        return "Confirm project ownership and redistribution terms, or prefer setup documentation instead of committing copied sample content."
    }
    if ($CommitReadiness -eq "BlockedByManualDecision") {
        return "Record owner decision and complete evidence before staging."
    }
    return "Inspect manually and record evidence before staging."
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
    $decisionEvidence = Import-DecisionEvidence -ProjectRoot $ProjectRoot -EvidencePath $EvidencePath

    $pathsToCheck = @(
        [PSCustomObject]@{
            Path = "Content\MA0T10\UI\WBP_VirtualSensorMonitorPanel.uasset"
            Category = "ReviewCandidate"
            Recommendation = "Open in editor and commit only if this is the intended production monitor WBP."
            DecisionOwner = "ProjectOwnerRequired"
            DecisionStatus = "EvidencePending"
            EvidenceNeeded = @("Editor open verification", "Optional binding check", "PIE smoke result", "Production WBP acceptance")
        },
        [PSCustomObject]@{
            Path = "Content\MA0T10\UI\WBP_VirtualSensorSettingsPanel.uasset"
            Category = "ReviewCandidate"
            Recommendation = "Verify the native parent and runtime settings panel in PIE before committing the generated WBP."
            DecisionOwner = "ProjectOwnerRequired"
            DecisionStatus = "EvidencePending"
            EvidenceNeeded = @("Editor open verification", "Native parent check", "PIE settings smoke result", "Generated WBP acceptance")
        },
        [PSCustomObject]@{
            Path = "Content\MA0T10\UI\WBP_VirtualSensorCaptureExportPanel.uasset"
            Category = "ReviewCandidate"
            Recommendation = "Verify the native parent and capture/export panel in PIE before committing the generated WBP."
            DecisionOwner = "ProjectOwnerRequired"
            DecisionStatus = "EvidencePending"
            EvidenceNeeded = @("Editor open verification", "Native parent check", "PIE capture/export smoke result", "Generated WBP acceptance")
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
            Recommendation = "Unused large environment pack; keep local or delete from the local project, but do not commit."
            DecisionOwner = "NotApplicable"
            DecisionStatus = "KeepLocal"
            EvidenceNeeded = @("Remove from local project or keep ignored as unused local asset")
        },
        [PSCustomObject]@{
            Path = "Content\Materials"
            Category = "LargeContentCandidate"
            Recommendation = "Unused local material assets; keep local or delete from the local project, but do not commit."
            DecisionOwner = "NotApplicable"
            DecisionStatus = "KeepLocal"
            EvidenceNeeded = @("Remove from local project or keep ignored as unused local asset")
        },
        [PSCustomObject]@{
            Path = "Content\Mega_Crane"
            Category = "LargeContentCandidate"
            Recommendation = "Unused local crane asset pack; keep local or delete from the local project, but do not commit."
            DecisionOwner = "NotApplicable"
            DecisionStatus = "KeepLocal"
            EvidenceNeeded = @("Remove from local project or keep ignored as unused local asset")
        },
        [PSCustomObject]@{
            Path = "Content\Meshes"
            Category = "LargeContentCandidate"
            Recommendation = "Unused local mesh assets; keep local or delete from the local project, but do not commit."
            DecisionOwner = "NotApplicable"
            DecisionStatus = "KeepLocal"
            EvidenceNeeded = @("Remove from local project or keep ignored as unused local asset")
        },
        [PSCustomObject]@{
            Path = "Content\Textures"
            Category = "LargeContentCandidate"
            Recommendation = "Unused local texture assets; keep local or delete from the local project, but do not commit."
            DecisionOwner = "NotApplicable"
            DecisionStatus = "KeepLocal"
            EvidenceNeeded = @("Remove from local project or keep ignored as unused local asset")
        },
        [PSCustomObject]@{
            Path = "Samples\PixelStreaming"
            Category = "SampleOrThirdParty"
            Recommendation = "Current LiDAR/virtual-sensor scope excludes Pixel Streaming. Keep local and untracked."
            DecisionOwner = "NotApplicableForCurrentScope"
            DecisionStatus = "KeepLocalCurrentScopeExcluded"
            EvidenceNeeded = @("Keep untracked during current scope", "Reopen ownership/license review only if Pixel Streaming becomes a project requirement")
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
        $decisionChecklist = Get-DecisionChecklist -RelativePath $relativePath -Category $entry.Category
        if (Test-EmptyRuntimeOverrideConfig -RelativePath $relativePath -FullPath $fullPath) {
            $entry.DecisionOwner = "NotApplicable"
            $entry.DecisionStatus = "KeepLocal"
            $entry.Recommendation = "Keep Config/Game.ini local by default. Empty DTCore runtime override values are local placeholders, not useful shared defaults."
        }
        $evidenceRecord = $decisionEvidence.DecisionsByPath[(Normalize-RepoPath $relativePath)]
        $evidenceReview = Get-DecisionEvidenceReview -Entry $entry -EvidenceRecord $evidenceRecord
        $commitReadiness = Get-CommitReadiness -State $summary.State -Category $entry.Category
        if ($summary.State -eq "present" -and -not [string]::IsNullOrWhiteSpace($evidenceReview.CommitReadinessOverride)) {
            $commitReadiness = $evidenceReview.CommitReadinessOverride
        }
        $reviewQueue = Get-ReviewQueue -CommitReadiness $commitReadiness -Category $entry.Category
        $gitState = Get-DecisionPointGitState -RelativePath $relativePath -UntrackedGitPaths $untrackedGitPaths -StagedGitPaths $stagedGitPaths -UnstagedGitPaths $unstagedGitPaths
        $outOfScopeThisIteration = ((Normalize-RepoPath $relativePath) -eq "samples/pixelstreaming")
        if ($summary.State -eq "present" -and $outOfScopeThisIteration) {
            $commitReadiness = "KeepLocalOutOfScope"
            $reviewQueue = "KeepLocal"
        }
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
            DecisionStatus = $evidenceReview.EffectiveDecisionStatus
            EvidenceNeeded = $entry.EvidenceNeeded
            EvidenceStatus = $evidenceReview.EvidenceReviewStatus
            EvidenceSatisfied = $evidenceReview.EvidenceComplete
            EvidenceReviewStatus = $evidenceReview.EvidenceReviewStatus
            EvidenceComplete = $evidenceReview.EvidenceComplete
            EvidenceRecordFound = $evidenceReview.EvidenceRecordFound
            MissingEvidence = $evidenceReview.MissingEvidence
            EvidenceAcceptedBy = $evidenceReview.EvidenceAcceptedBy
            EvidenceAcceptedAt = $evidenceReview.EvidenceAcceptedAt
            EvidenceAcceptedSource = $evidenceReview.EvidenceAcceptedSource
            DetectedNote = $decisionNote
            GitState = $gitState
            CommitReadiness = $commitReadiness
            ReviewQueue = $reviewQueue
            OutOfScopeThisIteration = $outOfScopeThisIteration
            MustRemainUntracked = ($outOfScopeThisIteration -and $summary.State -eq "present")
            StageAllowed = (-not $outOfScopeThisIteration -and $reviewQueue -eq "ReadyToStage")
            CountsTowardRemainingWork = -not $outOfScopeThisIteration
            ReviewPriority = Get-ReviewPriority -RelativePath $relativePath -Category $entry.Category -SizeBytes $summary.SizeBytes
            CommitBlocker = ($summary.State -eq "present" -and $reviewQueue -ne "ReadyToStage")
            BlockingReason = Get-BlockingReason -CommitReadiness $commitReadiness -DecisionStatus $evidenceReview.EffectiveDecisionStatus -EvidenceStatus $evidenceReview.EvidenceReviewStatus -MissingEvidence $evidenceReview.MissingEvidence
            NextReviewAction = Get-NextReviewAction -RelativePath $relativePath -Category $entry.Category -ReviewQueue $reviewQueue -CommitReadiness $commitReadiness
            DecisionChecklist = $decisionChecklist
            ContentSummary = $contentSummary
        }
    }

    $readyDecisionPaths = @(
        $decisionPoints |
            Where-Object { $_.ReviewQueue -eq "ReadyToStage" } |
            ForEach-Object { $_.Path }
    )
    $stagedBlockedDecisionPaths = @(
        $stagedDecisionPaths |
            Where-Object { -not (Test-PathIsUnderDecisionPoint -RepoPath $_ -DecisionPaths $readyDecisionPaths) } |
            Sort-Object
    )

    $report = [PSCustomObject]@{
        ProjectRoot = $ProjectRoot
        UProject = if ($uproject) { $uproject.Name } else { $null }
        GitBranch = (git branch --show-current)
        RecentCommits = @(git log --oneline -3)
        GitStatus = @(git status --short)
        DecisionEvidence = [PSCustomObject]@{
            Path = $decisionEvidence.Path
            Exists = $decisionEvidence.Exists
        }
        UntrackedGitPaths = $untrackedGitPaths
        AllowedCommitCandidatePaths = $allowedCommitCandidatePaths
        UnclassifiedUntrackedPaths = $unclassifiedUntrackedPaths
        StagedGitPaths = $stagedGitPaths
        StagedDecisionPaths = $stagedDecisionPaths
        StagedBlockedDecisionPaths = $stagedBlockedDecisionPaths
        ActionPlan = @(
            $decisionPoints |
                Where-Object { $_.State -eq "present" -and $_.CommitBlocker } |
                Sort-Object ReviewPriority, Path |
                ForEach-Object {
                    [PSCustomObject]@{
                        Priority = $_.ReviewPriority
                        Path = $_.Path
                        Category = $_.Category
                        ReviewQueue = $_.ReviewQueue
                        DecisionOwner = $_.DecisionOwner
                        BlockingReason = $_.BlockingReason
                        NextReviewAction = $_.NextReviewAction
                    }
                }
        )
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
            StagedBlockedDecisionPointCount = $stagedBlockedDecisionPaths.Count
            CommitBlockedDecisionPointCount = @($decisionPoints | Where-Object { $_.State -eq "present" -and $_.CommitBlocker }).Count
            ActionPlanItemCount = @($decisionPoints | Where-Object { $_.State -eq "present" -and $_.CommitBlocker }).Count
            OutOfScopeDecisionPointCount = @($decisionPoints | Where-Object { $_.State -eq "present" -and $_.OutOfScopeThisIteration }).Count
            RemainingWorkDecisionPointCount = @($decisionPoints | Where-Object { $_.State -eq "present" -and $_.CountsTowardRemainingWork }).Count
            HasStagedDecisionPoints = ($stagedDecisionPaths.Count -gt 0)
            HasStagedBlockedDecisionPoints = ($stagedBlockedDecisionPaths.Count -gt 0)
            PresentCategoryCounts = [PSCustomObject]$presentCategoryCounts
            DefaultAction = "Do not stage these paths until each item has an explicit content or packaging decision."
        }
        SuggestedChecks = [PSCustomObject]@{
            Build = "& 'C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat' ma0t10_dtEditor Win64 Development '$ProjectRoot\ma0t10_dt.uproject' -WaitMutex -NoHotReloadFromIDE"
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
        Write-Host "Evidence file: $($report.DecisionEvidence.Path)"
        Write-Host "Evidence file exists: $($report.DecisionEvidence.Exists)"
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
            Write-Host "  reviewPriority: $($point.ReviewPriority)"
            Write-Host "  commitBlocker: $($point.CommitBlocker)"
            if (-not [string]::IsNullOrWhiteSpace($point.BlockingReason)) {
                Write-Host "  blockingReason: $($point.BlockingReason)"
            }
            Write-Host "  nextReviewAction: $($point.NextReviewAction)"
            Write-Host "  recommendation: $($point.Recommendation)"
            Write-Host "  decisionOwner: $($point.DecisionOwner)"
            Write-Host "  decisionStatus: $($point.DecisionStatus)"
            Write-Host "  evidenceStatus: $($point.EvidenceStatus)"
            Write-Host "  evidenceSatisfied: $($point.EvidenceSatisfied)"
            if (-not [string]::IsNullOrWhiteSpace($point.EvidenceAcceptedBy)) {
                Write-Host "  evidenceAcceptedBy: $($point.EvidenceAcceptedBy)"
            }
            if (-not [string]::IsNullOrWhiteSpace($point.EvidenceAcceptedAt)) {
                Write-Host "  evidenceAcceptedAt: $($point.EvidenceAcceptedAt)"
            }
            if (-not [string]::IsNullOrWhiteSpace($point.EvidenceAcceptedSource)) {
                Write-Host "  evidenceAcceptedSource: $($point.EvidenceAcceptedSource)"
            }
            if ($point.EvidenceNeeded.Count -gt 0) {
                Write-Host "  evidence needed:"
                foreach ($evidence in $point.EvidenceNeeded) {
                    Write-Host "    - $evidence"
                }
            }
            if ($point.MissingEvidence.Count -gt 0) {
                Write-Host "  missing evidence:"
                foreach ($evidence in $point.MissingEvidence) {
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
        Write-Host "Staged blocked decision-point paths: $($report.Summary.StagedBlockedDecisionPointCount)"
        Write-Host "Commit-blocked decision points: $($report.Summary.CommitBlockedDecisionPointCount)"
        Write-Host "Action-plan items: $($report.Summary.ActionPlanItemCount)"
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

        if ($stagedBlockedDecisionPaths.Count -gt 0) {
            Write-Section "Staged decision-point paths"
            $stagedBlockedDecisionPaths | ForEach-Object { Write-Host $_ }
            Write-Host "Recommendation: unstage these paths unless each one is ReadyToStage with accepted evidence."
        }

        if ($report.ActionPlan.Count -gt 0) {
            Write-Section "Action plan"
            foreach ($item in $report.ActionPlan) {
                Write-Host "$($item.Priority) $($item.Path)"
                Write-Host "  queue: $($item.ReviewQueue)"
                Write-Host "  owner: $($item.DecisionOwner)"
                Write-Host "  blocker: $($item.BlockingReason)"
                Write-Host "  next: $($item.NextReviewAction)"
            }
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
    if ($FailOnStagedDecisionPoints -and $stagedBlockedDecisionPaths.Count -gt 0) {
        $previewPaths = @($stagedBlockedDecisionPaths | Select-Object -First 10) -join ", "
        throw "Decision-point paths are staged without ReadyToStage evidence: $previewPaths. Unstage them or record complete AcceptedForRepository evidence before staging."
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
