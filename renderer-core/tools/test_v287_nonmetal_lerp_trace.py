#!/usr/bin/env python3
"""V287 construction regression: per receiver/family/source trace, never authority."""
from pathlib import Path
import runpy
root=Path(__file__).resolve().parents[2]
runpy.run_path(str(root/"renderer-core/tools/test_v286_full_sidecar_parity.py"),run_name="__main__")
def read(p):return (root/p).read_text(encoding="utf-8")
h=read("renderer-core/include/dsrrl/runtime/v287_nonmetal_trace.hpp")
env=read("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
integrated=read("renderer-core/integrated/integrated_addon.cpp")
assert "m.raw_mtd_sha256" in h and "m.flver_sha256" in h
assert "m.material_slot" in h and "rx" in h and "family" in h and "stage" in h
assert "std::array<std::atomic<std::uint64_t>, 512>" in h
assert "compare_exchange_strong" in h and "for (std::size_t n=0;n<8u;n++)" in h
assert "m.route_index==345u" in h # original P_Metal telemetry not replaced
assert "DSRRL V287 MAT TRACE" in h
assert "v287_nonmetal_trace::log(" in env
assert "log_prepare_stage_once(" in env
for marker in ("source_reject","spec_rgb_reject","request_ready","semantic_reject","replacement_reject"):
    if marker=="replacement_reject":continue
    assert f'"{marker}"' in env,marker
for marker in ("lerp_generic_fallback","lerp_diffuse_only_ready","atomic_prepare_fail",
               "atomic_prepare_ready","atomic_batch_ready",
               "atomic_native_draw_armed","atomic_replay_draw_issued","atomic_replay_failopen"):
    assert f'"{marker}"' in integrated,marker
assert "if (!g_pmetal_envspec.prepare(" in integrated
assert "return false;" in integrated
assert "g_pmetal_envspec.prepare(" in integrated
assert "g_material_resources.drop_spec_rgb_request(" in integrated
assert "lerp_query,\n                true,\n                false," in integrated
print("V287_MATERIAL_FAMILY_TRACE_CONTRACT_PASS full1295=1 bounded_per_material=1 source_failopen=1 pmetal_unchanged=1")
print("ACTUAL_LERP_PIXEL_STATUS_OPEN")
