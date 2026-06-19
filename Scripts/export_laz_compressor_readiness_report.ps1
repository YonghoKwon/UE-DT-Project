param(
    [string]$ProjectRoot = "",
    [string]$CompressorPath = "",
    [string]$ReaderPath = "",
    [string]$LazEvidencePath = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
    [switch]$ProbeToolVersions,
    [switch]$RunReaderProbe,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Get-DefaultProjectRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Write-TextFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Lines -Encoding UTF8
}

function Convert-ToMarkdownCell {
    param([object]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Resolve-ToolCandidate {
    param(
        [string]$Name,
        [string]$ExplicitPath,
        [string[]]$DefaultArguments,
        [string]$Role,
        [bool]$ProbeVersion
    )

    $resolvedPath = ""
    $source = "NotFound"
    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (Test-Path -LiteralPath $ExplicitPath -PathType Leaf) {
            $resolvedPath = (Resolve-Path -LiteralPath $ExplicitPath).Path
            $source = "ExplicitPath"
        }
        else {
            return [PSCustomObject]@{
                Name = $Name
                Role = $Role
                Source = "ExplicitPathMissing"
                Path = $ExplicitPath
                Exists = $false
                VersionProbeSucceeded = $false
                VersionText = ""
                SuggestedArgumentList = $DefaultArguments
                SuggestedArguments = ($DefaultArguments -join " ")
            }
        }
    }
    else {
        $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) {
            $resolvedPath = $command.Source
            $source = "PATH"
        }
    }

    $versionProbeSucceeded = $false
    $versionText = ""
    if ($ProbeVersion -and -not [string]::IsNullOrWhiteSpace($resolvedPath)) {
        foreach ($arg in @("--version", "-version", "-h")) {
            try {
                $output = @(& $resolvedPath $arg 2>&1 | Select-Object -First 6)
                if ($LASTEXITCODE -eq 0 -or $output.Count -gt 0) {
                    $versionProbeSucceeded = $true
                    $versionText = (($output | ForEach-Object { [string]$_ }) -join " ").Trim()
                    break
                }
            }
            catch {
            }
        }
    }

    return [PSCustomObject]@{
        Name = $Name
        Role = $Role
        Source = $source
        Path = $resolvedPath
        Exists = (-not [string]::IsNullOrWhiteSpace($resolvedPath))
        VersionProbeSucceeded = $versionProbeSucceeded
        VersionText = $versionText
        SuggestedArgumentList = $DefaultArguments
        SuggestedArguments = ($DefaultArguments -join " ")
    }
}

function Test-LazEvidenceFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return [PSCustomObject]@{
            Path = ""
            Exists = $false
            ExtensionLooksLikeLaz = $false
            SizeBytes = 0
            NonEmpty = $false
        }
    }

    $resolvedPath = $Path
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    }

    $exists = Test-Path -LiteralPath $resolvedPath -PathType Leaf
    $sizeBytes = 0
    if ($exists) {
        $sizeBytes = [int64](Get-Item -LiteralPath $resolvedPath).Length
    }

    return [PSCustomObject]@{
        Path = $resolvedPath
        Exists = [bool]$exists
        ExtensionLooksLikeLaz = ([System.IO.Path]::GetExtension($resolvedPath).ToLowerInvariant() -eq ".laz")
        SizeBytes = [int64]$sizeBytes
        NonEmpty = [bool]($sizeBytes -gt 0)
    }
}

function Test-KnownPointCloudReader {
    param([object]$ReaderTool)

    if ($null -eq $ReaderTool) {
        return $false
    }

    $knownNames = @("lasinfo", "pdal")
    if ($knownNames -contains ([string]$ReaderTool.Name).ToLowerInvariant()) {
        return $true
    }

    if (-not [string]::IsNullOrWhiteSpace($ReaderTool.Path)) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($ReaderTool.Path).ToLowerInvariant()
        return ($knownNames -contains $baseName)
    }

    return $false
}

