# Dot-source only. Credentials stay in process memory, never arguments or logs.
function Get-Pcd20ArtemisSession([string]$ProjectRoot) {
    $scriptPath=Join-Path $ProjectRoot 'Scripts/run_sensor_map_stream_rhi_smoke.ps1'
    $parseTokens=$null;$parseErrors=$null
    $ast=[System.Management.Automation.Language.Parser]::ParseFile($scriptPath,[ref]$parseTokens,[ref]$parseErrors)
    $session=@{UserName='';Passcode='';Source='existing smoke-runner defaults'}
    foreach($parameter in $ast.ParamBlock.Parameters){
        if($parameter.Name.VariablePath.UserPath -eq 'UserName'){$session.UserName=$parameter.DefaultValue.SafeGetValue()}
        if($parameter.Name.VariablePath.UserPath -eq 'Password'){$session.Passcode=$parameter.DefaultValue.SafeGetValue()}
    }
    foreach($relative in @('Config/Game.ini','Saved/Config/WindowsEditor/Game.ini','Config/SensorTransport.ini')){
        $path=Join-Path $ProjectRoot $relative;if(!(Test-Path -LiteralPath $path)){continue}
        $content=[IO.File]::ReadAllText($path)
        $pass=[regex]::Match($content,'(?im)^[ \t]*(?:WebSocketPasscode|Passcode|Password)[ \t]*=[ \t]*([^\r\n]+)')
        $user=[regex]::Match($content,'(?im)^[ \t]*(?:WebSocketLogin|UserName)[ \t]*=[ \t]*([^\r\n]+)')
        if($pass.Success -and $pass.Groups[1].Value.Trim().Length){$session.Passcode=$pass.Groups[1].Value.Trim();if($user.Success){$session.UserName=$user.Groups[1].Value.Trim()};$session.Source=$relative;break}
    }
    if(!$session.UserName -or !$session.Passcode){throw 'No usable credential in existing local settings; do not guess or modify broker accounts'}
    return $session
}
