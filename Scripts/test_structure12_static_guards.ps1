param([string]$ReportRoot = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $ReportRoot) { $ReportRoot = Join-Path $root ('Saved/Reports/structure12-static/' + [Guid]::NewGuid().ToString('N')) }
$owned = [IO.Path]::GetFullPath($ReportRoot)
$allowed = [IO.Path]::GetFullPath((Join-Path $root 'Saved/Reports')).TrimEnd('\') + '\'
if (-not $owned.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $owned)) { throw 'Use a fresh owned Saved/Reports directory.' }
New-Item -ItemType Directory -Path $owned | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$paths = @('Config/DefaultEngine.ini', 'Plugins/DTDevice/TestHost/Config/DefaultEngine.ini')
$section = '[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]'
foreach ($relative in $paths) {
    $file = Join-Path $owned $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file) | Out-Null
    [IO.File]::WriteAllText($file, $section + "`nSecurityToken=`n", $utf8)
}
& powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/validate_repository_tokens.ps1" -ProjectRoot $owned -Json *> "$owned/empty.log"
if ($LASTEXITCODE) { throw 'Empty token fixture rejected.' }
$marker = 'not-a-real-secret-fixture'
[IO.File]::WriteAllText((Join-Path $owned $paths[1]), $section + "`nSecurityToken=$marker`n", $utf8)
$ErrorActionPreference = 'Continue'
& powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/validate_repository_tokens.ps1" -ProjectRoot $owned -Json *> "$owned/nonempty.log"
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -eq 0) { throw 'Nonempty token fixture accepted.' }
if ([IO.File]::ReadAllText("$owned/nonempty.log").Contains($marker)) { throw 'Guard printed token fixture.' }
# Both repositories must explicitly override the engine's auto-generated token default.
foreach ($relative in $paths) {
    foreach ($reset in $paths) { [IO.File]::WriteAllText((Join-Path $owned $reset), $section + "`nSecurityToken=`n", $utf8) }
    foreach ($missing in @('key', 'section')) {
        $fixture = if ($missing -eq 'key') { $section + "`nbEnablePlugin=True`n" } else { "[OtherSection]`nSecurityToken=`n" }
        [IO.File]::WriteAllText((Join-Path $owned $relative), $fixture, $utf8)
        $log = Join-Path $owned ("missing-$missing-" + ($relative -replace '[^a-zA-Z0-9]', '_') + '.log')
        $ErrorActionPreference = 'Continue'
        & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/validate_repository_tokens.ps1" -ProjectRoot $owned -Json *> $log
        $ErrorActionPreference = 'Stop'
        if ($LASTEXITCODE -eq 0) { throw "Missing $missing fixture accepted." }
    }
}
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot/validate_point_cloud_preview_policy.ps1", [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Preview policy script parse failed.' }
foreach ($name in @('Assert-FileExists', 'Assert-ContainsText')) {
    $node = @($ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name }, $true))
    if ($node.Count -ne 1) { throw "Expected one $name" }
    . ([scriptblock]::Create($node[0].Extent.Text))
}
$first = Join-Path $owned 'body.cpp'; $second = Join-Path $owned 'export.cpp'
[IO.File]::WriteAllText($first, 'void MainBody();', $utf8)
[IO.File]::WriteAllText($second, 'Preview is uncapped', $utf8)
Assert-ContainsText -Path @($first, $second) -Pattern 'Preview is uncapped' -Label 'split implementation'
[IO.File]::WriteAllText($second, 'missing', $utf8)
$rejected = $false
try { Assert-ContainsText -Path @($first, $second) -Pattern 'Preview is uncapped' -Label 'split implementation' }
catch { $rejected = $true }
if (-not $rejected) { throw 'Missing product warning accepted.' }
@{ Pass = $true; EmptyTokenAccepted = $true; NonemptyTokenRejected = $true; MissingKeyRejected = $true; MissingSectionRejected = $true; ValuesPrinted = $false; MissingWarningRejected = $rejected; NoUnrealExecution = $true } |
    ConvertTo-Json | Set-Content "$owned/manifest.json" -Encoding UTF8
Write-Output 'STRUCTURE-12 static guard tests PASS.'
