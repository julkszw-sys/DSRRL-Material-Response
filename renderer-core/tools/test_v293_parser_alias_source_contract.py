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
gate=(root/"include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp").read_text()
draw=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
source=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
for name in ("P_RoughCloth[DSB].mtd","P_RoughCloth[DSB]_Edge.mtd",
             "C_RoughCloth[DSB].mtd","C_RoughCloth[DSB]_Edge.mtd"):
    assert name in gate, name
assert "exact_spx_roughcloth_draw(material, decision)" in draw
assert "exact_spx_roughcloth_material(material)" in draw
assert "exact_spx_roughcloth_material(material)" in source
assert "exact_spx_roughcloth_material(identity)" in sel
assert "has_exact_flver_material_ownership(query)" in gate
assert "env.envspc_slot == 0u" in gate
assert "material.owner_tuple_exact" in gate
assert "DSRRL_V293_PARSER_EXACT_FLVER_ALIAS" in gate

print("V293_SOURCE_EXACT_ALIAS_CONTRACT_PASS no_guessed_owner=1 cloth_exact_homologous_spx_only=1")
print("OWNER_RUNTIME_ACTIVATION_AND_PTDE_PIXEL_OPEN")