function Invoke-ReaderProbe {
    param(
        [object]$ReaderTool,
        [object]$LazEvidence
    )

    $knownPointCloudReader = Test-KnownPointCloudReader -ReaderTool $ReaderTool
    if ($null -eq $ReaderTool -or -not $ReaderTool.Exists) {
        return [PSCustomObject]@{
            Requested = $true
            RequestedByUser = $true
            Succeeded = $false
            KnownPointCloudReader = [bool]$knownPointCloudReader
            BlockedReason = "ReaderToolMissing"
            Tool = if ($ReaderTool) { $ReaderTool.Name } else { "" }
            ExitCode = $null
            OutputPreview = ""
        }
    }
    if (-not $LazEvidence.Exists) {
        return [PSCustomObject]@{
            Requested = $true
            RequestedByUser = $true
            Succeeded = $false
            KnownPointCloudReader = [bool]$knownPointCloudReader
            BlockedReason = "LazEvidenceMissing"
            Tool = $ReaderTool.Name
            ExitCode = $null
            OutputPreview = ""
        }
    }

    $arguments = @($ReaderTool.SuggestedArgumentList)
    if ($arguments.Count -eq 0) {
        $arguments = @("{output}")
    }
    $arguments = @($arguments | ForEach-Object { ([string]$_).Replace("{output}", $LazEvidence.Path) })

    $outputPreview = ""
    $exitCode = $null
    $succeeded = $false
    try {
        $output = @(& $ReaderTool.Path @arguments 2>&1 | Select-Object -First 12)
        $exitCode = $LASTEXITCODE
        $outputPreview = (($output | ForEach-Object { [string]$_ }) -join " ").Trim()
        $succeeded = ($exitCode -eq 0)
    }
    catch {
        $outputPreview = $_.Exception.Message
        $succeeded = $false
    }

    return [PSCustomObject]@{
        Requested = $true
        RequestedByUser = $true
        Succeeded = [bool]$succeeded
        KnownPointCloudReader = [bool]$knownPointCloudReader
        BlockedReason = ""
        Tool = $ReaderTool.Name
        ExitCode = $exitCode
        OutputPreview = $outputPreview
    }
}

