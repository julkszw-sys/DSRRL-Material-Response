#!/usr/bin/env python3
"""V299: close source producer + material consumer PTDE slot0 authorization.

Run AFTER V296, V297, and V298 patchers.
No speculative MTD aliases. Exact owner/slot + actual parser SHA and
runtime RX identity are unchanged prerequisites.
Opt-in V299 only: default builds stay stock DSR on unknown materials.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
draw=root/"src/runtime/pmetal_envspec_draw_runtime.cpp"
source=root/"src/runtime/pmetal_env_source_runtime.cpp"
selector=root/"src/runtime/flver_engine_hooks.cpp"

def insert_once(s, old, new, label):
    n=s.count(old)
    if n!=1:
        if s.count(new)==1 and n==0:
            return s
        raise RuntimeError(f"{label}: expected one preimage, found {n}")
    return s.replace(old,new,1)

changes=[]
s=draw.read_text(encoding="utf-8")
s=insert_once(s,
    '#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"',
    '#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"\n#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)\n#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"\n#endif',
    "draw header")
s=insert_once(s,
'''    if (exact_pmetal_material(material) &&
        exact_pmetal_decision(decision))
        return true;
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)''',
'''    if (exact_pmetal_material(material) &&
        exact_pmetal_decision(decision))
        return true;
#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)
    if (v299::authored_slot0_draw(material, decision))
        return true;
#endif
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)''',
    "draw candidate")
s=insert_once(s,
'''#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)
                    || v203_equipment::exact_spx_roughcloth_material(material)
#endif
                )) {''',
'''#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)
                    || v203_equipment::exact_spx_roughcloth_material(material)
#endif
#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)
                    || v299::authored_slot0_draw(material, decision)
#endif
                )) {''',
    "nonmetal lerp shader family")
changes.append((draw,s))

s=source.read_text(encoding="utf-8")
s=insert_once(s,
    '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"',
    '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"\n#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)\n#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"\n#endif',
    "producer header")
s=insert_once(s,
'''    if (exact_pmetal_material_selection(material))
        return true;
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)''',
'''    if (exact_pmetal_material_selection(material))
        return true;
#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)
    if (v299::authored_slot0_source(material))
        return true;
#endif
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)''',
    "LightBank selector+consumer source")
changes.append((source,s))

s=selector.read_text(encoding="utf-8")
s=insert_once(s,
    '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"',
    '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"\n#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)\n#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"\n#endif',
    "selector header")
# V394 producer is model-wide and therefore subsumes V299's material-specific
# source gate. V299 receiver/material operator gates above remain unchanged.
if "void observe_model_lightbank_source(" in s and "model_identity.model_owner_instance" in s:
    print("V299_SELECTOR_SOURCE_SUBSUMED_BY_V394_MODEL_INSTANCE=PASS")
else:
    s=insert_once(s,
'''        || v203_equipment::exact_spx_roughcloth_material(identity)
#endif
#endif
    ) {''',
'''        || v203_equipment::exact_spx_roughcloth_material(identity)
#endif
#endif
#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)
        || v299::authored_slot0_source(identity)
#endif
    ) {''',
    "selector predecode PTDE LightBank producer")
changes.append((selector,s))

for path,data in changes:
    path.write_text(data,encoding="utf-8")
    print("V299_OPERATOR_CUT_PATCH:",path.relative_to(root))
print("V299_SOURCE_CONSTRUCTION_PASS — NATIVE_COMPILE_RUNTIME_PIXEL_OPEN")
