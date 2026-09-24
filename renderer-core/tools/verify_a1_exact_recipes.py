#!/usr/bin/env python3
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

EXPECTED_PART_SHA256 = [
    "5b7d419f6db669123b8d136600ff87c5f3d360917186b7c288b65dee81824f9c",
    "c8ab0183d338ff5ad18e4fa07bfaf356cf2219113e7bc252880c3d251d90c7f6",
    "984846862069124303d040c95f76911b354c9ca133807d565487e7f3d60c4af8",
]
EXPECTED_CORPUS_SHA256 = "c38d5560e7dfc99f149da0908d754400e8bf654fe7e0a0469e5f08f47d7575b0"
EXPECTED_PLANS = 144
EXPECTED_OPS = 312
EXPECTED_MASK_COUNTS = {8: 72, 39: 12, 193: 24, 256: 36}

def parse_ops(field: str):
    ops = []
    for item in field.split(";"):
        off, old, new = (int(v) for v in item.split(","))
        ops.append((off, old, new))
    return ops

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--index", required=True)
    ap.add_argument("--part", action="append", required=True)
    ap.add_argument("--stamp")
    args = ap.parse_args()

    if len(args.part) != 3:
        raise SystemExit(f"expected 3 recipe parts, got {len(args.part)}")

    chunks = []
    rows = []
    for i, path in enumerate(args.part):
        raw = Path(path).read_bytes()
        sha = hashlib.sha256(raw).hexdigest()
        if sha != EXPECTED_PART_SHA256[i]:
            raise SystemExit(f"part {i + 1} SHA mismatch: {sha}")
        chunks.append(raw)
        for line_no, line in enumerate(raw.decode("utf-8").splitlines(), start=1):
            fields = line.split("\t")
            if len(fields) != 5:
                raise SystemExit(f"{path}:{line_no}: expected 5 TSV fields")
            original_sha, replacement_sha, code_size, mask, op_field = fields
            rows.append({
                "original_sha256": original_sha,
                "replacement_sha256": replacement_sha,
                "code_size": int(code_size),
                "mask": int(mask),
                "ops": parse_ops(op_field),
            })

    corpus_sha = hashlib.sha256(b"".join(chunks)).hexdigest()
    if corpus_sha != EXPECTED_CORPUS_SHA256:
        raise SystemExit(f"aggregate corpus SHA mismatch: {corpus_sha}")

    if len(rows) != EXPECTED_PLANS:
        raise SystemExit(f"expected {EXPECTED_PLANS} recipe rows, got {len(rows)}")
    if len({r["original_sha256"] for r in rows}) != EXPECTED_PLANS:
        raise SystemExit("duplicate original SHA in exact recipe corpus")

    op_count = sum(len(r["ops"]) for r in rows)
    if op_count != EXPECTED_OPS:
        raise SystemExit(f"expected {EXPECTED_OPS} patch ops, got {op_count}")

    mask_counts = Counter(r["mask"] for r in rows)
    if dict(mask_counts) != EXPECTED_MASK_COUNTS:
        raise SystemExit(f"mask counts mismatch: {dict(mask_counts)}")

    index_doc = json.loads(Path(args.index).read_text(encoding="utf-8"))
    plans = index_doc["plans"]
    if len(plans) != EXPECTED_PLANS:
        raise SystemExit(f"index plan count mismatch: {len(plans)}")

    for pos, (row, plan) in enumerate(zip(rows, plans)):
        checks = {
            "original_sha256": plan["original_sha256"],
            "replacement_sha256": plan["replacement_sha256"],
            "code_size": int(plan["code_size"]),
            "mask": int(plan["plan_mask"]),
        }
        for key, expected in checks.items():
            if row[key] != expected:
                raise SystemExit(
                    f"row {pos}: {key} mismatch: {row[key]!r} != {expected!r}"
                )

        last = -1
        for off, old, new in row["ops"]:
            if off % 4 != 0:
                raise SystemExit(f"row {pos}: unaligned DWORD offset {off}")
            if off + 4 > row["code_size"]:
                raise SystemExit(f"row {pos}: out-of-bounds DWORD offset {off}")
            if off <= last:
                raise SystemExit(f"row {pos}: patch offsets not strictly increasing")
            if not (0 <= old <= 0xFFFFFFFF and 0 <= new <= 0xFFFFFFFF):
                raise SystemExit(f"row {pos}: DWORD outside uint32 range")
            if old == new:
                raise SystemExit(f"row {pos}: no-op patch at {off}")
            last = off

    stamp = {
        "recipe_corpus_sha256": corpus_sha,
        "selected_plans": len(rows),
        "selected_ops": op_count,
        "mask_counts": {str(k): v for k, v in sorted(mask_counts.items())},
        "first_identity": rows[0]["original_sha256"],
        "first_offset": rows[0]["ops"][0][0],
        "last_identity": rows[-1]["original_sha256"],
        "last_offset": rows[-1]["ops"][-1][0],
    }
    if args.stamp:
        Path(args.stamp).write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
    else:
        print(json.dumps(stamp, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
