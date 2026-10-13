#!/usr/bin/env python3
"""Merge any number of V254 ReShade logs, retaining ALL sampled LERP events.

  python analyze_v254_lerp_log.py ReShade*.log --out V254_ANALYSIS

No PTDE pixels, c84.w, c85.w or c110.w are inferred from DSR logs.
"""
import argparse
import csv
import json
from collections import Counter,defaultdict
from pathlib import Path
import re

RE = re.compile(r'\[(DSRRL V254 LERP (?:EVENTS|SOURCE|SUMMARY)|DSRRL V251 LERP NATIVE)\]\s*(.*)')
FIELD = re.compile(r'([A-Za-z][A-Za-z0-9_]*)=([^\s]+)')
TYPES = {'DSRRL V254 LERP EVENTS':'events', 'DSRRL V254 LERP SOURCE':'sources',
         'DSRRL V254 LERP SUMMARY':'summaries','DSRRL V251 LERP NATIVE':'native'}

def extract(paths):
    out={x:[] for x in TYPES.values()}
    for path in paths:
        with open(path,errors='replace',encoding='utf-8') as f:
            for line_number,line in enumerate(f,1):
                match=RE.search(line)
                if not match: continue
                typ=TYPES[match.group(1)]
                entry={'file':str(path),'line':line_number}
                entry.update(FIELD.findall(match.group(2)))
                out[typ].append(entry)
    return out

def output(rows,path):
    keys=['file','line']+sorted({k for r in rows for k in r if k not in ('file','line')})
    with open(path,'w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=keys)
        writer.writeheader()
        writer.writerows(rows)

def analyze(paths,dest):
    rows=extract(paths)
    dest.mkdir(parents=True,exist_ok=True)
    for name,data in rows.items():
        output(data,dest/f'v254_{name}.csv')
    stages=Counter((r.get('class'),r.get('rx'),r.get('stage')) for r in rows['events'])
    bands=Counter((r.get('rx'),r.get('beta_band')) for r in rows['sources'])
    changed=0
    for r in rows['sources']:
        try:
            left=[float(x) for x in r['dsr_spcA'].split(',')]
            right=[float(x) for x in r['ptde_spcA'].split(',')]
            left2=[float(x) for x in r['dsr_spcB'].split(',')]
            right2=[float(x) for x in r['ptde_spcB'].split(',')]
            if any(abs(a-b)>1e-5 for a,b in zip(left,right)) or any(abs(a-b)>1e-5 for a,b in zip(left2,right2)):
                changed+=1
        except (KeyError,ValueError):
            pass
    native_bad=sum(any(r.get(key) not in ('1',None) for key in ('ps','cb','srv','smp')) for r in rows['native'])
    summary={
        'inputs':[str(p) for p in paths],
        'sampled_events':len(rows['events']),
        'source_events':len(rows['sources']),
        'periodic_summaries':len(rows['summaries']),
        'sampled_native_readbacks':len(rows['native']),
        'native_state_mismatch_samples':native_bad,
        'source_samples_with_dsr_ptde_spec_endpoint_difference':changed,
        'sampled_event_counts_by_class_rx_stage':{':'.join(map(str,k)):v for k,v in stages.items()},
        'source_counts_by_rx_beta_band':{':'.join(map(str,k)):v for k,v in bands.items()},
        'interpretation_constraints':{
            'ptde_c84w_c85w_c110w':'UNOBSERVED; source_beta cannot stand in without PTDE producer proof',
            'gpu_premerge_rgb':'UNOBSERVED; cannot promote PHN merge hypothesis',
            'gpu_postmerge_rgb':'UNOBSERVED; cannot promote PHN merge hypothesis',
            'pixel_equivalence':'OPEN; requires matched PTDE/DSR capture',
            'sampling':'event CSV is sampled, SUMMARY contains all eligible counts at each checkpoint',
            'no_native_lines':'if original V251 logging absent, no claim of native PS/CB/SRV equality',
        }
    }
    (dest/'v254_report.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False),encoding='utf-8')
    return summary

def main():
    p=argparse.ArgumentParser()
    p.add_argument('logs',nargs='+',type=Path)
    p.add_argument('--out',default='V254_ANALYSIS',type=Path)
    args=p.parse_args()
    report=analyze(args.logs,args.out)
    print(json.dumps(report,indent=2,ensure_ascii=False))

if __name__=='__main__': main()
