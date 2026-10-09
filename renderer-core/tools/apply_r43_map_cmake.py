#!/usr/bin/env python3
"""Enable one exact P_Metal map-source operator flag in integrated addon."""
from pathlib import Path
p=Path("renderer-core/integrated/CMakeLists.txt")
s=p.read_text()
assert "DSRRL_PMETAL_R43_MAP_SOURCE_JOIN" not in s
s+="""

# R43/2.0.2 map-qualified LightBank selector, opt-in exact receiver island.
option(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN
    "P_Metal source by exact material and bound GI map, not latest process bank"
    OFF)
if(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
    target_compile_definitions(
        dsrrl_renderer_core_integrated PRIVATE
        DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
endif()
add_executable(dsrrl_pmetal_map_source_test
    "${DSRRL_RENDERER_CORE_ROOT}/tests/pmetal_map_source_test.cpp")
target_include_directories(dsrrl_pmetal_map_source_test PRIVATE
    "${DSRRL_RENDERER_CORE_ROOT}/include")
target_compile_features(dsrrl_pmetal_map_source_test PRIVATE cxx_std_17)
"""
p.write_text(s)
print("PASS R43 opt-in P_Metal per-map source flag")
