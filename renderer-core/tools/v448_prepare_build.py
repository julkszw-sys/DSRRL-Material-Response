#!/usr/bin/env python3
"""Schedule V448 after V447 which is scheduled after V443/V439."""
from pathlib import Path
p=Path('renderer-core/tools/v438_apply_draw_part_b.py')
s=p.read_text(encoding='utf-8')
needle='import v447_actor_shadow_overlay\n'
assert s.count(needle)==1
assert 'import v448_actor_live_overlay' not in s
p.write_text(s.replace(needle,needle+'import v448_actor_live_overlay\n'),encoding='utf-8')
print('V448 live native builder probe scheduled behind V447')
