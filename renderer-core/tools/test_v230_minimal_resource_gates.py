#!/usr/bin/env python3
"""Static invariants: each texture is authorized by its OWN bound SRV."""
from pathlib import Path
import re
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
    "bool probe_for_native_view(", 1
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
# V359 retains its original source-read ABI, now a *single*
# forwarding shim to the shared exact registry resolver.
assert env.count("bool probe_for_native_view(") == 1
v359_forwarder = env.split("bool probe_for_native_view(",1)[1].split(
    "bool get_ptde_cube(",1
)[0]
assert "std::lock_guard<std::mutex> lock(g_mutex)" in v359_forwarder
assert "return resolve_tracked_probe_locked(view,probe,true);" in v359_forwarder
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
# The independent native-t11 SHA atlas has its OWN verified source hash;
# count only the shared exact PTDE pack-reader hash, not the atlas loader.
pack_reader = loader.split("bool load_pack(", 1)[0]
assert pack_reader.count("hashing::sha256(bytes.data(), bytes.size())") == 1
assert "load_native_diffuse_hash_atlas(" in loader
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

# V356 remains the authoritative SOURCE materializer of the exact
# SHA-certified native BC6H backing alias. The V230 shared resolver must
# receive that same V356 implementation; eliminating the orphan old helper
# must NEVER eliminate its active build-time authorization path.
v356_materializer = (root / "tools/materialize_v356_exact_probe_backing.py").read_text("utf-8")
unified_v356 = v356_materializer.split(
    'if "bool resolve_tracked_probe_locked(" in s:', 1
)[1].split('raise SystemExit(0)', 1)[0]
assert "once(unified_anchor,impl+unified_anchor" in unified_v356
assert "v356_resolve_exact_native_backing_locked(view,probe)" in unified_v356
assert "DXGI_FORMAT_BC6H_UF16" in v356_materializer
assert "DXGI_FORMAT_BC6H_SF16" in v356_materializer
assert "D3D11_SRV_DIMENSION_TEXTURECUBE" in v356_materializer
assert "g_native_resources.find(" in v356_materializer
assert "same_device" in v356_materializer
assert 'once(anchor,impl+anchor,"helper")' in v356_materializer
assert 'once(before,after,"probe_for_native_view")' in v356_materializer
print("V230_V356_ALIAS_MATERIALIZER_COMPAT_PASS exact_resource_backing_still_in_unified_resolver")

# No authority escalation: V356 permitted same-backing rescue for the PTDE
# PackedGI materializer, NOT the old native V13 falsifier. Shared lookup
# mechanically routes both but explicitly marks the original receiver scope.
assert "bool allow_verified_alias) noexcept" in registry
assert ptde_resolution.count("probe_a,\n            true))") == 1
assert ptde_resolution.count("probe_b,\n                true))") == 1
assert native_resolution.count("probe_a,\n                false) &&") == 1
assert native_resolution.count("probe_b,\n                 false) &&") == 1
assert "if (allow_verified_alias)\n        return v356_resolve_exact_native_backing_locked(view,probe);" in unified_v356
print("V230_V356_ALIAS_SCOPE_PASS ptde_only_native_v13_stays_strict")

