#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/tools/build_v438_one_shot.ps1')
s=p.read_text(encoding='utf-8')
a='DSRRL_V450_Manual_RX34_Light_Audit.addon64'
assert s.count(a)>=2
p.write_text(s.replace(a,'DSRRL_V451_ModelResource_RX34_Manual_Audit.addon64'),encoding='utf-8')
print('V451 one addon output name')
