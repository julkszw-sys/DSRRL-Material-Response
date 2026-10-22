#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
trace=(root/"include/dsrrl/runtime/envdiffuse_cinematic_trace.hpp").read_text()
env=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
native=(root/"src/runtime/pmetal_native_draw_bridge.cpp").read_text()
integrated=(root/"integrated/integrated_addon.cpp").read_text()
assert "[DSRRL EDC SRC]" in env
assert "[DSRRL EDC READY]" in env
assert "source.envdiffuse_a[0]" in env and "source.envdiffuse_b[2]" in env
assert "prepared.envdiffuse_resources.ptde_a" in env
assert "prepared.envdiffuse_resources.ptde_b" in env
assert "context->PSGetShaderResources(11u,1u,&original_t11)" in env
assert "context->PSGetSamplers(13u,1u,&original_s13)" in env
assert "[DSRRL EDC NATIVE]" in native
assert native.count("const bool edc_restored = restore_state(")==4
assert native.count("issued_original=1 mutation_applied=1")==4
assert "original(" in native
assert "[DSRRL EDC FAILOPEN]" in integrated
assert "[DSRRL EDC HEARTBEAT]" in integrated
assert "g_pmetal_envspec.prepare(" in integrated
assert "g_pmetal_native_draw.arm_draw_indexed(" in integrated
assert "0xc9b879a8c5e624d0ull" in trace
assert "0xe447e0e74af8d1d3ull" in trace
assert "n<=1536u" in trace and "n%128u" in trace
assert "envdiffuse_cinematic_trace::reset();" in integrated
assert "envdiffuse_cinematic_trace::ready_count.fetch_add(" in env
print("EDC PTDE SOURCE CB RESOURCE NATIVE-COMMIT LOGGING CONTRACT PASS")
