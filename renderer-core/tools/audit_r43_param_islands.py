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
    dof_handoff=(root/"src/runtime/dof_tonemap_handoff_runtime.cpp").read_text(encoding="utf-8")
    dof_contract=(root/"include/dsrrl/operators/dof/dof_island.hpp").read_text(encoding="utf-8")
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
    cb0=cluster.find("float __fastcall clustered_source_override_callback(")
    if cb0 < 0:
        cb0=cluster.find("float __fastcall clustered_source_override_callback(")
    cb1=cluster.find("bool build_clustered_source_override_stub(",cb0)
    if cb0<0 or cb1<0:
        fail("clustered source callback boundaries missing")
    if "float __fastcall clustered_source_override_callback(" in cluster:
        require(cluster, "record+0x28 = PTDE attenuation authority",
            "R44 per-light attenuation marker carrier")
    cb=cluster[cb0:cb1]
    if "pointlight_ptde_source::capture(" in cb:
        fail("clustered active callback still uses embedded donor lookup")
    require(cluster,
        "record+0x28 = PTDE attenuation authority",
        "R44 row-aware t18 marker")
    require(cluster,
        "{0xF3,0x0F,0x11,0x44,0xC8,0x28}",
        "R44 single marker store")
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
    require(dof_contract,"tonemap_pass13_executor_rva = 0u;",
        "DoF pass13 executor contract field")
    require(dof_contract,"0x004572A0u",
        "DoF exact retail pass13 executor")
    require(dof_handoff,"dsr_active_output_cut.tonemap_pass13_executor_rva",
        "DoF handoff uses canonical pass13 executor")
    require(dof_handoff,"constexpr std::size_t k_stolen = 14u;",
        "DoF pass13 instruction-boundary hook size")
    if "0x00457E50u" in dof_handoff:
        fail("DoF handoff regressed to pass 0x1C/0x1D executor 0x457E50")
    require(cluster,
        "bank_identity=ROW0_EXACT_PAYLOAD",
        "Clustered lightweight bank gate marker")
    require(cluster,
        "k_clustered_source_bank_fingerprints",
        "Clustered exact sparse bank fingerprints")
    require(cluster,
        "clustered_source_bank_identity_cache_index(",
        "Clustered direct bank-identity cache")
    require(cluster,
        "stock_dsr_signature_complete_unknown",
        "Clustered unknown-bank fail-open")
    require(cluster,
        "dsr_only_semantic_row(bank, row_id)",
        "Clustered ten-row DSR-only gate")
    for forbidden in (
        "clustered_source_bank_guard(",
        "clustered_bank_structure_signature(",
        "bank_structure_revalidate=TABLE_PLUS_NAMES_ONLY",
    ):
        if forbidden in cluster:
            fail("Clustered lightweight bank gate regressed to full structure/name scan: "+forbidden)
    require(integrated,"[DSRRL DoF R43-DRAWPARAM]",
        "DoF live DrawParam marker")
    require(integrated,"[DSRRL DoF R43-HANDOFF] pass=0x13 executor_rva=0x4572A0 source=pass_desc+0x0C alias=image_state+0x104",
        "DoF exact pass13 handoff marker")
    require(integrated,"[DSRRL R43 PARAM SOURCE FIX V2]",
        "R43 PARAM source fix marker")
    require(integrated,"[DSRRL TELEMETRY R43-PARAM]",
        "full telemetry marker")
    require(cmake,"DSRRL_DOF_DEFAULT_ON","DoF build option")
    require(cmake,"dsrrl_core_dof_plain_rate_embedded","DoF-rate shader generator")
    require(cmake,"DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE",
        "R43 physical-cut option preserved")

    print("R43_PARAM_ISLANDS_PASS: exact R43 base + Clustered live PointLightBank source + R44 row-aware Spc attenuation carrier + DoF live DoFBank + donor-free authored rows")

if __name__=="__main__":
    main()
