#!/usr/bin/env python3
"""V413 diagnostic-only cleanup: remove only rejected V296 cross-operator veto.
Keep V408-V413 read-only source logs, exact Source1 and R43 Source2 unchanged.
Do not disable telemetry or use release's LOG_FREE instrumentation removal.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=r.parent/"build-v413-selector-diagnostic/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp"
s=p.read_text("utf-8")
gate_start="    bool v296_partial_material = false;"
gate_end="    const bool diffuse_mr_active =\n        decision.active &&\n        !v296_partial_material &&"
if s.count(gate_start)!=1 or s.count(gate_end)!=1:
    raise RuntimeError("V413 diagnostic V296 gate preimage mismatch")
a=s.index(gate_start);b=s.index(gate_end,a)
removed=s[a:b]
if not (500<=len(removed)<=3000 and
        "probe_exact_specular_companion" in removed and
        "suppress_partial_material_response" in removed and
        "V296_ATOMIC_SIDECAR_NO_HYBRID" in removed):
    raise RuntimeError("Unexpected V296 cross-operator veto scope")
s=s[:a]+"    // V413: no speculative cross-operator V296 MR veto.\n"+s[b:]
s=s.replace(gate_end,"    const bool diffuse_mr_active =\n        decision.active &&",1)
old="    if (context != nullptr &&\n        !v296_partial_material) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only."
new="    if (context != nullptr) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only."
if s.count(old)!=1:raise RuntimeError("V413 native normal MR gate mismatch")
s=s.replace(old,new)
if "v296_partial_material" in s:
    raise RuntimeError("V296 global veto remains in diagnostic build")
p.write_text(s,"utf-8")
print("V413 DIAGNOSTIC NONMETAL GATE FIX PASS; V408-V413 logs remain intact")
