#!/usr/bin/env python3
"""Name the single newly compiled addon; never touch V459 stable artifact."""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf8")
old="DSRRL_V459_Typed_RX34_Light_Pair_Audit.addon64"
assert s.count(old)>=2,("V464 binary-name drift",s.count(old))
p.write_text(s.replace(old,"DSRRL_V464_Player_Enemy_RX34_Handle_Owner_Probe.addon64"),encoding="utf8")
print("V464 ONE output addon: player/enemy typed RX34 handle observer")
