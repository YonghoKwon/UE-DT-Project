# Rename remaining long MA0T10 source files

This script completes the remaining long source-file moves that are safer to perform locally with `git mv` than through the GitHub contents API.

## Why this exists

The remaining files are long C++ files. Copying them through the GitHub contents API requires manual recomposition from multiple chunks, which can accidentally drop or duplicate lines. Running this script locally keeps the full file contents intact.

## Files handled

- `Source/m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.cpp`
- `Source/m7at10_dt/M7AT10/Sensor/VirtualSensorManager.cpp`
- `Source/m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h`
- `Source/m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.cpp`
- `Source/m7at10_dt/M7AT10/Camera/VirtualCameraComp.cpp`

They are moved to the matching `Source/ma0t10_dt/MA0T10/...` path.

## Usage

From the repository root on the `agent/rename-ma0t10` branch:

```powershell
git pull
powershell -ExecutionPolicy Bypass -File .\Scripts\rename_ma0t10_long_files.ps1
powershell -ExecutionPolicy Bypass -File .\Scripts\rename_ma0t10_long_files.ps1 -Apply

git status
git add -A
git commit -m "Rename remaining long source files to MA0T10"
git push
```

The first command without `-Apply` is a dry run. It prints the planned moves and text edits without modifying files.

## After running

Regenerate Visual Studio project files and rebuild the Unreal project.

Also search for leftovers:

```powershell
Get-ChildItem -Recurse Source,Scripts -File | Select-String "m7at10_dt|M7AT10|LogM7AT10|m7at10"
```

Do not raw-rename `.uasset` or `.umap` folders unless you plan to fix redirectors inside Unreal Editor.
