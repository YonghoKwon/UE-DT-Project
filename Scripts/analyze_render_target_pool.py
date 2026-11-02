"""Read-only descriptor/lifetime observations; never substitutes for memory gates."""
import argparse
from collections import Counter,defaultdict
import json
from pathlib import Path

DESCRIPTOR_FIELDS=('width','height','depth','array_size','dimension','format','uav_format',
                   'mips','samples','flags','ext_data','gpu_mask','fast_vram_percent',
                   'clear_binding','clear_rgba','clear_depth','clear_stencil','desc_hash')

def descriptor(element):
    return tuple(element.get(key) for key in DESCRIPTOR_FIELDS)

def identity(element):
    # Pool compaction changes slot indices. Addresses are observations, not a
    # globally unique allocation generation or proof that an object stayed live.
    return element['element'],element['rhi']

def analyze(rows,creations=None):
    pools=[r for r in rows if r.get('type')=='pool']
    if len(pools)<2:raise ValueError('At least two pool snapshots required')
    if any(a['qpc']>=b['qpc'] for a,b in zip(pools,pools[1:])):
        raise ValueError('Pool clocks must increase')
    if len([r for r in pools if r['stage']=='before'])!=1 or len([r for r in pools if r['stage']=='after'])!=1:
        raise ValueError('Exactly one original before/after boundary required')
    before=next(r for r in pools if r['stage']=='before')
    after=next(r for r in pools if r['stage']=='after')
    if before['qpc']>=after['qpc']:raise ValueError('Boundary ordering invalid')
    seen_names=defaultdict(set);seen_objects={};changes=[];appearances=[];renames=[];departures=[]
    max_bytes_by_name=Counter();previous={}
    for row in pools:
        current={identity(e):e for e in row['elements']}
        if len(current)!=len(row['elements']):raise ValueError('Duplicate pool identity in snapshot')
        for key,e in current.items():
            desc=descriptor(e);name=e['name'];max_bytes_by_name[name]=max(max_bytes_by_name[name],e['bytes'])
            if seen_names[name] and desc not in seen_names[name]:
                closest=min(seen_names[name],key=lambda old:sum(a!=b for a,b in zip(old,desc)))
                changes.append(dict(Stage=row['stage'],Cycle=row['cycle'],Qpc=row['qpc'],Name=name,
                                    ChangedFields=[k for k,a,b in zip(DESCRIPTOR_FIELDS,closest,desc) if a!=b],
                                    Descriptor=dict(zip(DESCRIPTOR_FIELDS,desc))))
            seen_names[name].add(desc)
            if key not in seen_objects:
                appearances.append(dict(Stage=row['stage'],Cycle=row['cycle'],Qpc=row['qpc'],Name=name,Bytes=e['bytes'],Free=e['free'],Identity=list(key)))
            elif seen_objects[key]['name']!=name:
                renames.append(dict(Qpc=row['qpc'],From=seen_objects[key]['name'],To=name,DescriptorUnchanged=descriptor(seen_objects[key])==desc,Identity=list(key),
                                    Limit='Same observed addresses; address reuse is possible. CPU creation events can strengthen the reuse inference.'))
            seen_objects[key]=e
        for key,e in previous.items():
            if key not in current:departures.append(dict(Qpc=row['qpc'],Name=e['name'],Identity=list(key),Bytes=e['bytes']))
        previous=current
    def boundary(row):return dict(Stage=row['stage'],Qpc=row['qpc'],Count=row['count'],AllocatedKiB=row['allocated_kib'],UsedKiB=row['used_kib'],
                                  CommitBytes=row['commit_bytes'],PoolMinMiB=row.get('pool_min_mib'),FreeCount=sum(e['free'] for e in row['elements']),
                                  Names=dict(Counter(e['name'] for e in row['elements'])))
    out=dict(Before=boundary(before),After=boundary(after),Post=[boundary(r) for r in pools if r['stage'].startswith('post_')],
             SnapshotCount=len(pools),DescriptorVariants={k:len(v) for k,v in seen_names.items() if len(v)>1},
             DescriptorChanges=changes,FirstObservations=appearances,NameChangesOnObservedIdentity=renames,DeparturesFromPool=departures,
             MaxSingleTextureBytesByName=dict(max_bytes_by_name.most_common()),
             MaxCollectionRtMs=max(r['collect_rt_ms'] for r in pools),
             Limits=['A departure from the pool is not proof of GPU or driver backing release.',
                     'Pointer identity may be recycled between snapshots; no allocation generation is inferred.',
                     'Per-name variants can be legitimate simultaneous resources/views, not necessarily descriptor churn.',
                     'Only observed descriptors and pool state are reported; no leak-absence or performance verdict.'])
    if creations is not None:
        summaries=[r for r in creations if r.get('type')=='summary']
        if len(summaries)!=1 or not summaries[0].get('complete'):raise ValueError('CPU creation trace incomplete')
        events=[r for r in creations if r.get('type')=='pool_create']
        window=[r for r in events if before['qpc']<=r['qpc']<=after['qpc']]
        out['CreationEventsDuringMeasurement']=dict(Counter(r['name'] for r in window).most_common())
        out['CreationEventCountDuringMeasurement']=len(window)
        known=set(seen_names)
        out['CreationNamesWithoutObservedDescriptor']=dict(Counter(r['name'] for r in window if r['name'] not in known))
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('snapshots',type=Path);parser.add_argument('--creations',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    rows=[json.loads(x) for x in args.snapshots.read_text(encoding='utf-8-sig').splitlines() if x.strip()]
    creates=[json.loads(x) for x in args.creations.read_text(encoding='utf-8-sig').splitlines() if x.strip()] if args.creations else None
    args.output.write_text(json.dumps(analyze(rows,creates),indent=2),encoding='utf-8')
if __name__=='__main__':main()
