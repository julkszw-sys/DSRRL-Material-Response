#!/usr/bin/env python3
from pathlib import Path
import argparse

FORBIDDEN = (
    "hemdir3_mode_detour.asm",
    "src/runtime/subsurface_pipeline_registry.cpp",
    "src/runtime/subsurface_draw_runtime.cpp",
    "src/runtime/upper_lower_draw_runtime.cpp",
    "src/runtime/upper_lower_pipeline_registry.cpp",
    "src/runtime/upper_lower_hemenv_draw_runtime.cpp",
    "src/runtime/hemdir3_mode_transport.cpp",
    "src/runtime/hemdir3_pipeline_registry.cpp",
    "src/runtime/hemdir3_draw_runtime.cpp",
    "src/operators/lightbank/hemdir3_b13_materializer.cpp",
    "src/operators/lightbank/upper_lower_hemenv_materializer.cpp",
)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source-dir",required=True)
    ns=ap.parse_args()
    root=Path(ns.source_dir)
    cm=(root/"integrated/CMakeLists.txt").read_text(encoding="utf-8")
    for item in FORBIDDEN:
        if item in cm:
            raise SystemExit(f"PHYSICAL_CUT_FAIL forbidden source still linked: {item}")
    if "physical_cut_disabled_islands_stubs.cpp" not in cm:
        raise SystemExit("PHYSICAL_CUT_FAIL missing ABI stub TU")
    if "dsrrl_core_hemdir3_authority" in cm.split("add_dependencies(",1)[1].split(")",1)[0]:
        raise SystemExit("PHYSICAL_CUT_FAIL HemDir3 authority still in integrated dependencies")
    if "dsrrl_core_upper_lower_authority" in cm.split("add_dependencies(",1)[1].split(")",1)[0]:
        raise SystemExit("PHYSICAL_CUT_FAIL U/L authority still in integrated dependencies")
    print("DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE_AUDIT_PASS")

if __name__=="__main__":
    main()
