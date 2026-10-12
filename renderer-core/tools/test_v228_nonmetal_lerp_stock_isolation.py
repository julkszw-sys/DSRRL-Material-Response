#!/usr/bin/env python3
"""V228 exact full-island fail-open A/B negative authority and invariance."""
import importlib.util
from pathlib import Path
p=Path(__file__).with_name("apply_v228_nonmetal_lerp_stock_isolation.py")
spec=importlib.util.spec_from_file_location("v228",p)
v=importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)
sample="""bool prepare_island_batch() {
    const bool exact_pmetal_envspec = route_auth();
"""+v.ANCHOR+""" cmd_list, material, decision, envspec_family, prepared.envspec))
            return false;
        return true;
    }
    if (hemenvlerp_bound) {
       return generic_mr();
    }
}
"""
patched=v.patch(sample)
assert v.patch(patched)==patched
assert patched.count("[DSRRL V228 LERP STOCK AB]")==1
assert patched.count("if (exact_pmetal_envspec) {")==1
assert patched.count("if (hemenvlerp_bound) {")==1
assert "material.route_index != 345u" in patched
assert "if (hemenvlerp_bound &&\n        exact_pmetal_envspec" in patched
assert patched.index("generic_mr_fallback=DISALLOWED")<patched.index("if (exact_pmetal_envspec) {")
assert patched.index("return false;\n    }\n#endif")<patched.index("if (exact_pmetal_envspec) {")
assert patched.index("#endif")<patched.index("if (hemenvlerp_bound) {")
assert "pmetal=UNCHANGED stable_nonmetal=UNCHANGED" in patched
assert "if (!g_pmetal_envspec.prepare(" in patched
assert "V228_NOT_PTDE_PIXEL_FIX" in p.read_text()
for broken in (
 sample.replace("const bool exact_pmetal_envspec =","const bool drift ="),
 sample.replace(v.ANCHOR, "BROKEN"),
 sample.replace("if (hemenvlerp_bound) {","if (unmapped_family) {"),
):
    try: v.patch(broken)
    except RuntimeError: pass
    else: raise AssertionError("V228 drift not detected")
print("V228_FULL_NONMETAL_LERP_STOCK_AB_NEGATIVE_ORACLE_PASS")
print("V228_PMETAL_AND_NONMETAL_STABLE_PRESERVED")
