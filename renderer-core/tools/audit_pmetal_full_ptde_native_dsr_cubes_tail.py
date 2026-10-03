#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")
cmake = (root/"integrated"/"CMakeLists.txt").read_text(encoding="utf-8")
runtime = (root/"src"/"runtime"/"pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
resource = (root/"src"/"runtime"/"envspec_resource_runtime.cpp").read_text(encoding="utf-8")
header = (root/"include"/"dsrrl"/"runtime"/"envspec_resource_runtime.hpp").read_text(encoding="utf-8")
stable = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_materializer.cpp").read_text(encoding="utf-8")
lerp = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")
phn = (root/"include"/"dsrrl"/"operators"/"surface"/"phn_scene_encoding.hpp").read_text(encoding="utf-8")

checks = {
    "native_dsr_option": "DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED" in cmake,
    "runtime_native_feed": "defined(DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED)" in runtime,
    "runtime_prepare_native": "prepare_native_dsr(" in runtime,
    "mip3_api": "prepare_native_dsr_mip3(" in header and "prepare_native_dsr_mip3(" in resource,
    "mip3_view_cache": "g_native_mip3_views" in resource,
    "mip3_exact_index": "k_ptde_equivalent_mip = 3u" in resource,
    "mip3_single_level_view": "mip_desc.TextureCube.MipLevels = 1u" in resource,
    "native_prepare_routes_mip3": "return prepare_native_dsr_mip3(" in resource,
    "runtime_mip3_identity": "mode=native_dsr_bc6h_mip3_as_lod0_ptde_operator" in runtime,
    "stable_full_ptde": "!apply_build131(" in stable,
    "stable_ptde_envdiffuse": "!apply_linear_envdiffuse_consumer_diag(" in stable,
    "shared_phn_formula": "c135.x / c135.y" in phn,
    "shared_phn_mul_sat": "mul_sat" in phn and "apply_unique_phn_scene_encoding_words" in phn,
    "shared_phn_fail_open": "fail_ambiguous_terminal" in phn and "fail_unverified_terminal_shape" in phn,
    "stable_phn_terminal": "!compose_exact_phn_scene_encoding(base)" in stable,
    "stable_phn_postcondition": "phn_scene_encoding_exact(words)" in stable,
    "stable_phn_outcome": "outcome.phn_scene_encoding_composed = true;" in stable,
    "stable_no_v13_for_profile": "DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED" not in stable,
    "lerp_ptde_envspec_chain": "k_ptde_rgba_envspec_chain" in lerp,
    "lerp_phn_terminal": "!apply_exact_phn_scene_encoding(" in lerp,
    "lerp_phn_postcondition": "phn_scene_encoding_exact(words)" in lerp,
    "lerp_phn_outcome": "outcome.phn_scene_encoding_composed = true;" in lerp,
    "lerp_spec_consumer": "materialize_spec_rgb_consumer(" in lerp,
    "lerp_ul_forced_off": "const bool compose_upper_lower = false;" in lerp,
    "runtime_stable_ul_rejected": "outcome.upper_lower_composed ||" in runtime,
    "runtime_k135_carrier": "decision.c101,\n            0.5f" in runtime,
    "runtime_stable_phn_required": "!outcome.phn_scene_encoding_composed ||" in runtime,
    "runtime_lerp_sat_required": "!outcome.terminal_sat_rgb_composed ||" in runtime,
    "runtime_k135_identity": "phn_k135=0.5 terminal=k135_then_sat" in runtime,
}
failed = [k for k,v in checks.items() if not v]
if failed:
    raise SystemExit("AUDIT_FAIL: " + ",".join(failed))

# The dedicated build must not enable any V13 or PackedGI override. Those are
# incompatible with the requested semantic cut: full PTDE EnvSpec operator,
# native DSR cubemap carrier, PTDE artifact-prevention terminal SAT.
print("PMETAL_PTDE_PHN_K135_NATIVE_DSR_MIP3_AUDIT_PASS")
