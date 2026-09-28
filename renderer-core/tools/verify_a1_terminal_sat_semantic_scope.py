#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
index = json.loads(
    (ROOT / "renderer-core/data/a1_island_plan_index_v1.json")
    .read_text(encoding="utf-8")
)
parts = [
    ROOT / "renderer-core/data/provenance/a1_exact_recipes_v1.part1.tsv",
    ROOT / "renderer-core/data/provenance/a1_exact_recipes_v1.part2.tsv",
    ROOT / "renderer-core/data/provenance/a1_exact_recipes_v1.part3.tsv",
]
materializer = (
    ROOT / "renderer-core/include/dsrrl/operators/legacy_plan/"
    "a1_create_time_materializer.hpp"
).read_text(encoding="utf-8")

rows = []
for path in parts:
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line:
            continue
        original, replacement, size, mask, ops = line.split("\t")
        parsed = []
        for item in ops.split(";"):
            off, old, new = (int(v) for v in item.split(","))
            parsed.append((off, old, new))
        rows.append((original, replacement, int(size), int(mask), parsed))

plans = index["plans"]
if len(rows) != 144 or len(plans) != 144:
    raise SystemExit(f"unexpected A1 corpus size: rows={len(rows)} plans={len(plans)}")

certified = 0
rejected = 0
mask8_rejected = 0
mask256_rejected = 0

for pos, (row, plan) in enumerate(zip(rows, plans)):
    original, replacement, size, mask, ops = row
    if original != plan["original_sha256"] or mask != int(plan["plan_mask"]):
        raise SystemExit(f"row/index mismatch at plan {pos}")

    name = str(plan["representative"])
    for _, old, new in ops:
        is_sat_toggle = (old & 0x2000) == 0 and new == (old | 0x2000)
        if not is_sat_toggle:
            continue

        certified_scope = (
            mask in (39, 193)
            and name.startswith("FRPG_Phn_")
            and "HemEnv" in name
        )
        certified_write_shape = (
            (mask == 39 and old == 0x05000036 and new == 0x05002036)
            or
            (mask == 193 and old == 0x09000037 and new == 0x09002037)
        )

        if certified_scope and certified_write_shape:
            certified += 1
        else:
            rejected += 1
            if mask == 8:
                mask8_rejected += 1
            if mask == 256:
                mask256_rejected += 1

if certified != 36:
    raise SystemExit(f"expected 36 certified Phn HemEnv RGB terminal SAT ops, got {certified}")
if rejected != 108:
    raise SystemExit(f"expected 108 historical SAT candidates to fail semantic authority, got {rejected}")
if mask8_rejected != 72:
    raise SystemExit(f"expected all 72 Gst SAT candidates rejected, got {mask8_rejected}")
if mask256_rejected != 36:
    raise SystemExit(f"expected all 36 Non/FaceEye SAT candidates rejected, got {mask256_rejected}")

required_runtime_tokens = (
    "a1_op_semantically_authorized",
    "0x05000036u",
    "0x05002036u",
    "0x09000037u",
    "0x09002037u",
    "plan.legacy_mask == 39u",
    "plan.legacy_mask == 193u",
    '"FRPG_Phn_"',
    '"HemEnv"',
)
missing = [token for token in required_runtime_tokens if token not in materializer]
if missing:
    raise SystemExit("A1 TerminalSat semantic runtime gate missing: " + ", ".join(missing))

print(
    "a1_terminal_sat_semantic_scope: PASS "
    "36 certified Phn HemEnv terminal RGB ops; "
    "108 historical Gst/Non/FaceEye SAT candidates fail open"
)
