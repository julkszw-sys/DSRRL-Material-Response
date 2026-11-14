#!/usr/bin/env python3
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
old="DSRRL_V469_RX34_Pruned_Nonmetal_Telemetry.addon64"
new="DSRRL_V470_RX34_Focused_LogLowOverhead.addon64"
assert s.count(old)>=2,(s.count(old),"V469 binary lineage drift")
p.write_text(s.replace(old,new),encoding="utf-8")
print("V470: one integrated .addon64")
