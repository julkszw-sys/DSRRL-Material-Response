#!/usr/bin/env python3
"""V289 source-only A/B guard: isolate R8 SPEC transfer to nonmetal Lerp.

Runtime/PDE pixel result is OPEN. This guard cannot prove GPU branch.
"""
from pathlib import Path
import runpy,re
root=Path(__file__).resolve().parents[2]
runpy.run_path(str(root/"renderer-core/tools/test_v288_indexed_draw_outcome.py"),run_name="__main__")
def rd(p):return (root/p).read_text(encoding="utf-8")
cpp=rd("renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
h=rd("renderer-core/include/dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp")
runtime=rd("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
rh=rd("renderer-core/include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp")
addon=rd("renderer-core/integrated/integrated_addon.cpp")
cmake=rd("renderer-core/integrated/CMakeLists.txt")
assert "materialize_nonmetal_lerp_r8_spec_linear_candidate" in cpp and "materialize_nonmetal_lerp_r8_spec_linear_candidate" in h
assert "words[branch.offset+3u] != 85u" in cpp
assert "material_if_matches!=1u || matches!=1u" in cpp
assert "words[mul.offset+6u]!=k_f22" in cpp
assert "words[mul.offset+7u]!=k_f22" in cpp
assert "words[mul.offset+8u]!=k_f22" in cpp
assert "(words[gamma_exp]&~0x7ffu)|0x36u" in cpp
assert "checksum_container_valid(" in cpp and "if (!rebuild(" in cpp
assert "register_lerp_nonmetal_r8_candidate" in rh and "register_lerp_nonmetal_r8_candidate" in runtime
assert "lerp_nonmetal_r8_replacements_" in rh
assert runtime.count("lerp_nonmetal_r8_replacements_.clear()")==2
assert "if (material.route_index != 345u &&" in runtime
assert "v203_equipment::exact_envspec36_material(material)" in runtime
assert 'v289_r8_ps_selected' in runtime
assert 'metal_ps=V288' in addon
assert "DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB" in cmake
assert re.search(r'DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB\s*\n\s*"V289[^\n]*\n\s*OFF\)',cmake)
assert "NOT DSRRL_V285_ATOMIC_NONMETAL_DIAG" in cmake
assert "NOT DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW" in cmake
assert "NOT DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE" in cmake
print("V289_NONMETAL_ONLY_R8_EXACT_ELSE_CONSTRUCTION_PASS")
print("GPU_b0_85_BRANCH=OPEN PTDE_PIXEL=OPEN")
