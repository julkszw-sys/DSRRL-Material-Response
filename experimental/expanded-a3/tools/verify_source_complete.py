#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
A3 = ROOT / "experimental" / "expanded-a3"

REQUIRED = [
    "README.md",
    "SOURCE_COMPLETENESS.md",
    "SOURCE_MANIFEST.json",
    "predecessors/r2d/closed_ops_companion.cpp",
    "predecessors/r2d/generate_closed_plan.py",
    "predecessors/a1/material_response_expanded.cpp",
    "predecessors/a2/a2_receiver_telemetry.cpp",
    "predecessors/ul/src/addon/addon.cpp",
    "predecessors/ul/src/addon/selector_hook.asm",
    "predecessors/ul/src/core/dxbc_checksum.cpp",
    "predecessors/ul/src/core/sha256.cpp",
    "predecessors/ul/include/dsrrl/dxbc_checksum.hpp",
    "predecessors/ul/include/dsrrl/sha256.hpp",
    "predecessors/ul/scripts/build_ul48_live.py",
    "predecessors/ul/scripts/build_windows_real_ptde_ul.ps1",
]

def main() -> int:
    missing = [p for p in REQUIRED if not (A3 / p).is_file()]
    if missing:
        print("SOURCE-COMPLETENESS FAIL: missing committed source:")
        for p in missing:
            print(" -", p)
        return 1

    manifest_path = A3 / "SOURCE_MANIFEST.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1:
        print("SOURCE-COMPLETENESS FAIL: unsupported SOURCE_MANIFEST schema")
        return 1

    external = {x.get("role"): x for x in manifest.get("external_inputs", [])}
    release = external.get("shipping_1_45_basis")
    if not release or release.get("sha256") != "e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342":
        print("SOURCE-COMPLETENESS FAIL: exact 1.45 basis identity is not locked")
        return 1

    ul_input = external.get("ul_param_input_artifact267")
    if not ul_input or ul_input.get("sha256") != "2c99a74b45cbc9627a9d3181063cfa6138ca1b6668a7a923d02aa3aa40c569fb":
        print("SOURCE-COMPLETENESS FAIL: U/L external input identity is not locked")
        return 1

    print("SOURCE-COMPLETENESS PASS")
    print(f"required_committed_files={len(REQUIRED)}")
    print("shipping_1_45_basis=e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342")
    print("ul_input_artifact267=2c99a74b45cbc9627a9d3181063cfa6138ca1b6668a7a923d02aa3aa40c569fb")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
