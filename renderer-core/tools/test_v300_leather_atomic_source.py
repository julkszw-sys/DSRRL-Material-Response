from pathlib import Path
r=Path(__file__).resolve().parents[1]
g=(r/'include/dsrrl/runtime/v300_exact_leather_material.hpp').read_text()
d=(r/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
p=(r/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
f=(r/'src/runtime/flver_engine_hooks.cpp').read_text()
a=(r/'integrated/integrated_addon.cpp').read_text()
assert g.count('.mtd')==2
for s in ['m.owner_tuple_exact','m.actual_material_exact','m.material_slot_valid','e.envspc_slot!=1u','out.c101=1.5f','out.ptde_specular_power=4.0f']:
 assert s in g,s
for s,t in [('v300::draw(material, decision)',d),('v300::source(material)',p),('v300::source(identity)',f),('v300::k_leather[i].sha',f),('effective_envspec_decision=leather;',a)]:
 assert s in t,s
assert a.count('leather_candidate')==6
assert 'g_pmetal_envspec.prepare(' in a
print('V300_EXACT_LEATHER_CONTRACT_PASS runtime_pixel_OPEN')