function Write-MarkdownReport {
    param(
        [object]$Report,
        [string]$Path
    )

    $lines = @()
    $lines += "# LAZ Compressor Readiness Report"
    $lines += ""
    $lines += "Generated UTC: $($Report.GeneratedUtc)"
    $lines += ""
    $lines += "## Summary"
    $lines += ""
    $lines += "- Compressor candidate found: $($Report.Summary.CompressorCandidateFound)"
    $lines += "- Reader candidate found: $($Report.Summary.ReaderCandidateFound)"
    $lines += "- Tool candidate discovery only: $($Report.Summary.ToolCandidateDiscoveryOnly)"
    $lines += "- Tool readiness only: $($Report.Summary.ToolReadinessOnly)"
    $lines += "- Tool candidate is acceptance evidence: $($Report.Summary.ToolCandidateIsAcceptanceEvidence)"
    $lines += "- Compressor candidate is acceptance evidence: $($Report.Summary.CompressorCandidateIsAcceptanceEvidence)"
    $lines += "- Reader candidate is acceptance evidence: $($Report.Summary.ReaderCandidateIsAcceptanceEvidence)"
    $lines += "- LAZ evidence file present: $($Report.Summary.LazEvidenceFilePresent)"
    $lines += "- Reader probe requested: $($Report.Summary.ReaderProbeRequested)"
    $lines += "- Reader probe blocked reason: $($Report.Summary.ReaderProbeBlockedReason)"
    $lines += "- Tool version probes requested: $($Report.Summary.ToolVersionProbesRequested)"
    $lines += "- Known point-cloud reader: $($Report.Summary.KnownPointCloudReader)"
    $lines += "- Reader probe succeeded: $($Report.Summary.ReaderProbeSucceeded)"
    $lines += "- Reader probe requires produced LAZ: $($Report.Summary.ReaderProbeRequiresProducedLaz)"
    $lines += "- Reader probe is acceptance evidence: $($Report.Summary.ReaderProbeIsAcceptanceEvidence)"
    $lines += "- Readable-output evidence present: $($Report.Summary.ReadableOutputEvidencePresent)"
    $lines += "- Readable LAZ acceptance evidence present: $($Report.Summary.ReadableLazAcceptanceEvidencePresent)"
    $lines += "- Ready to claim readable LAZ output: $($Report.Summary.ReadyToClaimReadableLazOutput)"
    $lines += "- Ready to claim readable LAZ acceptance: $($Report.Summary.ReadyToClaimReadableLazAcceptance)"
    $lines += "- Ready for real LAZ automation: $($Report.Summary.ReadyForRealLazAutomation)"
    $lines += "- Recommended next action: $($Report.Summary.RecommendedNextAction)"
    $lines += ""
    $lines += "## Tool Candidates"
    $lines += ""
    $lines += "| Name | Role | Source | Exists | Version probe | Path | Suggested arguments |"
    $lines += "| --- | --- | --- | --- | --- | --- | --- |"
    foreach ($tool in $Report.ToolCandidates) {
        $lines += "| $(Convert-ToMarkdownCell $tool.Name) | $(Convert-ToMarkdownCell $tool.Role) | $(Convert-ToMarkdownCell $tool.Source) | $($tool.Exists) | $($tool.VersionProbeSucceeded) | $(Convert-ToMarkdownCell $tool.Path) | $(Convert-ToMarkdownCell $tool.SuggestedArguments) |"
    }
    $lines += ""
    $lines += "## Readable Output Evidence"
    $lines += ""
    $lines += "| Evidence path | Exists | .laz extension | Size bytes | Non-empty | Reader tool | Known reader | Reader requested | Blocked reason | Reader succeeded | Reader output preview |"
    $lines += "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"
    $lines += "| $(Convert-ToMarkdownCell $Report.LazEvidence.Path) | $($Report.LazEvidence.Exists) | $($Report.LazEvidence.ExtensionLooksLikeLaz) | $($Report.LazEvidence.SizeBytes) | $($Report.LazEvidence.NonEmpty) | $(Convert-ToMarkdownCell $Report.ReaderProbe.Tool) | $($Report.ReaderProbe.KnownPointCloudReader) | $($Report.ReaderProbe.Requested) | $(Convert-ToMarkdownCell $Report.ReaderProbe.BlockedReason) | $($Report.ReaderProbe.Succeeded) | $(Convert-ToMarkdownCell $Report.ReaderProbe.OutputPreview) |"
    $lines += ""
    $lines += "## Evidence Boundaries"
    $lines += ""
    foreach ($item in $Report.EvidenceBoundaries) {
        $lines += "- $item"
    }
    $lines += ""
    $lines += "## Required Follow-Up"
    $lines += ""
    foreach ($item in $Report.RequiredFollowUp) {
        $lines += "- $item"
    }

    Write-TextFile -Path $Path -Lines $lines
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Get-DefaultProjectRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$toolCandidates = @()
if (-not [string]::IsNullOrWhiteSpace($CompressorPath)) {
    $toolCandidates += Resolve-ToolCandidate -Name "external-compressor" -ExplicitPath $CompressorPath -DefaultArguments @("-i", "{input}", "-o", "{output}") -Role "Compressor" -ProbeVersion ([bool]$ProbeToolVersions)
}
else {
    $toolCandidates += Resolve-ToolCandidate -Name "laszip" -ExplicitPath "" -DefaultArguments @("-i", "{input}", "-o", "{output}") -Role "Compressor" -ProbeVersion ([bool]$ProbeToolVersions)
    $toolCandidates += Resolve-ToolCandidate -Name "las2las" -ExplicitPath "" -DefaultArguments @("-i", "{input}", "-o", "{output}") -Role "Compressor" -ProbeVersion ([bool]$ProbeToolVersions)
    $toolCandidates += Resolve-ToolCandidate -Name "pdal" -ExplicitPath "" -DefaultArguments @("translate", "{input}", "{output}") -Role "Compressor" -ProbeVersion ([bool]$ProbeToolVersions)
}

if (-not [string]::IsNullOrWhiteSpace($ReaderPath)) {
    $toolCandidates += Resolve-ToolCandidate -Name "external-reader" -ExplicitPath $ReaderPath -DefaultArguments @("{output}") -Role "Reader" -ProbeVersion ([bool]$ProbeToolVersions)
}
else {
    $toolCandidates += Resolve-ToolCandidate -Name "lasinfo" -ExplicitPath "" -DefaultArguments @("{output}") -Role "Reader" -ProbeVersion ([bool]$ProbeToolVersions)
    $toolCandidates += Resolve-ToolCandidate -Name "pdal" -ExplicitPath "" -DefaultArguments @("info", "{output}") -Role "Reader" -ProbeVersion ([bool]$ProbeToolVersions)
}

$compressorCandidates = @($toolCandidates | Where-Object { $_.Role -eq "Compressor" -and $_.Exists })
$readerCandidates = @($toolCandidates | Where-Object { $_.Role -eq "Reader" -and $_.Exists })
$lazEvidence = Test-LazEvidenceFile -Path $LazEvidencePath
$readerProbeTool = $null
if ($readerCandidates.Count -gt 0) {
    $readerProbeTool = $readerCandidates | Select-Object -First 1
}
$readerProbe = if ($RunReaderProbe) {
    Invoke-ReaderProbe -ReaderTool $readerProbeTool -LazEvidence $lazEvidence
}
else {
    [PSCustomObject]@{
        Requested = $false
        RequestedByUser = $false
        Succeeded = $false
        KnownPointCloudReader = [bool](Test-KnownPointCloudReader -ReaderTool $readerProbeTool)
        BlockedReason = "NotRequested"
        Tool = if ($readerProbeTool) { $readerProbeTool.Name } else { "" }
        ExitCode = $null
        OutputPreview = ""
    }
}

$evidenceBoundaries = @(
    "This report detects local tool readiness only; it does not claim true LAZ compression by itself.",
    "Current Unreal automation proves placeholder LAS source export, missing-compressor failure, and external process-contract success with a copy surrogate.",
    "Readable `.laz` evidence still requires a real compressor output plus an independent reader such as lasinfo or pdal info."
)

$requiredFollowUp = @(
    "Select one compressor path and confirm license/redistribution ownership.",
    "Configure `ExternalLazCompressorPath` and `ExternalLazCompressorArguments` with `{input}` and `{output}`.",
    "Produce a `.laz` through `ExportLastPointCloudLaz()` or an accepted post-process path.",
    "Run this report with `-LazEvidencePath` and `-RunReaderProbe` against the produced `.laz`.",
    "Run a reader/inspector against the produced `.laz` and archive command output.",
    "Add automation that distinguishes real compressed output from the current LAS-copy surrogate."
)

$compressorCandidateFound = $compressorCandidates.Count -gt 0
$readerCandidateFound = $readerCandidates.Count -gt 0
$readableOutputEvidencePresent = $lazEvidence.Exists -and $lazEvidence.ExtensionLooksLikeLaz -and $lazEvidence.NonEmpty -and $readerProbe.Requested -and $readerProbe.Succeeded -and $readerProbe.KnownPointCloudReader
$readyForRealLazAutomation = $compressorCandidateFound -and $readerCandidateFound -and $readableOutputEvidencePresent
$toolReadinessOnly = (-not $readableOutputEvidencePresent)
$recommendedNextAction = if ($compressorCandidateFound -and $readerCandidateFound) {
    "Run an end-to-end real compressor/readability smoke and capture output evidence."
}
elseif ($compressorCandidateFound) {
    "Add or select an independent LAZ reader such as lasinfo or pdal info before claiming readable output."
}
else {
    "Install/select an accepted LAZ compressor or choose native/server post-processing before claiming true LAZ."
}

$report = [PSCustomObject]@{
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    ToolCandidates = $toolCandidates
    LazEvidence = $lazEvidence
    ReaderProbe = $readerProbe
    EvidenceBoundaries = $evidenceBoundaries
    RequiredFollowUp = $requiredFollowUp
    Summary = [PSCustomObject]@{
        CompressorCandidateFound = [bool]$compressorCandidateFound
        ReaderCandidateFound = [bool]$readerCandidateFound
        ToolCandidateDiscoveryOnly = $true
        ToolReadinessOnly = [bool]$toolReadinessOnly
        ToolCandidateIsAcceptanceEvidence = $false
        CompressorCandidateIsAcceptanceEvidence = $false
        ReaderCandidateIsAcceptanceEvidence = $false
        ToolCandidateAcceptanceBlocked = [bool]$toolReadinessOnly
        ToolCandidateAcceptanceBlockers = if ($readableOutputEvidencePresent) { @() } else { @("Tool candidates are readiness metadata only until a non-empty .laz output is validated by a known reader.") }
        LazEvidenceFilePresent = [bool]($lazEvidence.Exists -and $lazEvidence.NonEmpty)
        ReaderProbeRequested = [bool]$readerProbe.Requested
        ReaderProbeRequestedByUser = [bool]$readerProbe.RequestedByUser
        ReaderProbeBlockedReason = [string]$readerProbe.BlockedReason
        ToolVersionProbesRequested = [bool]$ProbeToolVersions
        KnownPointCloudReader = [bool]$readerProbe.KnownPointCloudReader
        ReaderProbeSucceeded = [bool]$readerProbe.Succeeded
        ReaderProbeRequiresProducedLaz = $true
        ProducedLazEvidencePresent = [bool]($lazEvidence.Exists -and $lazEvidence.ExtensionLooksLikeLaz -and $lazEvidence.NonEmpty)
        ReaderProbeIsAcceptanceEvidence = [bool]$readableOutputEvidencePresent
        ReaderProbeAcceptanceBlocked = (-not [bool]$readableOutputEvidencePresent)
        ReaderProbeAcceptanceBlockers = if ($readableOutputEvidencePresent) { @() } else { @("Produced .laz evidence missing or invalid.", "Known-reader success evidence missing.") }
        ReadableOutputEvidencePresent = [bool]$readableOutputEvidencePresent
        ReadableLazAcceptanceEvidencePresent = [bool]$readableOutputEvidencePresent
        ReadyToClaimReadableLazOutput = [bool]$readableOutputEvidencePresent
        ReadyToClaimReadableLazAcceptance = [bool]$readableOutputEvidencePresent
        ReadableLazOutputAcceptanceBlocked = (-not [bool]$readableOutputEvidencePresent)
        ReadableLazOutputMissingReason = if ($readableOutputEvidencePresent) { "" } elseif (-not ($lazEvidence.Exists -and $lazEvidence.NonEmpty)) { "A non-empty .laz evidence file is missing." } elseif (-not $readerProbe.Requested) { "Reader probe was not requested." } elseif (-not $readerProbe.KnownPointCloudReader) { "Reader is not a known point-cloud reader." } elseif (-not $readerProbe.Succeeded) { "Reader probe did not succeed." } else { "Readable LAZ output evidence is incomplete." }
        ReadyForRealLazAutomation = [bool]$readyForRealLazAutomation
        CandidateToolCount = [int]$toolCandidates.Count
        FoundToolCount = [int](@($toolCandidates | Where-Object { $_.Exists }).Count)
        RecommendedNextAction = $recommendedNextAction
        Valid = $true
    }
}

if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $JsonPath -Encoding UTF8
}
if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
    Write-MarkdownReport -Report $report -Path $MarkdownPath
}

