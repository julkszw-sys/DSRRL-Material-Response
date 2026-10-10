#!/usr/bin/env python3
"""Offline source/provenance contract for the experimental 36-owner bridge.
Never interprets construction as GPU activation or PTDE pixel equivalence.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
read = lambda p: (root / p).read_text(encoding="utf-8")
prefix = "renderer-core/"
manifest = read(prefix+"include/dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp")
router = read(prefix+"include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp")
gate = read(prefix+"include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp")
source = read(prefix+"src/runtime/pmetal_env_source_runtime.cpp")
hook = read(prefix+"src/runtime/flver_engine_hooks.cpp")
draw = read(prefix+"src/runtime/pmetal_envspec_draw_runtime.cpp")
overlay = read(prefix+"tools/apply_v203_clean_equipment_spec.py")
cmake = read(prefix+"integrated/CMakeLists.txt")
recipe = read(prefix+"tools/configure_v203_clean_equipment.ps1")
integration = read(prefix+"integrated/integrated_addon.cpp")

rows = re.findall(r'\{ "([a-f0-9]{64})", (\d+)u, "([^"]+)", "([^"]+)" \},',manifest)
assert len(rows) == 36 and len(set(rows)) == 36
assert len({r[0] for r in rows}) == 21
assert len({r[3].lower() for r in rows}) == 18
assert all(n.lower().startswith(("am_", "bd_", "lg_", "hd_", "wp_")) for *_,n in rows)
assert all(m in ("P_Metal[DSB].mtd", "P_Leather[DSB].mtd",
                 "P_DullLeather[DSB].mtd") for _,_,m,_ in rows)
# Exact raw-MTD router already differentiates all three equipment material families.
# This is offline source authority, not a live draw or PTDE pixel claim.
expected_slots = {
    "P_DullLeather[DSB].mtd": {0},
    "P_Leather[DSB].mtd": {1},
    "P_Metal[DSB].mtd": {2},
}
for mtd, slots in expected_slots.items():
    # Match one generated two-line MTD record at a time: no cross-record
    # traversal and no escaped-regex-literal ("\\s") false negatives.
    matches = re.findall(
        r'^\s*"' + re.escape(mtd) +
        r'",[^\n]*\n\s*\{\{[^\n]*?\}\}, envspec_router_state::(\w+), (\d+)u',
        router, flags=re.MULTILINE)
    assert matches, (mtd, "no complete raw-MTD router records")
    assert all(state == "present" for state, _ in matches), (mtd, matches)
    found = {int(n) for _, n in matches}
    assert found == slots, (mtd, found, slots)
assert "exact_proven_owner_slot(query)" in gate
assert 'mtd_semantic_hash("DifSpcBmp")' in gate
assert "classify_mtd_envspec_semantics(query)" in gate
assert "ptde_envspec_presence::present" in gate
assert "mtd_envspec_router_state::present" in gate
assert "env.envspc_slot < 4u" in gate
assert "decision.receiver_id >= 33u" in gate and "decision.receiver_id <= 35u" in gate
assert "std::isfinite(decision.c101)" in gate
assert "exact_envspec36_material(identity)" in hook
assert "exact_envspec36_material(material)" in source
assert "exact_envspec36_draw(material, decision)" in draw
assert "env_semantics.envspc_slot >= 4u" in draw
assert "#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)" in hook
assert "#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)" in source
assert "#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)" in draw
assert "if (!exact_pmetal_material_selection(material))" in source
assert "if (latest_hook_source(out))" in source
assert source.index("if (!exact_pmetal_material_selection(material))") < source.index("if (latest_hook_source(out))")
assert "DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG" in cmake
assert "DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL" in cmake
assert "DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE" in cmake
assert "DSRRL_V203_EQUIPMENT_ENVSPEC36" in recipe
assert "CONSUMER_GATE" in overlay
# The resource router has TWO overloads. Legacy (context,query) demands the
# old generic spec_rgb census bit, which can be absent for exact EnvSpec MTD.
# A strict 36-owner, rx33..35, current-gSpec branch must use the isolated
# context-only overload; every other material remains on the census overload.
assert 'ROUTER_ANCHOR' in overlay and 'ROUTER_PATCH' in overlay
assert 'exact_equipment_envspec_consumer' in overlay
assert 'exact_proven_slot_spec_hash(query, h1)' in overlay
assert 'receiver_id >= 33u && receiver_id <= 35u' in overlay
assert 'evaluate_spec_rgb_route(context_spec)' in overlay
assert 'evaluate_spec_rgb_route(context_spec, query)' in overlay

assert "mtd_semantic_operator::env_spec" in overlay
assert "exact_envspec36_material(query.material)" in overlay
# Ordinary MR remains stock-PBL without an authorized SpecRGB island.
assert "a SpecRGB consumer" in integration
assert integration.count("                false,\n                prepared.resources);") >= 1
# Reject the invalid NPC/character test target as an equipment mapping.
assert "c5330_s" not in manifest
print("V203_ENVSPEC36_OFFLINE_PORT_PASS rows=36 flver=21 names=18")
print("V203_ENVSPEC36_NONPMETAL_UNKEYED_FALLBACK=DISABLED")
print("V203_ENVSPEC36_CONSTRUCTION_ONLY runtime=OPEN pixel=OPEN")
