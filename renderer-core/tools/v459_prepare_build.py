#!/usr/bin/env python3
"""Make one integrated V459 addon only after exact V451 native capture overlay."""
from pathlib import Path
p=Path("renderer-core/tools/v438_apply_draw_part_b.py")
s=p.read_text(encoding="utf-8")
old="import v451_model_resource_overlay\n"
assert s.count(old)==1 and "v459_light_pair_overlay" not in s,"V459 overlay scheduling drift"
p.write_text(s.replace(old,old+"import v459_light_pair_overlay\n",1),encoding="utf-8")
print("V459 scheduled after V451 in one addon")
