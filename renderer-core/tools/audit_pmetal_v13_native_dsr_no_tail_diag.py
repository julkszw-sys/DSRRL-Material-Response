#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")
cmake = (root/"integrated"/"CMakeLists.txt").read_text(encoding="utf-8")
stable = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_materializer.cpp").read_text(encoding="utf-8")
lerp = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")
runtime = (root/"src"/"runtime"/"pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")

required = {
    "cmake": [
        "DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG",
    ],
    "stable": [
        "k_v13_native_dsr_no_tail_chain",
        "apply_v13_native_dsr_no_tail",
        "v13_native_dsr_no_tail_postcondition",
        "t10_sample == 0u",
        "t9_sample == 0u",
        "merge->opcode == 0x32u",
    ],
    "lerp": [
        "v13_native_dsr_no_tail_lerp_postcondition",
        "sample_count(words, 10u) == 0u",
        "sample_count(words, 9u) == 0u",
        "preserved_envdiffuse",
        "outcome.spec_rgb_consumer = false",
        "outcome.terminal_sat_rgb_composed = false",
    ],
    "runtime": [
        "k_v13_native_dsr_no_tail_diag",
        "prepare_native_dsr",
        "[DSRRL PMETAL V13 NO TAIL]",
        "no_specrgb_tail=1",
        "envdiffuse=stock",
        "lerp=paired",
    ],
}
texts={"cmake":cmake,"stable":stable,"lerp":lerp,"runtime":runtime}
for group, needles in required.items():
    for needle in needles:
        if needle not in texts[group]:
            raise SystemExit(f"MISSING {group}: {needle}")

# Exact diagnostic runtime must not require the PR199 EnvDiffuse source.
if "if (!k_v13_native_dsr_no_tail_diag &&" not in runtime:
    raise SystemExit("EnvDiffuse linear-source gate is not bypassed only for diagnostic")

# In the diagnostic branch, SpecRGB material-resource preparation and explicit
# t11/t13 rebinding must be compile-excluded.
if "#if !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)\n    ID3D11ShaderResourceView *shadow_material" not in runtime:
    raise SystemExit("SpecRGB resource block is not isolated")
if "#if !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)\n    // Diagnostic ownership test" not in runtime:
    raise SystemExit("PR199 EnvDiffuse resource rebind is not isolated")

print("PMETAL_V13_NATIVE_DSR_NO_TAIL_AUDIT_PASS")
