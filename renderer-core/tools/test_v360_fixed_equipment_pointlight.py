#!/usr/bin/env python3
"""Strict V360 post-materialization source contract; no runtime/pixel claims."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
src=(root/"integrated/integrated_addon.cpp").read_text("utf-8")
mat=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text("utf-8")
shader=(root/"src/operators/point_light/fixed_local_specular_single_materializer.cpp").read_text("utf-8")
cmake=(root/"integrated/CMakeLists.txt").read_text("utf-8")
build=(root/"tools/build_v360_fixed_ptde_equipment.ps1").read_text("utf-8")

def require(condition,what):
    if not condition:
        raise RuntimeError("V360 fixed-only source contract: "+what)

for needle in (
    "constexpr bool k_clustered_pointlight_runtime_enabled = false;",
    "if (k_clustered_pointlight_runtime_enabled) {",
    "if (k_clustered_pointlight_runtime_enabled &&\n        clustered_pnts_candidate)",
    "const bool clustered_pointlight_init_exact =\n        k_clustered_pointlight_runtime_enabled &&",
    "const bool clustered_pointlight_hooks =\n        k_clustered_pointlight_runtime_enabled &&",
    "if (k_clustered_pointlight_runtime_enabled)\n        g_clustered_pnts.frame_event(present);",
    "const bool fixed_pointlight_hooks =\n        k_pointlight_drawtime_runtime_enabled &&",
    "prepare_fixed_pointlight_material_requests(",
    "g_fixed_pointlight.prepare_t19(",
    "g_fixed_pointlight_pipeline.prepare_bound_shader(",
    "[DSRRL V360 FIXED EQUIPMENT] stage=addon_loaded",
    "[DSRRL V360 FIXED EQUIPMENT] stage=draw_issued",
    "[DSRRL V360 FIXED EQUIPMENT] tag=%s",
    "g_v360_fixed_restore_failed.fetch_add(",
):
    require(needle in src,"missing "+repr(needle))

require(src.count("const bool fixed_in_batch =\n        prepared.fixed_in_batch;")==2,
        "both indexed and nonindexed paths must be accounted")
require(src.count("if (fixed_in_batch) {")==2,
        "both dispatch result paths must account fixed draw")
require(src.count("account_effect_dispatch(")>=3, "effect matrix preserved")
for needle in ("spec_equipment_allowed(h1)","generated::diffuse_pair_allowed_v12(h1,h0)",
               "generated::normal_tuple_allowed_v12(","query.material.owner_tuple_exact",
               "prepare_fixed_pointlight_material_requests(","endpoint_a_exact"):
    require(needle in mat,"equipment material gate absent "+needle)
for needle in ("spec_capture.payload[8]=10u;","spec_b_capture.payload[8]=16u;",
               "append_mad_accum(","fixed_local_single_materialize_outcome"):
    require(needle in shader,"fixed local operator mismatch "+needle)
require("NOT DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL AND\n       NOT DSRRL_V360_FIXED_EQUIP_TEST)" in cmake,
        "CMake nonmetal guard not scoped specifically to V360")
for needle in ("build_v350_gpu_resident_pointlight_off_rc2.ps1",
               "materialize_v359_attested_gi_name_alias.py",
               "materialize_v360_fixed_equipment_pointlight.py",
               "'-DDSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=OFF'",
               "'-DDSRRL_POINTLIGHT_DRAWTIME_BYPASS=OFF'",
               "'-DDSRRL_V360_FIXED_EQUIP_TEST=ON'",
               " /DDSRRL_V360_FIXED_EQUIP_TEST"):
    require(needle in build,"V359 lineage or fixed route omitted: "+needle)
print("V360_FIXED_EQUIPMENT_CONTRACT_PASS construction_only runtime_open pixel_open")
