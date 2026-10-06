#!/usr/bin/env python3
import argparse
from pathlib import Path

def fail(msg):
    raise SystemExit("R43 PARAM islands audit: FAIL: " + msg)

def require(text, needle, label):
    if needle not in text:
        fail(f"{label}: missing {needle!r}")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source-dir",required=True)
    ns=ap.parse_args()
    root=Path(ns.source_dir)

    integrated=(root/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    cluster=(root/"src/runtime/clustered_pnts_draw_runtime.cpp").read_text(encoding="utf-8")
    dof=(root/"src/runtime/dof_authored_state_runtime.cpp").read_text(encoding="utf-8")
    dof_host=(root/"src/runtime/dof_host_depth_route_runtime.cpp").read_text(encoding="utf-8")
    cmake=(root/"integrated/CMakeLists.txt").read_text(encoding="utf-8")

    require(integrated,
        "constexpr bool k_clustered_pointlight_receiver_runtime_enabled = false;",
        "clustered receiver retirement")
    require(integrated,
        "flver_identity_transport::install(\n            false, // clustered PntS is source-only; no builder/receiver bridge",
        "clustered FLVER builder disabled")
    require(cluster,"k_clustered_source_override_rva = 0xB7E02u",
        "clustered exact source cut")
    require(cluster,"capture_clustered_live_drawparam_source(",
        "clustered live DrawParam source")
    require(cluster,"g_source_dsr_only_fail_open",
        "clustered DSR-only source gate")
    cb0=cluster.find("void __fastcall clustered_source_override_callback(")
    cb1=cluster.find("bool build_clustered_source_override_stub(",cb0)
    if cb0<0 or cb1<0:
        fail("clustered source callback boundaries missing")
    cb=cluster[cb0:cb1]
    if "pointlight_ptde_source::capture(" in cb:
        fail("clustered active callback still uses embedded donor lookup")
    for needle in (
        "evaluate_direct_pointlight_material_identity(",
        "build_clustered_sidecar_v1(",
    ):
        if needle in cb:
            fail("clustered source callback depends on receiver/material work")

    require(dof,"read_selected_live_row(source_a,selector_a,a)",
        "DoF stock-selected live row A")
    require(dof,"read_selected_live_row(source_b,selector_b,b)",
        "DoF stock-selected live row B")
    require(dof,"0xc8c2fbb1f61b4915ULL","DoF second a15 bank")
    for needle in (
        "ptde_dofbank_embedded.hpp",
        "ptde_bank::blend(",
        "area_slot_from_signature(",
    ):
        if needle in dof:
            fail("DoF authored runtime still uses donor machinery: "+needle)

    require(dof_host,"0x24B8u",
        "DoF retail PASS01 support selector")
    require(dof_host,"0xC0u",
        "DoF alternate support resource")
    require(dof_host,"0x88u",
        "DoF primary support resource")
    if "0x250u" in dof_host or "0x230u" in dof_host:
        fail("DoF PASS01 classifier regressed to unrelated +0x250/+0x230 resources")
    require(dof,"std::uint8_t far_mul = 0u;",
        "DoF live far multiplier byte")
    require(dof,"std::uint8_t near_mul = 0u;",
        "DoF live near multiplier byte")
    require(cluster,"live_bank_structure",
        "Clustered one-shot source failure stage")
    require(integrated,"[DSRRL DoF R43-DRAWPARAM]",
        "DoF live DrawParam marker")
    require(integrated,"[DSRRL R43 PARAM SOURCE FIX V2]",
        "R43 PARAM source fix marker")
    require(integrated,"[DSRRL TELEMETRY R43-PARAM]",
        "full telemetry marker")
    require(cmake,"DSRRL_DOF_DEFAULT_ON","DoF build option")
    require(cmake,"dsrrl_core_dof_plain_rate_embedded","DoF-rate shader generator")
    require(cmake,"DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE",
        "R43 physical-cut option preserved")

    print("R43_PARAM_ISLANDS_PASS: exact R43 base + Clustered live PointLightBank source + DoF live DoFBank + stock receivers + donor-free authored rows")

if __name__=="__main__":
    main()
