#!/usr/bin/env python3
"""V465 after V459: native actor owner hook + Ally log-only auto aggregation."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf-8")
needle="import v459_light_pair_overlay\n"
assert s.count(needle)==1 and "v465_actor_owner_overlay" not in s
s=s.replace(needle,needle+"import v465_actor_owner_overlay\nimport v465_ally_log_overlay\n",1)
p.write_text(s,encoding="utf-8")
print("V465 one addon: V459 -> owner -> Ally log-only auto")
