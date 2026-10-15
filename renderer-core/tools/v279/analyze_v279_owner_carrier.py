#!/usr/bin/env python3
"""V279 exact material/CB/SRV/sampler mutation-pair differences, source only.
python analyze_v279_owner_carrier.py ReShade.log --out-prefix v279_owner
The operator masks, D3D11 objects and host slots are source/transaction
witnesses, not actual GPU CB contents or PTDE pixel outputs.
"""
import argparse,re,json,csv
from pathlib import Path
from collections import defaultdict

PREFIX='[DSRRL V279 MATERIAL CARRIER]'
def get_rows(p):
    out=[]
    for line in p.read_text(errors='replace').splitlines():
        if PREFIX not in line:continue
        suffix=line.split(PREFIX,1)[1]
        fields=dict(re.findall(r'\b([A-Za-z][A-Za-z0-9_]*)=([^ ]+)',suffix))
        if fields.get('class') not in ('METAL_GOOD','LEATHER_BAD_FAMILY'):
            continue
        fields['log_line']=line[:17]
        out.append(fields)
    return out
def run(path,prefix):
    data=get_rows(path)
    pairmap=defaultdict(dict)
    for x in data:
        key=(x['phase'],x['rx'],x['asset'],x['fog_bucket'])
        if x['class'] in pairmap[key]:
            raise ValueError('ambiguous duplicate exact material stage '+str(key))
        pairmap[key][x['class']]=x
    pairs=[];orphans=[]
    for key,items in sorted(pairmap.items()):
        if len(items)!=2:
            orphans.append(dict(phase=key[0],rx=key[1],asset=key[2],
                                bucket=key[3],present=sorted(items)))
            continue
        metal,leather=items['METAL_GOOD'],items['LEATHER_BAD_FAMILY']
        compare=sorted(set(metal)&set(leather))
        diff={k:dict(metal=metal[k],leather=leather[k])
              for k in compare if metal[k]!=leather[k]
              and k not in ('class','mtd','route','slot','log_line')}
        pairs.append(dict(phase=key[0],rx=key[1],asset=key[2],
                    fog_bucket=key[3],metal_slot=metal['slot'],
                    leather_slot=leather['slot'],
                    differences=diff,
                    common_signal_keys=[k for k in compare if k not in diff]))
    prefix.parent.mkdir(parents=True,exist_ok=True)
    (prefix.parent/(prefix.name+'_pairs.json')).write_text(
        json.dumps(dict(pairs=pairs,orphans=orphans,record_count=len(data),
             original_log=str(path),pixel='UNOBSERVED'),indent=2)+'\n')
    with (prefix.parent/(prefix.name+'_diff_matrix.csv')).open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(
          ('phase','rx','asset','fog_bucket','carrier','metal','leather'))
        for pair in pairs:
            for k,v in pair['differences'].items():
                writer.writerow((pair['phase'],pair['rx'],pair['asset'],
                      pair['fog_bucket'],k,v['metal'],v['leather']))
    summary=dict(record_count=len(data),paired=len(pairs),orphan=len(orphans),
         differing_fields=sorted({k for x in pairs for k in x['differences']}),
         mutation_shaders_same_in_pairs=sum(not x['differences'].get('mutation_ps')
                  for x in pairs),
         pixel='OPEN')
    (prefix.parent/(prefix.name+'_summary.json')).write_text(
         json.dumps(summary,indent=2)+'\n')
    print('V279 paired',len(pairs),'orphans',len(orphans),
          'sampled source fields differing',summary['differing_fields'])
    return summary
def main():
    ap=argparse.ArgumentParser();ap.add_argument('log',type=Path)
    ap.add_argument('--out-prefix',type=Path,default=Path('v279_exact_carrier'))
    a=ap.parse_args()
    run(a.log,a.out_prefix)
if __name__=='__main__':main()
