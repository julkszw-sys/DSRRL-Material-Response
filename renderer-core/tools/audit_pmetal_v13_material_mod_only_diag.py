#!/usr/bin/env python3
from pathlib import Path
import csv
import subprocess
import sys
import tempfile

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")
stable = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_materializer.cpp").read_text(encoding="utf-8")
lerp = (root/"src"/"operators"/"env_spec"/"pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")
runtime = (root/"src"/"runtime"/"pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
resources_h = (root/"include"/"dsrrl"/"runtime"/"material_resource_draw_runtime.hpp").read_text(encoding="utf-8")
resources_cpp = (root/"src"/"runtime"/"material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
cmake = (root/"integrated"/"CMakeLists.txt").read_text(encoding="utf-8")
tsv_path = root/"data"/"provenance"/"pmetal_envspec_hemenvlerp_v1.tsv"
gen_path = root/"tools"/"generate_pmetal_envspec_hemenvlerp_v1.py"

for needle in [
    "DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG",
    "apply_v13_native_dsr_material_mod_only",
    "v13_native_dsr_material_mod_postcondition",
    "specrgb_c101_color0=envspec_only",
    "keep_only_spec_rgb_request",
]:
    if needle not in stable + lerp + runtime + resources_h + resources_cpp + cmake:
        raise SystemExit(f"MISSING: {needle}")

# Stable invariant: the new material factor multiplies only r1.yzw immediately
# inside the former NOP corridor, and t10 is the only new texture sample.
for needle in [
    "0x001000e2u,1u",
    "0x00100e56u,1u",
    "0x00100246u,12u",
    "t9_sample != 0u",
    "t14_sample != 1u",
]:
    if needle not in stable:
        raise SystemExit(f"stable invariant missing: {needle}")

# Runtime invariant: stock EnvDiffuse is not rebound/translated in this profile,
# while the material bundle is filtered down to only the explicit SpecRGB request.
if "k_v13_preserve_stock_envdiffuse" not in runtime:
    raise SystemExit("stock EnvDiffuse preservation gate missing")
if "material_resources_.keep_only_spec_rgb_request" not in runtime:
    raise SystemExit("SpecRGB-only runtime filter missing")

# Exact Lerp COLOR0 provenance recovered from the retained DSR shader binder.
with tsv_path.open("r", encoding="utf-8", newline="") as f:
    rows = list(csv.DictReader(f, delimiter="\t"))
expected = {9:6, 10:7, 11:6}
if {int(r["pair_index"]): int(r["color0_register"]) for r in rows} != expected:
    raise SystemExit("Lerp COLOR0 provenance mismatch")

with tempfile.TemporaryDirectory() as td:
    out = Path(td)/"generated.hpp"
    subprocess.run([
        sys.executable, str(gen_path),
        "--input", str(tsv_path),
        "--output", str(out),
    ], check=True)
    generated = out.read_text(encoding="utf-8")
    if "std::uint32_t color0_register;" not in generated:
        raise SystemExit("generated Lerp authority lacks COLOR0 register")
    for token in ("9u,33u", "10u,34u", "11u,35u"):
        if token not in generated.replace(" ",""):
            raise SystemExit(f"generated Lerp row missing: {token}")

print("PMETAL_V13_MATERIAL_MOD_ONLY_AUDIT_PASS")
