#!/usr/bin/env python3
"""v2.3.0 stock DSR EnvDiffuse-only rollback: post-materialization source guards.

These are *construction* checks, not runtime/pixel-equivalence evidence.
Fails closed when later generated-source steps undo or broaden the cut.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
stock = "DSRRL_V230_STOCK_DSR_ENVDIFFUSE"

def read(path):
    return (root / path).read_text("utf-8")

def require(name, content, *patterns):
    for pattern in patterns:
        assert pattern in content, f"{name}: missing required guard: {pattern!r}"

builder = read("tools/build_v230_production.ps1")
src = read("src/runtime/pmetal_env_source_runtime.cpp")
shader = read("src/operators/env_spec/pmetal_rgba_materializer.cpp")
lerp = read("src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
draw = read("src/runtime/pmetal_envspec_draw_runtime.cpp")
resource = read("src/runtime/envspec_resource_runtime.cpp")

require("builder", builder,
        "/DDSRRL_V230_PRODUCTION", "/DDSRRL_V230_STOCK_DSR_ENVDIFFUSE",
        "[DSRRL V230 STOCK DSR ENVDIFFUSE]",
        "test_v230_stock_envdiffuse_contract.py")
require("native EnvSpec/SpecRGB flags retained", builder,
        "DSRRL_V359_ATTESTED_GI_NAME_ALIAS",
        "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE")
require("EnvSpec source producer kept", src,
        "latest_hook_source(", "publish_hook_source(",
        "g_envspec_single_original(", "g_envspec_blend_original(")
require("PTDE EnvDiffuse donor is disabled independently", src,
        f"#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined({stock})",
        "const bool envdiffuse_ok = donor_ok;",
        "if (!load_exact_envdiffuse_donor())")
require("stable DXBC EnvDiffuse consumer remains untouched", shader,
        f"#if defined({stock})",
        "Stock DSR EnvDiffuse: retain the exact original 29-DWORD producer",
        "outcome.envdiffuse_linear_consumer_diag = false;",
        "apply_build131(", "compose_exact_phn_scene_encoding(base)")
require("lerp exact native A/B block preserved", lerp,
        f"defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined({stock})",
        "preserved_envdiffuse.assign(",
        "outcome.envdiffuse_preserved = true;",
        "v344_expected_t11_delta = 129u;",
        "materialize_spec_rgb_consumer(")
require("P_Metal draw still uses independent EnvSpec/SpecRGB/Material Response",
        draw, f"constexpr bool k_stock_dsr_envdiffuse_only = true;",
        "core::operator_id::env_spec",
        "core::operator_id::material_response",
        "core::operator_id::spec_rgb",
        "prepared.env_resources.ptde_a",
        "prepared.env_resources.ptde_b",
        "if (family == pmetal_envspec_receiver_family::hemenvlerp)")
require("no t11/t13/s11/s13 sidecar or rebind on stock branch", draw,
        f"#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined({stock})",
        f"#elif !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) && !defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG) && !defined({stock})",
        f"#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)\n        !outcome.envdiffuse_preserved",
        "prepared.request.additional_resource_owners = 0u;",
        "prepared.request.samplers[")
require("PTDE EnvSpec pack retained, EnvDiffuse pack bypassed", resource,
        f"#if !defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) || defined({stock})",
        "load_pack(pack);", "load_envdiffuse_pack(",
        "g_envdiffuse_pack_ready =")

assert draw.count(f"defined({stock})") >= 10, "P_Metal draw rollback incompletely guarded"
assert resource.count(f"|| defined({stock})") == 3, "EnvDiffuse resource loader/get/prepare not all gated"
assert src.count(f"&& !defined({stock})") >= 4, "EnvDiffuse source producer still depends on a donor"
assert "DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG=ON" not in builder, "Do not enable unrelated V13 diagnostics"
assert "DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG=ON" not in builder, "Do not enable unrelated V13 diagnostics"
print("STOCK_DSR_ENVDIFFUSE_CONSTRUCTION_CONTRACT_PASS")
print("ENV_DIFFUSE=stock_source+stock_t11_t13+s11_s13  OTHER_ISLANDS=retained")
print("LIMITATION=shared_postmerge_material_and_terminal_paths_not_pixel_certified")
