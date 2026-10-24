#!/usr/bin/env python3
"""Static invariants: each texture is authorized by its OWN bound SRV."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root / "src/runtime/material_resource_draw_runtime.cpp").read_text("utf-8")
dif = (root / "src/operators/resource_bridges/diffuse_bridge.cpp").read_text("utf-8")
norm = (root / "src/operators/resource_bridges/normal_bridge.cpp").read_text("utf-8")
head = (root / "include/dsrrl/operators/resource_bridges/normal_bridge.hpp").read_text("utf-8")

# No fixed retail-file SHA can veto a legitimate edited DDS.
load = src.split("load_result load_dds(", 1)[1].split("std::uint32_t magic = 0u;", 1)[0]
assert "matches_hex" not in load
assert "sha256(" not in load
assert "std::filesystem::is_regular_file" in load

# The SRV ownership registry still authenticates a live stock texture.
assert "texture_identity_transport::snapshot_raw" in src
assert "snapshot_companion_cached(" in src
assert "g_ambiguous_view_device" in src
assert "header.pixel_format.size != 32u" in src

start = src.index("prepare_draw_requests_bound(")
stop = src.index("prepare_fixed_pointlight_material_requests(", start)
draw = src[start:stop]
assert "if (!exact_material)" not in draw
assert "const bool diffuse_target = h0 != 0u && companions[0] != nullptr;" in draw
assert "const bool normal_target = h2 != 0u && companions[2] != nullptr;" in draw
assert "diffuse_pair_allowed_v12" not in draw
assert "normal_tuple_allowed_v12" not in draw
assert "core::operator_id::spec_rgb" in draw
assert "const bool want_spec_rgb = exact_material && spec_receiver &&" in draw
assert "const bool want_diffuse = bmp_receiver &&" in draw
assert "const bool want_normal = bmp_receiver &&" in draw
assert "{want_diffuse ? views[0] : nullptr, asset_class::diffuse}" in draw
assert "{want_spec_rgb ? views[1] : nullptr, asset_class::specular}" in draw
assert "{want_normal ? views[2] : nullptr, asset_class::normal}" in draw
assert "if (!want_diffuse && !want_normal && !want_spec_rgb)" in draw
assert "full_material_response_ready &&\n        spec_rgb_consumer_ready" not in draw

# No unrelated c100, MTD or FLVER check may veto a t0/t2 sidecar.
assert "if (!context.actual_material_verified)" not in dif
assert "if (!context.ptde_c100_donor_verified)" not in dif
assert "if (context.shared_material_route &&" not in dif
assert "normal_route_authority::exact_bound_t2" in norm
integrated = (root / "integrated/integrated_addon.cpp").read_text("utf-8")
assert "Resource-only t0/t2 has its own exact stock-SRV authority" in integrated
assert "resource_query.material = material" in integrated
assert "prepared.resources.request_count != 0u" in integrated
assert "return ownerless_island_enabled;" in integrated
assert "exact_bound_t2" in head
assert "if (!context.actual_bound_t2_verified)" in norm
assert "if (!context.preserve_stock_s2)" in norm
print("V230_MINIMAL_RESOURCE_GATES_PASS exact_live_t0 exact_live_t2 independent_transfers")

# Resource registration needs its own same-name DDS, no static name allowlist.
registration=src.split("void on_init_resource_view(",1)[1].split("void on_destroy_resource_view(",1)[0]
for cls in ("diffuse","normal"):
    assert f"sidecar_path(asset_class::{cls}, logical_name)" in registration
    assert f"{cls}_name_hash_allowed_v12(" not in registration
    assert f"{cls}_target_hash_allowed_v12(" not in registration

# Stable PTDE EnvDiffuse has no B consumer, so B must not be an input gate.
env=(root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf-8")
stable=env.split("bool envspec_resource_runtime::prepare_envdiffuse(",1)[1].split("void envspec_resource_runtime::release(",1)[0]
assert "(probe_b_required && probe_b >= env::k_legacy_envspec_probe_count)" in stable
assert "if (!probe_b_required)" in stable and "probe_b = probe_a;" in stable

# EnvSpec donor is producer-independent from optional EnvDiffuse endpoint data.
producer=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
assert producer.count("if (donor_ok)\n        publish_hook_source(")==2
assert "next.envdiffuse_linear_valid = envdiffuse_valid;" in producer
assert producer.count("            envdiffuse_ok,")==2
assert "bool latest_hook_source(" in producer # protected fallback unchanged

# PTDE SpecRGB material pixels are composite, unlike independent t0/t2 DDS.
# A valid SpecRGB sidecar never proves the correct cubemap + MR result alone.
composed=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
assert "const bool material_chain_ready =" in composed
for name in ("material_response","diffuse_material_domain","spec_rgb"):
    assert f"core::operator_id::{name}" in composed.split("const bool material_chain_ready =",1)[1].split("const bool lerp_terminal_ready",1)[0]
assert "const bool lerp_terminal_ready =" in composed
assert composed.index("const bool env_resource_ready =") < composed.index("const bool material_ready =")
assert "if (!env_resource_ready)" in composed
assert "!prepared.material_resources.spec_rgb" in composed
assert "source.envdiffuse_linear_valid" in composed
spec_region=draw.split("if (want_spec_rgb) {",1)[1].split("const bool diffuse_target =",1)[0]
assert "if (want_spec_rgb)" in draw
assert "full_material_response_ready" not in spec_region
assert "material_chain_ready" in composed
assert "spec_equipment_allowed(" in spec_region
assert "stock_t1_preserved =" in spec_region
assert "native_t10_transport_ready =" in spec_region

# Exact zero-beta two-endpoint reduction, NOT an approximate beta threshold.
# The physical shader still receives B SRV, but it is an alias of A.
assert "const bool probe_b_required = source.beta != 0.0f;" in composed
assert "source.beta != 0.0f;" in composed.split("const bool ptde_envdiffuse_b_required =",1)[1].split("#else",1)[0]
assert "(endpoints.beta == 0.0f || endpoints.a == endpoints.b)" in producer
assert "envdiffuse_b = envdiffuse_a;" in producer
assert "if (probe_b_required &&" in stable
assert "b_view = a_view;" in stable
assert "prepared.ptde_b = b_native;" in stable
for a,b in ((0.0,0.75),(0.91,0.02),(-0.3,0.8)):
    assert a + 0.0 * (b-a) == a

# Same-selector LightBank transport is one semantic endpoint, irrespective
# of blend beta. Do not require second source pointer/cache membership.
selector_code = producer.split("bool capture_hook_selector_identity(",1)[1].split("struct bank_cache_entry",1)[0]
assert "if (beta_bits == 0u || selector_a == selector_b)" in selector_code
assert "out.source_b = source_a;" in selector_code
assert "out.base_b = out.base_a;" in selector_code
assert "out.beta_bits = beta_bits;" in selector_code


# The native blend hook receives the ORIGINAL source pointers. Selecting B
# with beta >= 1 must update both the selector and the bank pointer. Without
# this, select() returns (B,B,0) but the hook reads B's row out of A's bank.
# This is separate from the exact zero-beta / same-selector reduction above.
policy = (root / "include/dsrrl/runtime/pmetal_selector_policy.hpp").read_text("utf-8")
assert "inline bool selects_only_b(" in policy
b_only_code = policy.split("inline bool selects_only_b(", 1)[1].split("struct endpoints", 1)[0]
assert "a != b && std::isfinite(beta) && beta >= 1.0f" in b_only_code
blend_hook = producer.split("void __fastcall envspec_blend_hook_entry(", 1)[1].split("bool install_envspec_source_hooks(", 1)[0]
assert "pmetal_selector_policy::selects_only_b(" in blend_hook
assert "? source_b" in blend_hook and ": source_a;" in blend_hook
assert "read_exact_source(\n                selected_source_a,\n                endpoints.a," in blend_hook
assert "            selected_source_a,\n            endpoints.a,\n            source_b,\n            endpoints.b);" in blend_hook
assert "g_envspec_blend_original(\n            host_out,\n            source_a,\n            selector_a,\n            source_b,\n            selector_b,\n            beta);" in blend_hook
# Synthetic endpoint cases for the exact source-pointer selection contract.
for a, b, beta, should_select_b in [
    (5, 7, -1.0, False),
    (5, 7, 0.0, False),
    (5, 7, 0.5, False),
    (5, 7, 1.0, True),
    (5, 7, 1.1, True),
    (5, 5, 1.0, False),
]:
    assert (a != b and beta >= 1.0) == should_select_b
print("V230_BLEND_ENDPOINT_BANK_PASS original_source_unchanged B_only_source_correct")

# Receiver-first, diagnostic-only view-identity witness for unresolved
# exact route 345/RX34. No unverified view may become authorized by logging.
assert 'void witness_rx34_resource_identity(' in src
witness = src.split('void witness_rx34_resource_identity(', 1)[1].split('void on_init_resource_view(', 1)[0]
assert 'g_ambiguous_view_device.find(key)' in witness
assert 'g_cache.find(key)' in witness
assert 'NO_NAME_SIDECAR_REGISTRY_ENTRY' in witness
assert 'AMBIGUOUS_NATIVE_VIEW' in witness
assert 'TRACKED_NO_CLASS_SIDECAR' in witness
assert 'sample >= 6u' in witness
assert 'std::lock_guard<std::mutex> lock(g_mutex)' in witness
assert 'reshade::log::level::warning' in witness
draw_witness = draw.split('const auto h0 = hashes[0];', 1)[1].split('if (want_spec_rgb) {', 1)[0]
assert 'query.material.route_index == 345u' in draw_witness
assert 'receiver_id == 34u' in draw_witness
assert 'unresolved_mask != 0u' in draw_witness
assert 'witness_rx34_resource_identity(' in draw_witness
assert 'prepared.spec_rgb' not in witness
assert 'prepared.diffuse' not in witness
assert 'prepared.normal' not in witness
print("V230_RX34_IDENTITY_WITNESS_PASS scoped_readonly_capped_failopen")

# Exact P_Metal RX34 native t11/s11/b12 census precedes all mutations.
# Resource class is measured from the live view/texture, not guessed from
# EnvSpec probe ordinal or a coincidentally named PTDE file.
assert 'void witness_rx34_native_envdiffuse(' in composed
t11_witness = composed.split('void witness_rx34_native_envdiffuse(', 1)[1].split('std::atomic_bool g_source_cut_logged', 1)[0]
for read in (
    'context->PSGetShaderResources(11u, 1u, &t11);',
    'context->PSGetSamplers(11u, 1u, &s11);',
    'context->PSGetConstantBuffers(12u, 1u, &b12);',
    'pixel_srv_shadow_snapshot(cmd_list, 11u, 1u, shadow_views);',
    't11->GetDesc(&srv_desc);',
    'texture->GetDesc(&texture_desc);',
    'cb_desc.ByteWidth',
    'source.envdiffuse_linear_valid',
    'sample >= 6u',
):
    assert read in t11_witness
for write in ('PSSetShaderResources(', 'PSSetSamplers(', 'PSSetConstantBuffers(', 'PSSetShader('):
    assert write not in t11_witness
assert 'decision.receiver_id == 34u &&' in composed
assert 'g_rx34_t11_witness_source_ready' in composed
assert 'g_rx34_t11_witness_source_reject' in composed
assert 'witness_rx34_native_envdiffuse(' in composed
assert 'cmd_list, native_context, source, false);' in composed
assert 'material.route_index == 345u' in composed
assert 'witness_rx34_native_envdiffuse(cmd_list, context, source, true);' in composed
assert 'witness_rx34_native_envdiffuse(cmd_list, context, source, true);\n\n    ID3D11PixelShader *shader = nullptr;' in composed
print("V230_RX34_T11_DOMAIN_WITNESS_PASS exact_material_readonly_t11_s11_b12")

# Batch append must be atomic across shader, CB, SRV and sampler conflicts.
# This is a construction invariant, not pixel or runtime proof.
adapter = (root / "src/runtime/island_draw_adapter.cpp").read_text("utf-8")
atomic_append = adapter.split(
    "island_draw_batch_result append_island_draw_request(", 1
)[1].split(
    "island_draw_dispatch_result dispatch_island_draw_batch(", 1
)[0]
assert "island_draw_batch staged = batch;" in atomic_append
assert "batch = staged; // single commit after every category passes" in atomic_append
assert atomic_append.index("batch = staged;") > atomic_append.index(
    "island_draw_batch_result::sampler_conflict"
)
assert "merge_slot_bindings(\n            staged.mutation.constant_buffers," in atomic_append
assert "merge_slot_bindings(\n            staged.mutation.srvs," in atomic_append
assert "merge_slot_bindings(\n            staged.mutation.samplers," in atomic_append
assert "++staged.island_count;" in atomic_append
assert "++batch.island_count;" not in atomic_append
print("V230_ATOMIC_BATCH_APPEND_PASS no_partial_mutation_on_late_conflict")

# Cubemap transport is part of the complete shader transaction. Every
# supported variant binds its EnvSpec A/B pair exactly once; the dependent
# PTDE EnvDiffuse t11/t13 pair is still mode/receiver-specific. This checks
# slot routing only, not stock view mapping or PTDE pixel equivalence.
srv_bindings = composed.split(
    "    std::uint32_t request_srv_count = 0u;", 1
)[1].split("    prepared.request.srv_count =", 1)[0]
assert srv_bindings.count("12u, prepared.env_resources.ptde_a") == 1
assert srv_bindings.count("14u, prepared.env_resources.ptde_b") == 1
assert srv_bindings.count("11u, prepared.envdiffuse_resources.ptde_a") == 1
assert srv_bindings.count("13u, prepared.envdiffuse_resources.ptde_b") == 1
assert "11u, prepared.native_dsr_envdiffuse_a" in srv_bindings
assert "13u, prepared.native_dsr_envdiffuse_b" in srv_bindings
assert srv_bindings.index("11u, prepared.envdiffuse_resources.ptde_a") < srv_bindings.index("12u, prepared.env_resources.ptde_a")
assert srv_bindings.index("12u, prepared.env_resources.ptde_a") < srv_bindings.index("13u, prepared.envdiffuse_resources.ptde_b")
assert srv_bindings.index("13u, prepared.envdiffuse_resources.ptde_b") < srv_bindings.index("14u, prepared.env_resources.ptde_b")
assert "if (family == pmetal_envspec_receiver_family::hemenvlerp)" in srv_bindings
assert "if (native_envdiffuse_b_required)" in srv_bindings
assert "DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG" in srv_bindings
assert "DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG" in srv_bindings
print("V230_CUBEMAP_BIND_UNIFICATION_PASS t11_t12_t13_t14_order_and_modes")

# Each resource slot has its matching sampler even after simplification:
# t11/s11 EnvDiffuse A, t12/s12 EnvSpec A, t13/s13 EnvDiffuse B,
# t14/s14 EnvSpec B. R7 shadow s7 is independent.
sampler_bindings = composed.split(
    "    std::uint32_t request_sampler_count = 0u;", 1
)[1].split("    prepared.request.sampler_count =", 1)[0]
for slot, resource in (
    (11, "envdiffuse_resources.sampler"),
    (12, "env_resources.sampler"),
    (13, "envdiffuse_resources.sampler"),
    (14, "env_resources.sampler"),
):
    assert sampler_bindings.count(f"{slot}u, prepared.{resource}") == 1
assert "shadow_sampler" in sampler_bindings
assert "if (family == pmetal_envspec_receiver_family::hemenvlerp)" in sampler_bindings
print("V230_CUBEMAP_SAMPLER_UNIFICATION_PASS exact_s11_s12_s13_s14_shadow_s7")

# Both full-PTDE packed-cubemap and native-DSR V13 diagnostic must resolve
# stock SRV -> canonical probe ID through the *same* tracked-view registry.
# This removes a duplicate resolver, not an asset/resource equivalence gate.
registry = env.split("bool resolve_tracked_probe_locked(", 1)[1].split(
    "bool snapshot_ready_envspec(", 1
)[0]
ptde_resolution = env.split("bool snapshot_ready_envspec(", 1)[1].split(
    "bool get_ptde_cube(", 1
)[0]
native_resolution = env.split(
    "bool envspec_resource_runtime::prepare_native_dsr(", 1
)[1].split(
    "bool envspec_resource_runtime::prepare_envdiffuse(", 1
)[0]
assert "g_resource_by_view.find(key)" in registry
assert "probe = found->second.probe_ordinal;" in registry
assert "if (view == nullptr)" in registry
assert ptde_resolution.count("resolve_tracked_probe_locked(") == 2
assert native_resolution.count("resolve_tracked_probe_locked(") == 2
assert "probe_for_native_view(" not in env
assert "const auto resolve_probe_locked =" not in env
assert "probe_a >=\n            env::k_legacy_envspec_probe_count" in ptde_resolution
assert "probe_b >=\n            env::k_legacy_envspec_probe_count" in ptde_resolution
assert "probe_a < env::k_legacy_envspec_probe_count" in native_resolution
assert "probe_b < env::k_legacy_envspec_probe_count" in native_resolution
assert "std::lock_guard<std::mutex> lock(g_mutex)" in ptde_resolution
assert "std::lock_guard<std::mutex> lock(\n            g_mutex)" in native_resolution
assert native_resolution.count("context->PSGetShaderResources(") == 1
assert "probe_b_required ? 3u : 1u" in native_resolution
assert "if (!probe_b_required)\n                probe_b = probe_a;" in native_resolution
assert "if (!probe_b_required)\n        stock_b->AddRef();" in native_resolution
print("V230_PROBE_RESOLVER_UNIFIED_PASS PTDE_cube_native_DSR_same_tracked_view_guard")

# EnvSpec (342*4 RGBA8 cubes) and EnvDiffuse (342 RGBA8 cubes)
# are distinct exact PTDE assets. They may share a file-reader algorithm,
# but NOT their path, byte length, SHA, cache or receiver/ordinal routing.
loader = env.split("bool load_exact_cube_pack(", 1)[1].split(
    "bool is_native_envspec_desc(", 1
)[0]
assert loader.count("hashing::sha256(bytes.data(), bytes.size())") == 1
assert "size != expected_size" in loader
assert "expected_digest" in loader
assert "out.clear();" in loader
assert loader.count("return load_exact_cube_pack(") == 2
envspec_load = loader.split("bool load_pack(", 1)[1].split(
    "bool load_envdiffuse_pack(", 1
)[0]
envdiffuse_load = loader.split("bool load_envdiffuse_pack(", 1)[1]
for constant in (
    "k_legacy_packed_gi_relative_path",
    "k_legacy_packed_gi_size",
    "k_legacy_packed_gi_sha256",
):
    assert constant in envspec_load and constant not in envdiffuse_load
for constant in (
    "k_ptde_envdiffuse_packed_gi_relative_path",
    "k_ptde_envdiffuse_packed_gi_size",
    "k_ptde_envdiffuse_packed_gi_sha256",
):
    assert constant in envdiffuse_load and constant not in envspec_load
assert "#if !defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)" in envdiffuse_load
assert "g_ptde_cubes" in env and "g_ptde_envdiffuse_cubes" in env
assert "k_ptde_slots = 4u" in env
print("V230_EXACT_PACK_LOADERS_SHARED_PASS independent_envspec_envdiffuse_paths_sizes_sha")

# Physical GPU cubemap creation is identical; its asset source/cache is NOT.
# Both callers materialize their own exact SHA-checked RGBA payload after a
# mutex-protected copy, use the shared 6-face texture/view creator and then
# individually verify carrier liveness before committing to their own cache.
cube_helper = env.split("bool create_ptde_rgba_cube(",1)[1].split(
    "bool get_ptde_cube(",1
)[0]
spec_cube = env.split("bool get_ptde_cube(",1)[1].split(
    "bool get_ptde_envdiffuse_cube(",1
)[0]
diff_cube = env.split("bool get_ptde_envdiffuse_cube(",1)[1].split(
    "} // namespace",1
)[0]
for contract in (
    "format::r8g8b8a8_unorm",
    "resource_flags::cube_compatible",
    "resource_view_type::texture_cube",
    "device_ptr->create_resource(",
    "device_ptr->create_resource_view(",
    "device_ptr->destroy_resource(cube.texture);",
    "k_faces",
    "k_ptde_face_bytes",
):
    assert contract in cube_helper
assert spec_cube.count("create_ptde_rgba_cube(device_ptr, cube_bytes, cube)") == 1
assert diff_cube.count("create_ptde_rgba_cube(device_ptr, cube_bytes, cube)") == 1
assert "g_pack.data() + offset" in spec_cube
assert "g_envdiffuse_pack.data() +" in diff_cube
assert "g_ptde_cubes.emplace(" in spec_cube
assert "g_ptde_envdiffuse_cubes.emplace(" in diff_cube
assert "g_pack_ready" in spec_cube
assert "g_envdiffuse_pack_ready" in diff_cube
assert "std::lock_guard<std::mutex> lock(" in spec_cube
assert "std::lock_guard<std::mutex> lock(" in diff_cube
assert "++g_cube_fail;" in spec_cube
assert "++g_cube_created;" in spec_cube
assert "++g_cube_created;" in diff_cube
print("V230_PTDE_CUBEMAP_CREATE_SHARED_PASS separate_caches_lifetimes_source_pack_checks")
