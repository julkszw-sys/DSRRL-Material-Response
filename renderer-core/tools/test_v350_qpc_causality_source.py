#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2]
def read(p): return (r/p).read_text("utf-8")
h=read("renderer-core/include/dsrrl/runtime/qpc_causality_bisect.hpp")
f=read("renderer-core/src/runtime/flver_engine_hooks.cpp")
g=read("renderer-core/src/runtime/flver_identity_registry.cpp")
m=read("renderer-core/src/runtime/material_resource_draw_runtime.cpp")
a=read("renderer-core/integrated/integrated_addon.cpp")
b=read("renderer-core/tools/build_v350_qpc_bisect_one_addon.ps1")
for key in ("DSRRL_QPC_MODE.txt","QPC_READS_TLS_ONLY","ATOMIC_RMW_ONLY","QPC_PLUS_ATOMIC_RMW","QueryPerformanceCounter"):
    assert key in h,key
for key in ("stage::selector","stage::pmetal_selector","stage::mtd_observe","stage::flver_parse"):
    assert key in f,key
for key in ("stage::flver_digest","stage::flver_registry"):assert key in g,key
for key in ("stage::resource_view_init","stage::dds_load","stage::create_texture"):assert key in m,key
assert "qpc_bisect::init_from_game_directory();" in a
assert "qpc_bisect::report();" in a
assert "v350_rc1_qpc_causality_four_modes" in b
assert "DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS" in b
print("V350_QPC_SAME_BINARY_MODES_SOURCE_PASS")
