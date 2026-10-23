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
assert "if (exact_material &&" in draw

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
spec_region=draw.split("if (exact_material &&",1)[1].split("const bool bmp_receiver =",1)[0]
assert "full_material_response_ready &&" in spec_region
assert "spec_equipment_allowed(" in spec_region
assert "stock_t1_preserved =" in spec_region
assert "native_t10_transport_ready =" in spec_region
