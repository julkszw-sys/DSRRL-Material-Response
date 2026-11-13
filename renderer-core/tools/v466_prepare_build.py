#!/usr/bin/env python3
"""Schedule V466 after the full existing V465 single-addon diagnostic chain."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf-8")
needle="import v465_ally_log_overlay\n"
assert s.count(needle)==1 and "v466_prewindow_owner_overlay" not in s, "source lineage drift"
p.write_text(s.replace(needle,needle+"import v466_prewindow_owner_overlay\n",1),encoding="utf-8")
print("V466: preserve V465 one-addon branch; add phase-aware owner evidence")
