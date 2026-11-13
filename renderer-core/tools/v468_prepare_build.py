#!/usr/bin/env python3
"""Apply V468 performance guard after existing single-addon V467 overlay."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf-8")
needle="import v467_owner_probe_overlay\n"
assert s.count(needle)==1 and "v468_low_overhead_overlay" not in s,"V467 lineage drift"
p.write_text(s.replace(needle,needle+"import v468_low_overhead_overlay\n",1),encoding="utf-8")
print("V468 single-addon chain retained; redundant native source observer omitted")
