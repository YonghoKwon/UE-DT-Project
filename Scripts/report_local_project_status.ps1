param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt"
)

$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "== $Title =="
}

if (-not (Test-Path -LiteralPath $ProjectRoot)) {
    throw "ProjectRoot not found: $ProjectRoot"
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
Push-Location $ProjectRoot
try {
    Write-Section "Project"
    Write-Host "Root: $ProjectRoot"
    $uproject = Get-ChildItem -LiteralPath $ProjectRoot -Filter *.uproject -File | Select-Object -First 1
    if ($uproject) {
        Write-Host "UProject: $($uproject.Name)"
    }

    Write-Section "Git"
    git branch --show-current
    git log --oneline -3
    git status --short

    Write-Section "Submodules"
    git submodule status --recursive

    Write-Section "Local asset decision points"
    $pathsToCheck = @(
        "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset",
        "Config\Game.ini",
        "Content\ChemicalPlantEnv",
        "Content\Mega_Crane",
        "Samples\PixelStreaming",
        "Windows.zip",
        "Windows",
        "launcher.config.json"
    )

    foreach ($relativePath in $pathsToCheck) {
        $fullPath = Join-Path $ProjectRoot $relativePath
        if (Test-Path -LiteralPath $fullPath) {
            $item = Get-Item -LiteralPath $fullPath
            if ($item.PSIsContainer) {
                $fileCount = (Get-ChildItem -LiteralPath $fullPath -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
                Write-Host "$relativePath : present directory ($fileCount files)"
            }
            else {
                Write-Host "$relativePath : present file ($([Math]::Round($item.Length / 1KB, 1)) KB)"
            }
        }
        else {
            Write-Host "$relativePath : missing"
        }
    }

    Write-Section "Suggested next checks"
    Write-Host "Build: & 'C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat' m7at10_dtEditor Win64 Development '$ProjectRoot\m7at10_dt.uproject' -WaitMutex"
    Write-Host "Smoke: powershell -ExecutionPolicy Bypass -File '.\Scripts\run_smoke_tests.ps1' -SkipBuild"
}
finally {
    Pop-Location
}
