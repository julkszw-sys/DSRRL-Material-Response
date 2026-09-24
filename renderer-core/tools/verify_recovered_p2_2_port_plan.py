#!/usr/bin/env python3
import argparse
import gzip
import hashlib
import json
from pathlib import Path

EXPECTED_SOURCE_SHA256 = "5cc15f8084fb75cb33c27be3f0e94be7fd186f07c16274b33ee09735b747a1ec"
EXPECTED_SOURCE_PLANS = 518
EXPECTED_SOURCE_OPS = 1436
EXPECTED_SELECTED_PLANS = 144
EXPECTED_SELECTED_OPS = 312

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-gzip", required=True)
    ap.add_argument("--index", required=True)
    ap.add_argument("--stamp")
    args = ap.parse_args()

    compressed = Path(args.source_gzip).read_bytes()
    raw = gzip.decompress(compressed)
    source_sha = hashlib.sha256(raw).hexdigest()
    if source_sha != EXPECTED_SOURCE_SHA256:
        raise SystemExit(f"source PORT_PLAN SHA mismatch: {source_sha}")

    source_doc = json.loads(raw.decode("utf-8"))
    source_plans = source_doc["plans"]
    source_ops = sum(len(p.get("ops", [])) for p in source_plans)
    if len(source_plans) != EXPECTED_SOURCE_PLANS:
        raise SystemExit(f"expected {EXPECTED_SOURCE_PLANS} source plans, got {len(source_plans)}")
    if source_ops != EXPECTED_SOURCE_OPS:
        raise SystemExit(f"expected {EXPECTED_SOURCE_OPS} source ops, got {source_ops}")

    index_doc = json.loads(Path(args.index).read_text(encoding="utf-8"))
    index_plans = index_doc["plans"]
    if len(index_plans) != EXPECTED_SELECTED_PLANS:
        raise SystemExit(f"expected {EXPECTED_SELECTED_PLANS} selected plans, got {len(index_plans)}")

    by_sha = {p["original_sha256"]: p for p in source_plans}
    if len(by_sha) != len(source_plans):
        raise SystemExit("duplicate original SHA in recovered source plan")

    selected_ops = 0
    offset_rows = []
    for ip in index_plans:
        sha = ip["original_sha256"]
        sp = by_sha.get(sha)
        if sp is None:
            raise SystemExit(f"selected identity missing from recovered source: {sha}")

        expected_aliases = [a["name"] for a in sp["aliases"]]
        checks = {
            "replacement_sha256": sp["replacement_sha256"],
            "code_size": sp["code_size"],
            "plan_mask": sp["mask"],
            "representative": sp["representative"],
            "aliases": expected_aliases,
        }
        for key, expected in checks.items():
            actual = ip[key]
            if actual != expected:
                raise SystemExit(
                    f"{sha}: index/source mismatch for {key}: {actual!r} != {expected!r}"
                )

        ops = sp.get("ops", [])
        if not ops:
            raise SystemExit(f"{sha}: selected plan has no patch operations")

        last_offset = -1
        for op in ops:
            off = int(op["byte_offset"])
            old = int(op["old"])
            new = int(op["new"])
            if off % 4 != 0:
                raise SystemExit(f"{sha}: unaligned DWORD offset {off}")
            if off + 4 > int(sp["code_size"]):
                raise SystemExit(f"{sha}: out-of-bounds DWORD offset {off}")
            if off <= last_offset:
                raise SystemExit(f"{sha}: patch offsets are not strictly increasing")
            if not (0 <= old <= 0xFFFFFFFF and 0 <= new <= 0xFFFFFFFF):
                raise SystemExit(f"{sha}: DWORD value outside uint32 range")
            if old == new:
                raise SystemExit(f"{sha}: no-op patch at offset {off}")
            last_offset = off
            offset_rows.append({
                "original_sha256": sha,
                "mask": int(sp["mask"]),
                "byte_offset": off,
                "old": old,
                "new": new,
                "reason": op.get("reason", ""),
            })
        selected_ops += len(ops)

    if selected_ops != EXPECTED_SELECTED_OPS:
        raise SystemExit(f"expected {EXPECTED_SELECTED_OPS} selected ops, got {selected_ops}")

    stamp = {
        "source_plan_sha256": source_sha,
        "source_plans": len(source_plans),
        "source_ops": source_ops,
        "selected_plans": len(index_plans),
        "selected_ops": selected_ops,
        "first_selected_op": offset_rows[0],
        "last_selected_op": offset_rows[-1],
    }
    if args.stamp:
        Path(args.stamp).write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
    else:
        print(json.dumps(stamp, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
