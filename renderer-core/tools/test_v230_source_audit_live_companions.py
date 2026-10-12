#!/usr/bin/env python3
"""V230 strict source, hooks, and operator preservation oracle.

The live SRV reuse only optimizes already authenticated PTDE companions.
Protected renderer semantics and the last working texture resource hooks
must be bit-for-bit unchanged from the V229 exact source commit.
"""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
EXPECT={
    "renderer-core/src/runtime/texture_identity_transport.cpp":
        "0fd0646b64d5f0cb6b50efec04e36ecb7dc6bb33",
    "renderer-core/src/runtime/flver_engine_hooks.cpp":
        "a8e5dc26bc972cb298c677a7a9023088c3584660",
    "renderer-core/src/runtime/envspec_resource_runtime.cpp":
        "72b9732712b974746a26490689141b48a98057ed",
    "renderer-core/src/runtime/pmetal_env_source_runtime.cpp":
        "a6904844c97c712027ad79bbcd136066952a7116",
    "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp":
        "016eaf968dd209835c6cdc465ce3dc783e439180",
    "renderer-core/integrated/integrated_addon.cpp":
        "a9dcd328547130d014d8c60c9f7fbb50698dc242",
    "renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp":
        "223fb4091e7fdf714e8b6b39a1de091a00078d5d",
    "renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp":
        "e5a728b3f89e373b34e36c1c2228b7470662d72e",
    "renderer-core/src/operators/material_response/material_response_island.cpp":
        "6d7b013ebdbdeac19cbf043ca45facc5ac854b80",
}
def check_protected():
    for p,sha in EXPECT.items():
        actual=subprocess.check_output(
            ["git","hash-object",p],cwd=ROOT,text=True).strip()
        if actual!=sha:
            raise AssertionError(
                "PROTECTED HOOK / SHADER MODIFIED: "+
                p+" "+actual+" != "+sha)
def check_hotpath():
    p=ROOT/"renderer-core/src/runtime/material_resource_draw_runtime.cpp"
    s=p.read_text(encoding="utf-8")
    must=[
        'texture_identity_transport::snapshot_raw(',
        'reshade::addon_event::init_resource_view',
        'reshade::addon_event::destroy_resource_view',
        'reshade::addon_event::destroy_device',
        'g_internal_create = true;',
        'g_internal_create = false;',
        'g_live_by_logical_hash',
        'erase_live_companion_index(',
        'g_live_by_logical_hash.clear();',
        'release_cache_for_device(',
        'candidate.exact_logical_name',
        'may_reuse_live_sidecar_v230(',
        'live_sidecar_reuse_need_v230',
        'std::move(found->second)',
        'std::move(set)',
        'if (need_diffuse)',
        'if (need_normal)',
        'if (spec_member)',
        'g_live_companion_reuses',
        'process_dir() noexcept',
        'static const std::filesystem::path root',
        'std::ifstream stream(',
        'load_dds(',
        'k_hd_a_9550_s_sha256',
        'k_hd_a_9550_diffuse_sha256',
        'k_hd_a_9550_normal_sha256',
        'g_ambiguous_view_device',
        'invalidate_companion_epoch(key)',
        'g_cache.emplace(',
    ]
    for k in must:
        if k not in s: raise AssertionError("Missing V230 invariant "+k)
    forbidden=[
        'std::filesystem::is_regular_file(path)',
        'if (candidate.device != native_device ||',
        'old = found->second;',
        'dead = it->second;',
        'dead.push_back(it->second);',
    ]
    for k in forbidden:
        if k in s: raise AssertionError("Non-optimized source "+k)
    assert s.count('safe_load_sidecar(') == 4
    assert s.count('g_live_by_logical_hash.emplace(') == 1
    assert s.count('erase_live_companion_index(') == 4
    assert s.count('g_live_by_logical_hash.clear();') == 1
    assert s.count('reshade::addon_event::init_resource_view') >= 1
    assert s.count('g_internal_create = true;') == 1
    assert s.count('g_internal_create = false;') >= 2
    # Cold DDS path and GPU creation remain intact.
    assert 'device->CreateTexture2D(' in s
    assert 'device->CreateShaderResourceView(' in s
    # No persistent cache of extra GPU objects beyond live stock views:
    assert 'std::unordered_multimap<std::uint64_t,std::uint64_t>' in s
def main():
    check_protected()
    check_hotpath()
    print("V230_PROTECTED_LAST_RESOURCE_AND_FLVER_HOOK_BLOBS_BIT_IDENTICAL_PASS")
    print("V230_PMETAL_STABLE_LERP_SHADERS_AND_MATERIAL_RESPONSE_BIT_IDENTICAL_PASS")
    print("V230_EXACT_LIVE_SRV_NAME_DEVICE_COMPLETE_CLASS_PRECONDITIONS_PASS")
    print("V230_CURRENT_RUNTIME_PIXEL_PERFORMANCE_OPEN")
if __name__=="__main__":
    main()
