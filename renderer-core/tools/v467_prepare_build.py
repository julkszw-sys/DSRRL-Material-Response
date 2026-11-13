#!/usr/bin/env python3
"""Chain V467 after V466 on the same original V359-RC1-derived addon."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf-8")
needle="import v466_prewindow_owner_overlay\n"
assert s.count(needle)==1 and "v467_owner_probe_overlay" not in s,"V466 lineage drift"
p.write_text(s.replace(needle,needle+"import v467_owner_probe_overlay\n",1),encoding="utf-8")
print("V467: same integrated addon; no new native detour")
