param(
    [string]$ProjectRoot = "",
    [string]$OutputPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        return (Resolve-Path -LiteralPath $ProjectRoot).Path
    }
    return (Split-Path -Parent $PSScriptRoot)
}

function New-EvidenceItem {
    param(
        [string]$Name,
        [string]$RequiredEvidence,
        [string]$Owner = "PointCloudExportOwner"
    )

    return [PSCustomObject]@{
        Name = $Name
        Owner = $Owner
        Status = "Pending"
        RequiredEvidence = $RequiredEvidence
        EvidencePath = ""
        Reviewer = ""
        ReviewedAtUtc = ""
        Notes = ""
    }
}

function Write-MarkdownTemplate {
    param(
        [object]$Template,
        [string]$Path
    )

    $lines = @()
    $lines += "# LAZ Compression Acceptance Template"
    $lines += ""
    $lines += "Generated UTC: $($Template.GeneratedUtc)"
    $lines += ""
    $lines += "This is a fillable evidence template. It records the evidence needed before true LAZ compression can be claimed."
    $lines += ""
    $lines += "## Summary"
    $lines += ""
    $lines += "- Required evidence count: $($Template.Summary.RequiredEvidenceCount)"
    $lines += "- Pending evidence count: $($Template.Summary.PendingEvidenceCount)"
    $lines += "- Current ready to claim true LAZ: $($Template.Summary.CurrentReadyToClaimTrueLaz)"
    $lines += "- Does not install compressor: $($Template.Summary.DoesNotInstallCompressor)"
    $lines += "- Does not run compressor: $($Template.Summary.DoesNotRunCompressor)"
    $lines += "- Stages files: $($Template.Summary.StagesFiles)"
    $lines += ""
    $lines += "## Acceptance Metadata"
    $lines += ""
    $lines += "- Evidence run id:"
    $lines += "- Environment name:"
    $lines += "- Operator:"
    $lines += "- Map name:"
    $lines += "- Sensor id:"
    $lines += "- Compressor path:"
    $lines += "- Compressor version:"
    $lines += "- Reader path:"
    $lines += "- LAZ evidence path:"
    $lines += "- Reader probe report path:"
    $lines += ""
    $lines += "## Required Evidence"
    $lines += ""
    $lines += "| Item | Owner | Status | Required evidence | Evidence path | Reviewer | Reviewed UTC | Notes |"
    $lines += "| --- | --- | --- | --- | --- | --- | --- | --- |"
    foreach ($item in $Template.RequiredEvidence) {
        $lines += "| $($item.Name) | $($item.Owner) | $($item.Status) | $($item.RequiredEvidence) | $($item.EvidencePath) | $($item.Reviewer) | $($item.ReviewedAtUtc) | $($item.Notes) |"
    }
    $lines += ""
    $lines += "## Safety Boundary"
    $lines += ""
    $lines += $Template.SafetyBoundary

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $lines -Encoding UTF8
}

$ProjectRoot = Resolve-ProjectRoot
$requiredEvidence = @(
    (New-EvidenceItem -Name "Compressor selection" -RequiredEvidence "Accepted compressor/native/server workflow, version, license, redistribution owner, and UE 5.3 packaging decision are recorded."),
    (New-EvidenceItem -Name "Produced LAZ output" -RequiredEvidence "A non-empty .laz file produced by ExportLastPointCloudLaz() or the accepted post-process path is recorded."),
    (New-EvidenceItem -Name "Known reader validation" -RequiredEvidence "A known reader such as lasinfo or pdal info successfully reads the produced .laz, with output evidence archived."),
    (New-EvidenceItem -Name "Placeholder distinction" -RequiredEvidence "Evidence proves the accepted output is not merely the current *_laz_source_*.las placeholder or copy-surrogate process contract."),
    (New-EvidenceItem -Name "Automation or repeatable command" -RequiredEvidence "A repeatable command, script, or automation step recreates the LAZ export and reader validation."),
    (New-EvidenceItem -Name "Owner acceptance" -RequiredEvidence "Project/export owner accepts the selected LAZ workflow and records reviewer/date/source evidence.")
)

$pendingEvidence = @($requiredEvidence | Where-Object { $_.Status -ne "Recorded" })
$template = [PSCustomObject]@{
    SchemaVersion = "LazCompressionAcceptanceEvidenceV1"
    GeneratedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    ProjectRoot = $ProjectRoot
    AcceptanceMetadata = [PSCustomObject]@{
        EvidenceRunId = ""
        EnvironmentName = ""
        Operator = ""
        MapName = ""
        SensorId = ""
        CompressorPath = ""
        CompressorVersion = ""
        ReaderPath = ""
        LazEvidencePath = ""
        ReaderProbeReportPath = ""
    }
    RequiredEvidence = $requiredEvidence
    SafetyBoundary = "This template records evidence only. It does not install tools, does not run a compressor, does not modify assets, and does not stage files."
    Summary = [PSCustomObject]@{
        RequiredEvidenceCount = $requiredEvidence.Count
        PendingEvidenceCount = $pendingEvidence.Count
        CurrentReadyToClaimTrueLaz = $false
        DoesNotInstallCompressor = $true
        DoesNotRunCompressor = $true
        ModifiesAssets = $false
        StagesFiles = $false
    }
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    if ([System.IO.Path]::GetExtension($OutputPath).ToLowerInvariant() -eq ".json") {
        $parent = Split-Path -Parent $OutputPath
        if (-not [string]::IsNullOrWhiteSpace($parent)) {
            New-Item -ItemType Directory -Force -Path $parent | Out-Null
        }
        $template | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
    }
    else {
        Write-MarkdownTemplate -Template $template -Path $OutputPath
    }
}

if ($Json) {
    $template | ConvertTo-Json -Depth 8
}
else {
    Write-Host "LAZ compression acceptance template generated."
    Write-Host "Required evidence count: $($template.Summary.RequiredEvidenceCount)"
    Write-Host "Pending evidence count: $($template.Summary.PendingEvidenceCount)"
    Write-Host "Ready to claim true LAZ: $($template.Summary.CurrentReadyToClaimTrueLaz)"
    if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
        Write-Host "Output: $OutputPath"
    }
}
