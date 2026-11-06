#!/usr/bin/env python3
"""Run AFTER V439 preparation and BEFORE the existing V438/V439 build."""
from pathlib import Path
p=Path('renderer-core/tools/v438_apply_draw_part_b.py')
s=p.read_text(encoding='utf-8')
needle='import v439_hook_status\n'
assert s.count(needle)==1
assert 'import v443_actor_probe_overlay' not in s
s=s.replace(needle,needle+'import v443_actor_probe_overlay\n',1)
p.write_text(s,encoding='utf-8')
print('V443 scheduled on V439 overlay lineage')
