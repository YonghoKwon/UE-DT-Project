#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
const args=new Map();for(let i=2;i<process.argv.length;i+=2)args.set(process.argv[i],process.argv[i+1]);
const quantile=(values,p=.95)=>{const a=[...values].sort((x,y)=>x-y);return a.length?a[Math.max(0,Math.ceil(a.length*p)-1)]:null;};
const median=a=>quantile(a,.5);
function readRun(directory,label){
  const report=JSON.parse(fs.readFileSync(path.join(directory,label+'.json'),'utf8').replace(/^\uFEFF/,''));
  const ledger=JSON.parse(fs.readFileSync(path.join(directory,label+'.ledger.json'),'utf8').replace(/^\uFEFF/,''));
  const external=new Map((report.metadataLedger??[]).map(m=>[m.requestId,m]));
  const groups=new Map();let missing=0,invalid=0,clock=0,gaps=0;
  for(const f of ledger.frames){
    const group=groups.get(f.kind)??{kind:f.kind,frames:[],internal:[],external:[],receipt:[],derived:[]};
    group.frames.push(f);
    const msg=external.get(f.request_id);
    if(!msg)++missing;else if(!msg.valid)++invalid;
    if(!f.clock_valid||f.e2e_ms<0)++clock;
    if(f.submit_monotonic<=0||f.receipt_monotonic<=0||f.consumer_monotonic<=0)++missing;
    group.internal.push(f.e2e_ms);group.receipt.push(f.receipt_ms);
    group.derived.push(Date.parse(f.accepted_utc)-Date.parse(f.acquisition_utc));
    if(msg){const delay=Date.parse(msg.receivedUtc)-Date.parse(f.acquisition_utc);if(!Number.isFinite(delay)||delay<0)++clock;else group.external.push(delay);}
    groups.set(f.kind,group);
  }
  const streams=[...groups.values()].map(g=>{
    const bySensor=new Map();for(const f of g.frames){const ids=bySensor.get(f.sensor_id)??[];ids.push(f.frame_id);bySensor.set(f.sensor_id,ids);}
    for(const ids of bySensor.values()){ids.sort((a,b)=>a-b);for(let i=1;i<ids.length;i++)gaps+=Math.abs(ids[i]-ids[i-1]-1);}
    return {kind:g.kind,count:g.frames.length,hz:g.frames.length/ledger.duration_seconds,e2eP95Ms:quantile(g.internal),externalE2eP95Ms:quantile(g.external),receiptP95Ms:quantile(g.receipt),derivedP95Ms:quantile(g.derived)};
  });
  const thresholds=streams.every(s=>s.hz>=(s.kind===1?29:19)&&s.e2eP95Ms<=(s.kind===1?250:200)&&s.externalE2eP95Ms<=(s.kind===1?250:200));
  return {label,actualViewport:[ledger.actual_viewport_x,ledger.actual_viewport_y],fps:report.averageFps,low:report.onePercentLowFps,p95:report.p95FrameMs,serializeP95:report.publisherPcdSerializationP95Ms,streams,missing,invalid,clock,gaps,
    passed:!!report.unrealAutomationPassed&&missing===0&&invalid===0&&clock===0&&gaps===0&&thresholds&&report.averageFps>=55&&report.onePercentLowFps>=45&&report.p95FrameMs<=20&&report.publisherPcdSerializationP95Ms<=20};
}
const baseline=args.get('--baseline-dir'),final=args.get('--final-dir'),output=args.get('--output');
const cases=[];
for(const width of [1280,1920])for(const kind of ['pcd','three']){
  const base=[],after=[];for(let i=1;i<=3;i++){base.push(readRun(baseline,`accept_base_${width}_${kind}_${i}`));after.push(readRun(final,`accept_final_${width}_${kind}_${i}`));}
  const deltas={fpsPercent:100*(median(after.map(r=>r.fps))-median(base.map(r=>r.fps)))/median(base.map(r=>r.fps)),lowPercent:100*(median(after.map(r=>r.low))-median(base.map(r=>r.low)))/median(base.map(r=>r.low)),p95Percent:100*(median(after.map(r=>r.p95))-median(base.map(r=>r.p95)))/median(base.map(r=>r.p95)),serializeP95Percent:100*(median(after.map(r=>r.serializeP95))-median(base.map(r=>r.serializeP95)))/median(base.map(r=>r.serializeP95))};
  const latency=[];for(const s of base[0].streams){const b=median(base.map(r=>r.streams.find(x=>x.kind===s.kind).e2eP95Ms));const a=median(after.map(r=>r.streams.find(x=>x.kind===s.kind).e2eP95Ms));latency.push({kind:s.kind,baseline:b,final:a,changePercent:100*(a-b)/Math.max(.001,b)});}
  cases.push({width,kind,baseline:base,final:after,deltas,latency,passed:base.every(r=>r.passed)&&after.every(r=>r.passed)&&deltas.fpsPercent>=-5&&deltas.lowPercent>=-5&&deltas.p95Percent<=5&&deltas.serializeP95Percent<=5&&latency.every(r=>r.changePercent<=5)});
}
const result={generatedUtc:new Date().toISOString(),passed:cases.every(c=>c.passed),cases};
fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(result,null,2));
console.log(JSON.stringify({passed:result.passed,cases:cases.map(c=>({width:c.width,kind:c.kind,passed:c.passed,deltas:c.deltas,latency:c.latency}))},null,2));
if(!result.passed)process.exitCode=1;
