#!/usr/bin/env python3
"""V307: exact 3 C_DullLeather slot0 MTD donor sources into V297 PTDE draw.

Applied AFTER V306 source parser extension. New records are exact source
SHA + original PTDE authored c100,c101,c102, never generic MTD aliases.
Uses one correct material parser cohort index space for both V297 and V300.
"""
import json
from pathlib import Path

r=Path(__file__).resolve().parents[1]
source=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
assert source["source_binary"]["record_count"]==325
names=("C_DullLeather[DSB].mtd","C_DullLeather[DSB]_Alp.mtd",
       "C_DullLeather[DSB]_Edge.mtd")
lookups={name:[x for x in source["records"] if x["mtd_name"]==name]
         for name in names}
assert all(len(rows)==1 for rows in lookups.values()),"V307 ambiguous exact DullLeather MTD name"
rows=[]
for name in names:
    x=lookups[name][0]
    assert x["state"]=="PRESENT" and x["homology_class"]=="HOMOLOGOUS_SPC"
    assert x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"] and x["ptde_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx")
    assert x["envspc_slot"]==0
    assert x["c100_f32_bits"]==["0x3f000000"]*3
    assert x["c101_f32_bits"]==["0x3f800000"]*3
    assert x["c102_f32_bits"]=="0x40000000"
    sha=x["dsr_mtd_sha256"]
    assert len(sha)==64 and set(sha)<=set("0123456789abcdef")
    identity=0x8000+int(x["index"])
    assert identity < 0xffff
    rows.append('    {"'+name+'",\n     "'+sha+'",\n     0.5f,1.0f,2.0f,'+str(identity)+'u}')

p=r/"include/dsrrl/runtime/v297_authored_slot0_material.hpp"
s=p.read_text("utf-8")
old='''    {"Ps_Body[DSB].mtd",
     "af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",
     0.5f,1.0f,8.0f,492u}
};'''
new='''    {"Ps_Body[DSB].mtd",
     "af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",
     0.5f,1.0f,8.0f,492u},
'''+',\n'.join(rows)+'''\n};
inline constexpr std::size_t k_authored_slot0_count=
    sizeof(k_authored_slot0)/sizeof(k_authored_slot0[0]);
static_assert(k_authored_slot0_count==6u,"V307 exact authored slot0 count");'''
assert s.count(old)==1,"V307 original V297 three-row preimage drift"
s=s.replace(old,new,1)
s=s.replace('#include <cmath>\n#include <cstdint>',
            '#include <cmath>\n#include <cstdint>\n#include <cstddef>',1)
p.write_text(s,"utf-8")

p=r/"src/runtime/flver_engine_hooks.cpp"
s=p.read_text("utf-8")
patches=[
('''#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
    + 3u
#endif''',
 '''#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
    + dsrrl::runtime::v297::k_authored_slot0_count
#endif'''),
('''  std::size_t found=3u;
  for(std::size_t i=0u;i<3u;++i){''',
 '''  const std::size_t cohort=dsrrl::runtime::v297::k_authored_slot0_count;
  std::size_t found=cohort;
  for(std::size_t i=0u;i<cohort;++i){'''),
('if(found!=3u)return false;','if(found!=cohort)return false;'),
('if(found!=3u){','if(found!=cohort){'),
('if(pilot_index>=3u) {','if(pilot_index>=dsrrl::runtime::v297::k_authored_slot0_count) {'),
('k_exact_binding_mr_v1.size()+3u;',
 'k_exact_binding_mr_v1.size()+dsrrl::runtime::v297::k_authored_slot0_count;'),
('const auto leather_index=pilot_index-3u;',
 'const auto leather_index=pilot_index-dsrrl::runtime::v297::k_authored_slot0_count;')
]
for old,new in patches:
    assert s.count(old)==1,"V307 retail parser offset drift: "+old[:90]
    s=s.replace(old,new,1)
p.write_text(s,"utf-8")
print("V307_EXACT_6_DULLLEATHER_BODY_SLOT0_PLUS_5_LEATHER_SLOT1_PARSER_COHORT_PASS")
