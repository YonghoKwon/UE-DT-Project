param(
    [switch]$Apply,
    [switch]$VerifyAfterApply
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

function Move-PathIfNeeded {
    param(
        [Parameter(Mandatory=$true)][string]$From,
        [Parameter(Mandatory=$true)][string]$To
    )

    $fromExists = Test-Path -LiteralPath $From
    $toExists = Test-Path -LiteralPath $To

    if ($fromExists -and $toExists) {
        throw "Both source and target exist. Resolve manually before continuing. Source=$From Target=$To"
    }

    if (-not $fromExists -and -not $toExists) {
        throw "Both source and target are missing. Source=$From Target=$To"
    }

    if (-not $fromExists -and $toExists) {
        Write-Host "[OK] Already moved: $To"
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

# These files were still present under Source/m7at10_dt/M7AT10 after the long-file pass.
# They already had most identifiers replaced by the previous text pass; this script completes
# the physical path move so old Source/m7at10_dt/M7AT10 files do not remain modified in place.
$Moves = @(
    @{ From = 'Source/m7at10_dt/M7AT10/Api/Handler/CraneInfoGetByBittInOracle.cpp'; To = 'Source/ma0t10_dt/MA0T10/Api/Handler/CraneInfoGetByBittInOracle.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Api/Handler/CraneInfoGetByBittInOracle.h'; To = 'Source/ma0t10_dt/MA0T10/Api/Handler/CraneInfoGetByBittInOracle.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/CameraJsonLiveSourceComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/CameraJsonLiveSourceComp.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/CameraJsonLiveSourceComp.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/CameraJsonLiveSourceComp.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/CsvPointCloudPreviewActor.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/CsvPointCloudPreviewActor.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComp.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComp.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarHttpJsonLiveSourceComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarHttpJsonLiveSourceComp.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarHttpJsonLiveSourceComp.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarHttpJsonLiveSourceComp.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarJsonLinesReplaySourceComp.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarJsonLinesReplaySourceComp.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarJsonLiveSourceComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarJsonLiveSourceComp.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarJsonLiveSourceComp.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarJsonLiveSourceComp.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarUdpJsonLiveSourceComp.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarUdpJsonLiveSourceComp.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/LidarUdpJsonLiveSourceComp.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/LidarUdpJsonLiveSourceComp.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/Tests/CsvPointCloudPreviewAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/CsvPointCloudPreviewAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/Tests/EditorMapSmokeAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/EditorMapSmokeAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/Tests/LidarReplayAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/LidarReplayAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/Tests/RealSensorSourceAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/RealSensorSourceAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/Tests/SensorManagerAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/SensorManagerAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/Tests/VirtualSensorDataTransportAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/VirtualSensorDataTransportAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/Tests/VirtualSensorRecorderAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/Sensor/Tests/VirtualSensorRecorderAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/Sensor/VirtualSensorAct.h'; To = 'Source/ma0t10_dt/MA0T10/Sensor/VirtualSensorAct.h' },
    @{ From = 'Source/m7at10_dt/M7AT10/UI/Tests/VirtualSensorMonitorHostAutomationTests.cpp'; To = 'Source/ma0t10_dt/MA0T10/UI/Tests/VirtualSensorMonitorHostAutomationTests.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/WebSocket/TC/LidarJsonLiveFrameTC.cpp'; To = 'Source/ma0t10_dt/MA0T10/WebSocket/TC/LidarJsonLiveFrameTC.cpp' },
    @{ From = 'Source/m7at10_dt/M7AT10/WebSocket/TC/LidarJsonLiveFrameTC.h'; To = 'Source/ma0t10_dt/MA0T10/WebSocket/TC/LidarJsonLiveFrameTC.h' }
)

foreach ($move in $Moves) {
    Move-PathIfNeeded -From $move.From -To $move.To
}

Write-Host ''
if ($Apply) {
    Write-Host 'Residual old-path moves complete.'
    if ($VerifyAfterApply) {
        $verifyScript = Join-Path $PSScriptRoot 'verify_ma0t10_rename.ps1'
        if (Test-Path -LiteralPath $verifyScript) {
            & $verifyScript -IncludeContent
        }
        else {
            Write-Host '[WARN] verify_ma0t10_rename.ps1 not found.' -ForegroundColor Yellow
        }
    }
    Write-Host 'Review changes with: git status'
    Write-Host 'Then commit with: git add -A; git commit -m "Rename residual old-path source files to MA0T10"; git push'
}
else {
    Write-Host 'Dry run complete. Re-run with -Apply to make changes.'
}
