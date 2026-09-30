#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise SystemExit(f"FAIL {label}: missing {needle!r}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    args = parser.parse_args()

    root = Path(args.source_dir)
    env_h = (root / "include/dsrrl/runtime/envspec_resource_runtime.hpp").read_text(encoding="utf-8")
    env_cpp = (root / "src/runtime/envspec_resource_runtime.cpp").read_text(encoding="utf-8")
    pmetal_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
    cmake = (root / "integrated/CMakeLists.txt").read_text(encoding="utf-8")

    require(
        cmake,
        "DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED",
        "reproducible diagnostic build option",
    )
    require(
        env_h,
        "bool prepare_native_dsr(",
        "native DSR resource API",
    )
    require(
        env_h,
        "bool prepare_native_dsr_mip3(",
        "native DSR mip3 resource API",
    )
    require(
        pmetal_cpp,
        "k_native_dsr_cubemap_feed",
        "compile-scoped diagnostic selector",
    )
    require(
        pmetal_cpp,
        "env_resources_.prepare_native_dsr(",
        "P_Metal native resource branch",
    )
    require(
        pmetal_cpp,
        "env_resources_.prepare(",
        "default PTDE PackedGI resource branch",
    )
    require(
        pmetal_cpp,
        "source_.latest(source)",
        "PTDE LightBank source feed retained through isolated V13 carrier",
    )
    require(
        pmetal_cpp,
        "prepared.material_resources.spec_rgb",
        "PTDE SpecRGB material carrier retained",
    )
    require(
        pmetal_cpp,
        "replace_pixel_shader =\n        true;",
        "PTDE EnvSpec shader replacement retained",
    )
    require(
        pmetal_cpp,
        "mode=native_dsr_bc6h_mip3_as_lod0_ptde_operator",
        "runtime diagnostic identity",
    )

    begin = env_cpp.index("bool envspec_resource_runtime::prepare_native_dsr_mip3(")
    end = env_cpp.index("\nvoid envspec_resource_runtime::release(", begin)
    block = env_cpp[begin:end]

    require(
        block,
        "context->PSGetShaderResources(",
        "currently bound DSR t12/t14 source",
    )
    require(
        block,
        "g_resource_by_view.find(",
        "exact native probe identity gate",
    )
    require(
        block,
        "g_sampler_ready",
        "PTDE sampler readiness gate",
    )
    require(
        block,
        "get_native_mip3_view(",
        "mip3-only native cube view",
    )
    require(
        env_cpp,
        "constexpr UINT k_ptde_equivalent_mip = 3u;",
        "256-to-32 mip relation",
    )
    require(
        env_cpp,
        "mip_desc.TextureCube.MipLevels = 1u;",
        "single native mip exposure",
    )
    if "g_pack_ready" in block or "get_ptde_cube(" in block:
        raise SystemExit(
            "FAIL native DSR mip3 path still depends on PTDE PackedGI materialization"
        )

    sampler_at = env_cpp.index("sampler_desc desc{};")
    sampler_block = env_cpp[sampler_at:sampler_at + 900]
    require(
        sampler_block,
        "desc.max_lod = 0.0f;",
        "PTDE LOD0 sampling clamp",
    )

    print("DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FALSIFIER_PASS")
    print("  consumer=exact current PTDE P_Metal EnvSpec island")
    print("  source=A/B LightBank feed unchanged")
    print("  material=PTDE SpecRGB/c101 path unchanged")
    print("  resource=native DSR BC6H t12/t14, mip3 only (32x32)")
    print("  view LOD0=DSR mip3; sampler=PTDE max_lod=0")
    print("  PackedGI dependency=absent from diagnostic resource path")
    print("  default production path=unchanged")


if __name__ == "__main__":
    main()
