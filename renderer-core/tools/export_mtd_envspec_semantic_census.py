#!/usr/bin/env python3
"""Export exact EnvSpec semantic rows from the checked-in 325-record router.

Construction/static evidence only. `present` is USE. `explicit_none` is NO_USE
only as an exact payload observation; it is not inferred from the MTD name.
`nospc_host` and `unknown` remain UNKNOWN unless separately proven.
"""
from __future__ import annotations
import argparse,json,re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp'
OUT=ROOT/'data/census/mtd_envspec_exact_rows_v1.jsonl'
ENTRY=re.compile(r'\{\s*"([^"]+)",\s*0x([0-9a-f]+)ull,\s*0x[0-9a-f]+ull,\s*\{\{([^}]*)\}\},\s*envspec_router_state::(\w+),\s*(\d+)u,\s*(true|false)',re.I|re.S)
BYTE=re.compile(r'0x([0-9a-f]{2})u',re.I)

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,default=OUT);a=ap.parse_args()
 text=SRC.read_text(encoding='utf-8'); rows=[]
 for m in ENTRY.finditer(text):
  name,h,raw,state,slot,safe=m.groups(); b=BYTE.findall(raw)
  if len(b)!=32: raise SystemExit(f'bad sha bytes for {name}: {len(b)}')
  semantic={'present':'USE','explicit_none':'NO_USE'}.get(state,'UNKNOWN')
  rows.append({'mtd_name':name,'semantic_name_hash':h.lower(),'raw_mtd_sha256':''.join(x.lower() for x in b),'env_spec':semantic,'router_state':state,'envspc_slot':int(slot),'explicit_none_safe':safe=='true','claim_scope':'CONSTRUCTION_AND_STATIC_ROUTING_ONLY','runtime_activation':'OPEN','pixel_equivalence':'OPEN'})
 if len(rows)!=325: raise SystemExit(f'expected 325 exact rows, got {len(rows)}')
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(''.join(json.dumps(x,separators=(',',':'))+'\n' for x in rows),encoding='utf-8')
 from collections import Counter
 c=Counter(x['env_spec'] for x in rows);print(f"MTD_ENVSPEC_EXPORT_PASS rows={len(rows)} use={c['USE']} no_use={c['NO_USE']} unknown={c['UNKNOWN']}")
if __name__=='__main__': main()
