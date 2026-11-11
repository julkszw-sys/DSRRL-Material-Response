#!/usr/bin/env python3
"""V463 integration after V459, one source tree, one addon."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf8")
needle="import v459_light_pair_overlay\n"
assert s.count(needle)==1 and "v463_playerins_owner_overlay" not in s,"source drift"
p.write_text(s.replace(needle,needle+"import v463_playerins_owner_overlay\n",1),encoding="utf8")
print("V463 scheduled AFTER V459, same one integrated addon")
