#!/usr/bin/env python3
"""Parse a V360 ReShade log. Runtime activation != PTDE pixel equivalence.

Usage: python renderer-core/tools/audit_v360_fixed_equipment_log.py "ReShade(...).log"
"""
import argparse
import json
import re
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument("log",type=Path)
parser.add_argument("--output",type=Path,default=None)
args=parser.parse_args()
content=args.log.read_text(encoding="utf-8",errors="replace")
tag="[DSRRL V360 FIXED EQUIPMENT]"
lines=[line for line in content.splitlines() if tag in line]
startup=any("stage=addon_loaded" in x for x in lines)
first_draw=any("stage=draw_issued" in x for x in lines)
aggregates=[x for x in lines if "tag=LIVE" in x and "candidate=" in x]
final=aggregates[-1] if aggregates else ""
keys=("candidate","material","sidecars","b12","t19","batch",
      "fail_open","draw_issued","draw_not_issued","restore_failed")
counts={}
for k in keys:
    m=re.search(r"\b"+k+r"=(\d+)",final)
    counts[k]=int(m.group(1)) if m else None
statuses={
    "addon_loaded":"OBSERVED" if startup else "NOT_OBSERVED",
    "fixed_receiver":"OBSERVED" if (counts["candidate"] or 0)>0 else "NOT_OBSERVED",
    "material_authorized":"OBSERVED" if (counts["material"] or 0)>0 else "NOT_OBSERVED",
    "ptde_sidecars_ready":"OBSERVED" if (counts["sidecars"] or 0)>0 else "NOT_OBSERVED",
    "b12_ready":"OBSERVED" if (counts["b12"] or 0)>0 else "NOT_OBSERVED",
    "t19_ready":"OBSERVED" if (counts["t19"] or 0)>0 else "NOT_OBSERVED",
    "batch_ready":"OBSERVED" if (counts["batch"] or 0)>0 else "NOT_OBSERVED",
    "fixed_draw_issued":"OBSERVED" if (counts["draw_issued"] or 0)>0 or first_draw else "NOT_OBSERVED",
    "restore_integrity":"FAILED" if (counts["restore_failed"] or 0)>0 else (
        "NO_FAILURE_OBSERVED" if aggregates else "NOT_OBSERVED"),
    "ptde_pixel_equivalence":"OPEN",
}
overall="ACTIVATION_OBSERVED" if statuses["fixed_draw_issued"]=="OBSERVED" else (
    "ADDON_ONLY" if startup else "NO_V360_RUNTIME_WITNESS")
report={"schema":"dsrrl.v360.fixed_equipment_runtime_audit.v1",
        "log":str(args.log),"status":overall,
        "telemetry":statuses,"counters":counts,
        "unknowns":["PTDE visible/pixel equivalence remains OPEN",
                    "No counter proves that every sidecar-equipped equipment draw is routed",
                    "On a scene without Fixed PntSS/PntSSSS hits, zero counters are inconclusive"]}
output=json.dumps(report,ensure_ascii=False,indent=2)
if args.output:
    args.output.write_text(output+"\n",encoding="utf-8")
print(output)
