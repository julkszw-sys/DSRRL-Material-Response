#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/tools/build_v438_one_shot.ps1')
s=p.read_text(encoding='utf-8')
old='DSRRL_V443_Packer_Actor_ReadOnly.addon64'
assert s.count(old)>=2
p.write_text(s.replace(old,'DSRRL_V447_Actor_Source_Shadow.addon64'),encoding='utf-8')
print('V447 integrated single-addon basename configured')
