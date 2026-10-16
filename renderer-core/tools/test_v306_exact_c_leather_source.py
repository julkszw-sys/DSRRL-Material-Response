#!/usr/bin/env python3
"""V306 exact source and operator contract. Runtime/pixels remain unverified."""
from pathlib import Path
import json
r=Path(__file__).resolve().parents[1]
hdr=(r/"include/dsrrl/runtime/v300_exact_leather_material.hpp").read_text("utf-8")
hooks=(r/"src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
draw=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
src=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
addon=(r/"integrated/integrated_addon.cpp").read_text("utf-8")
reg=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
rows={x["mtd_name"]:x for x in reg["records"]}
assert hdr.count('.mtd')==5
assert "k_leather_count==5u" in hdr
for name in ("C_Leather[DSB].mtd","C_Leather[DSB]_Alp.mtd","C_Leather[DSB]_Edge.mtd"):
    x=rows[name]
    assert x["state"]=="PRESENT" and x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"]
    assert x["envspc_slot"]==1
    assert x["c100_f32_bits"]==["0x3f000000"]*3
    assert x["c101_f32_bits"]==["0x3fc00000"]*3
    assert x["c102_f32_bits"]=="0x40800000"
    assert hdr.count('"'+name+'"')==1
    assert hdr.count(x["dsr_mtd_sha256"])==1
for needle in ("m.owner_tuple_exact","m.actual_material_exact","m.material_slot_valid",
               "has_exact_flver_material_ownership(q)","e.envspc_slot!=1u",
               "out.c101=1.5f","out.ptde_specular_power=4.0f",
               "v297::raw_sha_equal(m.raw_mtd_sha256,r.sha)"):
    assert needle in hdr,needle
for needle in ("+ dsrrl::runtime::v300::k_leather_count",
               "i<dsrrl::runtime::v300::k_leather_count",
               "leather_index>=dsrrl::runtime::v300::k_leather_count"):
    assert needle in hooks,needle
for needle,body in (("v300::source(material)",src),("v300::draw(material, decision)",draw),
                    ("v300::source(identity)",hooks),("effective_envspec_decision=leather;",addon),
                    ("g_pmetal_envspec.prepare(",addon),("prepared.material_resources.spec_rgb",draw)):
    assert needle in body,needle
assert "source_.latest(material, source)" in draw
assert "if (latest_hook_source(out))" in src
print("V306_C_LEATHER_5_EXACT_MTD_SELECTOR_SOURCE_CONSUMER_ATOMIC_CONTRACT_PASS")
print("V306_RUNTIME_LIVENESS_BRIDGE_ACTIVATION_PIXEL_OPEN")
