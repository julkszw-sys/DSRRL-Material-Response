#!/usr/bin/env python3
"""V416 exact bidirectional FgModel/Source1/RX34 issued-draw log audit.
An absent bounded witness is UNKNOWN, never an actor/pixel classification.
"""
import argparse,json,re
from collections import Counter,defaultdict
from pathlib import Path
SRC='[DSRRL V416 FGMODEL SOURCE JOIN]'
DRAW='[DSRRL V411 RX34 OWNER SEQ DRAW]'
KV=re.compile(r'([a-zA-Z_]\w*)=([^\s]+)')
def analyze(path):
    sources=defaultdict(set);draws=[];counts=Counter()
    for number,line in enumerate(Path(path).open(encoding='utf8',errors='replace'),1):
        if SRC in line:
            d=dict(KV.findall(line));counts['native_parent_source_samples']+=1
            key=(d.get('context'),d.get('native_producer_seq'))
            if all(key) and d.get('parent_reciprocal')=='1':
                sources[key].add((d.get('model'),d.get('fg_parent'),
                                  d.get('upstream_hint'),d.get('row_a')))
        elif DRAW in line:
            d=dict(KV.findall(line));d['log_line']=number
            draws.append(d);counts['rx34_draw_samples']+=1
    groups=defaultdict(lambda:defaultdict(set))
    for d in draws:
        if d.get('kind')!='1' or d.get('stamp_state')!='1':
            counts['other_source_or_unjoined']+=1;continue
        key=(d.get('owner'),d.get('native_producer_seq'))
        candidate=sources.get(key,set())
        if not candidate:
            counts['missing_parent_source_sample']+=1;continue
        if len(candidate)!=1:
            counts['conflicting_parent_source_witness']+=1;continue
        model,parent,upstream,row=next(iter(candidate))
        if d.get('row')!=row:
            counts['source_row_mismatch']+=1;continue
        counts['model_parent_issued_draw_observational_join']+=1
        material=(d.get('owner'),d.get('route'),d.get('slot'),
                  d.get('shader'),d.get('flver_sha'),d.get('mtd_sha'))
        groups[material][row].add((model,parent))
    pairs=[]
    for material,rows in groups.items():
        if not {'30','31'} <= set(rows):continue
        a,b=rows['30'],rows['31']
        pairs.append({
          'material':dict(zip(('context','route','slot','shader',
                               'flver_sha','mtd_sha'),material)),
          'row30_model_parent_pairs':sorted(a),
          'row31_model_parent_pairs':sorted(b),
          'shared_exact_model_parent_pairs':sorted(a&b),
          'interpretation':'shared_FgModel_candidate' if a&b
                           else 'different_FgModel_candidates',
          'gameplay_actor':'OPEN','gpu_cb_rtv_pass':'OPEN',
          'visible_pixel_causality':'OPEN'})
    return {'counts':dict(counts),'row_30_31_pairs':pairs,
            'limits':['parent RTTI/backlink is not a native actor lifetime token',
                      'missing bounded samples are UNKNOWN',
                      'native Source1 sequence to issued draw is observational',
                      'frame, RTV, CB and pixel provenance remain OPEN']}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('log',type=Path)
    p.add_argument('-o','--output',type=Path);a=p.parse_args()
    s=json.dumps(analyze(a.log),ensure_ascii=False,indent=2)+'\n'
    if a.output:a.output.write_text(s,encoding='utf8')
    else:print(s,end='')
