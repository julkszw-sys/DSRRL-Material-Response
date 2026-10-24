#!/usr/bin/env python3
"""Audits source contracts of every V230 shader/resource mutation family.

Strictly static: never interprets existence of source code as runtime or
PTDE-visible pixel equivalence. The owner DDS and stock DSR DXBC are not
bundled in this repo, so checks requiring their exact bytes remain OPEN.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
def source(path):
    return (root / path).read_text(encoding="utf-8")

checks = []
def require(kind, name, content, predicates):
    missing = [needle for needle in predicates if needle not in content]
    checks.append((kind, name, not missing, missing))
    if missing:
        raise AssertionError(f"{kind}/{name}: missing source invariants {missing!r}")

diff = source("src/operators/resource_bridges/diffuse_bridge.cpp")
normal = source("src/operators/resource_bridges/normal_bridge.cpp")
spec = source("src/operators/resource_bridges/spec_rgb_bridge.cpp")
assets = source("src/runtime/material_resource_draw_runtime.cpp")
probe = source("src/runtime/envspec_resource_runtime.cpp")
producer = source("src/runtime/pmetal_env_source_runtime.cpp")
stable = source("src/operators/env_spec/pmetal_rgba_materializer.cpp")
lerp = source("src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
shader_registration = source("src/runtime/pmetal_envspec_draw_runtime.cpp")
tx = source("src/runtime/draw_state_transaction.cpp")
adapter = source("src/runtime/island_draw_adapter.cpp")
integrated = source("integrated/integrated_addon.cpp")

require("asset", "diffuse_t0", assets, [
    "const bool diffuse_target = h0 != 0u && companions[0] != nullptr;",
    "asset_class::diffuse", "snapshot_companion_cached(",
    "sidecar_path(asset_class::diffuse, logical_name)",
    "prepared.diffuse = true;",
])
require("asset", "normal_t2", assets, [
    "const bool normal_target = h2 != 0u && companions[2] != nullptr;",
    "asset_class::normal", "sidecar_path(asset_class::normal, logical_name)",
    "prepared.normal = true;",
])
require("asset", "equipment_specrgb_t10", assets, [
    "spec_equipment_allowed(", "asset_class::specular",
    "context_spec.stock_t1_preserved =", "prepared.spec_rgb = true;",
    "query.material.owner_tuple_exact",
])
require("asset", "dds_parser", assets, [
    "header.size != 124u", "header.pixel_format.size != 32u",
    "k_fourcc_dxt1", "k_fourcc_dxt3", "k_fourcc_dxt5",
    "DXGI_FORMAT_BC7_UNORM", "mip_count > max_mips",
    "D3D11_USAGE_IMMUTABLE", "g_ambiguous_view_device",
    "on_destroy_resource_view(",
])
require("asset", "envspec_ptde_cubemap", probe, [
    "k_legacy_packed_gi_sha256", "k_ptde_envdiffuse_packed_gi_sha256",
    "snapshot_ready_envspec(", "prepare_envdiffuse(",
])
require("source", "ptde_lightbank_donor", producer, [
    "k_ptde_envdiffuse_donor_sha256", "load_exact_envdiffuse_donor()",
    "bool latest_hook_source(", "bool latest_hook_source_exact_selector(",
    "selected_source_a", "pmetal_selector_policy::selects_only_b(",
])
require("shader", "hemenv_stable_exact_dxbc", stable, [
    "checksum_container_valid(", "pmetal_rgba_authority::",
    "candidate.stock_sha256", "apply_build131(",
    "apply_exact_ptde_envdiffuse_consumer(", "final_postcondition(",
    "compose_exact_phn_scene_encoding(", "unique_rdef(", "patch_build131_rdef(",
])
require("shader", "hemenv_lerp_exact_dxbc", lerp, [
    "checksum_container_valid(", "k_pmetal_hemenvlerp_sites",
    "candidate.stock_sha256", "add_bridge_rdef(",
    "materialize_spec_rgb_consumer(", "final_postcondition(",
])
require("shader", "receiver_registration", shader_registration, [
    "outcome.receiver_id < 33u", "outcome.receiver_id > 35u",
    "outcome.semantic_receiver_id < 33u",
    "outcome.semantic_receiver_id > 35u",
    "device_->CreatePixelShader(", "payload_sha256",
    "quarantined_.store(", "material_chain_ready",
])
require("shader", "actual_create_dispatch", integrated, [
    "materialize_pmetal_rgba_receiver(", "register_replacement(",
    "materialize_pmetal_rgba_lerp_receiver(",
    "register_lerp_replacement(",
    "append_island_draw_request(", "dispatch_island_draw_batch(",
])
require("transaction", "d3d11_capture_restore", tx, [
    "ctx->PSGetShader(", "ctx->PSSetShader(",
    "ctx->PSGetConstantBuffers(", "ctx->PSSetConstantBuffers(",
    "ctx->PSGetShaderResources(", "ctx->PSSetShaderResources(",
    "ctx->PSGetSamplers(", "ctx->PSSetSamplers(",
    "PSGetConstantBuffers1(", "restore_ps_constant_buffer_window(",
    "draw_tx_result::issued_restored", "draw_tx_result::issued_restore_failed",
    "unique_slots(mutation.srvs, mutation.srv_count)",
    "quarantined_.store(true)",
])
append = adapter.split("island_draw_batch_result append_island_draw_request(",1)[1].split(
    "island_draw_dispatch_result dispatch_island_draw_batch(",1)[0]
require("transaction", "atomic_batch_append", append, [
    "island_draw_batch staged = batch;",
    "staged.mutation.constant_buffers",
    "staged.mutation.srvs", "staged.mutation.samplers",
    "batch = staged;", "++staged.island_count",
])
require("operator", "t0_only_diffuse", diff, ["evaluate_diffuse_route("])
require("operator", "t2_only_normal", normal, ["evaluate_normal_route("])
require("operator", "specrgb_operator", spec, ["evaluate_spec_rgb_route("])
print("V230_ASSET_SHADER_TX_SOURCE_PASS checks=%d" % len(checks))
print("UNVERIFIED_BY_SOURCE: owner DDS bytes; individual stock DXBC bytes; PTDE FPO pairing; exact draw live resource identity; replay state; PTDE-visible pixel error.")
