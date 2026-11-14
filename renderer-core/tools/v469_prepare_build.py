#!/usr/bin/env python3
"""Continue existing V468 one addon; prune only unrelated nonmetal telemetry."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf-8")
needle="import v468_low_overhead_overlay\n"
assert s.count(needle)==1 and "v469_prune_nonmetal_diag_overlay" not in s
p.write_text(s.replace(needle,needle+"import v469_prune_nonmetal_diag_overlay\n",1),encoding="utf-8")
print("V469 one-addon chain: bounded RX34, nonmetal trace pruned")
