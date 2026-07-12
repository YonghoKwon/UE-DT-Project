param(
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

function Move-PathIfExists {
    param(
        [Parameter(Mandatory=$true)][string]$From,
        [Parameter(Mandatory=$true)][string]$To
    )

    if (-not (Test-Path -LiteralPath $From)) {
        Write-Host "[SKIP] Missing: $From"
        return
    }

    if (Test-Path -LiteralPath $To) {
        Write-Host "[SKIP] Target already exists: $To"
        return
    }

    $toParent = Split-Path $To -Parent
    if ($toParent -and -not (Test-Path -LiteralPath $toParent)) {
        if ($Apply) {
            New-Item -ItemType Directory -Force -Path $toParent | Out-Null
        }
        Write-Host "[MKDIR] $toParent"
    }

    if ($Apply) {
        git mv $From $To
    }

    Write-Host "[MOVE] $From -> $To"
}

# Long source files are intentionally handled locally with git mv instead of
# GitHub contents API copy/delete to avoid truncation or manual recomposition errors.
$Moves = @(
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h'; To = 'Source/ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.cpp'; To = 'Source/ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Camera/VirtualCameraComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Camera/VirtualCameraComp.cpp' }
)

foreach ($move in $Moves) {
    Move-PathIfExists -From $move.From -To $move.To
}

$TextFileExtensions = @('.cs', '.cpp', '.h', '.hpp', '.c', '.cc', '.ini', '.json', '.md', '.uproject', '.uplugin', '.ps1', '.txt')

$Files = Get-ChildItem -Recurse -File |
    Where-Object {
        $TextFileExtensions -contains $_.Extension.ToLowerInvariant() -and
        $_.FullName -notmatch '\\.git\\' -and
        $_.FullName -notmatch '\\Binaries\\' -and
        $_.FullName -notmatch '\\Intermediate\\' -and
        $_.FullName -notmatch '\\Saved\\' -and
        $_.FullName -notmatch '\\DerivedDataCache\\'
    }

foreach ($file in $Files) {
    $old = Get-Content -LiteralPath $file.FullName -Raw
    $new = $old `
        -replace 'm7at10_dtEditor', 'ma0t10_dtEditor' `
        -replace 'm7at10_dt/M7AT10', 'ma0t10_dt/MA0T10' `
        -replace 'm7at10_dt', 'ma0t10_dt' `
        -replace 'M7AT10_DT_API', 'MA0T10_DT_API' `
        -replace 'M7AT10', 'MA0T10' `
        -replace 'LogM7AT10', 'LogMA0T10' `
        -replace 'm7at10', 'ma0t10'

    if ($new -ne $old) {
        if ($Apply) {
            Set-Content -LiteralPath $file.FullName -Value $new -NoNewline -Encoding UTF8
        }
        Write-Host "[EDIT] $($file.FullName.Substring($RepoRoot.Length + 1))"
    }
}

Write-Host ''
if ($Apply) {
    Write-Host 'Done. Review changes with: git status'
    Write-Host 'Then commit with: git add -A; git commit -m "Rename remaining long source files to MA0T10"; git push'
} else {
    Write-Host 'Dry run complete. Re-run with -Apply to make changes.'
}
