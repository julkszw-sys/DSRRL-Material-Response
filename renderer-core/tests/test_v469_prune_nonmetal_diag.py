#!/usr/bin/env python3
"""V469 source contract, not performance claim."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=(root/"tools/v469_prune_nonmetal_diag_overlay.py").read_text(encoding="utf-8")
prep=(root/"tools/v469_prepare_build.py").read_text(encoding="utf-8")
name=(root/"tools/v469_name_binary.py").read_text(encoding="utf-8")
for m in ("196608", "786432", "material.route_index == 345u",
 "material.route_index == 229u",
 "g_v230_nonmetal_route_logged[key].fetch_or(",
 "inline void v230_nonmetal_route_trace(",
 "(void)stage; (void)receiver; (void)material; (void)decision;"):
 assert m in p,m
assert "import v469_prune_nonmetal_diag_overlay" in prep
assert "DSRRL_V469_RX34_Pruned_Nonmetal_Telemetry.addon64" in name
for forbid in ("set_lightbank(","DrawIndexed(","DSRRL_V230_RUNTIME_TELEMETRY=OFF","shader_patch("):
 assert forbid not in p
print("V469 source isolation PASS; game FPS OPEN")
