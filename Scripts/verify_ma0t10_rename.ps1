param(
    [switch]$IncludeContent
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

$OldPaths = @(
    'm7at10_dt.uproject',
    'Source/m7at10_dt.Target.cs',
    'Source/m7at10_dtEditor.Target.cs',
    'Source/m7at10_dt',
    'Source/m7at10_dtEditor',
    'Source/m7at10_dt/M7AT10',
    'Source/m7at10_dtEditor/Private/m7at10_dtEditor.cpp'
)

$ExpectedPaths = @(
    'ma0t10_dt.uproject',
    'Source/ma0t10_dt.Target.cs',
    'Source/ma0t10_dtEditor.Target.cs',
    'Source/ma0t10_dt',
    'Source/ma0t10_dtEditor',
    'Source/ma0t10_dt/MA0T10',
    'Source/ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.cpp',
    'Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.cpp',
    'Source/ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.h',
    'Source/ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.cpp',
    'Source/ma0t10_dt/MA0T10/Camera/VirtualCameraComp.cpp'
)

$TextFileExtensions = @(
    '.cs', '.cpp', '.h', '.hpp', '.c', '.cc', '.ini', '.json', '.md', '.uproject', '.uplugin', '.ps1', '.txt'
)

$ExcludedDirectoryPatterns = @(
    '\.git\',
    '\Binaries\',
    '\Intermediate\',
    '\Saved\',
    '\DerivedDataCache\'
)

$Failures = New-Object System.Collections.Generic.List[string]

Write-Host '== MA0T10 rename verification =='
Write-Host "Repo: $RepoRoot"
Write-Host ''

Write-Host 'Checking removed old paths...'
foreach ($path in $OldPaths) {
    if (Test-Path -LiteralPath $path) {
        $Failures.Add("Old path still exists: $path")
        Write-Host "[FAIL] Old path still exists: $path" -ForegroundColor Red
    }
    else {
        Write-Host "[OK] Removed: $path"
    }
}

Write-Host ''
Write-Host 'Checking expected new paths...'
foreach ($path in $ExpectedPaths) {
    if (Test-Path -LiteralPath $path) {
        Write-Host "[OK] Exists: $path"
    }
    else {
        $Failures.Add("Expected path is missing: $path")
        Write-Host "[FAIL] Expected path is missing: $path" -ForegroundColor Red
    }
}

Write-Host ''
Write-Host 'Checking staged/working-tree status...'
$Status = git status --short
if ($Status) {
    Write-Host $Status
}
else {
    Write-Host '[OK] Working tree is clean.'
}

if ($IncludeContent) {
    Write-Host ''
    Write-Host 'Scanning text files for old identifiers...'

    $Files = Get-ChildItem -Recurse -File |
        Where-Object {
            $TextFileExtensions -contains $_.Extension.ToLowerInvariant() -and
            -not ($ExcludedDirectoryPatterns | Where-Object { $_ -and ($_.Length -gt 0) -and ($_.ToString() -and ($_.ToString() -ne '') -and ($_.ToString() -ne $null)) } | ForEach-Object { $false })
        }

    $Files = Get-ChildItem -Recurse -File | Where-Object {
        $TextFileExtensions -contains $_.Extension.ToLowerInvariant() -and
        $_.FullName -notmatch '\\.git\\' -and
        $_.FullName -notmatch '\\Binaries\\' -and
        $_.FullName -notmatch '\\Intermediate\\' -and
        $_.FullName -notmatch '\\Saved\\' -and
        $_.FullName -notmatch '\\DerivedDataCache\\'
    }

    $Patterns = @('m7at10_dt', 'M7AT10', 'LogM7AT10', 'm7at10')
    foreach ($file in $Files) {
        $text = Get-Content -LiteralPath $file.FullName -Raw
        foreach ($pattern in $Patterns) {
            if ($text.Contains($pattern)) {
                $relative = $file.FullName.Substring($RepoRoot.Length + 1)
                $Failures.Add("Old identifier '$pattern' remains in $relative")
                Write-Host "[FAIL] Old identifier '$pattern' remains in $relative" -ForegroundColor Red
            }
        }
    }
}
else {
    Write-Host ''
    Write-Host 'Content scan skipped. Re-run with -IncludeContent to scan text identifiers.'
}

Write-Host ''
if ($Failures.Count -gt 0) {
    Write-Host "Verification failed: $($Failures.Count) issue(s)." -ForegroundColor Red
    exit 1
}

Write-Host 'Verification passed.' -ForegroundColor Green
