#!/usr/bin/env python3
"""V410 bounded-evidence join checker. Never authorizes a LightBank carrier."""
import argparse,collections,csv,io,json,re,zipfile
from pathlib import Path
KVP=re.compile(r'([A-Za-z][A-Za-z0-9_]*)=([^\s]+)')
def table(z,suffix):
    with zipfile.ZipFile(z) as f:
        paths=[x for x in f.namelist() if x.endswith(suffix)]
        if len(paths)!=1:raise ValueError((suffix,len(paths)))
        return list(csv.DictReader(io.StringIO(f.read(paths[0]).decode('utf-8-sig'))))
def timestamp(t):
    h,m,s,ms=map(int,t.split(':'))
    return ((h*60+m)*60+s)*1000+ms
def historical(z):
    gpu=table(z,'03_GPU_ISSUED_SAMPLES.csv')
    native=table(z,'04_NATIVE_PACKER_SAMPLES.csv')
    remo=table(z,'05_REMO_MODEL_SELECTOR_SAMPLES.csv')
    keys=('actor_owner','rx','route','slot','sem','shader','flver_sha','mtd_sha')
    state=('kind','path','bank','row','diff_probe','c86')
    prior={}; transitions=[]
    for r in gpu:
        if r['rx']!='34' or r['stage']!='indexed_issued':continue
        k=tuple(r[x] for x in keys)
        old=prior.get(k)
        if old and any(old[x]!=r[x] for x in state):
            transitions.append({'owner':r['actor_owner'],'route':r['route'],
              'slot':r['slot'],'time':r['time'],'previous':{x:old[x] for x in state},
              'current':{x:r[x] for x in state},'pixel':'OPEN'})
        prior[k]=r
    s1=[r for r in gpu if r['kind']=='1']
    collisions=[]
    for serial in sorted({r['serial'] for r in s1}):
        drawn=[r for r in s1 if r['serial']==serial]
        events=[r for r in native if r['serial']==serial]
        collisions.append({'gpu_serial':serial,'gpu_samples':len(drawn),
          'gpu_rows':sorted({r['row'] for r in drawn}),
          'native_serial_rows':sorted({r['decoded_rowA'] for r in events}),
          'would_mismatch':bool(events and any(d['row']!=n['decoded_rowA']
            for d in drawn for n in events)),'proof':'serial is selector epoch in V398'})
    owner='00000000219A4D80'
    row25=[r for r in native if r['decoded_rowA']=='25']
    gpu25=[r for r in gpu if r['actor_owner']==owner and r['row']=='25']
    gaps=[timestamp(gpu25[0]['time'])-timestamp(r['time']) for r in row25] if gpu25 else []
    own=[r for r in remo if r['owner']==owner]
    return {'scope':'BOUNDED V398 SAMPLES ONLY',
        'gpu_samples':len(gpu),'native_samples':len(native),'remo_samples':len(remo),
        'distinct_rx34_keys':len(prior),'sampled_transitions':len(transitions),
        'source1_gpu_samples':len(s1),'source1_serial_collision':collisions,
        'row25':{'native_samples':len(row25),'producer_serials':[int(r['serial']) for r in row25],
           'owner_remo_samples':len(own),
           'owner_remo_selector1305_samples':sum(r['resolved_descriptor_a']=='1305' for r in own),
           'owner_gpu_row25_samples':len(gpu25),
           'time_gap_from_sampled_native_row25_to_first_owner_draw_ms':
             [min(gaps),max(gaps)] if gaps else None,
           'owner_native_producer_join':'OPEN'},
        'transitions':transitions,
        'caveat':'Absence in bounded samples never proves absence in runtime; no native actor ownership or PTDE pixel conclusion'}
def classify_log(file):
    natives=collections.defaultdict(list)
    readbacks=collections.defaultdict(list)
    draws=[]
    for i,line in enumerate(Path(file).read_text(errors='replace').splitlines(),1):
        kind=('N' if '[DSRRL V383 NATIVE LIGHTBANK]' in line else
          'R' if '[DSRRL V408 SOURCE1 PACKER JOIN]' in line else
          'G' if '[DSRRL V409 RX34 GPU TRANSITION]' in line else None)
        if not kind:continue
        d=dict(KVP.findall(line));d['_line']=i
        if kind=='N':natives[d.get('serial','')].append(d)
        elif kind=='R':readbacks[(d.get('owner'),d.get('native_producer_seq'))].append(d)
        else:draws.append(d)
    result=[]
    for d in draws:
        seq=d.get('native_producer_seq','0'); owner=d.get('owner')
        n=natives.get(seq,[])
        r=[x for x in readbacks.get((owner,seq),[])
           if x.get('cache_readback')=='1' and x.get('same_producer')=='1']
        if d.get('kind')=='2':stage='SOURCE2_UNOWNED_FALLBACK'
        elif seq=='0':stage='NO_NATIVE_SEQUENCE'
        elif len(n)>1:stage='DUPLICATE_PACKER_SEQUENCE'
        elif n and r:stage='CACHE_READBACK_AND_NATIVE_SEQ_MATCH'
        elif r:stage='CACHE_READBACK_ONLY'
        elif n:stage='NATIVE_SEQ_ONLY_NO_OWNER_JOIN'
        else:stage='NO_SAMPLED_NATIVE_JOIN'
        result.append({'line':d['_line'],'owner':owner,'seq':seq,
          'kind':d.get('kind'),'row':d.get('row'),'stage':stage,
          'native_samples':len(n),'readback_samples':len(r),
          'native_actor_ownership':'OPEN even when sequence matches',
          'pixel':'OPEN'})
    return {'scope':'BOUNDED V408/V409 SAMPLES ONLY',
      'logged_gpu_transitions':len(draws),'logged_native_sequences':len(natives),
      'logged_cache_keys':len(readbacks),'stages':dict(collections.Counter(d['stage'] for d in result)),
      'classified':result,'caveat':'owner+seq readback and native packer sequence coincidence is not native actor ownership certification'}
def main():
    p=argparse.ArgumentParser()
    p.add_argument('--v398-zip',type=Path,required=True)
    p.add_argument('--runtime-log',type=Path)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    result={'v398':historical(a.v398_zip)}
    if a.runtime_log:result['runtime']=classify_log(a.runtime_log)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    print('V410 samples/keys/changes:',result['v398']['gpu_samples'],
      result['v398']['distinct_rx34_keys'],result['v398']['sampled_transitions'])
    if a.runtime_log:print('V410 runtime classes:',result['runtime']['stages'])
if __name__=='__main__':main()
