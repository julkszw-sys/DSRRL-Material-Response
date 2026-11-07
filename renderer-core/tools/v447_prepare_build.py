#!/usr/bin/env python3
"""Apply after V443 prepare, before V438/V439 generated build."""
from pathlib import Path
p=Path('renderer-core/tools/v438_apply_draw_part_b.py')
s=p.read_text(encoding='utf-8')
old='import v443_actor_probe_overlay\n'
assert s.count(old)==1 and 'import v447_actor_shadow_overlay' not in s
p.write_text(s.replace(old,old+'import v447_actor_shadow_overlay\n',1),encoding='utf-8')
print('V447 actor-source shadow scheduled AFTER V443')
