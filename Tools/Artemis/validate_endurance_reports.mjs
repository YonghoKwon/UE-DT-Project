import fs from 'node:fs';
import path from 'node:path';
const args=new Map();for(let i=2;i<process.argv.length;i+=2)args.set(process.argv[i],process.argv[i+1]);
const root=args.get('--directory'),mode=args.get('--mode')??'steady';
const label=mode==='steady'?'soak-three':'repeat-three';
const read=f=>JSON.parse(fs.readFileSync(path.join(root,f),'utf8').replace(/^\uFEFF/,''));
const external=read(label+'.external.json');
const messages=new Map();let duplicates=0;
for(const m of external.metadataLedger??[]){if(messages.has(m.requestId))duplicates++;messages.set(m.requestId,m);}
const quantile=a=>{const b=[...a].sort((x,y)=>x-y);return b.length?b[Math.ceil(.95*b.length)-1]:null;};
const files=mode==='steady'?[label+'.ledger.json']:Array.from({length:10},(_,i)=>`cycle-${String(i+1).padStart(2,'0')}.ledger.json`);
const runs=[];
for(const file of files){
  const ledger=read(file);let missing=0,invalid=0,gaps=0,clock=0;const groups=new Map();
  for(const f of ledger.frames){
    const m=messages.get(f.request_id),g=groups.get(f.kind)??{kind:f.kind,rows:[],native:[],external:[]};
    g.rows.push(f);g.native.push(f.e2e_ms);
    if(!m)missing++;else {if(!m.valid)invalid++;const delay=Date.parse(m.receivedUtc)-Date.parse(f.acquisition_utc);if(!Number.isFinite(delay)||delay<0)clock++;else g.external.push(delay);}
    if(!f.submit_monotonic||!f.receipt_monotonic||!f.consumer_monotonic)missing++;
    if(!f.clock_valid)clock++;groups.set(f.kind,g);
  }
  const streams=[...groups.values()].map(g=>{
    const sensors=new Map();for(const f of g.rows){const a=sensors.get(f.sensor_id)??[];a.push(f.frame_id);sensors.set(f.sensor_id,a);}
    for(const a of sensors.values()){a.sort((x,y)=>x-y);for(let i=1;i<a.length;i++)gaps+=Math.abs(a[i]-a[i-1]-1);}
    return {kind:g.kind,count:g.rows.length,hz:g.rows.length/ledger.duration_seconds,nativeP95:quantile(g.native),externalP95:quantile(g.external)};
  });
  runs.push({file,viewport:[ledger.actual_viewport_x,ledger.actual_viewport_y],streams,missing,invalid,gaps,clock,passed:groups.size===3&&!missing&&!invalid&&!gaps&&!clock&&streams.every(s=>s.hz>=(s.kind===1?29:19)&&s.nativeP95<=(s.kind===1?250:200)&&s.externalP95<=(s.kind===1?250:200))});
}
const log=fs.readFileSync(path.join(root,label+'.log'),'utf8');
// Automation report EndEvents repeats the same log lines; do not count them as
// a second execution, and reject conflicting observations of a cycle.
const cycleMap=new Map();let conflictingCycles=0;
for(const m of log.matchAll(/\[AcceptanceCycle\] cycle=(\d+) private=(\d+) physical=(\d+) cleanupMs=([\d.]+)/g)){
  const c={cycle:+m[1],privateBytes:+m[2],physicalBytes:+m[3],cleanupMs:+m[4]},old=cycleMap.get(c.cycle);
  if(old&&JSON.stringify(old)!==JSON.stringify(c))conflictingCycles++;cycleMap.set(c.cycle,c);
}
const cycles=[...cycleMap.values()];
const memory=read(label+'.memory.json');
const result={mode,externalPassed:external.success,externalDuplicates:duplicates,runs,cycles,conflictingCycles,memorySamples:memory.length,memoryFirst:memory[0],memoryLast:memory.at(-1),passed:external.success&&!duplicates&&!conflictingCycles&&runs.every(r=>r.passed)&&(mode!=='repeated'||cycles.length===10&&cycles.every(c=>c.cleanupMs<=15000))};
fs.writeFileSync(path.join(root,label+'.validation.json'),JSON.stringify(result,null,2));console.log(JSON.stringify({mode,passed:result.passed,runs:runs.length,missing:runs.reduce((a,r)=>a+r.missing,0),invalid:runs.reduce((a,r)=>a+r.invalid,0),gaps:runs.reduce((a,r)=>a+r.gaps,0),cycles,memorySamples:memory.length},null,2));if(!result.passed)process.exitCode=1;
