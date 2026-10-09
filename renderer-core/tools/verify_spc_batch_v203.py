#!/usr/bin/env python3
"""Static census audit: all 25 exact SPC material profiles."""
import json
import pathlib
import re
ROOT = pathlib.Path(__file__).resolve().parents[1]
routes = json.loads((ROOT / "data/material_response_routes_v1.json").read_text())["routes"]
envs = json.loads((ROOT / "data/census/ptde_mtd_envspec_router_v1.json").read_text())["records"]
header = (ROOT / "include/dsrrl/runtime/ptde_metal_envspec_authority.hpp").read_text()
pattern = r'\{ptde_metal_envspec_profile::(\w+),\s*(\d+)u,\s*"([^"]+)",\s*"([0-9a-f]{64})",\s*([\d.]+)f,\s*(\d+)u\}'
compiled = re.findall(pattern, header)
assert len(compiled) == 25, len(compiled)

assert len({r[2] for r in compiled}) == len(compiled)
excluded = ("Body", "body", "FaceGen", "S_Metal")
eligible = [
    x for x in routes
    if x["material_family"] == "DifSpcBmp"
    and x["receiver_triplet"] == [33, 34, 35]
    and not any(word in x["mtd_name"] for word in excluded)
    and x["mtd_name"] != "P_DullLeather[DSB].mtd"
]
assert len(eligible) == 25, len(eligible)
assert {x["mtd_name"] for x in eligible} == {x[2] for x in compiled}
for profile, route, name, sha, gain, slot in compiled:
    matches = [x for x in routes if x["mtd_name"] == name]
    assert len(matches) == 1, name
    assert matches[0]["route_index"] == int(route), name
    assert matches[0]["sha256"] == sha, name
    assert float(matches[0]["c101"]) == float(gain), name
    assert matches[0]["receiver_triplet"] == [33, 34, 35], name
for profile, route, name, sha, gain, slot in compiled:
    assert len([e for e in envs if e["mtd_name"] == name]) == 1, name
    env = next(e for e in envs if e["mtd_name"] == name)
    assert env["dsr_mtd_sha256"] == sha, name
    assert env["envspc_slot"] == int(slot), name
    assert env["state"] == "PRESENT", name
    assert env["homology_class"] == "HOMOLOGOUS_SPC", name
    assert env["delta_kind"] == "EXACT_SPX", name
    assert env["ptde_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx"), name
    assert env["dsr_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx"), name
assert all(x["material_family"] == "DifSpcBmp" for x in routes if any(x["mtd_name"] == z[2] for z in compiled))
names = {x[2] for x in compiled}
assert "S_Metal[DSB]_Edge.mtd" not in names
assert "P_DullLeather[DSB].mtd" not in names
assert all("Body" not in name and "body" not in name for name in names)
selector = (ROOT / "src/runtime/flver_engine_hooks.cpp").read_text()
producer = (ROOT / "src/runtime/pmetal_env_source_runtime.cpp").read_text()
consumer = (ROOT / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
assert "should_dispatch_ptde_metal_selector_source(identity)" in selector
assert "pmetal_env_source_selector_event(" in selector
assert "match_ptde_metal_envspec_material(material) != nullptr" in producer
assert "source_.latest_exact_material(material, source)" in consumer
assert "env_semantics.envspc_slot != match_ptde_metal_envspec_material(material)->envspc_slot" in consumer
assert "decision.c101 != authority->c101" in consumer
# The diagnostic must observe both retail selector pathways and real draws.
# These assertions verify construction only, not live arrival of any route.
integrated = (ROOT / "integrated/integrated_addon.cpp").read_text()
assert "log_spc_material_census_once(identity, false)" in selector
assert "log_spc_material_census_once(runtime_material, true)" in selector
assert "log_spc_draw_receiver_once(" in integrated
assert "stage=draw_owner" in integrated
# The runtime-only fallback must identify whether the failed owner join is
# FLVER registry lookup, corpus MTD join or owner publish. It never
# authorizes a resource bridge using MTD alone.
assert "log_spc_owner_join_cut_once(" in selector
assert "flver_registry_miss" in selector
assert "flver_owner_mtd_join_miss" in selector
assert "selector_owner_publish_reject" in selector
assert "resource_bridge=FAIL_OPEN" in selector
# Regression: a runtime exact-MTD material only receives an exact FLVER
# owner if the SAME live selector resolved FLVER SHA + slot and the
# source-complete offline corpus positively attests the triple.
assert "dsr_flver_owner_tuple_authenticated(" in selector
assert "owner_lookup_ok && !exact_owner_published" in selector
assert "observation.material_slot_valid" in selector
assert "runtime_material.actual_material_exact" in selector
assert "runtime_material.semantic_name_hash" in selector
assert "recovered.flver_sha256 = observation.flver_sha256" in selector
assert "recovered.material_slot = observation.material_slot" in selector
assert "is_experimental_ptde_metal_envspec_material(recovered)" in selector
assert "stage=corpus_recovered_exact" in selector
assert "No fallback for unrecognized FLVER digests" in selector
assert "require fresh actual material pointer + corpus join every time" in selector
assert 'runtime_mtd_only' in selector and 'flver_owner_exact' in selector
assert "exact_bridge_authority=" in selector and "exact_bridge_authority=" in integrated
# The same exact native GPU t1 recovery gate is shared by all 25
# authenticated material profiles; no per-MTD heuristic exception.
resource = (ROOT / "src/runtime/material_resource_draw_runtime.cpp").read_text()
api = (ROOT / "include/dsrrl/runtime/material_resource_draw_runtime.hpp").read_text()
assert "try_recover_exact_bound_spec_from_native_name(" in api
assert "try_recover_exact_bound_spec_from_native_name(" in resource
assert "experimental_material &&" in consumer
assert "material_resources_.try_recover_exact_bound_spec_from_native_name(" in consumer
assert "generated::spec_equipment_name_hash_allowed_v12(hash)" in resource
assert "snapshot_native_exact_debug_name(stock, name, &view_label)" in resource
assert "stock->GetResource(&resource)" in resource
assert "g_late_native_t1_attempts" in resource
assert "stock_bound_native_exact_debug_name" in resource
assert "WKPDID_D3DDebugObjectNameW" in resource
assert "native_label_present_not_canonical_allowlisted" in resource
assert "stock_t1_missing_exact_native_name" in resource
assert "material_resources_.prepare_draw_requests(" in consumer
assert "DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH" in consumer
assert "DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH" in resource
assert "pixel=OPEN" in resource
# Explicitly verify original V13 remains the first source fallback.
assert "latest_hook_source(out)" in producer
assert "out.unkeyed_hook_fallback = true" in producer
print("Shared exact-stock-t1 late-native recovery: 25/25 SOURCE PASS; runtime/pixel OPEN")
print('Unique MTD and EnvSpec entries:',len(compiled))
