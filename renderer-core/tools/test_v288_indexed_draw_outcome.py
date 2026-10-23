#!/usr/bin/env python3
"""V288 exact indexed/non-indexed draw outcome trace source invariants."""
from pathlib import Path
import runpy
root=Path(__file__).resolve().parents[2]
runpy.run_path(str(root/"renderer-core/tools/test_v287_nonmetal_lerp_trace.py"),run_name="__main__")
trace=(root/"renderer-core/include/dsrrl/runtime/v287_nonmetal_trace.hpp").read_text()
draw=(root/"renderer-core/integrated/integrated_addon.cpp").read_text()
assert "seen_dispatch{}" in trace
assert "dispatch_stage ? seen_dispatch : seen" in trace
assert '"atomic_indexed_"' in trace
for phrase in ('"atomic_indexed_draw_issued"','"atomic_indexed_failopen"','"atomic_indexed_native_armed"'):
    assert phrase in draw,phrase
assert '"atomic_replay_draw_issued"' in draw
assert "dispatch_island_draw_indexed_batch(" in draw
indexed=draw[draw.index("bool on_draw_indexed("):draw.index("void on_present(")]
assert "if (prepared.envspec_in_batch)" in indexed
assert 'dsrrl::runtime::draw_tx_issued(dispatch.transaction)' in indexed
assert "atomic_indexed_draw_issued" in indexed and "atomic_indexed_failopen" in indexed
assert "atomic_replay_draw_issued" not in indexed
assert "return dsrrl::runtime::draw_tx_issued(" in indexed
assert "release_prepared_island_batch(" in indexed
# Diagnostic extension only; the stock/ported draw code remains independent.
assert "g_pmetal_envspec.prepare(" in draw
assert "if (!g_pmetal_envspec.prepare(" in draw
print("V288_INDEXED_DRAW_OUTCOME_TRACE_PASS per_material=1 family1=1 dedicated_dispatch_bank=1")
print("IN_GAME_INDEXED_ACTIVATION_AND_PIXEL_OPEN")
