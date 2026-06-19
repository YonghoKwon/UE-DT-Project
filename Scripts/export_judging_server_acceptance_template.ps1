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
        [string]$Owner = "JudgingServerOwner"
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
    $lines += "# Judging Server Acceptance Template"
    $lines += ""
    $lines += "Generated UTC: $($Template.GeneratedUtc)"
    $lines += ""
    $lines += "This is a fillable evidence template. Do not write endpoint URLs, tokens,"
    $lines += "passwords, secrets, or credential values into this file."
    $lines += ""
    $lines += "## Summary"
    $lines += ""
    $lines += "- Required evidence count: $($Template.Summary.RequiredEvidenceCount)"
    $lines += "- Pending evidence count: $($Template.Summary.PendingEvidenceCount)"
    $lines += "- Values redacted: $($Template.Summary.ValuesRedacted)"
    $lines += "- Modifies config: $($Template.Summary.ModifiesConfig)"
    $lines += "- Stages config: $($Template.Summary.StagesConfig)"
    $lines += "- Current ready to claim real server acceptance: $($Template.Summary.CurrentReadyToClaimRealServerAcceptance)"
    $lines += ""
    $lines += "## Acceptance Metadata"
    $lines += ""
    $lines += "- Evidence run id:"
    $lines += "- Environment name:"
    $lines += "- Operator:"
    $lines += "- Map name:"
    $lines += "- PIE/session id:"
    $lines += "- Payload contract report path:"
    $lines += "- Transport log path:"
    $lines += "- Server response evidence path:"
    $lines += "- Secret scan result path:"
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

    $lines | Set-Content -LiteralPath $Path -Encoding UTF8
}

$ProjectRoot = Resolve-ProjectRoot
$payloadFixtureDir = Join-Path $ProjectRoot "Samples\payload_fixtures"
$transportDoc = Join-Path $ProjectRoot "docs\server_transport_contract.md"
$payloadSchemaDoc = Join-Path $ProjectRoot "docs\lidar_payload_schema.md"
$cameraSchemaDoc = Join-Path $ProjectRoot "docs\camera_payload_schema.md"

$requiredEvidence = @(
    (New-EvidenceItem -Name "Endpoint ownership" -RequiredEvidence "Judging-server owner records the target environment, ownership, and override policy without committing URL values."),
    (New-EvidenceItem -Name "Authentication policy" -RequiredEvidence "Header/token scheme, storage policy, rotation owner, and redaction rules are approved without recording secret values."),
    (New-EvidenceItem -Name "LiDAR accepted response" -RequiredEvidence "A virtual-lidar.v1 payload is accepted by the owned server endpoint with status/body evidence saved outside shared secrets."),
    (New-EvidenceItem -Name "Camera accepted response" -RequiredEvidence "A virtual-camera.v1 payload is accepted by the owned server endpoint with status/body evidence saved outside shared secrets."),
    (New-EvidenceItem -Name "Rejected payload behavior" -RequiredEvidence "Malformed or intentionally rejected payload returns the approved error status/body and does not mark transport as accepted."),
    (New-EvidenceItem -Name "Retry timeout behavior" -RequiredEvidence "Timeout, retry count, retryable status codes, idempotency decision, and failure telemetry are recorded."),
    (New-EvidenceItem -Name "Batching and backpressure" -RequiredEvidence "Target LiDAR rate, queue limit, drop/throttle policy, ordering behavior, and operator/server telemetry are accepted."),
    (New-EvidenceItem -Name "Secret scan" -RequiredEvidence "Shared repo/config/template outputs are checked for endpoint, token, password, secret, and credential leakage.")
)

