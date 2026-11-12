#!/usr/bin/env python3
"""Schedule V464 once, immediately after V459 in existing one-addon overlay."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf8")
a="import v459_light_pair_overlay\n"
assert s.count(a)==1 and "import v464_actor_owner_overlay" not in s,("V464 schedule drift",s.count(a))
p.write_text(s.replace(a,a+"import v464_actor_owner_overlay\n",1),encoding="utf8")
print("V464 scheduled after V459 in ONE addon")
