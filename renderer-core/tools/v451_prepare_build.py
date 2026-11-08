#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/tools/v438_apply_draw_part_b.py')
s=p.read_text(encoding='utf-8')
a='import v450_low_cost_overlay\n'
assert s.count(a)==1 and 'import v451_model_resource_overlay' not in s
p.write_text(s.replace(a,a+'import v451_model_resource_overlay\n',1),encoding='utf-8')
print('V451 scheduled after V450')
