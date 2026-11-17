#!/usr/bin/env python3
"""B235: isolated post-audit removal of source-REJECTED V296 cross-operator veto.
Exact original V230 materializer logic from PR518. Never changes LightBank
R43 producer, Diffuse operator math, MTD owner, CB, shader, or on-disk EXE.
Fail-closed if generated source drifted; do not expand to other consumers.
"""
from pathlib import Path
p=Path("build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp")
assert p.is_file(), "B235 V296 source generated missing"
s=p.read_text(encoding="utf8")
def one(s,old,new,key):
    count=s.count(old)
    if count!=1:raise RuntimeError(f"B235 exact V296 {key} source drift {count}")
    return s.replace(old,new,1)

# V296 operator-crossing veto: confirmed SOURCE implementation but no
# witnessed avoided material-rendering failure or PTDE pixel improvement.
# It matched mere Oct10 stock-t1 membership, then suppressed separately
# certified diffuse-MR and t0/t2 resource operators even when the whole
# SpecRGB PTDE island was unavailable. The owner has explicitly forbidden
# new or inherited gates with no actual functional necessity proof.
# Remove this shared runtime veto only from V230 release generated source;
# keep individually verified MR consumer, stock t1, exact PTDE companion,
# shader identity, receiver and D3D state gates. The frozen V359 recipe and
# original V296 upstream source/contract remain unchanged.
gate_start="    bool v296_partial_material = false;"
gate_end="    const bool diffuse_mr_active =\\n        decision.active &&\\n        !v296_partial_material &&"
# gate_end is a Python string with literal backslash-n? See below.
gate_end = "    const bool diffuse_mr_active =\n        decision.active &&\n        !v296_partial_material &&"
if s.count(gate_start) != 1 or s.count(gate_end) != 1:
    raise RuntimeError("V230 V296 cleanup: no unique compiled nonmetal veto")
a=s.index(gate_start)
b=s.index(gate_end,a)
old_region=s[a:b]
if not (500 <= len(old_region) <= 3000 and
        "probe_exact_specular_companion" in old_region and
        "suppress_partial_material_response" in old_region and
        "V296_ATOMIC_SIDECAR_NO_HYBRID" in old_region):
    raise RuntimeError("V230 V296 cleanup: unexpected preimage")
s=s[:a]+"    // V230 no speculative SpecRGB->Diffuse/Normal global veto.\n"+s[b:]
s=one(s,gate_end,
      "    const bool diffuse_mr_active =\n        decision.active &&",
      "v296 partial MR veto")
s=one(s,
    "    if (context != nullptr &&\n        !v296_partial_material) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only.",
    "    if (context != nullptr) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only.",
    "v296 normal resource veto")
if "v296_partial_material" in s:
    raise RuntimeError("V296 speculative gate still in generated release")
print("V230_V296_UNPROVEN_CROSS_OPERATOR_VETO_REMOVED_FROM_RELEASE")

p.write_text(s,encoding="utf8")
print("B235 V296 exact cross-operator veto removed from post-audit generated CPP")
