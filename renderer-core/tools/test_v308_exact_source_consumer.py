#!/usr/bin/env python3
"""V308 source bits/identities and downstream original atomic routing."""
import importlib.util
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("v308",root/"tools/materialize_v308_exact_six_mtd.py")
m=importlib.util.module_from_spec(spec)
# no exec of materializer; test actual generated source. Baseline registry independent.
reg=json.loads((root/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
rows={name:[x for x in reg["records"] if x["mtd_name"]==name] for name in
    ("P_RoughCloth[DSB].mtd","P_RoughCloth[DSB]_Edge.mtd",
     "C_RoughCloth[DSB].mtd","C_RoughCloth[DSB]_Edge.mtd",
     "Ps_Body[DSB]_Alp.mtd","P_Leather[DSB]_Alp.mtd")}
assert len(rows)==6 and all(len(v)==1 for v in rows.values())
h0=(root/"include/dsrrl/runtime/v297_authored_slot0_material.hpp").read_text()
h1=(root/"include/dsrrl/runtime/v300_exact_leather_material.hpp").read_text()
hooks=(root/"src/runtime/flver_engine_hooks.cpp").read_text()
source=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
draw=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
integrated=(root/"integrated/integrated_addon.cpp").read_text()
assert "k_authored_slot0_count==11u" in h0 and "k_leather_count==6u" in h1
for name, found in rows.items():
    item=found[0]
    header=h1 if name=="P_Leather[DSB]_Alp.mtd" else h0
    assert header.count('"'+name+'"')==1,name
    assert header.count(item["dsr_mtd_sha256"])==1,name
    assert item["state"]=="PRESENT" and item["delta_kind"]=="EXACT_SPX"
    assert item["ptde_spx"]==item["dsr_spx"]
    assert item["envspc_slot"]==(1 if header==h1 else 0)
for needle in ("k_authored_slot0_count","k_leather_count",
               "if(found!=cohort)","leather_index>=dsrrl::runtime::v300::k_leather_count",
               "v299::authored_slot0_source(identity)","v300::source(identity)"):
    assert needle in hooks,needle
for needle in ("has_exact_flver_material_ownership(query)","m.owner_tuple_exact",
               "m.actual_material_exact","env.envspc_slot!=0u"):
    assert needle in h0,needle
for needle in ("has_exact_flver_material_ownership(q)","m.owner_tuple_exact",
               "m.actual_material_exact","e.envspc_slot!=1u"):
    assert needle in h1,needle
assert "if (latest_hook_source(out))" in source
for needle in ("v299::authored_slot0_source(material)","v300::source(material)"):
    assert needle in source,needle
for needle in ("v299::authored_slot0_draw(material, decision)","v300::draw(material, decision)",
               "prepared.material_resources.spec_rgb","source_.latest(material, source)"):
    assert needle in draw,needle
assert "g_pmetal_envspec.prepare(" in integrated
print("V308_EXACT_17_MTD_SOURCE_SELECTOR_ATOMIC_DRAW_CONTRACT_PASS")
print("ACTUAL_RECEIVER_BRIDGE_PIXEL_OPEN")
