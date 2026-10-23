from pathlib import Path
p=Path(__file__).resolve().parents[1]/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
needle='    const bool diffuse_mr_active =\n        decision.active &&\n'
insert='''    bool v296_partial_material = false;
#if defined(DSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID)
    // Manifest protection applies even when the legacy MR decision is INACTIVE.
    // Otherwise raw normal-sidecar requests can leak through on route=0.
    if (context) {
        const bool exact =
            dsrrl::runtime::exact_pmetal_envspec_candidate(material, decision);
        if (!exact) {
            const auto p = g_material_resources.probe_exact_specular_companion(context);
            v296_partial_material =
                dsrrl::runtime::v296::suppress_partial_material_response(
                    dsrrl::runtime::v296::decide_material_policy(
                        false, p.stock_bound, p.snapshot_resolved,
                        p.logical_hash_allowed));
            if (v296_partial_material) {
#if defined(DSRRL_V296_ROOTCAUSE_TELEMETRY)
                dsrrl::runtime::v296_diag::record(
                    material, decision, p, receiver_id,
                    g_present_count.load(std::memory_order_relaxed));
#endif
                static std::atomic_bool announced{false};
                if (!announced.exchange(true))
                    reshade::log::message(
                        reshade::log::level::info,
                        "[DSRRL V296] stage=non_atomic_spec_failopen"
                        " material=stock_dsr"
                        " independent_upper_lower=preserved");
            }
        }
    }
#endif
    const bool diffuse_mr_active =
        decision.active &&
        !v296_partial_material &&
'''
before='#include "dsrrl/runtime/material_resource_draw_runtime.hpp"'
after=before+'\n#include "dsrrl/runtime/v296_oct10_material_atomic_gate.hpp"'
after+='\n#include "dsrrl/runtime/v296_failopen_rootcause_diag.hpp"'
assert s.count(needle)==1 and s.count(before)==1
s=s.replace(before,after,1).replace(needle,insert,1)
# Normal substitution is a separate MR resource path. On an Oct10 draw
# without certified PTDE SpecRGB consumer, neither t0/t2 companions nor
# diffuse-only MR may leak into the stock DSR material operator.
normal_anchor='''    if (context != nullptr) {
        // MR reset invariant: generic stable MR is diffuse-v1 only.'''
normal_guard='''    if (context != nullptr &&
        !v296_partial_material) {
        // MR reset invariant: generic stable MR is diffuse-v1 only.'''
assert s.count(normal_anchor)==1
s=s.replace(normal_anchor,normal_guard,1)
p.write_text(s,"utf-8")
print("V296_ATOMIC_POLICY_PATCH_PASS")
