#!/usr/bin/env python3
"""Produce exactly one integrated V467 ReShade addon."""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
old="DSRRL_V466_Ally_RX34_Phase_Aware_Owner.addon64"
new="DSRRL_V467_RX34_WorldPlayer_Source_Census.addon64"
assert s.count(old)>=2,(s.count(old),"V466 build output drift")
p.write_text(s.replace(old,new),encoding="utf-8")
print("V467 native output: one .addon64")
