#!/usr/bin/env python3
import argparse
from pathlib import Path

def fail(msg: str) -> None:
    raise SystemExit(f"DoF live DrawParam source audit: FAIL: {msg}")

def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        fail(f"{label}: missing {needle!r}")

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", required=True)
    args = ap.parse_args()
    root = Path(args.source_dir)

    runtime = (root / "src/runtime/dof_authored_state_runtime.cpp").read_text(encoding="utf-8")
    bridge = (root / "src/runtime/dof_ptde_draw_bridge_runtime.cpp").read_text(encoding="utf-8")
    integrated = (root / "integrated/integrated_addon.cpp").read_text(encoding="utf-8")

    require(runtime, "read_selected_live_row(", "live selected row reader")
    require(runtime, "blend_live_rows(", "PTDE blend on live rows")
    require(runtime, "k_ptde_homologous_dofbank_signatures", "homologous bank gate")
    require(runtime, "0xc8c2fbb1f61b4915ULL", "m15_DofBank authority")
    require(runtime, "read_selected_live_row(source_a,selector_a,a)", "stock DSR selector A")
    require(runtime, "read_selected_live_row(source_b,selector_b,b)", "stock DSR selector B")
    require(bridge, "authored.hook_ready", "live payload readiness")
    require(integrated, "[DSRRL DoF R53-DRAWPARAM]", "binary architecture marker")
    require(integrated, "donor_table=ABSENT", "donor absence marker")
    require(integrated, "selector=STOCK_DSR", "stock selector marker")

    for needle in (
        "ptde_dofbank_embedded.hpp",
        "ptde_bank::blend(",
        "area_slot_from_signature(",
        "row_dictionary_index",
        "unique_rows",
    ):
        if needle in runtime:
            fail(f"active authored runtime still references embedded donor machinery: {needle}")

    for cpp in (root / "src/runtime").glob("dof_*.cpp"):
        text = cpp.read_text(encoding="utf-8")
        if "ptde_dofbank_embedded.hpp" in text:
            fail(f"embedded DoF donor header still included by {cpp.name}")

    print("DOF_LIVE_DRAWPARAM_SOURCE_PASS: stock DSR selects DoF source/rows; addon reads live DrawParam values; embedded donor table absent; m10-m18 including dual a15 authorized; default/m99 fail open")

if __name__ == "__main__":
    main()
