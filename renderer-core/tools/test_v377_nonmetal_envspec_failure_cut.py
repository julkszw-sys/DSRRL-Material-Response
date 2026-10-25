#!/usr/bin/env python3
"""V377 narrow, read-only nonmetal EnvSpec failure-stage witness."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
start = src.index("void log_prepare_stage_once(")
end = src.index("#if defined(DSRRL_PMETAL_FORCE_PTDE_PACKEDGI)", start)
cut = src[start:end]
assert "#if defined(DSRRL_V230_RUNTIME_TELEMETRY)" in cut
assert "[DSRRL V377 NONMETAL ENVSPEC CUT]" in cut
for route in (232,160,88,12,41,359):
    assert "case %du:" % route in cut
assert "material.route_index != 345u" in cut
assert "std::array<std::atomic<std::uint32_t>,24u>" in cut
assert "material.owner_tuple_exact && material.material_slot_valid" in cut
assert "decision.receiver_id == 33u || decision.receiver_id == 34u" in cut
assert "seen[key].fetch_or(bit" in cut
assert "v287_nonmetal_trace::log(" in cut
assert "return;" in cut
for forbidden in ("PSSet", "PSGet", "DrawIndexed(",
                  "prepare_bound(", "source_.latest("):
    assert forbidden not in cut, forbidden
src_v287 = (root / "include/dsrrl/runtime/v287_nonmetal_trace.hpp").read_text("utf-8")
assert "DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS" in src_v287
assert "return;" in src_v287[src_v287.index("DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS"):][:130]
print("V377_NONMETAL_ENVSPEC_FAILURE_CUT_SOURCE_PASS runtime=OPEN pixel=OPEN")
