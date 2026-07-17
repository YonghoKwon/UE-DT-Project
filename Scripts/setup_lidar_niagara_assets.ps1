param(
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3\Engine"
)

$ErrorActionPreference = "Stop"
$Project = (Resolve-Path (Join-Path $PSScriptRoot "..\ma0t10_dt.uproject")).Path
$EditorCmd = Join-Path $EngineRoot "Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path -LiteralPath $EditorCmd)) {
    throw "UnrealEditor-Cmd.exe not found: $EditorCmd"
}

& $EditorCmd $Project -run=SetupVirtualLidarNiagaraAssets -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) {
    throw "Virtual LiDAR Niagara asset setup failed with exit code $LASTEXITCODE"
}
