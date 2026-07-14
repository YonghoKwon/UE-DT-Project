param(
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

$Moves = @(
    @{ From = 'Source/ma0t10_dt/MA0T10'; To = 'Source/ma0t10_dt/MA0T10' },
    @{ From = 'Source/ma0t10_dtEditor/Private/EnsureLidarJsonLiveFrameTransactionCommandlet.cpp'; To = 'Source/ma0t10_dtEditor/Private/EnsureLidarJsonLiveFrameTransactionCommandlet.cpp' },
    @{ From = 'Source/ma0t10_dtEditor/Private/EnsureLidarJsonLiveFrameTransactionCommandlet.h'; To = 'Source/ma0t10_dtEditor/Private/EnsureLidarJsonLiveFrameTransactionCommandlet.h' }
)

foreach ($Move in $Moves) {
    $From = Join-Path $RepoRoot $Move.From
    $To = Join-Path $RepoRoot $Move.To

    if (-not (Test-Path $From)) {
        Write-Host "Skip missing: $($Move.From)"
        continue
    }

    $ToParent = Split-Path $To -Parent
    if (-not (Test-Path $ToParent)) {
        New-Item -ItemType Directory -Path $ToParent -Force | Out-Null
    }

    if ($Apply) {
        git mv $Move.From $Move.To
        Write-Host "Moved: $($Move.From) -> $($Move.To)"
    } else {
        Write-Host "Would move: $($Move.From) -> $($Move.To)"
    }
}

$TextFiles = Get-ChildItem -Path @('Source', 'Config', 'Scripts', 'docs', 'Samples') -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        $_.Extension -in @('.h', '.hpp', '.cpp', '.c', '.cs', '.ini', '.json', '.jsonl', '.md', '.ps1', '.uproject', '.uplugin', '.txt', '.csv')
    }

foreach ($File in $TextFiles) {
    $Original = Get-Content $File.FullName -Raw
    $Updated = $Original `
        -replace 'MA0T10', 'MA0T10' `
        -replace 'ma0t10_dtEditor', 'ma0t10_dtEditor' `
        -replace 'ma0t10_dt', 'ma0t10_dt' `
        -replace 'ma0t10', 'ma0t10'

    if ($Updated -ne $Original) {
        if ($Apply) {
            Set-Content -Path $File.FullName -Value $Updated -NoNewline
            Write-Host "Updated text: $($File.FullName.Substring($RepoRoot.Length + 1))"
        } else {
            Write-Host "Would update text: $($File.FullName.Substring($RepoRoot.Length + 1))"
        }
    }
}

if (-not $Apply) {
    Write-Host ''
    Write-Host 'Dry run only. Re-run with -Apply to modify files.'
}
