#!/usr/bin/env python3
"""V299 source-to-consumer atomic contract, not a pixel-behavior test."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
g=(r/"include/dsrrl/runtime/v299_authored_slot0_operator_gate.hpp").read_text()
d=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
p=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
f=(r/"src/runtime/flver_engine_hooks.cpp").read_text()
a=(r/"integrated/integrated_addon.cpp").read_text()
for token in (
    "promote_exact_slot0_source(", "decision.route_index != canonical.route_index",
    "decision.ptde_specular_power != ", "decision.c100[i] != canonical.c100[i]",
    "material, inactive, 33u, candidate",
):
    assert token in g,token
assert "v299::authored_slot0_draw(material, decision)" in d
assert "v299::authored_slot0_source(material)" in p
assert "v299::authored_slot0_source(identity)" in f
assert "exact_pmetal_material_selection(material)" in p
assert "exact_pmetal_decision(decision)" in d
assert "exact_v285_or_pmetal_source(" in p
assert "g_pmetal_envspec.prepare(" in a
assert "effective_envspec_decision" in a
assert "!v296_partial_material" in a
assert "probe_exact_specular_companion(context)" in a
assert a.count("slot0_candidate") == 6, "both draw census paths need the exact diagnostic"
assert "v299::authored_slot0_draw(material, decision)" in d.split("lerp_nonmetal_r8_replacements_")[0]
assert "DSRRL_V299_AUTHORED_SLOT0_OPERATOR" in d
assert "DSRRL_V299_AUTHORED_SLOT0_OPERATOR" in p
assert "DSRRL_V299_AUTHORED_SLOT0_OPERATOR" in f
print("V299_SOURCE_CONTRACT_PASS: PTDE_SLOT0_PRODUCER_AND_CONSUMER_LINKED")
print("V299_BUILD_RUNTIME_PIXEL_OPEN")
