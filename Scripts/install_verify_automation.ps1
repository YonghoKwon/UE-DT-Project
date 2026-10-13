param(
    [switch]$PreCommitHook,
    [switch]$NightlyFeature,
    [string]$NightlyTime = '03:00',
    [switch]$Uninstall
)
# QA-02 opt-in installer. Nothing is installed unless a switch is given; installing is a user decision.
# -PreCommitHook : .git/hooks/pre-commit runs `verify.ps1 -Level quick` (no Unreal, about 10 s).
# -NightlyFeature: Windows scheduled task 'MA0T10 nightly verify' runs `verify.ps1 -Level feature` daily.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$taskName = 'MA0T10 nightly verify'
$hooksDir = (& git.exe -C $root rev-parse --git-path hooks).Trim()
if (-not [IO.Path]::IsPathRooted($hooksDir)) { $hooksDir = Join-Path $root $hooksDir }
$hook = Join-Path $hooksDir 'pre-commit'
$marker = '# installed-by: Scripts/install_verify_automation.ps1'
if (-not ($PreCommitHook -or $NightlyFeature)) { throw 'Choose -PreCommitHook and/or -NightlyFeature (add -Uninstall to remove).' }

if ($PreCommitHook) {
    if ($Uninstall) {
        if ((Test-Path -LiteralPath $hook) -and (Select-String -LiteralPath $hook -SimpleMatch $marker -Quiet)) { Remove-Item -LiteralPath $hook; Write-Host "Removed $hook" }
    } else {
        if ((Test-Path -LiteralPath $hook) -and -not (Select-String -LiteralPath $hook -SimpleMatch $marker -Quiet)) { throw "A different pre-commit hook already exists: $hook" }
        New-Item -ItemType Directory -Force -Path $hooksDir | Out-Null
        $body = @(
            '#!/bin/sh'
            $marker
            'root="$(git rev-parse --show-toplevel)"'
            'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$root/Scripts/verify.ps1" -Level quick -BaseCommit HEAD || { echo "verify quick failed; see Saved/Reports/verify"; exit 1; }'
        ) -join "`n"
        [IO.File]::WriteAllText($hook, $body + "`n", [Text.UTF8Encoding]::new($false))
        Write-Host "Installed $hook"
    }
}
if ($NightlyFeature) {
    if ($Uninstall) {
        if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) { Unregister-ScheduledTask -TaskName $taskName -Confirm:$false; Write-Host "Removed task '$taskName'" }
    } else {
        $action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$root\Scripts\verify.ps1`" -Level feature" -WorkingDirectory $root
        $trigger = New-ScheduledTaskTrigger -Daily -At $NightlyTime
        $settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Hours 3)
        Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Description 'QA-02: nightly verify feature for ma0t10_dt. Skips itself when an Editor is open (verify refuses).' | Out-Null
        Write-Host "Registered task '$taskName' daily at $NightlyTime"
    }
}
