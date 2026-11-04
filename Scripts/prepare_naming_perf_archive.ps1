# NAMING-01: 이름만 다른 기준본의 소유 사본에서 공통 계측 코드를 컴파일한다.
# 측정 본문, workload, 시간, 판정식에는 접근하지 않는다.
function Resolve-NamingLegacyInclude([string]$Target, [string]$Include) {
    $plan=Get-Content (Join-Path $root 'Tools/NamingPlan/compatibility.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $legacy=$Include
    foreach($pair in $plan.names.PSObject.Properties){
        $legacy=$legacy.Replace($pair.Value.Substring(1)+'.h',$pair.Name.Substring(1)+'.h')
    }
    if(Test-Path (Join-Path $Target ('Source/'+$legacy))){return $legacy}
    if(-not(Test-Path (Join-Path $Target 'Source/ma0t10_dt'))){throw "Baseline public header missing: $Include"}
    $matches=@(Get-ChildItem (Join-Path $Target 'Source/ma0t10_dt') -Recurse -File -Filter ([IO.Path]::GetFileName($legacy)))
    if($matches.Count -ne 1){throw "Baseline public header missing: $Include (missing or ambiguous)"}
    return $matches[0].FullName.Substring((Join-Path $Target 'Source').Length+1).Replace('\','/')
}

function Prepare-NamingPerfArchive([string]$Target,[string]$InstrumentPath) {
    $plan=Get-Content (Join-Path $root 'Tools/NamingPlan/compatibility.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $forwarded=@();$aliases=@();$legacy=$false
    foreach($owner in @('host','plugin')){
        $base=if($owner -eq 'host'){$Target}else{Join-Path $Target 'Plugins/DTDevice'}
        foreach($move in $plan.moves.$owner.PSObject.Properties){
            if(-not $move.Name.EndsWith('.h') -or ($owner -eq 'plugin' -and $move.Name -notmatch '/Public/')){continue}
            $old=Join-Path $base $move.Name;$new=Join-Path $base $move.Value
            if(-not(Test-Path $old) -or (Test-Path $new)){continue}
            New-Item -ItemType Directory -Force -Path (Split-Path $new -Parent)|Out-Null
            $inc=if($owner -eq 'plugin'){$move.Name.Split(@('/Public/'),[StringSplitOptions]::None)[-1]}else{$move.Name.Substring('Source/'.Length)}
            $text="#pragma once`r`n#include `"$inc`"`r`n"
            foreach($pair in $plan.names.PSObject.Properties){
                if([IO.Path]::GetFileNameWithoutExtension($old) -eq $pair.Name.Substring(1)){
                    $text+="using $($pair.Value) = $($pair.Name);`r`n";$legacy=$true
                }
            }
            [IO.File]::WriteAllText($new,$text,[Text.UTF8Encoding]::new($false));$forwarded+=$move.Value
        }
    }
    # Includes are processed before aliases; undef in the common instrument prevents unity leakage.
    $capture=Join-Path $Target 'Plugins/DTDevice/Source/DeviceRuntime/Public/Device/Sensor/VirtualLidarSensorActor.h'
    if(Test-Path $capture){$legacy=(-not(Test-Path (Join-Path $Target 'Plugins/DTDevice/DTDevice.uplugin')) -or ([IO.File]::ReadAllText($capture) -match '\bScanComponent\b'))}
    $header=Join-Path (Split-Path $InstrumentPath -Parent) 'NamingPerfAliases.h'
    $text="#pragma once`r`n"
    if($legacy){
        $source=[IO.File]::ReadAllText($InstrumentPath)
        foreach($pair in @($plan.names.PSObject.Properties)+@($plan.members.PSObject.Properties)){
            if($source -match ('\b'+[regex]::Escape($pair.Value)+'\b')){
                $text+="#define $($pair.Value) $($pair.Name)`r`n";$aliases+=@{New=$pair.Value;Old=$pair.Name}
            }
        }
    }
    [IO.File]::WriteAllText($header,$text,[Text.UTF8Encoding]::new($false))
    return @{LegacyNaming=$legacy;ForwardHeaders=$forwarded;MemberAliases=$aliases;HeaderSha256=(Get-FileHash $header).Hash}
}
