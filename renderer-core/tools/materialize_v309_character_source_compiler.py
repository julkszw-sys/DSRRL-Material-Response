#!/usr/bin/env python3
"""V309 exact PTDE-authored character MTD compiler onto slot0 operator.

No global C_* permit. Source parser recognizes exact original raw DSR MTD
SHA; exact renderer and sidecar witnesses are still required at runtime.
"""
import json
import struct
from pathlib import Path

root=Path(__file__).resolve().parents[1]
registry=json.loads((root/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
assert registry["source_binary"]["record_count"]==325
# These original character SPX material families have the same original
# DifSpcBmp shader-equation family as the existing V297 slot0 bridge.
names=(
    "C_2231_Body[DSB].mtd",
    "C_5350_body[DSB].mtd",
    "C_Fire_DullLeather[DSB].mtd",
    "C_Fire_DullLeather[DSB]_Edge.mtd",
    "C_5260_Leather[DSB].mtd",
)
def scalar(x,field):
    rgba=x[field]
    assert len(rgba)==3 and len(set(rgba))==1,(
        "V309 non-scalar channel needs separate per-channel island",field,x["mtd_name"])
    raw=int(rgba[0],16)
    f=struct.unpack(">f",struct.pack(">I",raw))[0]
    assert 0.0<=f<=32.0 and f==f
    return f
source_rows=[]
all_rows=registry["records"]
for name in names:
    matches=[x for x in all_rows if x["mtd_name"]==name]
    assert len(matches)==1,"V309 ambiguous MTD name provenance: "+name
    x=matches[0]
    assert (x["state"],x["homology_class"],x["delta_kind"])==(
        "PRESENT","HOMOLOGOUS_SPC","EXACT_SPX")
    assert x["ptde_spx"]==x["dsr_spx"]
    assert x["ptde_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx")
    assert x["envspc_slot"]==0
    sha=x["dsr_mtd_sha256"]
    assert len(sha)==64 and set(sha)<=set("0123456789abcdef")
    assert sum(y["dsr_mtd_sha256"]==sha for y in all_rows)==1,(
        "V309 raw MTD SHA belongs to multiple MTD variants: "+name)
    a=scalar(x,"c100_f32_bits")
    b=scalar(x,"c101_f32_bits")
    c=struct.unpack(">f",struct.pack(">I",int(x["c102_f32_bits"],16)))[0]
    assert 0.0<c<=128.0
    # Unique source IDs are derived from the original row index;
    # do not alias a material to existing V297 or V307 route identities.
    provenance=0xa000+int(x["index"])
    assert provenance<65535
    source_rows.append(
        '    {"'+name+'",\n     "'+sha+'",\n     '
        +f"{a:.8g}f,{b:.8g}f,{c:.8g}f,{provenance}u"+'}')
h=root/"include/dsrrl/runtime/v297_authored_slot0_material.hpp"
s=h.read_text("utf-8")
needle='};\ninline constexpr std::size_t k_authored_slot0_count='
assert s.count(needle)==1,"V309 original MTD cohort boundary drift"
start=s.index(needle)
assert s[:start].rstrip().endswith("}"),"V309 last row incomplete"
s=s[:start]+',\n'+',\n'.join(source_rows)+'\n'+s[start:]
assert s.count("k_authored_slot0_count==11u")==1
s=s.replace("k_authored_slot0_count==11u","k_authored_slot0_count==16u",1)
h.write_text(s,"utf-8")
print("V309_ORIGINAL_CHARACTER_MTD_COMPILER_PASS newly=5 slot0=16 slot1=6 total=22")
