#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/tools/build_v438_one_shot.ps1')
s=p.read_text(encoding='utf-8')
a='DSRRL_V439_Auto_Flicker_Safe.addon64'
assert s.count(a)>=2
p.write_text(s.replace(a,'DSRRL_V443_Packer_Actor_ReadOnly.addon64'),encoding='utf-8')
print('V443 single addon artifact basename set')
