param([string]$RenderedTextReport='', [switch]$RequireRenderedText, [switch]$Json)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$policy=Get-Content -Raw -Encoding UTF8 "$PSScriptRoot/fixtures/sensor_ui_vocabulary.json" | ConvertFrom-Json
$violations=[Collections.Generic.List[object]]::new()
$count=0
function Inspect-Text([string]$Area,[string]$Text){
 foreach($word in $policy.forbidden){
  $literalException=@($policy.literalWordExceptions|Where-Object{$_.area -ceq $Area -and $_.word -ceq $word -and $Text -cin $_.texts}).Count -gt 0
  if(!$literalException -and $Text.IndexOf($word,[StringComparison]::OrdinalIgnoreCase) -ge 0){$violations.Add([pscustomobject]@{Area=$Area;Word=$word;Text=$Text})}
 }
}
# Only displayed LOCTEXT values are labels; UserReason's internal match tokens are not UI text.
$source=Get-Content -Raw -Encoding UTF8 "$root/Source/ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
foreach($match in [regex]::Matches($source,'LOCTEXT\("(?<key>[^"\r\n]+)"\s*,\s*"(?<text>(?:\\.|[^"\\])*)"\)')){
 $count++;Inspect-Text "constant:$($match.Groups['key'].Value)" $match.Groups['text'].Value
}
if(!$count){throw 'No vocabulary constants inspected'}
$areas=@();$allowed=@()
if($RenderedTextReport){
 $report=Get-Content -Raw -Encoding UTF8 $RenderedTextReport|ConvertFrom-Json
 if($report.schemaVersion -ne 1){throw 'Unknown rendered-text report schema'}
 foreach($area in $report.areas){
  if(@($area.texts).Count -eq 0){throw "Empty text area: $($area.name)"}
  if($area.name -in $policy.allowedAreas){$allowed+=@($area.name);continue}
  $areas+=@($area.name)
  foreach($value in $area.texts){Inspect-Text $area.name $value}
 }
 foreach($name in $policy.requiredAreas){if($name -notin $areas){throw "Required default area not inspected: $name"}}
 foreach($name in $policy.allowedAreas){if($name -notin $allowed){throw "Explicit diagnostic/compatibility area not recorded: $name"}}
}elseif($RequireRenderedText){throw 'Supply -RenderedTextReport from MA0T10.UXR07.OwnedScreenVocabulary'}
$result=[ordered]@{Valid=($violations.Count -eq 0);Constants=$count;DefaultAreas=$areas;AllowedAreas=$allowed;Violations=@($violations.ToArray());RenderedTextChecked=[bool]$RenderedTextReport;Limit='Fixture Slate labels and tooltips; arbitrary user IDs, editable values, Designer/legacy WBP and unexercised runtime states require manual review.'}
if($Json){$result|ConvertTo-Json -Depth 6}else{$result|ConvertTo-Json -Depth 6|Write-Output}
if($violations.Count){exit 1}
