#!/usr/bin/env python3
"""Reproducible, source-only V286 full-asset and nonmetal/PMetal parity gates."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/"renderer-core/tools"))
from generate_sidecar_spec_whitelist_20261010 import load_names,render_header
def read(s): return (root/s).read_text(encoding="utf-8")
names=load_names(root/"renderer-core/data/provenance/sidecar_specular_20261010_names.b85")
assert len(names)==1295
header=render_header(names)
assert "std::array<std::uint64_t,1295u>" in header
gate=read("renderer-core/include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp")
loader=read("renderer-core/src/runtime/material_resource_draw_runtime.cpp")
source=read("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
draw=read("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
integration=read("renderer-core/integrated/integrated_addon.cpp")
cmake=read("renderer-core/integrated/CMakeLists.txt")
assert "material.actual_material_exact" not in gate
assert "exact_proven_owner_slot(query)" not in gate
assert "material.owner_tuple_exact" in gate and "material.material_slot_valid" in gate
assert "classify_mtd_envspec_semantics(query)" in gate
assert "spec_equipment_allowed(" in loader
assert loader.count("spec_equipment_allowed(")==6
assert "sidecar_20261010::spec_allowed" in loader
assert "exact_proven_slot_spec_hash(query, h1)" not in loader
assert "generated_spec_whitelist_20261010.hpp" in loader
assert "evaluate_spec_rgb_route(context_spec)" in loader
assert "latest_hook_source(out)" in source
assert "if (!exact_pmetal_material_selection(material)) {" not in source
assert "exact_envspec36_material(identity)" in read("renderer-core/src/runtime/flver_engine_hooks.cpp")
assert "exact_envspec36_draw(material, decision)" in draw
assert "env.envspc_slot < 4u" in gate
assert "g_pmetal_envspec.prepare(" in integration
assert "[DSRRL V286 NONMETAL] stage=full_atomic_prepare_ready" in integration
assert "prepared.envspec.material_resources.requests[i]" in integration
assert "lerp_query,\n                true,\n                false," in integration
assert "DSRRL_V285_ATOMIC_NONMETAL_DIAG" in cmake
assert "V286_HEADER" in cmake and "generate_sidecar_spec_whitelist_20261010.py" in cmake
print("V286_SOURCE_PASS full_spec_names=1295 static_36_gate_removed=1 actual_material_flag_removed=1 fallback_latest_hook=ON")
print("GPU_ACTIVATION_AND_PTDE_PIXEL_OPEN")
