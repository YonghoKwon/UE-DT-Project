"""Read-only documentation audit: links, current symbols, includes and repository paths.
Historical text/evidence is reported separately, never rewritten or treated as current API.
No engine, build, browser, network or application is launched.
"""
from pathlib import Path
import argparse,json,re,urllib.parse
P=argparse.ArgumentParser();P.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);P.add_argument('--output',type=Path,required=True);A=P.parse_args();R=A.root.resolve();D=R/'Plugins/DTDevice'
headers=list((R/'Source').rglob('*.h'))+list((D/'Source').rglob('*.h'))+list((R/'Plugins/DTCore/Source').rglob('*.h'))
symbols=set();by_name={}
for p in headers:
 text=p.read_text(encoding='utf-8-sig');symbols.update(re.findall(r'\b(?:class|struct|enum\s+class)\s+(?:\w+_API\s+)?([AUFIE]\w+)',text));by_name.setdefault(p.name,[]).append(p)
legacy=set()
for p in [R/'Config/DefaultEngine.ini',D/'Config/DefaultDTDevice.ini']:
 for old,new in re.findall(r'\+ClassRedirects=\(OldName="([^"]+)",NewName="([^"]+)"',p.read_text(encoding='utf-8-sig')):
  old=old.rsplit('.',1)[-1];new=new.rsplit('.',1)[-1]
  if old!=new:
   for prefix in ['U','A']:
    if prefix+new in symbols:legacy.add(prefix+old)
link=re.compile(r'!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+"[^"\n]*")?\)')
files=[R/'README.md',R/'AGENTS.md']+list((R/'docs').rglob('*.md'))+[D/'README.md']+list((D/'Docs').rglob('*.md'))
current_errors=[];historical_missing=[];evidence=[];optional=[];checked=0;symbol_count=0;include_count=0;path_count=0
for p in files:
 if p.name in ['CLAUDE.md','IMPROVEMENTS_CLAUDE.md']:continue
 rel=p.relative_to(R).as_posix();historical='/archive/' in '/'+rel or '/research/' in '/'+rel or p.name=='VALIDATION_HISTORY.md';s=p.read_text(encoding='utf-8-sig')
 prose=re.sub(r'^```[^\n]*\n.*?^```[^\n]*$','',s,flags=re.M|re.S)
 for m in link.finditer(prose):
  href=m.group(1).strip('<>');target=urllib.parse.unquote(href.split('#',1)[0])
  if not target or re.match(r'^[a-zA-Z][\w+.-]*:',target) or target.startswith('/'):continue
  q=(p.parent/target).resolve();checked+=1
  if q.exists():continue
  item={'Document':rel,'Target':href}
  if '/Saved/' in q.as_posix() or '/Binaries/' in q.as_posix() or '/Intermediate/' in q.as_posix():evidence.append(item)
  elif historical:historical_missing.append(item)
  else:current_errors.append(dict(item,Kind='MissingRelativeLink'))
 if historical:continue
 tokens=set(re.findall(r'\b(?:[AU](?:Dx|Device|Virtual|Slab|Ship|Coil|Crane|Lidar|Real|Factory|Sensor|Train|Data|Status|Interactable)[A-Za-z0-9_]*|FVirtual[A-Za-z0-9_]+|IDevice[A-Za-z0-9_]+)\b',s))
 for token in sorted(tokens):
  symbol_count+=1
  if token not in symbols:current_errors.append({'Document':rel,'Kind':'UnknownCurrentSymbol','Symbol':token})
 if p in [R/'README.md',R/'AGENTS.md',D/'README.md',D/'Docs/GettingStarted.md',D/'Docs/Compatibility.md',D/'Docs/DataChannels.md']:
  for token in legacy:
   if re.search(r'\b'+re.escape(token)+r'\b',s):current_errors.append({'Document':rel,'Kind':'LegacyNameInCurrentEntry','Symbol':token})
 for inc in re.findall(r'^\s*#include\s+"([^"]+)"',s,re.M):
  include_count+=1
  options=[h for h in by_name.get(inc.rsplit('/',1)[-1],[]) if h.as_posix().endswith('/'+inc)]
  if not options and not inc.startswith(('Core','Engine','Components','Misc','HAL','UObject')):current_errors.append({'Document':rel,'Kind':'MissingInclude','Target':inc})
 for token in re.findall(r'`((?:Source|Plugins|Scripts|Tools|Config)/[^`\n]+)`',prose):
  if any(x in token for x in ['*','<','>','{','}','…','...',' ']) or not re.search(r'\.(?:h|cpp|cs|ps1|py|ini|json|uasset|umap|uplugin|txt)$',token):continue
  path_count+=1
  if token=='Config/Game.ini':optional.append({'Document':rel,'Target':token});continue
  if not (R/token).exists():current_errors.append({'Document':rel,'Kind':'MissingCurrentPath','Target':token})
# Cross-check the documented empty-project panel wiring without running Unreal.
guide_api={
 'Source/DeviceUI/Public/DeviceUI/DxDeviceSensorMonitorPanel.h':['RefreshSensors','SelectSensor','RefreshStatus','CaptureOnce','AdjustPreview'],
 'Source/DeviceUI/Public/DeviceUI/DeviceSensorMonitorHostActor.h':['PanelClass','bShowOnBeginPlay'],
 'Source/DeviceRuntime/Public/Device/Sensor/DeviceSemanticPolicy.h':['Configure'],
}
api_checked=0
for name,methods in guide_api.items():
 text=(D/name).read_text(encoding='utf-8-sig')
 for method in methods:
  api_checked+=1
  if not re.search(r'\b'+re.escape(method)+r'\b',text):current_errors.append({'Kind':'GuideApiMissing','File':name,'Name':method})
for name in ['Content/Sensor/Materials/M_LidarSemanticId.uasset','Content/Sensor/VFX/NS_VirtualLidarPointCloud.uasset','Content/Sensor/VFX/M_VirtualLidarPointSprite.uasset']:
 if not (D/name).is_file():current_errors.append({'Kind':'GuideAssetMissing','File':name})

root_docs=sorted(p.name for p in R.glob('*.md') if p.name!='CLAUDE.md')
if root_docs!=['AGENTS.md','README.md']:current_errors.append({'Kind':'RootDocumentPolicy','Found':root_docs})
result={'Pass':not current_errors,'UnrealOrBuildExecuted':False,'Files':len(files),'RelativeLinksChecked':checked,'CurrentSymbolsChecked':symbol_count,'IncludesChecked':include_count,'GuideApiChecked':api_checked,'CurrentPathsChecked':path_count,'Errors':current_errors,'HistoricalMissing':historical_missing,'UserLocalOptionalConfigReferences':optional,'ExternalLocalEvidenceReferences':evidence,'HistoricalPolicy':'Archive/research/immutable VALIDATION_HISTORY retain original names and external evidence. Missing links are reported separately; no current source/API assertion is derived from them.'}
A.output.parent.mkdir(parents=True,exist_ok=True);A.output.write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(json.dumps({k:result[k] for k in ['Pass','Files','RelativeLinksChecked','CurrentSymbolsChecked','IncludesChecked','CurrentPathsChecked']}));print('Current errors:',len(current_errors),'historical missing:',len(historical_missing),'local evidence:',len(evidence));raise SystemExit(0 if result['Pass'] else 1)
