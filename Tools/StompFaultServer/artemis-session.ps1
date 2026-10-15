# Dot-source only. Credentials stay in process memory, never arguments or logs.
function Get-Pcd20ArtemisSession([string]$ProjectRoot) {
    $session=@{UserName=[string]$env:MA0T10_ARTEMIS_USER;Passcode=[string]$env:MA0T10_ARTEMIS_PASSWORD;Source='MA0T10_ARTEMIS_USER/MA0T10_ARTEMIS_PASSWORD environment'}
    foreach($relative in @('Config/Game.ini','Saved/Config/WindowsEditor/Game.ini','Config/SensorTransport.ini')){
        $path=Join-Path $ProjectRoot $relative;if(!(Test-Path -LiteralPath $path)){continue}
        $content=[IO.File]::ReadAllText($path)
        $pass=[regex]::Match($content,'(?im)^[ \t]*(?:WebSocketPasscode|Passcode|Password)[ \t]*=[ \t]*([^\r\n]+)')
        $user=[regex]::Match($content,'(?im)^[ \t]*(?:WebSocketLogin|UserName)[ \t]*=[ \t]*([^\r\n]+)')
        if($pass.Success -and $pass.Groups[1].Value.Trim().Length){$session.Passcode=$pass.Groups[1].Value.Trim();if($user.Success){$session.UserName=$user.Groups[1].Value.Trim()};$session.Source=$relative;break}
    }
    if(!$session.UserName -or !$session.Passcode){throw 'No usable Artemis credential: set MA0T10_ARTEMIS_USER and MA0T10_ARTEMIS_PASSWORD (or existing local settings); do not guess or modify broker accounts'}
    return $session
}
