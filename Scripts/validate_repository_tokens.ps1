param([string]$ProjectRoot = '', [switch]$Json)
$ErrorActionPreference = 'Stop'
if (-not $ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$files = @('Config/DefaultEngine.ini', 'Plugins/DTDevice/TestHost/Config/DefaultEngine.ini')
$invalid = @()
foreach ($relative in $files) {
    $path = Join-Path $ProjectRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required configuration missing: $relative" }
    $section = ''
    $sectionFound = $false
    $tokenFound = $false
    $nonempty = $false
    foreach ($line in [IO.File]::ReadAllLines($path)) {
        $text = $line.Trim()
        if ($text -match '^\[(.+)\]$') { $section = $Matches[1]; continue }
        if ($section -eq '/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings') {
            $sectionFound = $true
            if ($text -match '^SecurityToken\s*=(.*)$') {
                $tokenFound = $true
                if ($Matches[1].Trim().Trim('"').Length) { $nonempty = $true }
            }
        }
    }
    if (-not $sectionFound -or -not $tokenFound -or $nonempty) { $invalid += $relative }
}
$result = @{ Pass = ($invalid.Count -eq 0); CheckedFiles = $files; InvalidFiles = $invalid; ValuesPrinted = $false }
if ($Json) { $result | ConvertTo-Json -Depth 4 } else { Write-Output "Repository token policy: $($result.Pass)" }
if ($invalid.Count) { throw 'AndroidFileServer section/key missing or token non-empty; values suppressed.' }
