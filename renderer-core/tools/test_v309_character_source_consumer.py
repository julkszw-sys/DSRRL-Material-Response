#!/usr/bin/env python3
"""V309 compile-time material source/consumer contract and negative scope."""
from pathlib import Path
import json,struct
r=Path(__file__).resolve().parents[1]
reg=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
slot0=(r/"include/dsrrl/runtime/v297_authored_slot0_material.hpp").read_text("utf-8")
slot1=(r/"include/dsrrl/runtime/v300_exact_leather_material.hpp").read_text("utf-8")
hooks=(r/"src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
producer=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
consumer=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
addon=(r/"integrated/integrated_addon.cpp").read_text("utf-8")
names=(
 "C_2231_Body[DSB].mtd","C_5350_body[DSB].mtd",
 "C_Fire_DullLeather[DSB].mtd","C_Fire_DullLeather[DSB]_Edge.mtd",
 "C_5260_Leather[DSB].mtd",
)
assert slot0.count('    {"')==16
assert "k_authored_slot0_count==16u" in slot0
assert "k_leather_count==6u" in slot1
for name in names:
    matches=[x for x in reg["records"] if x["mtd_name"]==name]
    assert len(matches)==1
    x=matches[0]
    assert x["envspc_slot"]==0 and x["state"]=="PRESENT"
    assert x["homology_class"]=="HOMOLOGOUS_SPC" and x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"]
    assert slot0.count('"'+name+'"')==1
    assert slot0.count('"'+x["dsr_mtd_sha256"]+'"')==1
    for f in ("c100_f32_bits","c101_f32_bits"):
        assert len(set(x[f]))==1
for needle in ("+ dsrrl::runtime::v297::k_authored_slot0_count",
               "+ dsrrl::runtime::v300::k_leather_count",
               "pilot_index>=dsrrl::runtime::v297::k_authored_slot0_count",
               "leather_index-dsr"):
    if needle=="leather_index-dsr":
        assert "const auto leather_index=pilot_index-dsrrl::runtime::v297::k_authored_slot0_count" in hooks
    else:assert needle in hooks,needle
assert "if (latest_hook_source(out))" in producer
assert "v299::authored_slot0_source(material)" in producer
assert "v299::authored_slot0_draw(material, decision)" in consumer
assert "source_.latest(material, source)" in consumer
for needle in ("m.owner_tuple_exact","m.actual_material_exact","m.material_slot_valid",
               "has_exact_flver_material_ownership(query)","env.envspc_slot!=0u"):
    assert needle in slot0
assert "g_pmetal_envspec.prepare(" in addon
print("V309_22_EXACT_ORIGINAL_MTD_SOURCE_AND_FULL_DRAW_CONTRACT_PASS")
print("NEW_RUNTIME_RECEIVER_ACTIVATION_AND_PTDE_PIXELS_OPEN")
