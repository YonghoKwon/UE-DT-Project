# Dot-source only. Resolves local Artemis test-broker credentials from the
# process environment; values are never echoed, logged or placed on command lines.
function Resolve-Ma0t10ArtemisCredential([string]$UserName = '', [string]$Password = '') {
    $previousUser = [Environment]::GetEnvironmentVariable('MA0T10_ARTEMIS_USER', 'Process')
    $previousPassword = [Environment]::GetEnvironmentVariable('MA0T10_ARTEMIS_PASSWORD', 'Process')
    if (-not $UserName) { $UserName = $previousUser }
    if (-not $Password) { $Password = $previousPassword }
    $missing = @()
    if (-not $UserName) { $missing += 'MA0T10_ARTEMIS_USER' }
    if (-not $Password) { $missing += 'MA0T10_ARTEMIS_PASSWORD' }
    if ($missing.Count) {
        throw "Artemis credentials are not set: $($missing -join ', '). Set them in this session first, e.g. `$env:MA0T10_ARTEMIS_USER='<user>'; `$env:MA0T10_ARTEMIS_PASSWORD='<password>' (see Tools/Artemis/README.md)."
    }
    # Child processes (Unreal automation, Node probes) inherit these; nothing passes the secret as an argument.
    [Environment]::SetEnvironmentVariable('MA0T10_ARTEMIS_USER', $UserName, 'Process')
    [Environment]::SetEnvironmentVariable('MA0T10_ARTEMIS_PASSWORD', $Password, 'Process')
    return [PSCustomObject]@{ UserName = $UserName; PreviousUserName = $previousUser; PreviousPassword = $previousPassword }
}
# Puts back whatever the caller's session had, so running a script never erases the user's own variables.
function Restore-Ma0t10ArtemisCredential($Credential) {
    if (-not $Credential) { return }
    [Environment]::SetEnvironmentVariable('MA0T10_ARTEMIS_USER', $Credential.PreviousUserName, 'Process')
    [Environment]::SetEnvironmentVariable('MA0T10_ARTEMIS_PASSWORD', $Credential.PreviousPassword, 'Process')
}
