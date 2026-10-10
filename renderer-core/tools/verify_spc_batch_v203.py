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
identity = (ROOT / "src/runtime/texture_identity_transport.cpp").read_text()
identity_api = (ROOT / "include/dsrrl/runtime/texture_identity_transport.hpp").read_text()
assert "texture_name_liveness liveness()" in identity
assert "texture_name_liveness liveness()" in identity_api
assert "g_texture_name_hook_calls.fetch_add" in identity
assert "g_texture_name_complete.fetch_add" in identity
assert "DSRRL SPC25 NAME CUT" in resource
physical = (ROOT / "include/dsrrl/runtime/spc25_physical_t1_probe.hpp").read_text()
source_manifest = (ROOT / "include/dsrrl/runtime/spc25_stock_source_manifest.hpp").read_text()
assert "spc25_physical::init(device,desc,initial_data,resource)" in resource
assert "spc25_physical::link_view(view.handle,resource.handle)" in resource
assert "spc25_physical::inspect(stock)" in resource
assert "spc25_physical::drop_resource(resource.handle)" in resource
assert "spc25_physical::drop_view(view.handle)" in resource
assert "spc25_physical::drop_device(native)" in resource
assert "view->GetResource(&r)" in physical
assert "current_gpu_bytes=UNVERIFIED srv_swap=0 pixel=OPEN" in physical
assert "PSSetShaderResources" not in physical
assert "k_original_dsr_spec" in source_manifest
assert "WP_A_0106_mailbreaker_s" in source_manifest
# Previously the 384 MiB session-wide hash budget was exhausted before the
# late 1024x2048/12/BC1 stock PS-t1 resource. Preserve this observed route
# as a passive diagnostic lane; other BC traffic has an exact atomic budget.
assert "budget_bytes.fetch_add" not in physical
assert "observed_spc25_t1_layout" in physical
assert "t.width==1024u && t.height==2048u" in physical
assert "t.levels==12u && fmt==71u" in physical
assert "budget_bytes.compare_exchange_weak" in physical
assert "RESOURCE_TOO_LARGE" in physical
# Additional asset fingerprints are source-only, not proof of material/receiver.
manifest_rows = re.findall(r'\{"([^"]+)",(\d+),(\d+),(\d+),(\d+),"([0-9a-f]{64})"\}', source_manifest)
assert len(manifest_rows)==18, len(manifest_rows)
assert len({r[0].lower() for r in manifest_rows})==18
assert len({r[5] for r in manifest_rows})==18
assert "No C_Metal/C_RoughCloth texture is claimed" in source_manifest
assert "AM_F_9450_L_s" in source_manifest and "WP_A_1700_s" in source_manifest
assert "return nullptr; // noninjective source digest" in source_manifest

assert "g_name_exact_init_resource.fetch_add" in resource
assert "g_name_exact_create_view.fetch_add" in resource
assert "g_name_exact_init_view.fetch_add" in resource

assert "material_resources_.prepare_draw_requests(" in consumer
assert "DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH" in consumer
assert "DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH" in resource
assert "pixel=OPEN" in resource
# Explicitly verify original V13 remains the first source fallback.
assert "latest_hook_source(out)" in producer
assert "out.unkeyed_hook_fallback = true" in producer
print("Shared exact-stock-t1 late-native recovery: 25/25 SOURCE PASS; runtime/pixel OPEN")
print('Unique MTD and EnvSpec entries:',len(compiled))
