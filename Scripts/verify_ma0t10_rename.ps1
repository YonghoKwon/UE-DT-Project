param(
    [switch]$IncludeContent
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

$OldPaths = @(
    'ma0t10_dt.uproject',
    'Source/ma0t10_dt.Target.cs',
    'Source/ma0t10_dtEditor.Target.cs',
    'Source/ma0t10_dt',
    'Source/ma0t10_dtEditor/Private/ma0t10_dtEditor.cpp',
    'Source/ma0t10_dt/MA0T10'
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
    '.cs', '.cpp', '.h', '.hpp', '.c', '.cc', '.ini', '.json', '.uproject', '.uplugin', '.txt'
)

$Failures = New-Object System.Collections.Generic.List[string]

function Add-Failure {
    param([Parameter(Mandatory=$true)][string]$Message)
    $Failures.Add($Message)
    Write-Host "[FAIL] $Message" -ForegroundColor Red
}

function Write-Ok {
    param([Parameter(Mandatory=$true)][string]$Message)
    Write-Host "[OK] $Message"
}

Write-Host '== MA0T10 rename verification =='
Write-Host "Repo: $RepoRoot"
Write-Host ''

Write-Host 'Checking removed old paths...'
foreach ($path in $OldPaths) {
    if (Test-Path -LiteralPath $path) {
        Add-Failure "Old path still exists: $path"
    }
    else {
        Write-Ok "Removed: $path"
    }
}

Write-Host ''
Write-Host 'Checking expected new paths...'
foreach ($path in $ExpectedPaths) {
    if (Test-Path -LiteralPath $path) {
        Write-Ok "Exists: $path"
    }
    else {
        Add-Failure "Expected path is missing: $path"
    }
}

Write-Host ''
Write-Host 'Checking staged/working-tree status...'
$Status = git status --short
if ($Status) {
    Write-Host $Status
}
else {
    Write-Ok 'Working tree is clean.'
}

if ($IncludeContent) {
    Write-Host ''
    Write-Host 'Scanning Source and project descriptor files for old identifiers...'

    $SourceFiles = @()
    if (Test-Path -LiteralPath 'Source') {
        $SourceFiles += Get-ChildItem -LiteralPath 'Source' -Recurse -File | Where-Object {
            $TextFileExtensions -contains $_.Extension.ToLowerInvariant() -and
            $_.FullName -notmatch '\\Binaries\\' -and
            $_.FullName -notmatch '\\Intermediate\\' -and
            $_.FullName -notmatch '\\Saved\\' -and
            $_.FullName -notmatch '\\DerivedDataCache\\'
        }
    }

    $RootFiles = Get-ChildItem -LiteralPath $RepoRoot -File | Where-Object {
        $_.Name -like '*.uproject' -or $_.Name -like '*.uplugin'
    }

    $Files = @($SourceFiles + $RootFiles) | Sort-Object FullName -Unique
    $Patterns = @('ma0t10_dt', 'MA0T10', 'LogMA0T10', 'ma0t10')

    foreach ($file in $Files) {
        $text = Get-Content -LiteralPath $file.FullName -Raw
        foreach ($pattern in $Patterns) {
            if ($text.Contains($pattern)) {
                $relative = $file.FullName.Substring($RepoRoot.Length + 1)
                Add-Failure "Old identifier '$pattern' remains in $relative"
            }
        }
    }

    if ($Files.Count -eq 0) {
        Add-Failure 'No source/project descriptor files were scanned.'
    }
    else {
        Write-Ok "Scanned $($Files.Count) source/project descriptor file(s)."
    }
}
else {
    Write-Host ''
    Write-Host 'Content scan skipped. Re-run with -IncludeContent to scan source/project identifiers.'
}

Write-Host ''
if ($Failures.Count -gt 0) {
    Write-Host "Verification failed: $($Failures.Count) issue(s)." -ForegroundColor Red
    exit 1
}

Write-Host 'Verification passed.' -ForegroundColor Green
