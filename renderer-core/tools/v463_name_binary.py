#!/usr/bin/env python3
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
a="DSRRL_V459_Typed_RX34_Light_Pair_Audit.addon64"
assert s.count(a)>=2,("V459 binary naming source drift",s.count(a))
p.write_text(s.replace(a,"DSRRL_V463_PlayerIns_RX34_Handle_Join.addon64"),encoding="utf-8")
print("V463 one addon name")
