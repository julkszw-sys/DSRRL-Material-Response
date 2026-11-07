#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/tools/build_v438_one_shot.ps1')
s=p.read_text(encoding='utf-8')
a='DSRRL_V447_Actor_Source_Shadow.addon64'
assert s.count(a)>=2
s=s.replace(a,'DSRRL_V448_Live_Builder_Actor_Shadow.addon64')
needle=' /DDSRRL_V418_RX34_GENERATIONAL_ISSUED_JOIN'
assert s.count(needle)==1
s=s.replace(needle,' /DDSRRL_V448_LIVE_FRAME_SHADOW'+needle)
p.write_text(s,encoding='utf-8')
print('V448 artifact + explicit shadow compile flag applied')
