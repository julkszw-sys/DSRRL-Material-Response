#!/usr/bin/env python3
"""Name exactly one V468 x64 ReShade addon."""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
old="DSRRL_V467_RX34_WorldPlayer_Source_Census.addon64"
new="DSRRL_V468_RX34_LowOverhead_WorldPlayer.addon64"
assert s.count(old)>=2,(s.count(old),"V467 output drift")
p.write_text(s.replace(old,new),encoding="utf-8")
print("V468: one integrated .addon64 output")
