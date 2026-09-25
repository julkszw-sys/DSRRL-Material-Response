#!/usr/bin/env python3
"""Export exact Upper/Lower HemEnv consumer evidence as semantic-census JSONL.

Input is the canonical exact consumer authority produced from RDEF+SHEX analysis.
This tool deliberately does not classify MTDs by names. It emits receiver-level
construction evidence that may be joined to an exact FLVER-slot->MTD identity
only when such ownership evidence exists. Non-joined identities remain UNKNOWN.
"""
from __future__ import annotations
import argparse,csv,json
from pathlib import Path

def main()->int:
 p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 lines=a.input.read_text(encoding='utf-8').splitlines(); meta={}; data=[]
 for line in lines:
  if line.startswith('#'):
   if '=' in line:
    k,v=line[1:].strip().split('=',1);meta[k.strip()]=v.strip()
   continue
  data.append(line)
 rows=list(csv.DictReader(data,delimiter='\t'))
 out=[]
 for r in rows:
  out.append({'evidence_kind':'exact_receiver_consumer','semantic':'Upper/Lower','classification':'USE','shader_index':int(r['shader_index']),'stable_receiver_id':int(r['stable_receiver_id']),'stratum':r['stratum'],'shader_name':r['name'],'stock_sha256':r['stock_sha256'],'u_slot_word':int(r['u_slot_word']),'d_slot_word_0':int(r['d_slot_word_0']),'d_slot_word_1':int(r['d_slot_word_1']),'mtd_identity':'UNKNOWN','runtime_activation':'NOT_PROVEN','pixel_equivalence':'OPEN'})
 a.output.parent.mkdir(parents=True,exist_ok=True)
 with a.output.open('w',encoding='utf-8',newline='\n') as f:
  f.write(json.dumps({'schema':'dsrrl.upper_lower_hemenv_semantic_census.v1','source_dcx_sha256':meta.get('source_dcx_sha256'),'source_bnd3_sha256':meta.get('source_bnd3_sha256'),'policy':'receiver USE is exact SHEX consumption evidence; MTD identity remains UNKNOWN until exact owner join; absence/name never implies NO_USE','row_count':len(out)},sort_keys=True)+'\n')
  for x in out:f.write(json.dumps(x,sort_keys=True)+'\n')
 print(f'UPPER_LOWER_HEMENV_CENSUS_PASS rows={len(out)}')
 return 0 if len(out)==36 else 2
if __name__=='__main__':raise SystemExit(main())
