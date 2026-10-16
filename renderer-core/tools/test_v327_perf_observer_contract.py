#!/usr/bin/env python3
"""V327 no-diagnostic-census profile source invariance: static, no FPS claims."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
cmake = (ROOT/"renderer-core/integrated/CMakeLists.txt").read_text(encoding="utf-8")
builder = (ROOT/"renderer-core/tools/build_v327_v323_perf_one_addon.ps1").read_text(encoding="utf-8")
cpp = (ROOT/"renderer-core/integrated/integrated_addon.cpp").read_text(encoding="utf-8")
v290 = (ROOT/"renderer-core/include/dsrrl/runtime/v290_spec_draw_census.hpp").read_text(encoding="utf-8")
v291 = (ROOT/"renderer-core/include/dsrrl/runtime/v291_material_stage_trace.hpp").read_text(encoding="utf-8")

def require(value, tag):
    if not value:
        raise SystemExit("V327 FAIL: "+tag)

# The V293 real FLVER owner bridge must remain ON; it cannot be dropped merely
# to disable the V291 instrumented material identity collector.
section=cmake.split("if(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)",1)[1].split("endif()",1)[0]
require("NOT DSRRL_V285_ATOMIC_NONMETAL_DIAG" in section, "V293 receiver guard lost")
require("NOT DSRRL_V291_MATERIAL_STAGE_CENSUS" not in section, "V293 still blocked by logging")
required=[
    "-DDSRRL_V285_ATOMIC_NONMETAL_DIAG=ON",
    "-DDSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON",
    "-DDSRRL_V293_PARSER_EXACT_FLVER_ALIAS=ON",
    "-DDSRRL_V315_DIRECT_749_RUNTIME_LEDGER",
    "-DDSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW=ON",
    "-DDSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON",
    "-DDSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON",
    "-DDSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE=ON",
    "test_v323_exact_resource_transport_contract.py",
]
for token in required:
    require(token in builder,"lost baseline semantically required "+token)
for tag in ["DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS=ON",
            "DSRRL_V291_MATERIAL_STAGE_CENSUS=ON",
            "DSRRL_V291_LERP_CUBEMAP_CENSUS=ON",
            "DSRRL_FULL_TELEMETRY_DEFAULT_ON=ON"]:
    require(tag not in builder,"diagnostic inadvertently ON: "+tag)
require("#if defined(DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS)" in cpp,"V290 callsite guard absent")
require("#if defined(DSRRL_V291_MATERIAL_STAGE_CENSUS)" in cpp,"V291 callsite guard absent")
require("diagnostic-only" in v290.lower() and "opt-in read-only" in v291.lower(),
        "source evidence that diagnostics are passive absent")
require("build-v327" in builder and "out-v327" in builder,"output not isolated")
print("V327 STATIC SOURCE CONTRACT PASS: V293 exact FLVER parser owner kept; V285/R19/V289/V315 and pointlight cuts kept; V290/V291 diag-only observers OFF; V323 resource bridge unchanged")
