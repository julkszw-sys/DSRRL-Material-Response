#!/usr/bin/env python3
"""Export HemDir3 Spc exact material donor evidence into MTD semantic census JSONL.

The donor table proves raw-MTD keyed PTDE c101/c102 payload availability only.
It does NOT prove a DSR MTD/owner reaches a HemDir3 Spc receiver; therefore
material_response is USE as payload evidence while receiver activation remains
UNKNOWN until the exact actual-material + receiver join succeeds. Names are not
used for classification and absence never means NO_USE.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,re
from pathlib import Path
HEX64=re.compile(r'^[0-9a-f]{64}$')
def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()
def main()->int:
 p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 raw=a.input.read_text(encoding='utf-8').splitlines(); meta={}; body=[]
 for line in raw:
  if line.startswith('#'):
   if '=' in line:
    k,v=line[1:].strip().split('=',1);meta[k.strip()]=v.strip()
   continue
  if line.strip():body.append(line)
 rows=list(csv.DictReader(body,delimiter='\t')); out=[]; seen=set()
 for r in rows:
  h=r['raw_mtd_sha256'].lower()
  if not HEX64.fullmatch(h) or h in seen:return 2
  seen.add(h)
  out.append({'evidence_kind':'exact_raw_mtd_payload','raw_mtd_sha256':h,'semantic':'Material Response','classification':'USE','payload':{'c101_rgb':[float(r['c101_r']),float(r['c101_g']),float(r['c101_b'])],'c102':float(r['c102']),'slot':int(r['slot'])},'hemdir3_spc_receiver_join':'UNKNOWN','runtime_activation':'NOT_PROVEN','pixel_equivalence':'OPEN'})
 a.output.parent.mkdir(parents=True,exist_ok=True)
 with a.output.open('w',encoding='utf-8',newline='\n') as f:
  f.write(json.dumps({'schema':'dsrrl.hemdir3_spc_material_semantic_census.v1','source_sha256':sha(a.input),'source_git_blob_sha':meta.get('source_git_blob_sha'),'policy':'exact raw-MTD payload USE only; HemDir3 Spc receiver reachability remains UNKNOWN until exact actual-material+receiver join; absence/name never implies NO_USE','row_count':len(out)},sort_keys=True)+'\n')
  for x in out:f.write(json.dumps(x,sort_keys=True)+'\n')
 print(f'HEMDIR3_SPC_MATERIAL_CENSUS_PASS rows={len(out)}')
 return 0 if len(out)==306 else 2
if __name__=='__main__':raise SystemExit(main())
