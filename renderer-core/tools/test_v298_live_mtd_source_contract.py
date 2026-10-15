#!/usr/bin/env python3
"""V298 source contract. Construction only; never a runtime/pixel verdict."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
engine=(root/"renderer-core/src/runtime/flver_engine_hooks.cpp").read_text()
integ=(root/"renderer-core/integrated/integrated_addon.cpp").read_text()
build=(root/"renderer-core/tools/build_v298_one_atomic_addon.ps1").read_text()
policy=(root/"renderer-core/include/dsrrl/runtime/v296_oct10_material_atomic_gate.hpp").read_text()
for needle in (
  "g_v298_live_parser_mtd.erase(material)",
  "g_v298_live_parser_mtd.publish(material,digest,len)",
  "g_v298_live_parser_mtd.lookup(actual_material,live)",
  "live.raw==identity.raw_mtd_sha256",
  "DSRRL V298 MTD",
  "stock_fail_open",
  "DSRRL_V298_LIVE_MTD_WITNESS",
  "identity.owner_tuple_exact",
):
    assert needle in engine,needle
for needle in ("effective_envspec_decision","!v296_partial_material","probe_exact_specular_companion(context)"):
    assert needle in integ,needle
assert "stock_material_fail_open" in policy
assert "DSRRL_V298_LIVE_MTD_WITNESS" in build
assert build.count("materialize_v298_live_mtd_witness.py")==1
assert build.count("dsrrl_renderer_core_integrated")==1
print("V298_SOURCE_CONTRACT_PASS: LIVE_PARSER_SHA_NOT_SOURCE_OWNER_SHA")
print("V298_RUNTIME_PIXEL_OPEN")
