param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt",
    [string]$ProjectPath = "",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [string]$BrokerUrl = "",
    [string]$Topic = "",
    [string]$SourceId = "JsonLiveLidarBridge",
    [string]$Operator = "",
    [string]$Notes = "",
    [switch]$RunCommandletDryRun,
    [switch]$RunEvidenceAutomation,
    [switch]$RunBrokerlessDTCoreDispatchAutomation,
    [switch]$ObservedSourceFrame,
    [switch]$ObservedTargetPoints,
    [switch]$ObservedCachedPayload,
    [switch]$ObservedTransportResult,
    [switch]$WriteReports,
    [switch]$NoWrite,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedNative {
    param(
        [string]$Label,
        [string]$FilePath,
        [string[]]$Arguments
    )

    if (-not $script:JsonMode) {
        Write-Host ""
        Write-Host "==> $Label"
        Write-Host "$FilePath $($Arguments -join ' ')"
    }

    if ($script:JsonMode) {
        & $FilePath @Arguments *> $null
    }
    else {
        & $FilePath @Arguments
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

function Invoke-JsonScript {
    param(
        [string]$Label,
        [string]$ScriptPath,
        [hashtable]$Parameters
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "$Label script not found: $ScriptPath"
    }

    if (-not $script:JsonMode) {
        Write-Host ""
        Write-Host "==> $Label"
        Write-Host $ScriptPath
    }

    $jsonText = & $ScriptPath @Parameters -Json
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
    return $jsonText | ConvertFrom-Json
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}
$script:JsonMode = [bool]$Json
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $ProjectRoot "m7at10_dt.uproject"
}
if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) {
    throw "Project file not found: $ProjectPath"
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sampleValidator = Join-Path $scriptRoot "validate_websocket_lidar_live_sample.ps1"
$registrationReport = Join-Path $scriptRoot "export_websocket_transaction_registration_report.ps1"
$brokerSmokeReport = Join-Path $scriptRoot "export_websocket_broker_smoke_report.ps1"
$editorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

$sample = Invoke-JsonScript `
    -Label "WebSocket live LiDAR sample validation" `
    -ScriptPath $sampleValidator `
    -Parameters @{ ProjectRoot = $ProjectRoot }

if (-not [bool]$sample.Summary.Valid) {
    throw "WebSocket live LiDAR sample validation did not report Valid=true."
}

$registration = Invoke-JsonScript `
    -Label "WebSocket transaction registration report" `
    -ScriptPath $registrationReport `
    -Parameters @{ ProjectRoot = $ProjectRoot; NoWrite = $true }

foreach ($check in @(
    "Valid",
    "ConfigPointsToExpectedTable",
    "HandlerClassExists",
    "SamplePayloadValid",
    "SafeSourceRoutingDocumented"
)) {
    if (-not [bool]$registration.Summary.$check) {
        throw "WebSocket transaction registration report did not report $check=true."
    }
}

$commandletArgs = @(
    $ProjectPath,
    "-run=EnsureLidarJsonLiveFrameTransaction",
    "-unattended",
    "-nop4",
    "-NoSave"
)

$commandletDryRun = [PSCustomObject]@{
    Ran = $false
    Passed = $false
    Command = "$editorCmd $($commandletArgs -join ' ')"
}

if ($RunCommandletDryRun) {
    if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
        throw "UnrealEditor-Cmd.exe not found: $editorCmd"
    }
    Invoke-CheckedNative `
        -Label "EnsureLidarJsonLiveFrameTransaction commandlet dry run" `
        -FilePath $editorCmd `
        -Arguments $commandletArgs

    $commandletDryRun = [PSCustomObject]@{
        Ran = $true
        Passed = $true
        Command = "$editorCmd $($commandletArgs -join ' ')"
    }
}

$testName = $registration.ExpectedRegistration.OptionalAutomationTestName
$automationArgs = @(
    $ProjectPath,
    "-NullRHI",
    "-Unattended",
    "-NoSplash",
    "-NoSound",
    "-ExecCmds=Automation RunTests $testName; Quit",
    "-TestExit=Automation Test Queue Empty"
)

$evidenceAutomation = [PSCustomObject]@{
    Ran = $false
    Passed = $false
    Command = "$editorCmd $($automationArgs -join ' ')"
    TestName = $testName
}

if ($RunEvidenceAutomation) {
    if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
        throw "UnrealEditor-Cmd.exe not found: $editorCmd"
    }

    Invoke-CheckedNative `
        -Label "Automation $testName" `
        -FilePath $editorCmd `
        -Arguments $automationArgs

    $evidenceAutomation = [PSCustomObject]@{
        Ran = $true
        Passed = $true
        Command = "$editorCmd $($automationArgs -join ' ')"
        TestName = $testName
    }
}

$brokerlessTestName = "M7AT10.RealSensorSource.JsonLiveDTCoreDispatch"
$brokerlessArgs = @(
    $ProjectPath,
    "-NullRHI",
    "-Unattended",
    "-NoSplash",
    "-NoSound",
    "-ExecCmds=Automation RunTests $brokerlessTestName; Quit",
    "-TestExit=Automation Test Queue Empty"
)

