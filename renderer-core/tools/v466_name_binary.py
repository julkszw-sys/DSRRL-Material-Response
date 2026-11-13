#!/usr/bin/env python3
"""Produce one V466 .addon64, with all earlier overlays embedded."""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
old="DSRRL_V465_Ally_Auto_Actor_RX34_LogOnly.addon64"
new="DSRRL_V466_Ally_RX34_Phase_Aware_Owner.addon64"
assert s.count(old)>=2,(s.count(old),"V465 build output drift")
p.write_text(s.replace(old,new),encoding="utf-8")
print("V466: one integrated .addon64 output")
