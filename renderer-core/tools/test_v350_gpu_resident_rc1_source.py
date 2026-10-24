#!/usr/bin/env python3
"""V350 GPU-resident RC1 source contract: always ON, no TXT, unchanged PTDE gates.
Only source/construction PASS. Runtime activation/FPS and PTDE pixels remain OPEN.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=(root/"renderer-core/src/runtime/material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
build=(root/"renderer-core/tools/build_v350_scope_shell_gpu_resident_rc1.ps1").read_text(encoding="utf-8")

def region(start,end):
    assert start in src and end in src,(start,end)
    return src.split(start,1)[1].split(end,1)[0]

assert "DSRRL_SIDECAR_RESIDENCY.txt" not in src, "TXT dependency forbidden in RC"
assert "sidecar_residency_requested(" not in src, "old TXT flag reader must be removed"
assert 'g_session_residency.store(true,std::memory_order_release);' in src
assert 'std::atomic_bool g_session_residency{true};' in src
assert '[DSRRL GPU RESIDENT RC1] default=ON config_file=NONE' in src
assert "addon_event::init_device" in src and "on_init_device_residency" in src
assert "addon_event::destroy_device" in src
pre=region("void on_init_device_residency(", "void account_load(")
assert "QueryVideoMemoryInfo(" in pre
assert "DXGI_MEMORY_SEGMENT_GROUP_LOCAL" in pre
assert "memory.Budget <= memory.CurrentUsage" in pre
assert "const auto limit = unused_budget / 2u;" in pre
assert "g_resident_gpu_limit = limit;" in pre
for token in (
    'Specular', 'Diffuse', 'Normals',
    "spec_equipment_allowed(hash)",
    "exact_subsurface_body_spec_hash(hash)",
    "diffuse_target_hash_allowed_v12(hash)",
    "normal_target_hash_allowed_v12(hash)",
    "load_dds(", "sidecar_shared_entry{result.view,0u,true}",
    "bytes <= g_resident_gpu_limit",
    "g_resident_gpu_limit - bytes",
):
    assert token in pre,token
loader=region("load_result load_dds(","void on_init_device_residency(")
assert "magic != k_dds_magic" in loader and "header.pixel_format.size != 32u" in loader
assert "sha256(" not in loader, "replacement DDS must not need fixed retail byte identity"
assert "CreateTexture2D(" in loader and "CreateShaderResourceView(" in loader
cold=region("acquired_sidecar acquire_sidecar(","bool snapshot_companion_cached(")
for token in (
    "g_resident_gpu_limit", "cold_size <= g_resident_gpu_limit",
    "g_resident_gpu_used <= g_resident_gpu_limit - cold_size",
    "if (inserted && resident) g_resident_gpu_used += cold_size;",
    "sidecar_path(cls, logical_name)"):
    assert token in cold,token
assert "!found->second.resident" in region("void release_shared_user(","void release_set(")
assert "g_sidecar_shared.clear();" in src
assert "std::vector<ID3D11ShaderResourceView *> resident;" in src
assert "release_cache_for_device(" in src
assert "DSRRL_QPC_SCOPE_SHELL_NO_CLOCK" in build, "retain user-known-good Scope Shell"
assert "DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS" in build
assert "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE" in build
assert "DSRRL_V350_SCOPE_SHELL_GPU_RESIDENT_RC1.addon64" in build
assert "v350_rc1_scope_shell_gpu_resident_default_on" in build
assert "test_v350_sidecar_gpu_resident_source.py" not in build
print("V350 GPU RESIDENT RC1 SOURCE PASS: permanent ON, no residency TXT, exact live-source identity and DDS format, UMA memory bound, device teardown")
