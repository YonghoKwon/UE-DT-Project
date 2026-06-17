param(
    [string]$ProjectRoot = "",
    [string]$CompressorPath = "",
    [string]$ReaderPath = "",
    [string]$MarkdownPath = "",
    [string]$JsonPath = "",
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
        [string]$Role
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
    if (-not [string]::IsNullOrWhiteSpace($resolvedPath)) {
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
        SuggestedArguments = ($DefaultArguments -join " ")
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
    $lines += "- Readable-output evidence present: $($Report.Summary.ReadableOutputEvidencePresent)"
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
    $toolCandidates += Resolve-ToolCandidate -Name "external-compressor" -ExplicitPath $CompressorPath -DefaultArguments @("-i", "{input}", "-o", "{output}") -Role "Compressor"
}
else {
    $toolCandidates += Resolve-ToolCandidate -Name "laszip" -ExplicitPath "" -DefaultArguments @("-i", "{input}", "-o", "{output}") -Role "Compressor"
    $toolCandidates += Resolve-ToolCandidate -Name "las2las" -ExplicitPath "" -DefaultArguments @("-i", "{input}", "-o", "{output}") -Role "Compressor"
    $toolCandidates += Resolve-ToolCandidate -Name "pdal" -ExplicitPath "" -DefaultArguments @("translate", "{input}", "{output}") -Role "Compressor"
}

if (-not [string]::IsNullOrWhiteSpace($ReaderPath)) {
    $toolCandidates += Resolve-ToolCandidate -Name "external-reader" -ExplicitPath $ReaderPath -DefaultArguments @("{output}") -Role "Reader"
}
else {
    $toolCandidates += Resolve-ToolCandidate -Name "lasinfo" -ExplicitPath "" -DefaultArguments @("{output}") -Role "Reader"
    $toolCandidates += Resolve-ToolCandidate -Name "pdal" -ExplicitPath "" -DefaultArguments @("info", "{output}") -Role "Reader"
}

$compressorCandidates = @($toolCandidates | Where-Object { $_.Role -eq "Compressor" -and $_.Exists })
$readerCandidates = @($toolCandidates | Where-Object { $_.Role -eq "Reader" -and $_.Exists })

$evidenceBoundaries = @(
    "This report detects local tool readiness only; it does not claim true LAZ compression by itself.",
    "Current Unreal automation proves placeholder LAS source export, missing-compressor failure, and external process-contract success with a copy surrogate.",
    "Readable `.laz` evidence still requires a real compressor output plus an independent reader such as lasinfo or pdal info."
)

$requiredFollowUp = @(
    "Select one compressor path and confirm license/redistribution ownership.",
    "Configure `ExternalLazCompressorPath` and `ExternalLazCompressorArguments` with `{input}` and `{output}`.",
    "Produce a `.laz` through `ExportLastPointCloudLaz()` or an accepted post-process path.",
    "Run a reader/inspector against the produced `.laz` and archive command output.",
    "Add automation that distinguishes real compressed output from the current LAS-copy surrogate."
)

$compressorCandidateFound = $compressorCandidates.Count -gt 0
$readerCandidateFound = $readerCandidates.Count -gt 0
$readableOutputEvidencePresent = $false
$readyForRealLazAutomation = $compressorCandidateFound -and $readerCandidateFound -and $readableOutputEvidencePresent
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
    EvidenceBoundaries = $evidenceBoundaries
    RequiredFollowUp = $requiredFollowUp
    Summary = [PSCustomObject]@{
        CompressorCandidateFound = [bool]$compressorCandidateFound
        ReaderCandidateFound = [bool]$readerCandidateFound
        ReadableOutputEvidencePresent = [bool]$readableOutputEvidencePresent
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
    Write-Host "Found tools: $($report.Summary.FoundToolCount)/$($report.Summary.CandidateToolCount)"
    Write-Host "Readable-output evidence present: $($report.Summary.ReadableOutputEvidencePresent)"
    Write-Host "Ready for real LAZ automation: $($report.Summary.ReadyForRealLazAutomation)"
    Write-Host "Recommended next action: $($report.Summary.RecommendedNextAction)"
}
