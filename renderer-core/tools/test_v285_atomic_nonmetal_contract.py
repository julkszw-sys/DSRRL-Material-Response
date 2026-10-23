#!/usr/bin/env python3
"""V285 source contract: exact nonmetal Lerp atomic operator, opt-in only.

These are static construction guards, not game-runtime/pixel assertions.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
def read(path):
    return (ROOT/path).read_text(encoding="utf-8")

manifest=read("renderer-core/include/dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp")
gate=read("renderer-core/include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp")
router=read("renderer-core/src/runtime/material_resource_draw_runtime.cpp")
producer=read("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
selector=read("renderer-core/src/runtime/flver_engine_hooks.cpp")
pmetal=read("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
integration=read("renderer-core/integrated/integrated_addon.cpp")
cmake=read("renderer-core/integrated/CMakeLists.txt")

rows=re.findall(r'\{ "([0-9a-f]{64})", (\d+)u, "([^"]+)", "([^"]+)" \}', manifest)
assert len(rows)==36,len(rows)
assert len(set(rows))==36
assert len({a for a,b,c,d in rows})==21
assert len({d.casefold() for a,b,c,d in rows})==18
nonmetal=[r for r in rows if "Metal" not in r[2]]
assert nonmetal
assert 'exact_proven_slot_spec_hash(query, h1)' in router
assert 'exact_nonmetal' in router and 'evaluate_spec_rgb_route(context_spec)' in router
assert 'exact_envspec36_draw(material, decision)' in pmetal
assert 'exact_envspec36_material(identity)' in selector
assert 'exact_v285_or_pmetal_source' in producer
assert 'if (!exact_pmetal_material_selection(material)) {' in producer
assert 'if (latest_hook_source(out)) {' in producer
assert 'exact_pmetal_material(material)' in pmetal
assert 'env_semantics.envspc_slot != 2u' in pmetal
assert 'DSRRL_V285_ATOMIC_NONMETAL_DIAG' in cmake
assert re.search(r'DSRRL_V285_ATOMIC_NONMETAL_DIAG\s*\n\s*"V285[^\n]*\n\s*OFF\)', cmake)
assert "DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW" in cmake
assert "DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE" in cmake
assert "exact_pmetal_envspec_candidate(" in integration
assert "g_pmetal_envspec.prepare(" in integration
assert "append_island_draw_request(" in integration
assert "prepared.envspec.material_resources.requests[i]" in integration
assert "spec_rgb_consumer_ready" not in gate
# No enabling generic MR SpecRGB: it must remain protected until a matched
# independent shader+source+resources can be staged in ONE draw.
assert re.search(r'lerp_query,\s*true,\s*false,\s*prepared.resources',integration)
assert 'g_material_resources.drop_spec_rgb_request(' in integration
assert 'classify_mtd_envspec_semantics(query)' in gate
assert 'material.route_index == 345u' in gate
assert 'material.owner_tuple_exact' in gate
assert 'material.actual_material_exact' in gate
print("V285_SOURCE_CONTRACT_PASS rows=36 owners=21 names=18 default_off=1")
print("V285_RUNTIME_PIXEL_OPEN")
