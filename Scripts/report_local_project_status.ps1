param(
    [string]$ProjectRoot = "C:\Unreal Projects\m7at10_dt"
)

$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "== $Title =="
}

function Get-PathSummary {
    param([string]$FullPath)

    if (-not (Test-Path -LiteralPath $FullPath)) {
        return [PSCustomObject]@{
            State = "missing"
            Kind = "-"
            FileCount = 0
            SizeBytes = 0
        }
    }

    $item = Get-Item -LiteralPath $FullPath
    if ($item.PSIsContainer) {
        $files = @(Get-ChildItem -LiteralPath $FullPath -Recurse -File -ErrorAction SilentlyContinue)
        $sizeBytes = ($files | Measure-Object -Property Length -Sum).Sum
        return [PSCustomObject]@{
            State = "present"
            Kind = "directory"
            FileCount = $files.Count
            SizeBytes = [int64]($sizeBytes)
        }
    }

    return [PSCustomObject]@{
        State = "present"
        Kind = "file"
        FileCount = 1
        SizeBytes = [int64]$item.Length
    }
}

function Format-Size {
    param([int64]$Bytes)

    if ($Bytes -ge 1GB) {
        return "{0:N1} GB" -f ($Bytes / 1GB)
    }
    if ($Bytes -ge 1MB) {
        return "{0:N1} MB" -f ($Bytes / 1MB)
    }
    if ($Bytes -ge 1KB) {
        return "{0:N1} KB" -f ($Bytes / 1KB)
    }
    return "$Bytes B"
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
        [PSCustomObject]@{
            Path = "Content\M7AT10\UI\WBP_VirtualSensorMonitor.uasset"
            Category = "ReviewCandidate"
            Recommendation = "Open in editor and commit only if this is the intended production monitor WBP."
        },
        [PSCustomObject]@{
            Path = "Config\Game.ini"
            Category = "ReviewCandidate"
            Recommendation = "Diff manually; commit only intentional project setting changes."
        },
        [PSCustomObject]@{
            Path = "Content\ChemicalPlantEnv"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit asset/vendor decision; otherwise keep out of this code change."
        },
        [PSCustomObject]@{
            Path = "Content\Materials"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit asset/material decision."
        },
        [PSCustomObject]@{
            Path = "Content\Mega_Crane"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit crane asset decision."
        },
        [PSCustomObject]@{
            Path = "Content\Meshes"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit mesh asset decision."
        },
        [PSCustomObject]@{
            Path = "Content\Textures"
            Category = "LargeContentCandidate"
            Recommendation = "Commit only with an explicit texture asset decision."
        },
        [PSCustomObject]@{
            Path = "Samples\PixelStreaming"
            Category = "SampleOrThirdParty"
            Recommendation = "Keep untracked unless Pixel Streaming samples are intentionally added to the project."
        },
        [PSCustomObject]@{
            Path = "Windows.zip"
            Category = "GeneratedOutput"
            Recommendation = "Do not commit packaged output archives."
        },
        [PSCustomObject]@{
            Path = "Windows"
            Category = "GeneratedOutput"
            Recommendation = "Do not commit packaged output directories."
        },
        [PSCustomObject]@{
            Path = "launcher.config.json"
            Category = "GeneratedOrLocalConfig"
            Recommendation = "Keep untracked unless a launcher workflow explicitly requires it."
        }
    )

    $presentCount = 0
    $generatedCount = 0
    foreach ($entry in $pathsToCheck) {
        $relativePath = $entry.Path
        $summary = Get-PathSummary -FullPath (Join-Path $ProjectRoot $relativePath)
        if ($summary.State -eq "present") {
            ++$presentCount
            if ($entry.Category -like "Generated*") {
                ++$generatedCount
            }
        }

        Write-Host "$relativePath"
        Write-Host "  state: $($summary.State) $($summary.Kind), files=$($summary.FileCount), size=$(Format-Size $summary.SizeBytes)"
        Write-Host "  category: $($entry.Category)"
        Write-Host "  recommendation: $($entry.Recommendation)"
    }

    Write-Section "Asset decision summary"
    Write-Host "Present decision points: $presentCount"
    Write-Host "Generated/local-output items present: $generatedCount"
    Write-Host "Default action: do not stage these paths until each item has an explicit content or packaging decision."

    Write-Section "Suggested next checks"
    Write-Host "Build: & 'C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat' m7at10_dtEditor Win64 Development '$ProjectRoot\m7at10_dt.uproject' -WaitMutex -NoHotReloadFromIDE"
    Write-Host "Smoke: powershell -ExecutionPolicy Bypass -File '.\Scripts\run_smoke_tests.ps1' -SkipBuild"
}
finally {
    Pop-Location
}
