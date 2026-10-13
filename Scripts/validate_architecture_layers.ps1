param(
    [string]$ProjectRoot = "",
    [string]$SourceRoot = "",
    [string]$AllowlistPath = "",
    [switch]$Json
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $ProjectRoot "Source/ma0t10_dt/MA0T10"
}
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\', '/')
if ([string]::IsNullOrWhiteSpace($AllowlistPath)) {
    $AllowlistPath = Join-Path $ProjectRoot "Scripts/architecture_layers_allowlist.txt"
}

# SourceRoot/AllowlistPath overrides support isolated negative-test fixtures.
# The normal gate always uses the production MA0T10 tree and checked-in allowlist.
function Get-Layer([string]$Path) {
    switch -Regex ($Path) {
        '^Contracts/' { return 0 }
        '^(Sensor|Camera|Core)/' { return 1 }
        '^(Scenario|Slab|WebSocket|Crane|Manager|Player|Api|InteractableActor)/' { return 2 }
        '^UI/Main/' { return 2 }
        '^UI/' { return 3 }
        default { return $null }
    }
}

$files = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File |
    Where-Object { $_.Extension -in @('.h', '.cpp') -and
        $_.FullName.Substring($SourceRoot.Length + 1) -notmatch '(^|[\\/])Tests([\\/]|$)' } |
    Sort-Object FullName)
$byPath = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
$byName = @{}
foreach ($file in $files) {
    $relative = $file.FullName.Substring($SourceRoot.Length + 1).Replace('\', '/')
    $byPath.Add($relative, $relative)
    if (-not $byName.ContainsKey($file.Name)) {
        $byName[$file.Name] = [Collections.Generic.List[string]]::new()
    }
    $byName[$file.Name].Add($relative)
}

function Resolve-Include([string]$Source, [string]$Include) {
    $includePath = $Include.Replace('\', '/')
    $prefix = [regex]::Match($includePath, '^(?:Source/)?(?:ma0t10_dt/)?MA0T10/(.+)$')
    if ($prefix.Success) {
        $candidate = $prefix.Groups[1].Value
        if ($byPath.ContainsKey($candidate)) { return $byPath[$candidate] }
        # Tests are deliberately outside the architectural include graph.
        if ($candidate -notmatch '(^|/)Tests/') {
            $errors.Add("Unresolved project include: $Source -> $Include")
        }
        return $null
    }

    # Quoted local includes, ../ paths, root-relative paths, and unique basenames.
    $sourceDirectory = Split-Path -Parent (Join-Path $SourceRoot $Source)
    $absolute = [IO.Path]::GetFullPath((Join-Path $sourceDirectory $includePath))
    if ($absolute.StartsWith($SourceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        $candidate = $absolute.Substring($SourceRoot.Length + 1).Replace('\', '/')
        if ($byPath.ContainsKey($candidate)) { return $byPath[$candidate] }
    }
    if ($byPath.ContainsKey($includePath)) { return $byPath[$includePath] }
    if ($includePath -notmatch '/' -and $byName.ContainsKey($includePath)) {
        if ($byName[$includePath].Count -eq 1) { return $byName[$includePath][0] }
        $errors.Add("Ambiguous project include: $Source -> $Include")
    }
    return $null # Engine, DTCore, generated headers, and other external modules.
}

$errors = [Collections.Generic.List[string]]::new()
$violations = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
$mappedFiles = 0
$includeCount = 0
foreach ($file in $files) {
    $source = $file.FullName.Substring($SourceRoot.Length + 1).Replace('\', '/')
    $sourceLayer = Get-Layer $source
    if ($null -eq $sourceLayer) { continue }
    $mappedFiles++
    $contents = [IO.File]::ReadAllText($file.FullName)
    # Preserve newlines so diagnostics retain the original source line numbers.
    $contents = [regex]::Replace($contents, '(?s)/\*.*?\*/', [Text.RegularExpressions.MatchEvaluator]{
        param($match)
        return [regex]::Replace($match.Value, '[^\r\n]', ' ')
    })
    $matches = [regex]::Matches($contents, '(?m)^[ \t]*#[ \t]*include[ \t]*[<"]([^>"\r\n]+)[>"]')
    foreach ($match in $matches) {
        $target = Resolve-Include $source $match.Groups[1].Value
        if ($null -eq $target) { continue }
        $includeCount++
        $targetLayer = Get-Layer $target
        if ($null -eq $targetLayer -or $sourceLayer -ge $targetLayer) { continue }
        $entry = "$source -> $target"
        if (-not $violations.ContainsKey($entry)) {
            $line = 1 + [regex]::Matches($contents.Substring(0, $match.Index), '\n').Count
            $violations.Add($entry, [PSCustomObject]@{
                Entry = $entry; Source = $source; Target = $target
                SourceLayer = "L$sourceLayer"; TargetLayer = "L$targetLayer"; Line = $line
            })
        }
    }
}

$allowed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
if (-not (Test-Path -LiteralPath $AllowlistPath -PathType Leaf)) {
    $errors.Add("Allowlist not found: $AllowlistPath")
}
else {
    foreach ($line in [IO.File]::ReadAllLines([IO.Path]::GetFullPath($AllowlistPath))) {
        $entry = $line.Trim()
        if (-not $entry -or $entry.StartsWith('#')) { continue }
        if ($entry -notmatch '^[^\\]+\.(h|cpp) -> [^\\]+\.(h|cpp)$') {
            $errors.Add("Malformed allowlist entry: $entry")
        }
        elseif (-not $allowed.Add($entry)) {
            $errors.Add("Duplicate allowlist entry: $entry")
        }
    }
}

$new = @($violations.Keys | Where-Object { -not $allowed.Contains($_) } | Sort-Object)
$stale = @($allowed | Where-Object { -not $violations.ContainsKey($_) } | Sort-Object)
$valid = $new.Count -eq 0 -and $stale.Count -eq 0 -and $errors.Count -eq 0
$report = [PSCustomObject]@{
    SchemaVersion = 1; ProjectRoot = $ProjectRoot; SourceRoot = $SourceRoot
    AllowlistPath = [IO.Path]::GetFullPath($AllowlistPath); Valid = $valid
    ScannedFileCount = $files.Count; MappedFileCount = $mappedFiles; ResolvedIncludeCount = $includeCount
    ViolationCount = $violations.Count; AllowlistCount = $allowed.Count
    NewViolationCount = $new.Count; StaleAllowlistCount = $stale.Count
    Violations = @($violations.Values | Sort-Object Entry)
    NewViolations = $new; StaleAllowlist = $stale; Errors = @($errors.ToArray())
}
if ($Json) {
    $report | ConvertTo-Json -Depth 6
}
else {
    Write-Host "Architecture layer gate: $(if ($valid) { 'PASS' } else { 'FAIL' })"
    Write-Host "Files=$($files.Count) mapped=$mappedFiles resolved-includes=$includeCount"
    Write-Host "Violations=$($violations.Count) allowlist=$($allowed.Count) new=$($new.Count) stale=$($stale.Count)"
    foreach ($entry in $new) { Write-Host "NEW: $entry" }
    foreach ($entry in $stale) { Write-Host "STALE: $entry" }
    foreach ($errorMessage in $errors) { Write-Host "ERROR: $errorMessage" }
}
if (-not $valid) { exit 1 }
exit 0
