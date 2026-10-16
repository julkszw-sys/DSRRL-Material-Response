#!/usr/bin/env python3
"""V313 independent original PTDE slot2/3 source coefficients & GPU cut."""
from pathlib import Path
import json,re,struct
r=Path(__file__).resolve().parents[1]
j=json.loads((r/"data/census/v313_exact_metal2_wet3_mtd_source.json").read_text("utf-8"))
reg=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
h=(r/"include/dsrrl/runtime/v313_exact_slot23_metal_wet.hpp").read_text("utf-8")
sel=(r/"src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
draw=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
source=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
a=(r/"integrated/integrated_addon.cpp").read_text("utf-8")
v311=(r/"include/dsrrl/runtime/v311_live_direct_owner_recovery.hpp").read_text("utf-8")
assert j["total"]==22 and j["slot2_count"]==14 and j["slot3_count"]==8
assert len(set(x["name"] for x in j["entries"]))==22
assert len(set(x["sha"] for x in j["entries"]))==22
for x in j["entries"]:
    rows=[z for z in reg["records"] if z["mtd_name"]==x["name"]]
    assert len(rows)==1
    y=rows[0]
    assert y["dsr_mtd_sha256"]==x["sha"] and y["envspc_slot"]==x["slot"]
    assert y["ptde_spx"]==y["dsr_spx"] and y["ptde_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx")
    assert y["state"]=="PRESENT" and y["delta_kind"]=="EXACT_SPX"
    assert h.count('"'+x["name"]+'"')==1
    assert h.count('"'+x["sha"]+'"')==1
def fb(s):
    return struct.unpack(">f",struct.pack(">I",int(s,16)))[0]
for x in j["entries"]:
    for n in ("c100","c101"):
        assert len(set(x[n]))==1 and fb(x[n][0])>=0
    assert fb(x["c102"])>0
for needle in ("has_exact_flver_material_ownership(q)",
               "classify_mtd_envspec_semantics(q)",
               "e.envspc_slot!=hit->env_slot",
               "m.owner_tuple_exact","m.actual_material_exact",
               "m.material_family_hash",
               "out.c101=hit->c101","out.ptde_specular_power=hit->power",
               "host.active"):
    assert needle in h,needle
for needle in ("+ dsrrl::runtime::v313::k_count",
               "v313::k_rows[i].sha","const auto slot23_index=",
               "v313::source(identity)"):
    assert needle in sel,needle
assert "v313::source(material)" in source
assert draw.count("v313::draw(material, decision)")>=2
assert "v313::promote(" in a
assert "current_mtd.material==actual_material" in sel
assert "v313::k_rows" in v311
assert "if (latest_hook_source(out))" in source
assert "g_pmetal_envspec.prepare(" in a
print("V313_ORIGINAL_14_METAL_SLOT2_8_WET_SLOT3_AND_ATOMIC_PHYSICAL_CONSUMER_SOURCE_PASS")
print("V313_RUNTIME_PIXEL_OPEN")