$evidenceSections = [PSCustomObject]@{
    EndpointOwnership = [PSCustomObject]@{
        EnvironmentName = ""
        Owner = ""
        EndpointReferenceLocation = ""
        RuntimeOverridePolicy = ""
        EvidencePath = ""
    }
    AuthenticationPolicy = [PSCustomObject]@{
        SchemeName = ""
        StoragePolicy = ""
        RotationOwner = ""
        RedactionRulesPath = ""
        EvidencePath = ""
    }
    ResponseSchema = [PSCustomObject]@{
        AcceptedResponseSchemaPath = ""
        RejectedResponseSchemaPath = ""
        ErrorCodeMappingPath = ""
        AnalysisResultSchemaPath = ""
    }
    RealEndpointSmoke = [PSCustomObject]@{
        LidarAcceptedEvidencePath = ""
        CameraAcceptedEvidencePath = ""
        RejectedPayloadEvidencePath = ""
        AuthFailureEvidencePath = ""
        TransportLogPath = ""
    }
    RateBackpressure = [PSCustomObject]@{
        TargetLidarRateHz = 0
        QueueLimit = ""
        DropOrThrottlePolicy = ""
        OrderingGuarantee = ""
        EvidencePath = ""
    }
    SecretRedaction = [PSCustomObject]@{
        SecretScanResultPath = ""
        EndpointValuesRedacted = $true
        CredentialValuesRedacted = $true
    }
    OwnerAcceptance = [PSCustomObject]@{
        Accepted = $false
        Owner = ""
        Reviewer = ""
        ReviewedAtUtc = ""
        EvidencePath = ""
    }
}

$generatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$pendingEvidence = @($requiredEvidence | Where-Object { $_.Status -ne "Recorded" })
$template = [PSCustomObject]@{
    GeneratedUtc = $generatedUtc
    ProjectRoot = $ProjectRoot
    ContractInputs = [PSCustomObject]@{
        PayloadFixtureDir = $payloadFixtureDir
        TransportDoc = $transportDoc
        LidarSchemaDoc = $payloadSchemaDoc
        CameraSchemaDoc = $cameraSchemaDoc
    }
    AcceptanceMetadata = [PSCustomObject]@{
        EvidenceRunId = ""
        EnvironmentName = ""
        Operator = ""
        MapName = ""
        PieOrSessionId = ""
        PayloadContractReportPath = ""
        TransportLogPath = ""
        ServerResponseEvidencePath = ""
        SecretScanResultPath = ""
    }
    EvidenceSections = $evidenceSections
    RequiredEvidence = $requiredEvidence
    SafetyBoundary = "This template is read-only with respect to project config and git staging. It records evidence paths and reviewer metadata only; endpoint URLs and credential values must remain outside the repository."
    Summary = [PSCustomObject]@{
        RequiredEvidenceCount = $requiredEvidence.Count
        PendingEvidenceCount = $pendingEvidence.Count
        EvidenceSectionCount = @($evidenceSections.PSObject.Properties).Count
        RequiredEvidenceSections = @("EndpointOwnership", "AuthenticationPolicy", "ResponseSchema", "RealEndpointSmoke", "RateBackpressure", "SecretRedaction", "OwnerAcceptance")
        ValuesRedacted = $true
        ModifiesConfig = $false
        StagesConfig = $false
        WritesEndpointValues = $false
        WritesCredentialValues = $false
        RealJudgingServerAcceptancePresent = $false
        CurrentReadyToClaimRealServerAcceptance = $false
        MustRemainEvidenceOnlyUntilAccepted = $true
    }
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $parent = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Write-MarkdownTemplate -Template $template -Path $OutputPath
}

if ($Json) {
    $template | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Judging server acceptance template generated."
    Write-Host "Required evidence count: $($template.Summary.RequiredEvidenceCount)"
    Write-Host "Pending evidence count: $($template.Summary.PendingEvidenceCount)"
    Write-Host "Values redacted: $($template.Summary.ValuesRedacted)"
    Write-Host "Modifies config: $($template.Summary.ModifiesConfig)"
    Write-Host "Stages config: $($template.Summary.StagesConfig)"
    Write-Host "Ready to claim real server acceptance: $($template.Summary.CurrentReadyToClaimRealServerAcceptance)"
    if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
        Write-Host "Output: $OutputPath"
    }
}
