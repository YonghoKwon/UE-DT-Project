"""Stage1/3 descriptive comparison; never promotes a cut to final #26 acceptance."""
import argparse,json,statistics
from pathlib import Path
from analyze_memory_trend import MIB,load_curve,analyze,fit

WARMUP=600
TAIL=1200
MIN_EFFECT=1.0

def describe(points, context):
    check=analyze(points,complete=context['complete'])
    if check['verdict']=='데이터 부족':raise ValueError('; '.join(check['reasons']))
    end=points[-1]['seconds']
    post=[dict(x) for x in points if x['seconds']>=WARMUP]
    tail=[x for x in post if x['seconds']>=end-TAIL]
    if len(tail)<12 or tail[-1]['seconds']-tail[0]['seconds']<TAIL-60:raise ValueError('Common20min tail unavailable after10min exclusion')
    for x in post:
        tags={**x.get('llm_default_top',{}),**x.get('llm_default',{})}
        if 'RHIMisc' not in tags:raise ValueError('RHIMisc missing, zero is valid')
        x['rhims_bytes']=tags['RHIMisc']
    # tail references the same copied records, now including rhims_bytes.
    metrics={name:{'postwarmup':fit(post,field,scale),'tail20':fit(tail,field,scale)}
             for name,field,scale in [('RSS','rss_bytes',MIB),('Commit','commit_bytes',MIB),('RHIMisc','rhims_bytes',MIB),('UObject','uobject_count',1)]}
    first,last=post[0],post[-1]
    hz={sid:(s['frame_id']-first['sensors'][sid]['frame_id'])/(last['seconds']-first['seconds']) for sid,s in last['sensors'].items()}
    if len(hz)!=3 or any(x<=0 for x in hz.values()):raise ValueError('Allthree active sensors required, zeroHz is not omitted')
    return {'Valid':True,'SourceCommit':context['metadata']['source_commit'],'Allocator':context['metadata']['allocator'],
            'DurationSeconds':end,'ExcludedFirstSeconds':WARMUP,'TailHorizonSeconds':TAIL,'PostwarmupSamples':len(post),'TailSamples':len(tail),
            'Metrics':metrics,'ActualHzPostwarmup':hz,'PCD':context['summary']['pcd'],'OriginalDiagnosticVerdict':check['verdict'],
            'Limits':['OLS uses actual elapsed time, correlated samples; no inferential p-value','Original#26 gates and final2hour/ABAB criteria not evaluated']}

def baseline_stats(runs):
    if len(runs)!=3:raise ValueError('Exactlythree baseline runs required')
    out={}
    for key in ['RSS','Commit','RHIMisc']:
        values=[x['Metrics'][key]['tail20']['slope_per_hour'] for x in runs]
        out[key]={'values':values,'median':statistics.median(values),'min':min(values),'max':max(values),'range':max(values)-min(values)}
    return out

def compare_cut(base,cut):
    noise=baseline_stats(base);hz_floor=min(min(x['ActualHzPostwarmup'].values()) for x in base)*.98
    comparable=min(cut['ActualHzPostwarmup'].values())>=hz_floor
    changes={}
    for key,n in noise.items():
        value=cut['Metrics'][key]['tail20']['slope_per_hour'];reduction=n['median']-value
        changes[key]={'slope':value,'reduction_from_median':reduction,'required_reduction_gt':max(n['range'],MIN_EFFECT),
                      'outside_observed_range':value<n['min']-1e-6,'significant_reduction':value<n['min']-1e-6 and reduction>max(n['range'],MIN_EFFECT)}
    tradeoff=any(changes[k]['slope']>n['max']+max(n['range'],MIN_EFFECT) for k,n in noise.items())
    return {'ComparableThroughput':comparable,'ActualHzFloor':hz_floor,'Changes':changes,'WorseningBeyondNoise':tradeoff,
            'Candidate':comparable and not tradeoff and any(x['significant_reduction'] for x in changes.values()),
            'Definition':'Operational significant reduction only: below all3baseline slopes and median reduction >max(observedrange,1MiB/h), minimum sensorHz >=98%of lowestbaseline, no otherRAMmetric worsening beyond max+max(range,1). N=3,onecut is descriptive, not statistical significance or finalacceptance.'}

def main():
    p=argparse.ArgumentParser();sub=p.add_subparsers(dest='cmd',required=True)
    a=sub.add_parser('analyze');a.add_argument('curve',type=Path);a.add_argument('--output',type=Path,required=True)
    a=sub.add_parser('compare');a.add_argument('--root',type=Path,required=True)
    a=p.parse_args()
    if a.cmd=='analyze':
        points,ctx,_=load_curve(a.curve);d=describe(points,ctx)
        a.output.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print('Analysis complete',a.output);return
    def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
    base=[read(a.root/'stage1'/('run-'+str(i))/'analysis-fixed.json') for i in [1,2,3]]
    cuts={p.parent.name:read(p) for p in sorted((a.root/'stage3').glob('*/analysis-fixed.json'))}
    d={'Valid':len(cuts)==5,'WarmupExcludedSeconds':WARMUP,'CommonTailSeconds':TAIL,'Baseline':baseline_stats(base),
       'Comparisons':{k:compare_cut(base,v) for k,v in cuts.items()},'FinalSuccessCriteria':'Not evaluated: needs2hour idle RSS/Commit<=8,RHIMisc<=2 plus original5pairs andABAB2percent; stage4 not started'}
    (a.root/'comparison.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    lines=['| Run | RSS tail MiB/h | Commit tail MiB/h | RHIMisc tail MiB/h | min actualHz | candidate |','|---|---:|---:|---:|---:|---|']
    for name,x in [(f'baseline{i}',v) for i,v in enumerate(base,1)]+list(cuts.items()):
        vals=[x['Metrics'][k]['tail20']['slope_per_hour'] for k in ['RSS','Commit','RHIMisc']]
        flag=str(d['Comparisons'][name]['Candidate']) if name in cuts else 'reference'
        lines.append('| '+name+' | '+' | '.join(f'{v:+.3f}' for v in vals)+f" | {min(x['ActualHzPostwarmup'].values()):.4f} | {flag} |")
    for label in ['min','median','max','range']:
        lines.append('| baseline '+label+' | '+' | '.join(f"{d['Baseline'][k][label]:.3f}" for k in ['RSS','Commit','RHIMisc'])+' | - | - |')
    (a.root/'comparison-table.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');print('Descriptive comparison complete, candidatecount',sum(x['Candidate'] for x in d['Comparisons'].values()))

if __name__=='__main__':main()
