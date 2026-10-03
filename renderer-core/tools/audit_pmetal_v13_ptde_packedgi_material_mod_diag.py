#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")
cmake = (root/"integrated"/"CMakeLists.txt").read_text(encoding="utf-8")
runtime = (root/"src"/"runtime"/"pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
stable = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_materializer.cpp").read_text(encoding="utf-8")
lerp = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")

required = [
    "DSRRL_PMETAL_V13_PTDE_PACKEDGI_MATERIAL_MOD_DIAG",
    "DSRRL_PMETAL_FORCE_PTDE_PACKEDGI",
    "DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG",
    "apply_v13_native_dsr_material_mod_only",
    "apply_v13_lerp_material_mod_only",
    "mode=ptde_packedgi_rgba_decode_ptde_ab_beta",
    "specrgb_c101_color0=envspec_only",
    "PR208 corrected stable path",
    "!apply_build131(",
    "!final_postcondition(",
]
blob = cmake + runtime + stable + lerp
for needle in required:
    if needle not in blob:
        raise SystemExit(f"MISSING: {needle}")

profile = cmake.split("if(DSRRL_PMETAL_V13_PTDE_PACKEDGI_MATERIAL_MOD_DIAG)", 1)[1]
profile = profile.split("endif()", 1)[0]
if "DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG" not in profile:
    raise SystemExit("PTDE PackedGI profile must reuse exact PR204 material-mod shader/operator")
if "DSRRL_PMETAL_FORCE_PTDE_PACKEDGI" not in profile:
    raise SystemExit("PTDE PackedGI profile must force PTDE resource carrier")
if "DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED" in profile:
    raise SystemExit("PTDE PackedGI profile must not enable native DSR cubemap feed")

guard = runtime.split("#if defined(DSRRL_PMETAL_FORCE_PTDE_PACKEDGI)", 1)
if len(guard) != 2 or "constexpr bool k_native_dsr_cubemap_feed = false;" not in guard[1].split("#elif",1)[0]:
    raise SystemExit("PTDE PackedGI override does not dominate native DSR carrier selection")

stable_diag = stable.split("#if defined(DSRRL_PMETAL_FORCE_PTDE_PACKEDGI)", 1)
if len(stable_diag) != 2:
    raise SystemExit("Stable PR208 path is not separately gated")
stable_diag = stable_diag[1].split("#else", 1)[0]
if "!apply_build131(" not in stable_diag:
    raise SystemExit("Stable PR208 must reuse recovered Build131 decoded EnvSpec operator")
if "false,\n            false" not in stable_diag:
    raise SystemExit("Stable PR208 must stop before terminal SAT composition")
if "apply_v13_native_dsr_no_tail(" in stable_diag:
    raise SystemExit("Stable PTDE PackedGI path must not regress to native-DSR V13 no-decode chain")

lerp_diag = lerp.split("#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)", 1)
if len(lerp_diag) != 2 or "k_ptde_rgba_envspec_chain" not in lerp:
    raise SystemExit("Lerp PR208 must retain recovered PTDE RGBA decode chain")

print("PMETAL_V13_PTDE_PACKEDGI_MATERIAL_MOD_AUDIT_PASS")
