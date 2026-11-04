param([string]$Baseline='3e639177', [string]$Candidate='HEAD')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root ('Saved/Reports/DTCORE-EXT-09/perf-core-fixture/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
$owned=Join-Path $report 'owned'
New-Item -ItemType Directory -Force -Path $owned|Out-Null
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run_perf_abab.ps1'),[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Perf script parse failure'}
foreach($name in @('Get-Git','Get-Sha256','Get-DTCorePerfCommit','Prepare-DTCorePerfArchive')){
 $definition=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true))
 if($definition.Count -ne 1){throw "Missing/ambiguous helper $name"}
 . ([scriptblock]::Create($definition[0].Extent.Text))
}
$rows=@()
foreach($case in @(@{Label='A';Revision=$Baseline},@{Label='B';Revision=$Candidate})){
 $target=Join-Path $owned $case.Label
 New-Item -ItemType Directory -Path $target|Out-Null
 $expectedLine=Get-Git @('ls-tree',$case.Revision,'--','Plugins/DTCore')
 $expected=($expectedLine -split '\s+')[2]
 $prepared=Prepare-DTCorePerfArchive -Target $target -Revision $case.Revision -Label $case.Label
 if($prepared.DTCore -ne $expected){throw 'Archive dependency differs from the referenced tree'}
 $relative='Source/DTCore/Public/Core/DxWebSocketSubsystem.h'
 $zip=Join-Path $owned ($case.Label+'-expected-header.zip')
 & git.exe -C (Join-Path $root 'Plugins/DTCore') archive --format=zip "--output=$zip" $expected $relative
 if($LASTEXITCODE){throw 'Expected dependency fixture failed'}
 $fixture=Join-Path $owned ($case.Label+'-fixture');Expand-Archive -LiteralPath $zip -DestinationPath $fixture
 $actual=Get-Sha256 (Join-Path $target ('Plugins/DTCore/'+$relative))
 if($actual -ne (Get-Sha256 (Join-Path $fixture $relative))){throw 'Wrong DTCore bytes in prepared archive'}
 $rows+=@{Label=$case.Label;Revision=$case.Revision;Commit=$prepared.DTCore;HeaderSha256=$actual}
}
if($rows[0].Commit -eq $rows[1].Commit){throw 'Fixture must exercise differing dependency pins'}
$rejected=$false
try{Get-DTCorePerfCommit -Revision ($Baseline+':Source')|Out-Null}catch{if($_.Exception.Message -like 'Missing/invalid DTCore gitlink*'){$rejected=$true}else{throw}}
if(-not $rejected){throw 'Missing pin silently fell back to current dependency'}
@{Pass=$true;Cases=$rows;MissingPinRejected=$rejected;Builds=0;UnrealRuns=0;Note='Preparation only. Measurement code, workload and 2% verdict remain untouched.'}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $report 'manifest.json') -Encoding UTF8
Write-Output "DTCore per-reference archive fixture PASS: $report/manifest.json"
