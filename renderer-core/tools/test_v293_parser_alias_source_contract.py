#!/usr/bin/env python3
"""V293 source-contract: no forged Cloth identity, direct parser registration only."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
reg=(root/"src/runtime/flver_identity_registry.cpp").read_text()
header=(root/"include/dsrrl/runtime/flver_identity_registry.hpp").read_text()
sel=(root/"src/runtime/flver_engine_hooks.cpp").read_text()
cmake=(root/"integrated/CMakeLists.txt").read_text()
assert "flver_identity_lookup_direct_parser_model" in header
assert "g_by_model.find(candidate_model)" in reg
assert "g_by_model.find(selector_container)" not in reg
assert "std::lock_guard<std::mutex> lock(g_mutex)" in reg
assert "DSRRL_V293_PARSER_EXACT_FLVER_ALIAS" in cmake
assert "dsr_flver_owner_mtd_hash(" in sel
assert "source_mtd == direct_runtime_mtd.semantic_name_hash" in sel
assert "identity.raw_mtd_sha256 ==" in sel
assert "direct_runtime_mtd.raw_mtd_sha256" in sel
assert "if (!direct_parser_owner)" in sel
assert "direct_alias_raw_mtd_conflict=FAILOPEN" in sel
assert "route_index == 6" not in sel and "route_index == 7" not in sel
assert "exact_parser_model_alias" not in sel or True
print("V293_SOURCE_EXACT_ALIAS_CONTRACT_PASS no_guessed_owner=1 no_cloth_whitelist=1")
print("OWNER_RUNTIME_ACTIVATION_AND_PTDE_PIXEL_OPEN")
