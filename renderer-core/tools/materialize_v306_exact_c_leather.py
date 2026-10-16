#!/usr/bin/env python3
"""V306 exact C_Leather producer/consumer admission on existing V300 PTDE island."""
import json
from pathlib import Path

r=Path(__file__).resolve().parents[1]
reg=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
assert reg["source_binary"]["record_count"]==325
names=("C_Leather[DSB].mtd","C_Leather[DSB]_Alp.mtd","C_Leather[DSB]_Edge.mtd")
# Other original MTD names may have multiple raw DSR identities. Reject
# ambiguity only in the exact three-row cohort being admitted.
by_name={}
for record in reg["records"]:
    name=record["mtd_name"]
    if name not in names:
        continue
    assert name not in by_name, "V306 exact Leather name has multiple raw sources"
    by_name[name]=record
assert set(by_name)==set(names)
out=[]
for n,name in enumerate(names,3):
    x=by_name[name]
    assert x["state"]=="PRESENT"
    assert x["homology_class"]=="HOMOLOGOUS_SPC"
    assert x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"]
    assert x["ptde_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx")
    assert x["envspc_slot"]==1
    assert x["c100_f32_bits"]==["0x3f000000"]*3
    assert x["c101_f32_bits"]==["0x3fc00000"]*3
    assert x["c102_f32_bits"]=="0x40800000"
    sha=x["dsr_mtd_sha256"]
    assert len(sha)==64 and set(sha)<=set("0123456789abcdef")
    out.append('{"'+name+'","'+sha+'",0x5200000'+str(n)+'u}')

header=r/"include/dsrrl/runtime/v300_exact_leather_material.hpp"
s=header.read_text("utf-8")
old='{"P_Leather[DSB]_Edge.mtd","cf968a22722667777d10429a03a2e4a87f867412949a7fe5590cb8cefaa2bfa4",0x52000002u}\n};'
new='{"P_Leather[DSB]_Edge.mtd","cf968a22722667777d10429a03a2e4a87f867412949a7fe5590cb8cefaa2bfa4",0x52000002u},\n'+',\n'.join(out)+'\n};\ninline constexpr std::size_t k_leather_count=sizeof(k_leather)/sizeof(k_leather[0]);\nstatic_assert(k_leather_count==5u,"V306 exact C_Leather cohort");'
assert s.count(old)==1,"V306: two original V300 leather rows expected"
s=s.replace(old,new,1)
s=s.replace('#include "dsrrl/runtime/v297_authored_slot0_material.hpp"',
            '#include "dsrrl/runtime/v297_authored_slot0_material.hpp"\n#include <cstddef>',1)
header.write_text(s,"utf-8")

hooks=r/"src/runtime/flver_engine_hooks.cpp"
s=hooks.read_text("utf-8")
patches=(
('''#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    + 2u
#endif''','''#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    + dsrrl::runtime::v300::k_leather_count
#endif'''),
("for(std::size_t i=0u;i<2u;++i) {\n   if(!dsrrl::runtime::v297::raw_sha_equal(",
 "for(std::size_t i=0u;i<dsrrl::runtime::v300::k_leather_count;++i) {\n   if(!dsrrl::runtime::v297::raw_sha_equal("),
("if(leather_index>=2u)return false;",
 "if(leather_index>=dsrrl::runtime::v300::k_leather_count)return false;")
)
for old,new in patches:
    assert s.count(old)==1,"V306 parser drift: "+old[:75]
    s=s.replace(old,new,1)
hooks.write_text(s,"utf-8")
print("V306_EXACT_5_LEATHER_MTD_SOURCE_DRAW_MATERIALIZED_PASS")
