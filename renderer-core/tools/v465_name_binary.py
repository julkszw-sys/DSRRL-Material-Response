#!/usr/bin/env python3
"""Rename only generated ReShade x64 addon from V459 to V465."""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
old="DSRRL_V459_Typed_RX34_Light_Pair_Audit.addon64"
assert s.count(old)>=2,(s.count(old),"V459 output drift")
s=s.replace(old,"DSRRL_V465_Ally_Auto_Actor_RX34_LogOnly.addon64")
p.write_text(s,encoding="utf-8")
print("V465 output: one integrated .addon64")
