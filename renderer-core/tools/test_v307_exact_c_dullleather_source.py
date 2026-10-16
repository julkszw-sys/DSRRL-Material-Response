#!/usr/bin/env python3
"""V307 signed source row and parser count regression guard (not pixel)."""
import json
from pathlib import Path
r=Path(__file__).resolve().parents[1]
src=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
slot=(r/"include/dsrrl/runtime/v297_authored_slot0_material.hpp").read_text("utf-8")
leather=(r/"include/dsrrl/runtime/v300_exact_leather_material.hpp").read_text("utf-8")
hooks=(r/"src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
draw=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
producer=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
addon=(r/"integrated/integrated_addon.cpp").read_text("utf-8")
assert slot.count('.mtd')==6 and "k_authored_slot0_count==6u" in slot
assert leather.count('.mtd')==5 and "k_leather_count==5u" in leather
for name in ("C_DullLeather[DSB].mtd",
             "C_DullLeather[DSB]_Alp.mtd","C_DullLeather[DSB]_Edge.mtd"):
    rows=[x for x in src["records"] if x["mtd_name"]==name]
    assert len(rows)==1
    x=rows[0]
    assert x["homology_class"]=="HOMOLOGOUS_SPC" and x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"] and x["envspc_slot"]==0
    assert x["c100_f32_bits"]==["0x3f000000"]*3
    assert x["c101_f32_bits"]==["0x3f800000"]*3
    assert x["c102_f32_bits"]=="0x40000000"
    assert slot.count(x["dsr_mtd_sha256"])==1
    assert slot.count('"'+name+'"')==1
for needle in ("m.actual_material_exact","m.owner_tuple_exact","m.material_slot_valid",
               "has_exact_flver_material_ownership(query)","env.envspc_slot!=0u",
               "row->ptde_c100","row->ptde_c101","row->ptde_c102"):
    assert needle in slot,needle
for needle in ("+ dsrrl::runtime::v297::k_authored_slot0_count",
               "i<cohort;++i", "if(found!=cohort)",
               "pilot_index>=dsrrl::runtime::v297::k_authored_slot0_count",
               "pilot_index-dsrrl::runtime::v297::k_authored_slot0_count",
               "k_exact_binding_mr_v1.size()+dsrrl::runtime::v297::k_authored_slot0_count",
               "+ dsrrl::runtime::v300::k_leather_count",
               "i<dsrrl::runtime::v300::k_leather_count"):
    assert needle in hooks,needle
for needle,where in (("v299::authored_slot0_source(material)",producer),
                     ("v299::authored_slot0_draw(material, decision)",draw),
                     ("v299::authored_slot0_source(identity)",hooks),
                     ("promote_exact_slot0_source(",addon),
                     ("g_pmetal_envspec.prepare(",addon)):
    assert needle in where,needle
assert "if (latest_hook_source(out))" in producer
print("V307_EXACT_C_DULLLEATHER_3_SLOT0_AND_C_LEATHER_3_SLOT1_SOURCE_CONSUMER_CONTRACT_PASS")
print("V307_ACTIVATION_AND_PIXEL_OPEN")
