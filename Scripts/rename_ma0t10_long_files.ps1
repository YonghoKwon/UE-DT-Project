param(
    [switch]$Apply,
    [switch]$VerifyAfterApply
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

$Failures = New-Object System.Collections.Generic.List[string]

function Add-Failure {
    param([Parameter(Mandatory=$true)][string]$Message)
    $Failures.Add($Message)
    Write-Host "[FAIL] $Message" -ForegroundColor Red
}

function Move-PathIfExists {
    param(
        [Parameter(Mandatory=$true)][string]$From,
        [Parameter(Mandatory=$true)][string]$To
    )

    $fromExists = Test-Path -LiteralPath $From
    $toExists = Test-Path -LiteralPath $To

    if (-not $fromExists -and $toExists) {
        Write-Host "[OK] Already moved: $To"
        return
    }

    if (-not $fromExists -and -not $toExists) {
        Add-Failure "Source and target are both missing: $From -> $To"
        return
    }

    if ($fromExists -and $toExists) {
        Add-Failure "Both source and target exist; resolve manually before continuing: $From -> $To"
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

Write-Host '== MA0T10 long source rename =='
Write-Host "Repo: $RepoRoot"
Write-Host "Mode: $(if ($Apply) { 'APPLY' } else { 'DRY RUN' })"
Write-Host ''

foreach ($move in $Moves) {
    Move-PathIfExists -From $move.From -To $move.To
}

if ($Failures.Count -gt 0) {
    Write-Host ''
    Write-Host "Stopped before content replacement because $($Failures.Count) path issue(s) were found." -ForegroundColor Red
    exit 1
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

$ReplacementRules = @(
    @{ From = 'm7at10_dtEditor'; To = 'ma0t10_dtEditor' },
    @{ From = 'm7at10_dt/M7AT10'; To = 'ma0t10_dt/MA0T10' },
    @{ From = 'm7at10_dt'; To = 'ma0t10_dt' },
    @{ From = 'M7AT10_DT_API'; To = 'MA0T10_DT_API' },
    @{ From = 'LogM7AT10'; To = 'LogMA0T10' },
    @{ From = 'M7AT10'; To = 'MA0T10' },
    @{ From = 'm7at10'; To = 'ma0t10' }
)

foreach ($file in $Files) {
    $old = Get-Content -LiteralPath $file.FullName -Raw
    $new = $old

    foreach ($rule in $ReplacementRules) {
        $new = $new.Replace($rule.From, $rule.To)
    }

    if ($new -ne $old) {
        if ($Apply) {
            Set-Content -LiteralPath $file.FullName -Value $new -NoNewline -Encoding UTF8
        }
        Write-Host "[EDIT] $($file.FullName.Substring($RepoRoot.Length + 1))"
    }
}

Write-Host ''
if ($Apply) {
    Write-Host 'Long-file rename and content replacement complete.' -ForegroundColor Green

    if ($VerifyAfterApply) {
        $Verifier = Join-Path $PSScriptRoot 'verify_ma0t10_rename.ps1'
        if (Test-Path -LiteralPath $Verifier) {
            Write-Host ''
            Write-Host 'Running verification...'
            & $Verifier -IncludeContent
        }
        else {
            Write-Host '[WARN] Verification script not found. Run Scripts/verify_ma0t10_rename.ps1 manually after pulling latest branch.' -ForegroundColor Yellow
        }
    }

    Write-Host ''
    Write-Host 'Review changes with: git status'
    Write-Host 'Then commit with: git add -A; git commit -m "Rename remaining long source files to MA0T10"; git push'
}
else {
    Write-Host 'Dry run complete. Re-run with -Apply to make changes.'
    Write-Host 'Recommended: powershell -ExecutionPolicy Bypass -File .\Scripts\rename_ma0t10_long_files.ps1 -Apply -VerifyAfterApply'
}
