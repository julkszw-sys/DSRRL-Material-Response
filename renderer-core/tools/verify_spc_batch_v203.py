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

# SPC25 bounded exact CPU provenance cut is diagnostic only, not a
# TexHdlResCap/FrpgTextureEntity -> D3D11 resource authority.
packet_source=(ROOT / "src/runtime/texture_identity_transport.cpp").read_text()
packet_asm=(ROOT / "integrated/spc25_packet_source_detour.asm").read_text()
integrated_cmake=(ROOT / "integrated/CMakeLists.txt").read_text()
assert "k_packet_rva = 0x583BCEu" in packet_source
assert "k_writer_rva = 0x57EFB0u" in packet_source
assert "k_packet_resume_rva = 0x583BF2u" in packet_source
assert "0xE8,0xD5,0xB3,0xFF,0xFF,0xEB,0x15" in packet_source
assert "prepare_hook(g_packet,k_packet_rva,k_packet_bytes" in packet_source
assert "g_status.packet_hook_armed = true" in packet_source
assert "const bool packet_ok = restore(g_packet)" in packet_source
assert "g_cpu_packet_writer_calls.fetch_add" in packet_source
assert "source_to_srv=UNVERIFIED" in resource
assert "cpu_808d_writer=%llu cpu_808d_named_scope=%llu" in resource
assert "mov rcx, rbx" in packet_asm
assert "mov rdx, rdi" in packet_asm
assert "mov r8d, esi" in packet_asm
assert "call qword ptr [g_dsrrl_spc25_writer_target]" in packet_asm
assert "jmp qword ptr [g_dsrrl_spc25_packet_resume]" in packet_asm
assert "spc25_packet_source_detour.asm" in integrated_cmake
assert "spc25_packet_reader_detour.asm" in integrated_cmake
assert "DSRRL_EXPERIMENTAL_SPC25_DECODER_PROBE" in integrated_cmake
assert "typed decoder hook is quarantined" in integrated_cmake
assert "if(MSVC AND DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)" in integrated_cmake
assert "$<$<COMPILE_LANGUAGE:CXX>:/MP>" in integrated_cmake
assert "#if defined(DSRRL_EXPERIMENTAL_SPC25_DECODER_PROBE)" in packet_source
assert "typed decoder QUARANTINED" in packet_source

decoder_asm=(ROOT / "integrated/spc25_packet_reader_detour.asm").read_text()
assert "k_decoder_rva = 0x57F000u" in packet_source
assert "k_decoder_resume_rva = 0x57F00Fu" in packet_source
assert "0x57,0x48,0x83,0xEC,0x20" in packet_source
assert "prepare_hook(g_decoder,k_decoder_rva,k_decoder_bytes" in packet_source
assert "g_status.decoder_hook_armed = true" in packet_source
assert "const bool decoder_ok = restore(g_decoder)" in packet_source
assert "cache_name_equal" in packet_source
assert "attest_cpu_object_rtti(" in packet_source
assert "data_begin = 0x1A25000u" in packet_source
assert "data_end = 0x1D0AF78u" in packet_source
assert "within_data(descriptor,16u+sizeof(type_name))" in packet_source
assert "within_rdata(lrva,24u)" in packet_source
assert "cpu_vtable_rva=%08X" in packet_source
assert "payload_vtable_rva == 0x014AA4A8u" in packet_source
assert "g_cpu_tex2d_fields_readable.fetch_add" in packet_source
assert "readable_range(fields_address,sizeof(managed_fields))" in packet_source
assert "p28_texture_snapshot=%p" in packet_source
assert "p30_srv_snapshot=%p" in packet_source
assert "tex2d_both_nonnull=%llu" in resource
assert "struct native_named_source" in packet_source
assert "std::array<native_named_source,1024u>" in packet_source
assert "published_srv.store(" in packet_source
assert "std::memory_order_release" in packet_source
assert "published_srv.load(std::memory_order_acquire)" in packet_source
assert "diagnose_native_ps_t1(" in packet_source
assert "texture_identity_transport::diagnose_native_ps_t1(" in resource
# The consumer comparison must live on the active exact-material draw
# path, not solely inside dormant late-native-name recovery.
active_cut = resource.split(
    "bool material_resource_draw_runtime::\nprepare_draw_requests(", 1
)[1].split(
    "bool material_resource_draw_runtime::\nprepare_draw_requests_bound(", 1
)[0]
assert "stage=active_receiver_prebind" in active_cut
assert "receiver_id >= 24u && receiver_id <= 35u" in active_cut
assert "should_sample_native_ps_t1(" in active_cut
assert "PSGetShaderResources(" in active_cut
assert "GetResource(&stock_texture)" in active_cut
assert "texture_identity_transport::diagnose_native_ps_t1(" in active_cut
assert "stock_texture->Release();" in active_cut
assert "g_quarantined.load()" in active_cut
assert "should_sample_native_ps_t1(" in packet_source
assert "k_native_t1_seen_capacity = 8192u" in packet_source
assert "g_native_t1_seen[(start + probe) & k_native_t1_seen_mask]" in packet_source
assert "retire_native_ps_t1(const void *native_view)" in packet_source
assert "writer_snapshots_tombstoned=%u" in packet_source
assert "slot.published_srv.compare_exchange_strong(" in packet_source
assert "texture_identity_transport::retire_native_ps_t1(" in resource
assert "void on_destroy_resource_view(" in resource
assert "g_native_t1_seen_rearmed.fetch_add(" in packet_source
assert "cross_epoch=UNVERIFIED srv_swap=0 pixel=OPEN" in packet_source
assert "bridge_authority=0 srv_swap=0 pixel=OPEN" in packet_source
assert "matching_srv_and_texture=%u" in packet_source
assert "cross_epoch_lifetime=OPEN reuse_collision=UNVERIFIED" in packet_source
assert "bridge_authority=0 srv_swap=0 pixel=OPEN" in packet_source
assert "source_to_srv=UNVERIFIED pixel=OPEN" in packet_source
assert "typed decoder QUARANTINED" in packet_source
assert "cpu_rtti_exact=%llu cpu_rtti_unavailable=%llu" in resource
assert "cache_name_exact=%llu" in resource
assert "decode_writer_ptr_seen=%llu" in resource
assert "stage=typed_decoder_input" in packet_source
assert "mov qword ptr [rsp+8], rbx" in decoder_asm
assert "mov qword ptr [rsp+10h], rsi" in decoder_asm
assert "jmp qword ptr [g_dsrrl_spc25_reader_resume]" in decoder_asm

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
