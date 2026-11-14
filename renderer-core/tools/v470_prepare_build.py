#!/usr/bin/env python3
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf-8")
needle="import v469_prune_nonmetal_diag_overlay\n"
assert s.count(needle)==1 and "v470_release_info_cleanup_overlay" not in s
p.write_text(s.replace(needle,needle+"import v470_release_info_cleanup_overlay\n",1),encoding="utf-8")
print("V470 one addon: source-verified release INFO-only cut")
