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
 if len(rows)!=42:raise SystemExit(f'expected 42 unique Phn Parallax executable consumers, got {len(rows)}')
 if [r['plan_index'] for r in rows]!=list(range(72,114)):raise SystemExit('plan_index must be contiguous 72..113')
 if len({r['stock_sha256'] for r in rows})!=42:raise SystemExit('duplicate stock SHA')
 if len({r['replacement_sha256'] for r in rows})!=42:raise SystemExit('duplicate replacement SHA')
 expected={'hemenv_parallax':18,'hemenvlerp_parallax':24}
 for fam,count in expected.items():
  if sum(r['family']==fam for r in rows)!=count:raise SystemExit(f'{fam} count mismatch')
 for r in rows:
  if r['family'] not in expected or r['stratum'] not in ('spc','nospc'):raise SystemExit('invalid family/stratum')
  if not (r['u_slot_word']<r['d_slot_word_0']<r['d_slot_word_1']):raise SystemExit(f'patch order {r["name"]}')
  if len(r['stock_sha256'])!=64 or len(r['replacement_sha256'])!=64:raise SystemExit('invalid SHA')
 out=['#pragma once\n','#include <array>\n','#include <cstdint>\n','#include <string_view>\n\n','namespace dsrrl::operators::lightbank::generated_parallax {\n\n',
      'enum class upper_lower_phn_parallax_family : std::uint8_t { hemenv_parallax = 0, hemenvlerp_parallax };\n',
      'enum class upper_lower_phn_parallax_stratum : std::uint8_t { nospc = 0, spc };\n\n',
      'struct upper_lower_phn_parallax_plan {\n    std::uint16_t plan_index;\n    std::uint16_t shader_index;\n    std::uint8_t stable_receiver_id;\n    upper_lower_phn_parallax_stratum stratum;\n    upper_lower_phn_parallax_family family;\n    std::string_view name;\n    std::uint32_t stock_size;\n    std::string_view stock_sha256;\n    std::uint32_t u_slot_word;\n    std::uint32_t d_slot_word_0;\n    std::uint32_t d_slot_word_1;\n    std::uint32_t replacement_size;\n    std::string_view replacement_sha256;\n};\n\n',
      'inline constexpr std::array<upper_lower_phn_parallax_plan,42> k_upper_lower_phn_parallax_plans = {{\n']
 for r in rows:
  fam='upper_lower_phn_parallax_family::'+r['family'];strat='upper_lower_phn_parallax_stratum::'+r['stratum']
  out.append('    {'+f"{r['plan_index']}u,{r['shader_index']}u,{r['stable_receiver_id']}u,{strat},{fam},{q(r['name'])},{r['stock_size']}u,{q(r['stock_sha256'])},{r['u_slot_word']}u,{r['d_slot_word_0']}u,{r['d_slot_word_1']}u,{r['replacement_size']}u,{q(r['replacement_sha256'])}"+'},\n')
 out+=['}};\n\n','} // namespace dsrrl::operators::lightbank::generated_parallax\n']
 Path(a.output).write_text(''.join(out),encoding='utf-8');return 0
if __name__=='__main__':raise SystemExit(main())
