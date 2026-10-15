from pathlib import Path
p=Path(__file__).resolve().parents[1]/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
needle='    const bool diffuse_mr_active =\n        decision.active &&\n'
insert='''    bool v296_partial_material = false;
#if defined(DSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID)
    if (decision.active && context) {
        const bool exact =
            dsrrl::runtime::exact_pmetal_envspec_candidate(material, decision);
        if (!exact) {
            const auto p = g_material_resources.probe_exact_specular_companion(context);
            v296_partial_material =
                dsrrl::runtime::v296::suppress_partial_material_response(
                    dsrrl::runtime::v296::decide_material_policy(
                        false, p.stock_bound, p.snapshot_resolved,
                        p.logical_hash_allowed));
        }
    }
#endif
    const bool diffuse_mr_active =
        decision.active &&
        !v296_partial_material &&
'''
before='#include "dsrrl/runtime/material_resource_draw_runtime.hpp"'
after=before+'\n#include "dsrrl/runtime/v296_oct10_material_atomic_gate.hpp"'
assert s.count(needle)==1 and s.count(before)==1
s=s.replace(before,after,1).replace(needle,insert,1)
p.write_text(s,"utf-8")
print("V296_ATOMIC_POLICY_PATCH_PASS")
