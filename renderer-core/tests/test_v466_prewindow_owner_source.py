#!/usr/bin/env python3
"""V466 synthetic contracts; source checks do NOT assert a live game join."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
o=(r/"tools/v466_prewindow_owner_overlay.py").read_text(encoding="utf-8")
p=(r/"tools/v466_prepare_build.py").read_text(encoding="utf-8")
n=(r/"tools/v466_name_binary.py").read_text(encoding="utf-8")
for required in (
    "pre_owner_hits", "live_owner_hits",
    "PRETRIGGER_ONLY", "CPU_JOIN", "identity_candidate",
    "v.live_owner_hits>0u", "prior_only",
    "no historical owner promoted to live join",
    "if(in_window)++entry->live_owner_hits",
    "else ++entry->pre_owner_hits",
    "dsrrl::runtime::v465::finish();",
    "s.finished.store(true,std::memory_order_release);",
):
    assert required in o,required
assert "import v466_prewindow_owner_overlay" in p
assert "DSRRL_V466_Ally_RX34_Phase_Aware_Owner.addon64" in n
for forbidden in ("DrawIndexed(", "latest_hook_source(", "shader_patch(", "set_lightbank("):
    assert forbidden not in o,forbidden
# Example matching row/handle, but seen only before the scene window:
def verdict(identity,pre,live):
    return "CPU_JOIN" if identity and live>0 else (
        "PRETRIGGER_ONLY" if identity and pre>0 else "UNKNOWN")
assert verdict(True,7,0)=="PRETRIGGER_ONLY"
assert verdict(True,7,1)=="CPU_JOIN"
assert verdict(False,7,0)=="UNKNOWN"
assert verdict(True,0,0)=="UNKNOWN"
print("V466 source + synthetic phase policy PASS; RX34 runtime/pixel OPEN")
