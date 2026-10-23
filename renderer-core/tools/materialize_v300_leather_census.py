#!/usr/bin/env python3
"""V300 V290 census: distinguish leather atomic candidate from activation."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
a='''#endif
        dsrrl::runtime::v290_spec_census::report(
            material, receiver_id,
            hemenvlerp_bound ? 1u : 0u,
            authorized, probe);'''
b='''#endif
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
        if (!authorized && !decision.active) {
            dsrrl::operators::material_response::decision leather_candidate{};
            if (dsrrl::runtime::v300::promote(
                    material,decision,receiver_id,leather_candidate))
                authorized =
                    dsrrl::runtime::exact_pmetal_envspec_candidate(
                        material,leather_candidate);
        }
#endif
        dsrrl::runtime::v290_spec_census::report(
            material, receiver_id,
            hemenvlerp_bound ? 1u : 0u,
            authorized, probe);'''
if s.count(a)!=2:
    if s.count(a)==0 and s.count(b)==2:
        print("V300 census already patched");raise SystemExit(0)
    raise RuntimeError(f"expected both draw flavors, found {s.count(a)}")
s=s.replace(a,b)
p.write_text(s,"utf-8")
print("V300_V290_LEATHER_CENSUS_PASS 2_DRAW_PATHS candidate_not_pixel")
