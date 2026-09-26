#!/usr/bin/env python3
"""Verify exported HemDir3 Spc MTD semantic census without promoting reachability.

Construction evidence is limited to exact raw-MTD keyed PTDE c101/c102 payloads.
Receiver reachability, runtime activation and pixel equivalence remain separate.
"""
from __future__ import annotations
import argparse,json,re
from pathlib import Path
HEX64=re.compile(r'^[0-9a-f]{64}$')
def main()->int:
 p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);a=p.parse_args()
 lines=[json.loads(x) for x in a.input.read_text(encoding='utf-8').splitlines() if x.strip()]
 if len(lines)!=307:return 2
 meta,rows=lines[0],lines[1:]
 if meta.get('schema')!='dsrrl.hemdir3_spc_material_semantic_census.v1' or meta.get('row_count')!=306:return 2
 seen=set()
 for r in rows:
  h=r.get('raw_mtd_sha256','')
  if not HEX64.fullmatch(h) or h in seen:return 2
  seen.add(h)
  if r.get('evidence_kind')!='exact_raw_mtd_payload':return 2
  if r.get('semantic')!='Material Response' or r.get('classification')!='USE':return 2
  if r.get('hemdir3_spc_receiver_join')!='UNKNOWN':return 2
  if r.get('runtime_activation')!='NOT_PROVEN' or r.get('pixel_equivalence')!='OPEN':return 2
  q=r.get('payload',{})
  if len(q.get('c101_rgb',[]))!=3 or not isinstance(q.get('slot'),int):return 2
 print('HEMDIR3_SPC_MATERIAL_CENSUS_VERIFY_PASS rows=306 exact_payload=306 receiver_join=UNKNOWN runtime=NOT_PROVEN pixel=OPEN')
 return 0
if __name__=='__main__':raise SystemExit(main())