if ($Json) {
    $report | ConvertTo-Json -Depth 6
}
else {
    Write-Host "LAZ compressor readiness report is ready."
    Write-Host "Project root: $ProjectRoot"
    Write-Host "Compressor candidate found: $($report.Summary.CompressorCandidateFound)"
    Write-Host "Reader candidate found: $($report.Summary.ReaderCandidateFound)"
    Write-Host "Tool candidate discovery only: $($report.Summary.ToolCandidateDiscoveryOnly)"
    Write-Host "Tool readiness only: $($report.Summary.ToolReadinessOnly)"
    Write-Host "Tool candidate is acceptance evidence: $($report.Summary.ToolCandidateIsAcceptanceEvidence)"
    Write-Host "Compressor candidate is acceptance evidence: $($report.Summary.CompressorCandidateIsAcceptanceEvidence)"
    Write-Host "Reader candidate is acceptance evidence: $($report.Summary.ReaderCandidateIsAcceptanceEvidence)"
    Write-Host "Found tools: $($report.Summary.FoundToolCount)/$($report.Summary.CandidateToolCount)"
    Write-Host "LAZ evidence file present: $($report.Summary.LazEvidenceFilePresent)"
    Write-Host "Reader probe requested: $($report.Summary.ReaderProbeRequested)"
    Write-Host "Reader probe blocked reason: $($report.Summary.ReaderProbeBlockedReason)"
    Write-Host "Tool version probes requested: $($report.Summary.ToolVersionProbesRequested)"
    Write-Host "Known point-cloud reader: $($report.Summary.KnownPointCloudReader)"
    Write-Host "Reader probe succeeded: $($report.Summary.ReaderProbeSucceeded)"
    Write-Host "Reader probe requires produced LAZ: $($report.Summary.ReaderProbeRequiresProducedLaz)"
    Write-Host "Reader probe is acceptance evidence: $($report.Summary.ReaderProbeIsAcceptanceEvidence)"
    Write-Host "Readable-output evidence present: $($report.Summary.ReadableOutputEvidencePresent)"
    Write-Host "Readable LAZ acceptance evidence present: $($report.Summary.ReadableLazAcceptanceEvidencePresent)"
    Write-Host "Ready to claim readable LAZ output: $($report.Summary.ReadyToClaimReadableLazOutput)"
    Write-Host "Ready to claim readable LAZ acceptance: $($report.Summary.ReadyToClaimReadableLazAcceptance)"
    Write-Host "Readable LAZ output missing reason: $($report.Summary.ReadableLazOutputMissingReason)"
    Write-Host "Ready for real LAZ automation: $($report.Summary.ReadyForRealLazAutomation)"
    Write-Host "Recommended next action: $($report.Summary.RecommendedNextAction)"
}
