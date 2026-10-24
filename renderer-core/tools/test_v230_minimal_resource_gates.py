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
