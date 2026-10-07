#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys

def fail(msg):
    raise RuntimeError(msg)

def require(text, needle, label):
    if needle not in text:
        fail(f"{label}: missing {needle!r}")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source-dir",required=True)
    ns=ap.parse_args()
    root=Path(ns.source_dir).resolve()

    draw=(root/"src/runtime/clustered_pnts_draw_runtime.cpp").read_text(encoding="utf-8")
    integrated=(root/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    flver=(root/"src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")

    require(integrated,
        "constexpr bool k_clustered_pointlight_receiver_runtime_enabled = false;",
        "clustered receiver hard-off")
    require(integrated,
        "flver_identity_transport::install(\n            false, // clustered PntS is source-only; no builder/receiver bridge",
        "clustered builder detour hard-off")
    require(integrated,
        "g_clustered_pointlight_selection_transport_active.store(\n        false,",
        "clustered selector transport hard-off")
    require(draw,"k_clustered_source_override_rva = 0xB7E02u;",
        "exact source cut")
    require(draw,"k_clustered_source_override_preimage",
        "exact source preimage")
    require(draw,"capture_clustered_live_drawparam_source(",
        "live DrawParam source decode")
    require(draw,"g_source_dsr_only_fail_open",
        "DSR-only row fail-open")
    require(draw,"g_source_unclassified_bank_fail_open",
        "unclassified/default/m99 fail-open")

    a=draw.find("float __fastcall clustered_source_override_callback(")
    b=draw.find("bool build_clustered_source_override_stub(",a)
    if a<0 or b<=a:
        fail("source callback boundaries missing")
    callback=draw[a:b]
    if "pointlight_ptde_source::capture(" in callback:
        fail("active Clustered callback still performs embedded donor lookup")
    for token in (
        "evaluate_direct_pointlight_material_identity(",
        "build_clustered_sidecar_v1(",
        "select_first_four_exact(",
    ):
        if token in callback:
            fail("active Clustered callback contains retired receiver/material work: "+token)

    publish_start=flver.find("bool publish_exact_selector_identity(")
    publish_end=flver.find('extern "C" void dsrrl_clustered_pnts_builder_observer(',publish_start)
    if publish_start>=0 and publish_end>publish_start:
        publish=flver[publish_start:publish_end]
        if "clustered_pnts_selector_event_bridge(" in publish:
            fail("FLVER selector still dispatches Clustered receiver bridge")
        if "direct_pointlight_material_candidate(" in publish:
            fail("FLVER selector still performs Clustered material candidate lookup")

    print("Clustered PntS source-first activation audit: PASS")
    print("  base=R43 clustered=source-only carrier=live-DrawParam receiver=stock-DSR donor-lookup=OFF material-lookup=OFF builder-detour=OFF")
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Clustered PntS source-first activation audit: FAIL: {exc}",file=sys.stderr)
        raise SystemExit(1)
