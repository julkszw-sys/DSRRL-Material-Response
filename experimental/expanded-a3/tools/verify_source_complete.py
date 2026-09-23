#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
A3 = ROOT / "experimental" / "expanded-a3"

LINEAGE_REQUIRED = [
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

CURRENT_IMPLEMENTATION_REQUIRED = [
    "CURRENT_IMPLEMENTATION_MANIFEST.json",
    "BUILD_AUDIT.json",
]

SHIPPING_145_SHA = "e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342"
UL_INPUT_SHA = "2c99a74b45cbc9627a9d3181063cfa6138ca1b6668a7a923d02aa3aa40c569fb"

def fail(message: str, items: list[str] | None = None) -> int:
    print("SOURCE-COMPLETENESS FAIL:", message)
    for item in items or []:
        print(" -", item)
    return 1

def main() -> int:
    missing = [p for p in LINEAGE_REQUIRED if not (A3 / p).is_file()]
    if missing:
        return fail("missing committed predecessor/provenance source", missing)

    manifest = json.loads((A3 / "SOURCE_MANIFEST.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != 1:
        return fail("unsupported SOURCE_MANIFEST schema")

    external = {x.get("role"): x for x in manifest.get("external_inputs", [])}
    release = external.get("shipping_1_45_basis")
    if not release or release.get("sha256") != SHIPPING_145_SHA:
        return fail("shipping 1.45 compatibility-oracle identity is not locked")

    ul_input = external.get("ul_param_input_artifact267")
    if not ul_input or ul_input.get("sha256") != UL_INPUT_SHA:
        return fail("U/L external input identity is not locked")

    print("LINEAGE-PROVENANCE PASS")
    print(f"predecessor_required_files={len(LINEAGE_REQUIRED)}")
    print(f"shipping_1_45_compatibility_oracle={SHIPPING_145_SHA}")

    missing_current = [p for p in CURRENT_IMPLEMENTATION_REQUIRED if not (A3 / p).is_file()]
    if missing_current:
        return fail(
            "A3 does not yet contain a committed monolithic current implementation/build audit; "
            "the shipping 1.45 addon cannot satisfy the implementation-source requirement",
            missing_current,
        )

    current = json.loads((A3 / "CURRENT_IMPLEMENTATION_MANIFEST.json").read_text(encoding="utf-8"))
    if current.get("uses_shipping_addon_as_implementation_basis") is not False:
        return fail("current implementation manifest does not explicitly reject shipping-binary implementation inheritance")

    source_paths = current.get("implementation_source_paths") or []
    if not source_paths:
        return fail("no current implementation source paths declared")

    missing_impl = [p for p in source_paths if not (A3 / p).is_file()]
    if missing_impl:
        return fail("declared current implementation source is missing", missing_impl)

    print("SOURCE-COMPLETENESS PASS")
    print(f"current_implementation_files={len(source_paths)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
