#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv
from pathlib import Path

def q(s:str)->str:return '"'+s.replace('\\','\\\\').replace('"','\\"')+'"'
def main()->int:
 ap=argparse.ArgumentParser();ap.add_argument('--input',required=True);ap.add_argument('--output',required=True);a=ap.parse_args()
 with Path(a.input).open('r',encoding='utf-8',newline='') as fh:data=[line for line in fh if not line.startswith('#')]
 rows=[]
 for raw in csv.DictReader(data,delimiter='\t'):
  rows.append({k:(int(v) if k in {'plan_index','shader_index','stable_receiver_id','stock_size','u_slot_word','d_slot_word_0','d_slot_word_1','replacement_size'} else v) for k,v in raw.items()})
 rows.sort(key=lambda r:r['plan_index'])
 if len(rows)!=24:raise SystemExit(f'expected 24 unique Phn HemEnvSubsurf consumers, got {len(rows)}')
 if [r['plan_index'] for r in rows]!=list(range(156,180)):raise SystemExit('plan_index must be contiguous 156..179')
 if sorted(r['stable_receiver_id'] for r in rows)!=list(range(24,48)):raise SystemExit('receiver IDs must be 24..47')
 if any(r['stratum']!='spc' for r in rows):raise SystemExit('all unique Subsurf rows must be Spc')
 if len({r['stock_sha256'] for r in rows})!=24 or len({r['replacement_sha256'] for r in rows})!=24:raise SystemExit('duplicate SHA')
 out=['#pragma once\n','#include <array>\n','#include <cstdint>\n','#include <string_view>\n\n','namespace dsrrl::operators::lightbank::generated_subsurf {\n\n',
 'struct upper_lower_phn_subsurf_plan {\n    std::uint16_t plan_index;\n    std::uint16_t shader_index;\n    std::uint8_t stable_receiver_id;\n    std::string_view name;\n    std::uint32_t stock_size;\n    std::string_view stock_sha256;\n    std::uint32_t u_slot_word;\n    std::uint32_t d_slot_word_0;\n    std::uint32_t d_slot_word_1;\n    std::uint32_t replacement_size;\n    std::string_view replacement_sha256;\n};\n\n',
 'inline constexpr std::array<upper_lower_phn_subsurf_plan,24> k_upper_lower_phn_subsurf_plans = {{\n']
 for r in rows:
  out.append('    {'+f"{r['plan_index']}u,{r['shader_index']}u,{r['stable_receiver_id']}u,{q(r['name'])},{r['stock_size']}u,{q(r['stock_sha256'])},{r['u_slot_word']}u,{r['d_slot_word_0']}u,{r['d_slot_word_1']}u,{r['replacement_size']}u,{q(r['replacement_sha256'])}"+'},\n')
 out+=['}};\n\n','} // namespace dsrrl::operators::lightbank::generated_subsurf\n']
 Path(a.output).write_text(''.join(out),encoding='utf-8');return 0
if __name__=='__main__':raise SystemExit(main())
