#!/usr/bin/env python3
"""V299: classify the newly admitted PTDE slot0 candidate in V290 census.

The V290 result is ATOMIC_CANDIDATE only, NEVER an activation/pixel PASS.
Run after V296/V297 integrated source materializers, before compilation.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"integrated/integrated_addon.cpp"
s=p.read_text(encoding="utf-8")
a='''        const bool authorized =
            dsrrl::runtime::exact_pmetal_envspec_candidate(
                material, decision);
        dsrrl::runtime::v290_spec_census::report('''
b='''        bool authorized =
            dsrrl::runtime::exact_pmetal_envspec_candidate(
                material, decision);
#if defined(DSRRL_V299_AUTHORED_SLOT0_OPERATOR)
        // V299 explicitly has a separate PTDE source decision from the
        // legacy route=0 decision. Report this as source candidate ONLY;
        // subsequent source/shader/CB/resource atomic preparation is still
        // required before a real draw.
        if (!authorized && !decision.active) {
            dsrrl::operators::material_response::decision slot0_candidate{};
            if (dsrrl::runtime::v297::promote_exact_slot0_source(
                    material,decision,receiver_id,slot0_candidate))
                authorized =
                    dsrrl::runtime::exact_pmetal_envspec_candidate(
                        material,slot0_candidate);
        }
#endif
        dsrrl::runtime::v290_spec_census::report('''
if s.count(a)!=2:
    if s.count(b)==2 and s.count(a)==0:
        print("V299_CENSUS_ALREADY_MATERIALIZED")
        raise SystemExit(0)
    raise RuntimeError(f"V290 material census source changed: {s.count(a)}")
s=s.replace(a,b)
p.write_text(s,encoding="utf-8")
print("V299_CENSUS_REPAIR_PASS two draw modes — candidate is NOT pixel pass")
