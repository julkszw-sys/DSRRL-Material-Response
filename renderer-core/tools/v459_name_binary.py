#!/usr/bin/env python3
"""Rename the only generated native x64 addon for reproducible V459 artifact."""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
old="DSRRL_V451_ModelResource_RX34_Manual_Audit.addon64"
assert s.count(old)>=2,("V451 binary name drift",s.count(old))
p.write_text(s.replace(old,"DSRRL_V459_Typed_RX34_Light_Pair_Audit.addon64"),encoding="utf-8")
print("V459 one output addon")
