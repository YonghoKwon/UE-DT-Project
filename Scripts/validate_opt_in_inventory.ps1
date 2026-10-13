param([string]$ProjectRoot = '', [switch]$Json)
$ErrorActionPreference = 'Stop'
if (-not $ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$sourceNames = @([IO.Directory]::EnumerateFiles((Join-Path $ProjectRoot 'Source'), '*', [IO.SearchOption]::AllDirectories) |
    Where-Object { [IO.Path]::GetExtension($_) -in @('.cpp', '.h') } | ForEach-Object {
        [regex]::Matches([IO.File]::ReadAllText($_), 'GetEnvironmentVariable\s*\(\s*TEXT\(\s*"(MA0T10_[A-Z0-9_]+)"') |
            ForEach-Object { $_.Groups[1].Value }
    } | Sort-Object -Unique)
$readme = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'README.md'))
$table = [regex]::Match($readme, '(?s)<!-- MA0T10_ENV_TABLE_BEGIN -->(.*?)<!-- MA0T10_ENV_TABLE_END -->')
if (-not $table.Success) { throw 'README environment inventory markers are missing' }
$documented = @([regex]::Matches($table.Groups[1].Value, '(?m)^\| `(MA0T10_[A-Z0-9_]+)` \|') |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
$missing = @($sourceNames | Where-Object { $_ -notin $documented })
$stale = @($documented | Where-Object { $_ -notin $sourceNames })
$report = [PSCustomObject]@{ Valid = ($missing.Count -eq 0 -and $stale.Count -eq 0); SourceCount = $sourceNames.Count; DocumentedCount = $documented.Count; Missing = $missing; Stale = $stale; Names = $sourceNames }
if ($Json) { $report | ConvertTo-Json -Depth 4 } else { Write-Host "Opt-in inventory: valid=$($report.Valid) source=$($sourceNames.Count) documented=$($documented.Count)" }
if (-not $report.Valid) { throw "Environment inventory mismatch: missing=$($missing -join ',') stale=$($stale -join ',')" }
