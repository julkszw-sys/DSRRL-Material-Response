#!/usr/bin/env python3
"""V438: only remove rejected V296 cross-operator veto in generated diagnostic.
Retain Source1 exact producer and R43 Source2 and full read-only telemetry.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=r.parent/'build-v438-one-shot-diagnostic/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp'
s=p.read_text(encoding='utf-8')
start='    bool v296_partial_material = false;'
end='    const bool diffuse_mr_active =\n        decision.active &&\n        !v296_partial_material &&'
assert s.count(start)==1 and s.count(end)==1,'V438 cross-operator V296 preimage drift'
a=s.index(start);b=s.index(end,a)
removed=s[a:b]
assert 500<=len(removed)<=3000 and 'probe_exact_specular_companion' in removed
assert 'suppress_partial_material_response' in removed and 'V296_ATOMIC_SIDECAR_NO_HYBRID' in removed
s=s[:a]+'    // V438 diagnostic: rejected V296 cross-operator veto excluded.\n'+s[b:]
s=s.replace(end,'    const bool diffuse_mr_active =\n        decision.active &&',1)
old='    if (context != nullptr &&\n        !v296_partial_material) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only.'
new='    if (context != nullptr) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only.'
assert s.count(old)==1
s=s.replace(old,new)
assert 'v296_partial_material' not in s
p.write_text(s,encoding='utf-8')
print('V438 generated native source operator isolation PASS')