# Runtime witness in the actual V230 one-addon profile (not build-only
# telemetry): capped RX33-35 x Stable/Lerp, no D3D state reads/writes,
# records request-ready / source-probe fail-open / native arm / replay
# issued / restore failure. Warning-level logs survive V230 INFO stripping.
integrated_rt = (root/"integrated/integrated_addon.cpp").read_text("utf-8")
pmetal_rt = (root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
build_rt = (root/"tools/build_v230_production.ps1").read_text("utf-8")
hot_audit = (root/"tools/audit_active_islands_hotpath.py").read_text("utf-8")
assert "/DDSRRL_V230_RUNTIME_TELEMETRY" in build_rt
assert "DSRRL_V230_RUNTIME_TELEMETRY" in integrated_rt
assert "DSRRL_V230_RUNTIME_TELEMETRY" in pmetal_rt
probe_rt = integrated_rt.split("void v230_rt_probe_once(",1)[1].split(
    "bool prepare_island_batch(",1
)[0]
assert "rx < 33u || rx > 35u || route != 345u" in probe_rt
assert "g_v230_rt_probe_seen.fetch_or(" in probe_rt
assert "const unsigned index = (rx - 33u) * 16u +" in probe_rt
assert "const std::uint64_t bit = 1ull << index;" in probe_rt
assert "envspec_resources" not in probe_rt or "pack_envspec" in probe_rt
assert "prepared->env_resources.ready" in probe_rt
assert "prepared->envdiffuse_resources.ready" in probe_rt
assert "prepared->material_resources.spec_rgb" in probe_rt
assert "srv_mask" in probe_rt and "sampler_mask" in probe_rt
assert "[DSRRL V230 RUNTIME]" in probe_rt
assert "pixel_pass=0" in probe_rt
for forbidden in ("PSGet", "PSSet", "CreatePixelShader", "DrawIndexed("):
    assert forbidden not in probe_rt
assert 'v230_rt_probe_once(0u,"prepare_reject"' in integrated_rt
assert 'v230_rt_probe_once(1u,"batch_ready"' in integrated_rt
assert integrated_rt.count('v230_rt_probe_once(2u,"native_armed"') == 2
assert integrated_rt.count('v230_rt_probe_once(7u,"restore_failed"') == 2
assert '"draw_issued"' in integrated_rt
assert '"indexed_issued"' in integrated_rt
assert "dsrrl::runtime::draw_tx_issued(dispatch.transaction)" in integrated_rt
assert "dsrrl::runtime::draw_tx_result::issued_restore_failed" in integrated_rt
assert "probe_resolver_boundary" in hot_audit
print("V230_RUNTIME_TELEMETRY_WIRING_PASS source_scope_native_one_addon_bounded_failopen_restore")

# Native-current P_Metal does NOT use normal replay callback for all paths.
# Arm is merely tentative. Verify four actual D3D11 original draws and
# restoration outcomes, logging only AFTER the original invocation and
# only when the pending mutation includes the EnvSpec operator.
native_draw_rt = (root/"src/runtime/pmetal_native_draw_bridge.cpp").read_text("utf-8")
runtime_body = native_draw_rt.split("void v230_native_rt_once(",1)[1].split(
    "void retain_mutation(",1
)[0]
assert "core::operator_id::env_spec" in runtime_body
assert "g_v230_native_rt_seen.fetch_or(" in runtime_body
assert "pixel_pass=0" in runtime_body
assert "original_draw_reached=%u restored=%u" in runtime_body
for draw, lane in (
    ("draw",0),
    ("draw_indexed",1),
    ("draw_instanced",2),
    ("draw_indexed_instanced",3),
):
    fn="pmetal_native_draw_bridge::"+draw+"_hook("
    assert fn in native_draw_rt
    region=native_draw_rt.split(fn,1)[1]
    # A second method cannot inadvertently satisfy the checks.
    region=region.split("release_pending_draw(\n        pending);\n}",1)[0]
    for outcome, stage in ((0,"capture_fail_stock"),(1,"restore_failed_quarantine"),(2,"draw_applied_restored")):
        assert f'v230_native_rt_once({lane}u,{outcome}u,"{draw}"' in region
        assert f'"{stage}",pending.mutation)' in region
    assert "original(" in region and "restore_state(" in region
assert native_draw_rt.count("v230_native_rt_once(") == 13
print("V230_NATIVE_DRAW_POST_RESTORE_WITNESS_PASS 4_paths_no_arm_as_pass")

# Exact SRV lifecycle diagnostic does not authorize a guessed sidecar.
# A shader-resource usage bit may be combined with other ReShade usage flags.
v230_resources = (root/"src/runtime/material_resource_draw_runtime.cpp").read_text("utf-8")
assert "enum class v230_view_stage" in v230_resources
assert "k_v230_view_origin_max = 16384u" in v230_resources
assert "texture_identity_transport::snapshot_raw(" in v230_resources
assert "v230_view_stage::no_name_scope" in v230_resources
assert "v230_view_stage::no_target_sidecar" in v230_resources
assert "v230_view_stage::conflicting_view_identity" in v230_resources
assert "reshade::api::resource_usage::undefined" in v230_resources
assert "usage !=\n            reshade::api::resource_usage::shader_resource" not in v230_resources
print("V230_RESOURCE_VIEW_LIFECYCLE_DIAG_SOURCE_PASS exact_only_no_alias")

# Exercise the complete V323 overlay against the current V230 source, not
# just the generator anchors. Reject drift before native Windows build.
import importlib.util
v323_spec = importlib.util.spec_from_file_location(
    "v230_v323_overlay", root / "tools/apply_v323_exact_resource_name_srv.py"
)
v323_module = importlib.util.module_from_spec(v323_spec)
v323_spec.loader.exec_module(v323_module)
generated_v323 = v323_module.patch(v230_resources)
assert "v214_resource_provenance::instance().snapshot(" in generated_v323
assert "v230_view_stage::no_name_scope" in generated_v323
assert "v230_view_origin origin{}" in generated_v323
assert "view_init=%s backing=%016llx" in generated_v323
assert "seen.begin()+seen_count" in generated_v323
assert "source_resource.handle" in generated_v323
assert "resource_handle.handle" not in generated_v323.split(
    "void on_init_resource_view(",1
)[1].split("void on_destroy_resource_view(",1)[0]
assert "v214_resource_provenance::status::ambiguous" in generated_v323
assert "resource_exact" in generated_v323
assert "g_cache.emplace(" in generated_v323
print("V230_V323_EXECUTED_GENERATOR_BRIDGE_PASS same_native_resource_scope_and_unique_rx34")

assert '[DSRRL V230 PROBE FAIL CUT]' in pmetal_rt
print('V230_PROBE_FAIL_WITNESS_PASS')

# Sidecar directory selection is a *filesystem* transport detail, not an
# additional name/MTD/receiver permission. No recursive or fuzzy mapping.
sidecar_path_fn = v230_resources.split(
    "std::filesystem::path sidecar_path(", 1
)[1].split("template <typename T>", 1)[0]
assert "const auto canonical = base / class_dir(cls) / filename;" in sidecar_path_fn
assert "std::filesystem::exists(canonical, ec) || ec" in sidecar_path_fn
for alternative in ('L"Speculars"', 'L"Normal"', 'L"Diffuses"'):
    assert alternative in sidecar_path_fn
assert "std::filesystem::is_regular_file(exact_alternative, ec)" in sidecar_path_fn
assert "recursive_directory_iterator" not in sidecar_path_fn
assert "return canonical;" in sidecar_path_fn
assert "spec_equipment_allowed(logical_hash)" in v230_resources
assert "texture_identity_transport::snapshot_raw(" in v230_resources
print("V230_EXACT_FOLDER_MANIFEST_FALLBACK_PASS original_stock_name_and_protect")

assert "spec_disk_present = std::filesystem::is_regular_file(" in v230_resources
assert "v230_disk_mask" in v230_resources
assert "disk_mask=%u" in generated_v323
assert "static_cast<unsigned>(origin.disk_mask)" in generated_v323
print("V230_INSTALLED_DDS_VS_ALLOWLIST_WITNESS_PASS")

# Source owner class must win over texture-name substrings. Genuine owner
# reverse join: c2500_wp_a_0200_s => chr/c2500.chrbnd, whereas
# wp_a_0200_s belongs independently to parts/WP_A_0200.partsbnd.
char_source = "0x236adf2eb0a2afccull"
assert char_source in v230_resources
assert "v230_observed_character_t1" in v230_resources
# Full exact bound-t1 source census (2026-10-24), not name-prefix guessing.
# Negative classifier is the 13 source-joined chr/chrbnd identities ONLY;
# 11 observed player parts/partsbnd t1 are never put in this list.
chr_switch = v230_resources.split(
    "inline bool v230_observed_character_t1(", 1
)[1].split("enum class asset_class", 1)[0]
negative_t1 = set(re.findall(r"case (0x[0-9a-f]{16}ull):", chr_switch))
source_confirmed_chr_t1 = {"0x03f695a24f8f9be0ull", "0x236adf2eb0a2afccull", "0x2d2fc5a100f0b05dull", "0x370ad6a106dc1480ull", "0x4afb32a9129e6896ull", "0x6f6c845ad6e9c1d4ull", "0x82f8f87655320badull", "0x8629e77c1b231175ull", "0x8f94806ba5d2be1bull", "0x903b214ffd5bdb01ull", "0xa283bf761630b10bull", "0xfe12c9d08265290aull", "0xfe2aa6373d1213faull"}
source_confirmed_parts_t1 = {"0x409676c83f3dbf70ull", "0x47e4afc842fe2f4bull", "0x644555ce9c04b58cull", "0x6ce2d3ff65aa3dd2ull", "0x7ddcf9d737353ea7ull", "0x877dac9e867b292dull", "0x96ea43ceb853c1f6ull", "0x9ff70f9e93ea9ca6ull", "0xc916b4715f9e5422ull", "0xdc1dd5dff91bfeeaull", "0xf9f4e98643b0c3c7ull"}
assert negative_t1 == source_confirmed_chr_t1
assert len(source_confirmed_parts_t1) == 11
assert not (negative_t1 & source_confirmed_parts_t1)

assert "verified_character_not_equipment" in v230_resources
assert "VERIFIED_CHR_NOT_PLAYER_EQUIPMENT" in v230_resources
assert "v230_view_stage::verified_character_not_equipment" in v230_resources
assert "prepared.verified_character_not_equipment = true;" in v230_resources
# Guard must stay effective in builds with runtime telemetry compiled out.
# Only a proven native (view, device) lifetime may exclude this equipment draw.
assert "g_v230_character_view_device" in v230_resources
assert "k_v230_character_views_max" in v230_resources
view_init = v230_resources.split("void on_init_resource_view(", 1)[1].split(
    "void on_destroy_resource_view(", 1
)[0]
assert "g_v230_character_view_device[key] = native;" in view_init
assert "g_v230_character_view_device.erase(" in view_init
assert view_init.index("g_v230_character_view_device.erase(") < view_init.index(
    "texture_identity_transport::snapshot_raw(")
# If a new exact stock view reuses a retired pointer but has no loader TLS,
# the previous chr classification must already have been discarded.
view_destroy = v230_resources.split("void on_destroy_resource_view(", 1)[1].split(
    "void on_destroy_device(", 1
)[0]
assert "g_v230_character_view_device.erase(key)" in view_destroy
assert "g_v230_view_origin.erase(key)" in view_destroy
assert "g_v230_character_view_device.clear();" in v230_resources
assert "it = g_v230_character_view_device.erase(it);" in v230_resources
prepare_negative = v230_resources.split(
    "// Certified negative chr owner view:", 1
)[1].split("const companion_lookup_request companion_requests", 1)[0]
assert "DSRRL_V230_RUNTIME_TELEMETRY" not in prepare_negative
assert "registered_device == bound_device" in prepare_negative
assert "bound_device->Release();" in prepare_negative
# prepare_draw_requests_bound accepts views, never a D3D context. Extract
# the whole native bound fast-path function rather than checking one text
# marker; this catches a regression hidden by superficial string tests.
bound_fn = v230_resources.split(
    "bool material_resource_draw_runtime::\nprepare_draw_requests_bound(", 1
)[1].split(
    "bool material_resource_draw_runtime::\nprepare_fixed_pointlight_material_requests(", 1
)[0]
assert "context->" not in bound_fn
assert "context !=" not in bound_fn
assert "views[1]->GetDevice(&bound_device);" in prepare_negative
assert "context->GetDevice(&bound_device);" not in prepare_negative
assert "prepared.verified_character_not_equipment = true;" in prepare_negative
assert "g_v230_view_origin.find(key)" not in prepare_negative
assert "if (prepared.material_resources.verified_character_not_equipment)" in pmetal_rt
assert '"character_not_equipment_stock"' in pmetal_rt
assert pmetal_rt.index("if (prepared.material_resources.verified_character_not_equipment)") < pmetal_rt.index("if (!material_ready ||")
assert "spec_equipment_allowed(logical_hash)" in v230_resources
print("V230_RX34_CHR_NOT_EQUIPMENT_PASS original_owner_negative_exact_stock")


# Warrior/Standard 9360 exact source first-cut witness (DIAGNOSTIC ONLY).
# Both Draw and DrawIndexed must observe the owner BEFORE the fast-return
# invalidates TLS; the full-SHA + slot-specific trace must be bounded.
warrior_owner = (root / "src/runtime/material_owner_selection.cpp").read_text("utf-8")
warrior_h = (root / "include/dsrrl/runtime/material_owner_selection.hpp").read_text("utf-8")
source_sha_9360 = {
    "1e1a21b2ca8d0ee05822edd8a898a0d337b2e04687a05887f119aef08857166f",
    "a115d89eb9f70265038c58cc3a48285c419013f4b6165b07cd1c9a172449fdee",
    "cf79f8450ddaa8f1c2a2ccea554a016daf84fb0427830849ce7d26592cb75989",
    "ab9df51c284b67f60f40be958403f7906b8fb492a158c6e9f10b944c6339da79",
    "59c937ced4aaf41ba3f6db4a351d47b41dabd176224ecca4123b72a11d23baee",
    "bfbef63e9611bd8b753f8197e5cabeabbf08ec400e098f2191561bdc5ce7396b",
    "dd2b106aab423417960fa96239eef69cab5b8c70ec795af32cc878386a3bfb1e",
    "ad7d888aa5caee9794d8ea5e39265df2c9d3c7c8fafcdf2d94ffeb22ab552413",
}
sha_vectors = re.findall(r'\{ "(?:AM|BD|HD|LG)_[FM]_9360", \{\{(.*?)\}\} \}', warrior_owner)
assert len(sha_vectors) == 8
recovered = set()
for vector in sha_vectors:
    data = [int(v.strip().removesuffix("u"), 16) for v in vector.split(",")]
    assert len(data) == 32
    recovered.add(bytes(data).hex())
assert recovered == source_sha_9360
assert "material_owner_selection_peek(" in warrior_h
assert "identity = *g_current;" in warrior_owner
assert "g_warrior_9360_logged[key].exchange(" in warrior_owner
assert "k_warrior_slots = 8u" in warrior_owner
assert "slot_bucket" in warrior_owner
assert "identity.material_slot < k_warrior_slots - 1u" in warrior_owner
assert "slot_valid=%u" in warrior_owner
assert "warrior_9360_stage::selector_invalid" in warrior_owner
assert "warrior_9360_stage::selector_not_authenticated" in warrior_owner
assert "warrior_9360_stage::selector_published" in warrior_owner
assert "warrior_9360_stage::pre_route" in integrated
assert integrated.count("warrior_9360_stage::pre_route") == 2
for gate in ("no_pipeline_route", "no_dynamic_route",
             "identity_ready", "prepare_reject", "prepared"):
    assert integrated.count("warrior_9360_stage::" + gate) == 2
# Two indexed/non-indexed branches, each handles valid material and pending
# owner fallback on identity rejection, so four occurrences are intentional.
assert integrated.count("warrior_9360_stage::identity_reject") == 4
route_a = integrated.index("bool on_draw(")
route_b = integrated.index("bool on_draw_indexed(")
for fn in (integrated[route_a:route_b], integrated[route_b:]):
    assert fn.index("warrior_9360_stage::pre_route") < fn.index("if (route_mask == 0u)")
    assert fn.index("warrior_9360_stage::no_pipeline_route") < fn.index("material_owner_selection_clear();")
# Witness never changes operator authority or the business logic return path.
assert "warrior_9360_trace" not in src
print("V230_WARRIOR_9360_FIRST_CUT_PASS 8 SHA full owner both Draw variants no authority widening")


# Two source-joined Standard Helm variants: narrow, executable, fail-open
# nonmetal P_Metal EnvSpec-family operator with exact PTDE t1 provenance.
manifest = (root / "include/dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp").read_text("utf-8")
nonmetal = (root / "include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp").read_text("utf-8")
assert "sizeof(k_proven_parts_specs[0]) == 38u" in manifest
helm_entries = (
    ('"bfbef63e9611bd8b753f8197e5cabeabbf08ec400e098f2191561bdc5ce7396b", 0u, "C_Metal[DSB].mtd", "hd_m_9360_s"', "0xe9c532be429d6f7eull"),
    ('"59c937ced4aaf41ba3f6db4a351d47b41dabd176224ecca4123b72a11d23baee", 0u, "P_Metal[DSB]_Edge.mtd", "hd_f_9360_s"', "0xb73e2e38d914a8f1ull"),
)
for source_row, logical_hash in helm_entries:
    assert manifest.count(source_row) == 1
    assert v230_resources.count(logical_hash) == 1
assert "if (!exact_proven_owner_slot(query))" not in nonmetal
assert "classify_mtd_envspec_semantics(query)" in nonmetal
assert "classify_mtd_envspec_semantics(query)" in nonmetal
assert "decision.receiver_id >= 33u" in nonmetal
assert "decision.receiver_id <= 35u" in nonmetal
assert "material.route_index == 345u" in nonmetal
bound = v230_resources.split(
    "bool material_resource_draw_runtime::\nprepare_draw_requests_bound(", 1
)[1].split(
    "bool material_resource_draw_runtime::\nprepare_fixed_pointlight_material_requests(", 1
)[0]
assert "v203_equipment::exact_proven_slot_spec_hash(query, h1)" not in bound
assert "const bool exact_nonmetal =" in bound
assert "h1 != 0u" in bound
assert "replacement != nullptr" in bound
assert "spec_equipment_allowed(h1)" not in bound  # two-line C++ layout
assert "spec_equipment_allowed(\n                    h1)" in bound
assert "if (exact_nonmetal)" not in bound # removed unsupported 38-entry texture veto
assert "exact_nonmetal" in bound # still selects its PTDE-island consumer
assert "spec_rgb_consumer_ready" in bound
assert "material.owner_tuple_exact" in bound or "const bool exact_material" in bound
assert "g_material_resources.prepare_draw_requests" in integrated
assert "material_resource_draw_runtime" in integrated
print("V230_WARRIOR_HELM_UNPROVEN_GATE_REMOVED_PASS exact material semantic, receiver and actual SRV identity retained")

# V314 read-only source instrument is inserted INSIDE an existing no-route
# branch; a second if (route_mask == 0u) was the exact Windows C1075 cause.
v314_materializer = (root / "tools/materialize_v314_unrouted_receiver_probe.py").read_text("utf-8")
assert "replace_twice('''#if defined(DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS)" in v314_materializer
v314_unrouted_patch = v314_materializer.split(
    "replace_twice('''#if defined(DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS)", 1
)[1]
assert "'''#if defined(DSRRL_V314_UNROUTED_LIT_MUL_CENSUS)" in v314_unrouted_patch
assert "'''    if (route_mask == 0u) {" not in v314_unrouted_patch
assert v314_unrouted_patch.count("v314::report_stock_unrouted(") == 1
print("V230_V314_BRACE_CONTRACT_PASS no_duplicate_unrouted_scope")

# The uploaded 2026-10-24 gameplay log had 0 Warrior FULL-SHA markers,
# despite a loaded addon and route345 native draw hits. Trace two genuinely
# earlier stages without weakening FLVER, MTD, shader or stock view authority.
warrior_flver = (root / "src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
assert "k_warrior_stages = 12u" in warrior_owner
assert "parser_registered" in warrior_h and "selector_flver_lookup" in warrior_h
assert "warrior_9360_stage::parser_registered" in warrior_flver
assert "warrior_9360_stage::selector_flver_lookup" in warrior_flver
parser_start=warrior_flver.index("void __fastcall parse_entry(")
parser_end=warrior_flver.index("void __fastcall destroy_entry(",parser_start)
parser_region=warrior_flver[parser_start:parser_end]
assert parser_region.index("flver_identity_observe_parse(") < parser_region.index(
    "warrior_9360_stage::parser_registered")
assert parser_region.index("warrior_9360_stage::parser_registered") < parser_region.index("if(g_po)")
selector_start=warrior_flver.index("bool owner_lookup_ok =")
selector_end=warrior_flver.index("if(owner_lookup_ok){\n  telemetry::hot_count(g_owner_sha_hits);",selector_start)
selector_region=warrior_flver[selector_start:selector_end]
assert "flver_identity_enrich_owner(" in selector_region
assert "warrior_9360_stage::selector_flver_lookup" in selector_region
assert "diagnostic.flver_sha256=observation.flver_sha256;" in selector_region
assert "flver_identity_lookup_direct_parser_model(" in selector_region
assert "warrior_9360_trace" not in (root / "src/runtime/material_owner_producer.cpp").read_text("utf-8")
print("V230_WARRIOR_PREOWNER_PROBE_SOURCE_PASS parserSHA selectorSHA no authorization widening")

# User's gameplay log: exact HD_M_9360 SHA parsed and selector lookup succeeded,
# but no later owner publish. C_Metal raw identity was absent from BOTH source
# registries despite a confirmed route229 raw SHA. Restore *identity only*.
import json as _v230_json
_v230_supp = _v230_json.loads(
    (root / "data/census/dsr_mtd_identity_certified_supplement_v1.json").read_text("utf-8"))
_v230_routes = _v230_json.loads(
    (root / "data/material_response_routes_v1.json").read_text("utf-8"))
_v230_cmetal = [x for x in _v230_supp["records"]
                if x["semantic_name"] == "C_Metal[DSB].mtd"]
_v230_cmetal_route = [x for x in _v230_routes["routes"]
                      if x["mtd_name"] == "C_Metal[DSB].mtd"]
assert len(_v230_cmetal) == len(_v230_cmetal_route) == 1
assert _v230_cmetal[0]["status"] == "CONFIRMED"
assert _v230_cmetal[0]["authority_class"] == "MATERIAL_IDENTITY_ONLY"
assert _v230_cmetal_route[0]["route_index"] == 229
assert _v230_cmetal[0]["raw_mtd_sha256"] == _v230_cmetal_route[0]["sha256"] == (
    "ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85")
assert _v230_cmetal[0]["basis"]["material_slot"] == 0
assert _v230_cmetal[0]["basis"]["pixel_equivalence"] == "OPEN"
assert "dsr_mtd_identity_supplement_resolve(" in (
    (root / "src/runtime/material_owner_producer.cpp").read_text("utf-8"))
print("V230_WARRIOR_CMETAL_RAW_IDENTITY_PASS route229 source certification only")

# 2026-10-24 source-exact MTD identity reconciliation, from owner-supplied
# original DSR Mtd.mtdbnd.dcx SHA 7c5a0952c1016a3efa3c6a7180bc60af81ccf7b2a4685c3c89028eda78c2d8b5.
# This certifies *transported raw identity* only, never PTDE pixel output.
_v230_supp = _v230_json.loads(
    (root / "data/census/dsr_mtd_identity_certified_supplement_v1.json").read_text("utf-8"))
_v230_routes = _v230_json.loads(
    (root / "data/material_response_routes_v1.json").read_text("utf-8"))
_v230_reg_text = (
    root / "include/dsrrl/operators/material_response/generated_dsr_mtd_identity_v1.hpp"
).read_text("utf-8")
import re as _v230_re
_v230_generic = {}
for _v230_match in _v230_re.finditer(
        r"\{(0x[0-9a-f]{16})ull,\{([^}]+)\},(true|false)\}", _v230_reg_text):
    _v230_generic[int(_v230_match.group(1), 16)] = "".join(
        _v230_re.findall(r"0x([0-9a-f]{2})", _v230_match.group(2)))
def _v230_fnv64(n):
    h = 14695981039346656037
    for b in n.encode("utf-8"):
        h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h
_v230_supp_by_name = {r["semantic_name"]: r for r in _v230_supp["records"]}
assert len(_v230_supp_by_name) == len(_v230_supp["records"]) == 57
_v230_original_bnd_sha = (
    "7c5a0952c1016a3efa3c6a7180bc60af81ccf7b2a4685c3c89028eda78c2d8b5")
_v230_source_rows = [
    r for r in _v230_supp["records"]
    if r.get("basis", {}).get("dcx_sha256") == _v230_original_bnd_sha
]
assert len(_v230_source_rows) == 30
assert all(r["status"] == "CONFIRMED" and
           r["authority_class"] == "MATERIAL_IDENTITY_ONLY"
           for r in _v230_source_rows)
_v230_covered = []
_v230_not_covered = []
for r in _v230_routes["routes"]:
    name = r["mtd_name"]
    source = _v230_supp_by_name.get(name)
    exact = source["raw_mtd_sha256"] if source else _v230_generic.get(
        _v230_fnv64(name))
    if exact == r["sha256"]:
        _v230_covered.append(name)
    else:
        _v230_not_covered.append(name)
assert len(_v230_routes["routes"]) == 35
assert len(_v230_covered) == 33
assert set(_v230_not_covered) == {
    "P_DullLeather[DSB].mtd", # 2 distinct DSR raw SHA for one name!
    "C_3530_Unique_Wet[DSB].mtd", # route231 SHA != source archive SHA
}
assert "P_DullLeather[DSB].mtd" not in _v230_supp_by_name
assert "C_3530_Unique_Wet[DSB].mtd" not in _v230_supp_by_name
_v230_producer = (root / "src/runtime/material_owner_producer.cpp").read_text("utf-8")
assert 'mr::mtd_semantic_hash("Ps_Body[DSB].mtd")' in _v230_producer
assert 'mr::mtd_semantic_hash("P_DullLeather[DSB]_Edge.mtd")' in _v230_producer
assert "const bool source_unique_bnd_override" in _v230_producer
assert _v230_producer.index("source_unique_bnd_override") < _v230_producer.index(
    "const bool sha_resolved")
print("V230_DSR_RAW_MTD_SOURCE_CENSUS_PASS 35 routes, 33 exact registrable, "
      "2 quarantined (ambiguous, bad route digest); 30 source-verified repairs "
      "and exactly 2 stale-unique overrides, pixel OPEN")


# Source-only diagnostic for ALL non-metal routes, including cloth/leather/wet.
# Existing release INFO logs do not prove individual material joins.
_v230_addon=(root / "integrated/integrated_addon.cpp").read_text("utf-8")
assert "[DSRRL NONMETAL FIRST CUT]" in _v230_addon
assert "g_v230_nonmetal_route_logged" in _v230_addon
assert "material.route_index == 345u || material.route_index == 229u" in _v230_addon
assert "material.route_index >= 512u" in _v230_addon
assert "pixel_pass=0" in _v230_addon
for _v230_stage in ("identity_visible", "mr_evaluated", "generic_candidate",
                   "lerp_mr_prepared", "stable_mr_prepared", "generic_mr_not_prepared"):
    assert _v230_stage in _v230_addon
assert "v230_nonmetal_route_trace(0u, receiver_id, out_material, out_decision)" in _v230_addon
assert "v230_nonmetal_route_trace(1u, receiver_id, out_material, out_decision)" in _v230_addon
assert "v230_nonmetal_route_trace(2u, receiver_id, material, decision)" in _v230_addon
assert "v230_nonmetal_route_trace(3u, receiver_id, material, decision)" in _v230_addon
assert "prepared.mr_in_batch ? 4u : 5u" in _v230_addon
print("V230_ALL_NONMETAL_FIRST_CUT_SOURCE_PASS route specific diagnostic only; no new shader permissions")

# 2026-10-24 second nonmetal owner log: first draw route160 RX33 rejected,
# later route160 RX34 prepared; first route232 RX33 prepared but RX34 rejected.
# Route-only latches conflate receiver, slot and time. Keep read-only
# result stamps AFTER actual indexed/nonindexed draw dispatch, not only prep.
assert "512u * 24u * 16u" in _v230_addon
assert "std::min(material.material_slot, 15u)" in _v230_addon
assert "(receiver - 24u)" in _v230_addon
assert "g_v230_nonmetal_route_logged[key].fetch_or(" in _v230_addon
for _stage in ("batch_mutation_ready","indexed_issued","indexed_failopen",
               "batch_prepare_fail","draw_issued","draw_failopen"):
    assert f'"{_stage}"' in _v230_addon
assert _v230_addon.count("v230_nonmetal_route_trace(6u, receiver_id, material, decision)") == 2
assert _v230_addon.count("v230_nonmetal_route_trace(9u, receiver_id, material, decision)") == 2
assert "draw_tx_issued(dispatch.transaction) ? 7u : 8u" in _v230_addon
assert "draw_tx_issued(dispatch.transaction) ? 10u : 11u" in _v230_addon
print("V230_NONMETAL_RECEIVER_SLOT_DOWNSTREAM_PASS per RX/slot actual issued marker and fail-open")

# MR negative TLS cache would retain present=false forever after a later
# registration of that exact bank/receiver. V359 PROTECT explicitly forbids
# global replacement epoch invalidation; use a per-key ready bit instead.
_v230_mr_tx=(root / "src/runtime/material_response_draw_transaction.cpp").read_text("utf-8")
_v230_mr_h=(root / "include/dsrrl/runtime/material_response_draw_transaction.hpp").read_text("utf-8")
_v230_register = _v230_mr_tx[_v230_mr_tx.index(
    "bool material_response_draw_runtime::register_replacement_record("):
    _v230_mr_tx.index("void material_response_draw_runtime::release_resources()")]
_v230_acquire = _v230_mr_tx[_v230_mr_tx.index(
    "bool material_response_draw_runtime::acquire_replacement("):
    _v230_mr_tx.index("bool material_response_draw_runtime::register_replacement_record(")]
assert "5u * k_replacement_key_receivers" in _v230_mr_h
assert "replacement_key_published_" in _v230_mr_h
assert "cached.present && cached.shader != nullptr" in _v230_acquire
assert "replacement_key_published_[key_index].load(" in _v230_acquire
assert "std::memory_order_acquire" in _v230_acquire
assert "bank.emplace(" in _v230_register
assert "bank_index * k_replacement_key_receivers + receiver_id" in _v230_register
assert ".store(true, std::memory_order_release)" in _v230_register
assert "resource_epoch_.fetch_add(" not in _v230_register
assert "for (auto &key : replacement_key_published_)" in _v230_mr_tx
print("V230_MR_NEGATIVE_TLS_EXACT_KEY_REFRESH_PASS no global cache invalidation or operator widening")


# Owner project-wide rule, Supabase revision 14340: a finite manifest may
# document specific provenance, but cannot veto all other valid nonmetal
# operators without a confirmed real incorrect-activation witness.
# The two 2026-10-24 Warrior gates were added in commits 7b1c4d6ce and
# 449d96db7 and lack demonstrated protective benefit in this mod.
assert "if (!exact_proven_owner_slot(query))" not in nonmetal
assert "exact_proven_slot_spec_hash(query, h1)" not in bound
for required_gate in (
    "material.owner_tuple_exact",
    "material.material_slot_valid",
    "material.route_index == 345u",
    "classify_mtd_envspec_semantics(query)",
    "env.presence == mr::ptde_envspec_presence::present",
    "env.envspc_slot_valid",
    "decision.receiver_id >= 33u",
    "decision.receiver_id <= 35u",
):
    assert required_gate in nonmetal
for required_resource in (
    "h1 != 0u",
    "spec_equipment_allowed(",
    "replacement != nullptr",
    "spec_rgb_consumer_ready",
    "core_.features().enabled(core::operator_id::spec_rgb)",
):
    assert required_resource in bound
print("V230_UNPROVEN_WARRIOR_GATES_ABSENT_PRESERVED_SOURCE_IDENTITY_PASS")