$brokerlessDTCoreDispatchAutomation = [PSCustomObject]@{
    Ran = $false
    Passed = $false
    Command = "$editorCmd $($brokerlessArgs -join ' ')"
    TestName = $brokerlessTestName
}

if ($RunBrokerlessDTCoreDispatchAutomation) {
    if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
        throw "UnrealEditor-Cmd.exe not found: $editorCmd"
    }

    Invoke-CheckedNative `
        -Label "Automation $brokerlessTestName" `
        -FilePath $editorCmd `
        -Arguments $brokerlessArgs

    $brokerlessDTCoreDispatchAutomation = [PSCustomObject]@{
        Ran = $true
        Passed = $true
        Command = "$editorCmd $($brokerlessArgs -join ' ')"
        TestName = $brokerlessTestName
    }
}

$brokerParams = @{
    ProjectRoot = $ProjectRoot
    BrokerUrl = $BrokerUrl
    Topic = $Topic
    SourceId = $SourceId
    Operator = $Operator
    Notes = $Notes
}
if ($ObservedSourceFrame) { $brokerParams.ObservedSourceFrame = $true }
if ($ObservedTargetPoints) { $brokerParams.ObservedTargetPoints = $true }
if ($ObservedCachedPayload) { $brokerParams.ObservedCachedPayload = $true }
if ($ObservedTransportResult) { $brokerParams.ObservedTransportResult = $true }
if ($NoWrite -or -not $WriteReports) { $brokerParams.NoWrite = $true }

$broker = Invoke-JsonScript `
    -Label "WebSocket broker smoke report" `
    -ScriptPath $brokerSmokeReport `
    -Parameters $brokerParams

$report = [PSCustomObject]@{
    ProjectRoot = $ProjectRoot
    ProjectPath = $ProjectPath
    Sample = $sample.Summary
    Registration = $registration.Summary
    CommandletDryRun = $commandletDryRun
    EvidenceAutomation = $evidenceAutomation
    BrokerlessDTCoreDispatchAutomation = $brokerlessDTCoreDispatchAutomation
    BrokerSmoke = $broker.Summary
    BrokerReportOutput = $broker.Output
    Summary = [PSCustomObject]@{
        Valid = $true
        SampleValid = [bool]$sample.Summary.Valid
        RegistrationChecklistValid = [bool]$registration.Summary.Valid
        ConfigPointsToExpectedTable = [bool]$registration.Summary.ConfigPointsToExpectedTable
        HandlerClassExists = [bool]$registration.Summary.HandlerClassExists
        SafeSourceRoutingDocumented = [bool]$registration.Summary.SafeSourceRoutingDocumented
        BinaryDataTableRowVerifiedByReport = [bool]$registration.Summary.BinaryDataTableRowVerified
        BinaryDataTableRowVerificationNote = $registration.Summary.BinaryDataTableRowVerificationNote
        CommandletDryRunPassed = ($RunCommandletDryRun -and $commandletDryRun.Passed)
        EvidenceAutomationPassed = ($RunEvidenceAutomation -and $evidenceAutomation.Passed)
        BrokerlessDTCoreDispatchAutomationPassed = ($RunBrokerlessDTCoreDispatchAutomation -and $brokerlessDTCoreDispatchAutomation.Passed)
        BrokerSmokeComplete = [bool]$broker.Summary.BrokerSmokeComplete
        ExternalBrokerStillRequired = -not [bool]$broker.Summary.BrokerSmokeComplete
        DoesNotConnectToBroker = [bool]$broker.Summary.DoesNotConnectToBroker
        ReportsWritten = [bool]($WriteReports -and -not $NoWrite)
    }
}

if ($Json) {
    $report | ConvertTo-Json -Depth 8
}
else {
    Write-Host ""
    Write-Host "WebSocket LiDAR smoke evidence workflow complete."
    Write-Host "Sample valid: $($report.Summary.SampleValid)"
    Write-Host "Registration checklist valid: $($report.Summary.RegistrationChecklistValid)"
    Write-Host "Commandlet dry run passed: $($report.Summary.CommandletDryRunPassed)"
    Write-Host "Evidence automation passed: $($report.Summary.EvidenceAutomationPassed)"
    Write-Host "Brokerless DTCore dispatch automation passed: $($report.Summary.BrokerlessDTCoreDispatchAutomationPassed)"
    Write-Host "Broker smoke complete: $($report.Summary.BrokerSmokeComplete)"
    Write-Host "Does not connect to broker: $($report.Summary.DoesNotConnectToBroker)"
    Write-Host "Reports written: $($report.Summary.ReportsWritten)"
    Write-Host "Commandlet dry run command: $($commandletDryRun.Command)"
    Write-Host "Evidence automation command: $($evidenceAutomation.Command)"
    Write-Host "Brokerless DTCore dispatch command: $($brokerlessDTCoreDispatchAutomation.Command)"
    if ($broker.Output -and $report.Summary.ReportsWritten) {
        Write-Host "Broker smoke JSON: $($broker.Output.JsonPath)"
        Write-Host "Broker smoke Markdown: $($broker.Output.MarkdownPath)"
    }
}
