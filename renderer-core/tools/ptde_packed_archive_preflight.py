#!/usr/bin/env python3
"""Read-only PTDE retail archive preflight (BHD5/BDT). No unpacking.
PTDE UXM DarkSoulsGameInfo.xml declares dvdbnd0..3; require complete,
nonempty archives co-located in exactly one directory for full-game scan.
"""
from pathlib import Path
import argparse, json, os, sys
EXPECTED=tuple(f'dvdbnd{i}.{ext}' for i in range(4) for ext in ('bhd5','bdt'))
def inspect(root:Path, max_depth:int=5):
    if not root.is_dir(): raise ValueError(f'PTDE folder not found: {root}')
    root=root.resolve()
    candidates={}
    for here,dirs,files in os.walk(root,followlinks=False):
        directory=Path(here)
        depth=len(directory.relative_to(root).parts)
        dirs[:]=sorted(d for d in dirs if depth<max_depth and not (directory/d).is_symlink())
        present=set(x.lower() for x in files)
        found=sorted(set(EXPECTED)&present)
        if not found: continue
        missing=sorted(set(EXPECTED)-present)
        sizes={n:(directory/n).stat().st_size for n in found if (directory/n).is_file()}
        nonempty=all(sizes.get(n,0)>0 for n in found)
        candidates[str(directory)]={'found':found,'missing':missing,'nonempty':nonempty,'complete':not missing and nonempty}
    complete=[k for k,v in candidates.items() if v['complete']]
    chosen=complete[0] if len(complete)==1 else None
    status='PASS' if chosen else ('AMBIGUOUS_COMPLETE_ROOTS' if complete else 'INCOMPLETE')
    return {'status':status,'requested':str(root),'chosen':chosen,
            'expected':list(EXPECTED),'candidates':candidates}
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ptde',required=True,type=Path)
    parser.add_argument('--json-out',type=Path)
    args=parser.parse_args()
    result=inspect(args.ptde)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True,exist_ok=True)
        args.json_out.write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    print('PTDE_PREFLIGHT',result['status'])
    for k,v in result['candidates'].items():
        print(k,'found',len(v['found']),'/8; missing:',', '.join(v['missing']) or 'none')
    if result['chosen']:print('CHOSEN',result['chosen'])
    else:print('INCOMPLETE source: do not claim full armor texture map')
    return 0 if result['status']=='PASS' else 2
if __name__=='__main__':sys.exit(main())
