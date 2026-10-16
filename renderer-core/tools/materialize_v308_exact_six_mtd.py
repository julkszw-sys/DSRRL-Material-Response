#!/usr/bin/env python3
"""V308 independently authored PTDE 6 exact raw-MTD rows.

Run after V307 and preserve 1295 logical resource identities, actual per
FLVER/slot owner requirements, and existing receiver/shader/state gates.
"""
import json
from pathlib import Path
r=Path(__file__).resolve().parents[1]
registry=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
assert registry["source_binary"]["record_count"]==325
# Four independently attested RoughCloth SPX sources (V293 family already
# independently scoped), one Ps_Body alpha, and one P_Leather alpha.
slot0={
    "P_RoughCloth[DSB].mtd":(0.5,1.0,2.0),
    "P_RoughCloth[DSB]_Edge.mtd":(0.5,1.0,2.0),
    "C_RoughCloth[DSB].mtd":(0.5,1.0,2.0),
    "C_RoughCloth[DSB]_Edge.mtd":(0.5,1.0,2.0),
    "Ps_Body[DSB]_Alp.mtd":(0.5,1.0,8.0),
}
slot1={"P_Leather[DSB]_Alp.mtd":(0.5,1.5,4.0)}
import struct
def float_bits(x):
    return "0x"+struct.pack(">f",float(x)).hex()
def verify(name,values,env_slot):
    candidates=[x for x in registry["records"] if x["mtd_name"]==name]
    assert len(candidates)==1, "source identity ambiguous "+name
    x=candidates[0]
    assert x["state"]=="PRESENT" and x["homology_class"]=="HOMOLOGOUS_SPC"
    assert x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"]
    assert x["ptde_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx")
    assert x["envspc_slot"]==env_slot
    assert x["c100_f32_bits"]==[float_bits(values[0])]*3
    assert x["c101_f32_bits"]==[float_bits(values[1])]*3
    assert x["c102_f32_bits"]==float_bits(values[2])
    sha=x["dsr_mtd_sha256"]
    assert len(sha)==64 and set(sha)<=set("0123456789abcdef")
    return x
rows0=[]
for name,values in slot0.items():
    x=verify(name,values,0)
    unique_provenance=0x9000+int(x["index"])
    assert unique_provenance < 65535
    rows0.append('    {"'+name+'",\n     "'+x["dsr_mtd_sha256"]+'",\n     '+','.join(format(v,'.1f')+'f' for v in values)+','+str(unique_provenance)+'u}')
rows1=[]
for name,values in slot1.items():
    x=verify(name,values,1)
    rows1.append('{"'+name+'","'+x["dsr_mtd_sha256"]+'",0x52000005u}')

def patch_array(path,anchor,rows,count_before,count_after):
    s=path.read_text("utf-8")
    assert s.count(anchor)==1, "V308 material cohort authority anchor drift"
    # Insert exactly before already-existing constexpr count member.
    index=s.index(anchor)
    prefix=s[:index]
    assert prefix.rstrip().endswith("}"),"V308 expected last exact donor row"
    s=prefix+',\n'+',\n'.join(rows)+'\n'+s[index:]
    pre='count=='+str(count_before)+'u'
    post='count=='+str(count_after)+'u'
    assert s.count(pre)==1
    s=s.replace(pre,post,1)
    path.write_text(s,"utf-8")

patch_array(r/"include/dsrrl/runtime/v297_authored_slot0_material.hpp",
            '};\ninline constexpr std::size_t k_authored_slot0_count=',
            rows0,6,11)
patch_array(r/"include/dsrrl/runtime/v300_exact_leather_material.hpp",
            '};\ninline constexpr std::size_t k_leather_count=',
            rows1,5,6)
print("V308_AUTHORED_EXACT_SOURCES_PASS slot0=11 slot1=6 total=17")
