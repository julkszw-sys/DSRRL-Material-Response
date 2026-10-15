#!/usr/bin/env python3
"""V300 exact PTDE Leather/Egde source to draw operator island.
Apply after V299 construction. Opt-in, never blanket-enables SPX family.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
def patch(rel,steps):
    p=r/rel;s=p.read_text("utf-8")
    for a,b in steps:
        if s.count(a)!=1:
            if s.count(b)==1 and not s.count(a):continue
            raise RuntimeError(f"{rel}: source preimage differs: {a[:75]}")
        s=s.replace(a,b,1)
    p.write_text(s,"utf-8")
    print("V300_SRC_OK",rel)

draw="src/runtime/pmetal_envspec_draw_runtime.cpp"
patch(draw,[
('#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"',
 '#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"\n#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)\n#include "dsrrl/runtime/v300_exact_leather_material.hpp"\n#endif'),
('''    if (v299::authored_slot0_draw(material, decision))
        return true;
#endif''',
 '''    if (v299::authored_slot0_draw(material, decision))
        return true;
#endif
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    if (v300::draw(material, decision))
        return true;
#endif'''),
('''                    || v299::authored_slot0_draw(material, decision)
#endif''',
 '''                    || v299::authored_slot0_draw(material, decision)
#endif
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
                    || v300::draw(material, decision)
#endif''')
])
source="src/runtime/pmetal_env_source_runtime.cpp"
patch(source,[
('#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"',
 '#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"\n#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)\n#include "dsrrl/runtime/v300_exact_leather_material.hpp"\n#endif'),
('''    if (v299::authored_slot0_source(material))
        return true;
#endif''',
 '''    if (v299::authored_slot0_source(material))
        return true;
#endif
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    if (v300::source(material))
        return true;
#endif''')
])
selector="src/runtime/flver_engine_hooks.cpp"
patch(selector,[
('#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"',
 '#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"\n#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)\n#include "dsrrl/runtime/v300_exact_leather_material.hpp"\n#endif'),
('''        || v299::authored_slot0_source(identity)
#endif''',
 '''        || v299::authored_slot0_source(identity)
#endif
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
        || v300::source(identity)
#endif''')
])
integrated="integrated/integrated_addon.cpp"
patch(integrated,[
('#include "dsrrl/runtime/v297_authored_slot0_material.hpp"',
 '#include "dsrrl/runtime/v297_authored_slot0_material.hpp"\n#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)\n#include "dsrrl/runtime/v300_exact_leather_material.hpp"\n#endif'),
('''    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(
            material,
            effective_envspec_decision);''',
 '''#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    if (!decision.active && !v297_authored) {
        dsrrl::operators::material_response::decision leather{};
        if (dsrrl::runtime::v300::promote(
                material,decision,receiver_id,leather))
            effective_envspec_decision=leather;
    }
#endif
    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(
            material,
            effective_envspec_decision);''')
])
print("V300_LEATHER_OPERATOR_SOURCE_CONSTRUCTION_PASS")
