#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")
src = (root / "integrated" / "integrated_addon.cpp").read_text(encoding="utf-8")
cmake = (root / "integrated" / "CMakeLists.txt").read_text(encoding="utf-8")

required = [
    "DSRRL_PMETAL_PTDE_CARRIER_STOCK_CONSUMER_DIAG",
    "decision.route_index == 345u",
    "decision.receiver_id >= 33u",
    "decision.receiver_id <= 35u",
    "!hemenvlerp_bound",
    "!pmetal_carrier_cut_candidate &&",
    "g_pmetal_envspec.prepare(",
    "prepared.mr_in_batch",
    "g_envspec_resources.prepare(",
    "context, 2u, false, prepared.pmetal_carrier_diag",
    "carrier.replace_pixel_shader = false",
    "carrier.srvs[0] = {12u, prepared.pmetal_carrier_diag.ptde_a}",
    "carrier.srvs[1] = {14u, prepared.pmetal_carrier_diag.ptde_b}",
    "carrier.srv_count = 2u",
    "[DSRRL PMETAL CARRIER CUT DIAG]",
    "sampler=stock",
]
missing = [x for x in required if x not in src and x not in cmake]
if missing:
    raise SystemExit("MISSING:\n" + "\n".join(missing))

# Diagnostic request itself must not bind a sampler or constant buffer.
start = src.index("#if defined(DSRRL_PMETAL_PTDE_CARRIER_STOCK_CONSUMER_DIAG)", src.index("prepared.mr_in_batch"))
end = src.index("#endif", start)
block = src[start:end]
for forbidden in ("carrier.samplers", "carrier.sampler_count", "carrier.constant_buffers", "carrier.constant_buffer_count"):
    if forbidden in block:
        raise SystemExit(f"FORBIDDEN diagnostic mutation: {forbidden}")

# Ensure the resource-only cut does not replace the PS.
if "carrier.replace_pixel_shader = false" not in block:
    raise SystemExit("resource cut unexpectedly replaces pixel shader")

print("PMETAL_PTDE_CARRIER_STOCK_CONSUMER_DIAG_AUDIT_PASS")
