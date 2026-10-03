#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")
stable = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_materializer.cpp").read_text(encoding="utf-8")
lerp = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")
runtime = (root/"src"/"runtime"/"pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
cmake = (root/"integrated"/"CMakeLists.txt").read_text(encoding="utf-8")

required = [
    "DSRRL_PMETAL_PTDE_PACKEDGI_POSTMERGE_VISIBILITY_DIAG",
    "DSRRL_PMETAL_POSTMERGE_VISIBILITY_BYPASS_DIAG",
    "bypass_host_environment_visibility_join_diag",
    "postmerge_visibility_bypass_postcondition",
    "baseline=ptde_packedgi_build131",
    "mode=bypass_post_env_merge_host_visibility",
    "words[visibility_word + 5u] = 0x00004001u",
    "words[visibility_word + 6u] = 0x3f800000u",
]
blob = stable + runtime + cmake
for needle in required:
    if needle not in blob:
        raise SystemExit(f"MISSING: {needle}")

profile = cmake.split("if(DSRRL_PMETAL_PTDE_PACKEDGI_POSTMERGE_VISIBILITY_DIAG)", 1)
if len(profile) != 2:
    raise SystemExit("missing dedicated visibility profile")
profile = profile[1].split("endif()", 1)[0]
for macro in [
    "DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG",
    "DSRRL_PMETAL_FORCE_PTDE_PACKEDGI",
    "DSRRL_PMETAL_POSTMERGE_VISIBILITY_BYPASS_DIAG",
]:
    if macro not in profile:
        raise SystemExit(f"profile missing {macro}")

stable_diag = stable.split("PR208 corrected stable path", 1)
if len(stable_diag) != 2:
    raise SystemExit("missing corrected PR208 stable baseline")
stable_diag = stable_diag[1].split("#else", 1)[0]
if "!apply_build131(" not in stable_diag:
    raise SystemExit("visibility diagnostic baseline must use decoded Build131")
if "bypass_host_environment_visibility_join_diag" not in stable_diag:
    raise SystemExit("visibility diagnostic not composed after Build131")

helper = stable.split("bool bypass_host_environment_visibility_join_diag", 1)
if len(helper) != 2:
    raise SystemExit("missing PR200 visibility helper")
helper = helper[1].split("bool postmerge_visibility_bypass_postcondition", 1)[0]
for token in [
    "authority.remove_visibility_exponent",
    "authority.merge_word",
    "k_build131_inserted_words = 15u",
    "0x09000032u",
    "0x07000038u",
    "0x00004001u",
    "0x3f800000u",
]:
    if token not in helper:
        raise SystemExit(f"visibility helper contract missing {token}")

# This diagnostic is intentionally stable-only. Do not silently extend it to
# the paired HemEnvLerp family without separate receiver proof.
if "DSRRL_PMETAL_POSTMERGE_VISIBILITY_BYPASS_DIAG" in lerp:
    raise SystemExit("visibility bypass must not mutate HemEnvLerp")

print("PMETAL_PTDE_PACKEDGI_POSTMERGE_VISIBILITY_AUDIT_PASS")
