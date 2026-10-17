#!/usr/bin/env python3
"""V350 GPU residency optional control-plane contract.
Only source/construction assertions. Runtime startup latency/FPS/pixel OPEN.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
s = (root / "renderer-core/src/runtime/material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
build = (root / "renderer-core/tools/build_v350_sidecar_gpu_resident_one_addon.ps1").read_text(encoding="utf-8")
def region(a,b):
    assert a in s and b in s,(a,b)
    return s.split(a,1)[1].split(b,1)[0]
request = region("bool sidecar_residency_requested()", "void on_init_device_residency(")
early = region("void on_init_device_residency(", "void account_load(")
loader = region("load_result load_dds(", "bool sidecar_residency_requested()")
draw = region("void on_init_resource_view(", "void on_destroy_resource_view(")
release = region("void release_shared_user(", "void release_set(")
assert "DSRRL_SIDECAR_RESIDENCY.txt" in request
assert "mode == '1'" in request and "return false;" in request, "default OFF"
assert "g_session_residency.load(" in early
assert "DXGI_MEMORY_SEGMENT_GROUP_LOCAL" in early
assert "memory.Budget - memory.CurrentUsage" in early
assert "unused_budget / 2u" in early, "must reserve memory for game streaming"
assert "Specular" in early and "Diffuse" in early and "Normals" in early
for token in ("spec_equipment_allowed(hash)", "exact_subsurface_body_spec_hash(hash)",
              "diffuse_target_hash_allowed_v12(hash)",
              "normal_target_hash_allowed_v12(hash)",
              "load_dds(", "sidecar_shared_entry{result.view,0u,true}"):
    assert token in early, token
assert "stock" in early and "receiver_auth=UNCHANGED" in early
assert "hash_mismatch" in loader and "sha256(" in loader
assert "CreateTexture2D(" in loader and "CreateShaderResourceView(" in loader
assert "acquire_sidecar(" in draw and "on_init_resource_view" in s
assert "!found->second.resident" in release
assert "std::vector<ID3D11ShaderResourceView *> resident;" in s
assert "g_sidecar_shared.clear();" in s
assert "on_init_device_residency" in s
assert "addon_event::init_device" in s
assert "DSRRL_QPC_SCOPE_SHELL_NO_CLOCK" in build
assert "DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS" in build
assert "DSRRL_V350_SCOPE_SHELL_GPU_RESIDENT.addon64" in build
assert "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE" in build
print("V350 GPU RESIDENCY SOURCE PASS: opt-in; exact sidecar gate; free DXGI budget; full session COM ownership; no PTDE operator change")
