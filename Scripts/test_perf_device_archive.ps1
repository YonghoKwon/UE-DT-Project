param([string]$Baseline = 'origin/master', [string]$ReportRoot = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $ReportRoot) { $ReportRoot = Join-Path $root ('Saved/Reports/perf-device-preparation/' + (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')) }
$ReportRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ReportRoot)
if (Test-Path -LiteralPath $ReportRoot) { throw 'Use a fresh report directory' }
$owned = Join-Path $ReportRoot 'owned'
New-Item -ItemType Directory -Path $owned -Force | Out-Null
$instrumentSource = Join-Path $root 'Source/ma0t10_dtEditor/Private/Perf01AcquisitionTests.cpp'
$sessionInstrument = 'Plugins/DTDevice/Source/DeviceRuntime/Public/Device/Tests/Sensor/FinalAcceptanceTestSession.h'
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run_perf_abab.ps1'), [ref]$tokens, [ref]$parseErrors)
if (@($parseErrors).Count) { throw 'Perf script does not parse' }
foreach ($name in @('Get-Git','Get-Sha256','Prepare-DevicePerfArchive')) {
    $definition = @($ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true))
    if ($definition.Count -ne 1) { throw "Expected one $name function" }
    . ([scriptblock]::Create($definition[0].Extent.Text))
}
$baselineSha = Get-Git @('rev-parse', $Baseline)
$currentSha = Get-Git @('rev-parse', 'HEAD')
$headers = @([regex]::Matches([IO.File]::ReadAllText($instrumentSource), '#include\s+"(Device/[^"\r\n]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
$legacySessionPath = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h'
$paths = @('Source/ma0t10_dtEditor/ma0t10_dtEditor.Build.cs', $legacySessionPath)
foreach ($header in $headers) {
    if ($header -ne 'Device/Tests/Sensor/FinalAcceptanceTestSession.h') { $paths += 'Source/ma0t10_dt/MA0T10/' + $header.Substring('Device/'.Length) }
}
$legacy = Join-Path $owned 'legacy'
$zip = Join-Path $owned 'legacy-source.zip'
& git.exe -C $root archive --format=zip "--output=$zip" $baselineSha @paths
if ($LASTEXITCODE -ne 0) { throw 'Legacy fixture archive failed' }
Expand-Archive -LiteralPath $zip -DestinationPath $legacy
$before = @{}; foreach ($path in $paths | Where-Object { $_ -like '*.h' -and $_ -ne $legacySessionPath }) { $before[$path] = Get-Sha256 (Join-Path $legacy $path) }
$old = Prepare-DevicePerfArchive -Target $legacy -Revision $baselineSha -Label 'Legacy'
if (-not $old.LegacyIncludes -or $old.Headers.Count -ne ($headers.Count - 1)) { throw 'Legacy forwarding coverage differs from instrument includes' }
if (Test-Path -LiteralPath (Join-Path $legacy 'Plugins/DTDevice/DTDevice.uplugin')) { throw 'Candidate plugin leaked into legacy baseline' }
foreach ($path in $before.Keys) { if ((Get-Sha256 (Join-Path $legacy $path)) -ne $before[$path]) { throw 'Baseline public header changed' } }
$forwardedSession = [IO.File]::ReadAllText((Join-Path $legacy $legacySessionPath))
$canonicalSession = (Join-Path $legacy $sessionInstrument).Replace('\', '/')
if (-not $old.LegacySessionForwarded -or -not $forwardedSession.Contains('#include "'+$canonicalSession+'"') -or $forwardedSession.Contains('class FStartFinalAcceptancePIE')) { throw 'Unity baseline still has two shared fixture bodies or requires an Editor-only include path' }
$newTarget = Join-Path $owned 'modern'
New-Item -ItemType Directory -Path $newTarget | Out-Null
$new = Prepare-DevicePerfArchive -Target $newTarget -Revision $currentSha -Label 'Modern'
if ($new.LegacyIncludes -or -not $new.DTDevice -or -not (Test-Path -LiteralPath (Join-Path $newTarget 'Plugins/DTDevice/DTDevice.uplugin'))) { throw 'Current gitlink archive absent' }
foreach ($target in @($legacy,$newTarget)) {
    $session = Join-Path $target $sessionInstrument
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $session) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $sessionInstrument) -Destination $session
    $testCopy = Join-Path $target 'Perf01AcquisitionTests.cpp'
    Copy-Item -LiteralPath $instrumentSource -Destination $testCopy
    if ((Get-Sha256 $session) -ne (Get-Sha256 (Join-Path $root $sessionInstrument)) -or (Get-Sha256 $testCopy) -ne (Get-Sha256 $instrumentSource)) { throw 'Common instrumentation bytes changed' }
}
$bad = Join-Path $owned 'missing-header'
New-Item -ItemType Directory -Path $bad | Out-Null
$rejected = $false
try { Prepare-DevicePerfArchive -Target $bad -Revision $baselineSha -Label 'Missing' | Out-Null }
catch { if ($_.Exception.Message -like 'Baseline public header missing:*') { $rejected = $true } else { throw } }
if (-not $rejected) { throw 'Missing legacy header accepted' }
$result = [ordered]@{ Pass=$true; Baseline=$baselineSha; Candidate=$currentSha; LegacyHeaders=$old.Headers.Count; ModernDTDevice=$new.DTDevice; BaselineHeadersUnchanged=$true; LegacySessionSingleBody=$true; InstrumentBytesIdentical=$true; MissingHeaderRejected=$true; NoUnrealExecution=$true; OwnedFixtures=$owned }
$result | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $ReportRoot 'manifest.json')
Write-Output "Perf archive preparation PASS: legacy=$($old.Headers.Count), modern pin=$($new.DTDevice), missing-header rejected"
