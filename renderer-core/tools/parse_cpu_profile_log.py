#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

MARKERS = {
    "selector": "[DSRRL PERF R32] SELECTOR",
    "pointlight_prep": "[DSRRL PERF R32] POINTLIGHT_PREP",
    "pointlight_tx": "[DSRRL PERF R32] POINTLIGHT_TX",
    "dof": "[DSRRL PERF R44] DOF",
}


def number(line: str, key: str) -> float | None:
    m = re.search(rf"(?:^|\s){re.escape(key)}=([0-9]+(?:\.[0-9]+)?)", line)
    return float(m.group(1)) if m else None


def sample_period(line: str) -> int | None:
    m = re.search(r"sample=1/(\d+)", line)
    return int(m.group(1)) if m else None


def plain_n(line: str) -> int | None:
    m = re.search(r"(?:^|\s)n=(\d+)(?:\s|$)", line)
    return int(m.group(1)) if m else None


def tagged_n(line: str, tag: str) -> int | None:
    m = re.search(rf"(?:^|\s)n=[^\n]*?{re.escape(tag)}:(\d+)", line)
    return int(m.group(1)) if m else None


def add(rows: list[dict], name: str, avg_us: float | None, n: int | None,
        period: int | None, parent: str | None = None) -> None:
    if avg_us is None or n is None or period is None or n <= 0:
        return
    estimated_calls = n * period
    estimated_cpu_ms = avg_us * estimated_calls / 1000.0
    rows.append({
        "component": name,
        "avg_us": avg_us,
        "sample_count": n,
        "sample_period": period,
        "estimated_calls": estimated_calls,
        "estimated_cpu_ms_since_launch": estimated_cpu_ms,
        "overlap_parent": parent,
    })


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Rank DSRRL sampled CPU telemetry from a ReShade log.")
    ap.add_argument("log", type=Path)
    ap.add_argument("--json", action="store_true", dest="as_json")
    args = ap.parse_args()

    latest: dict[str, str] = {}
    for raw in args.log.read_text(encoding="utf-8", errors="replace").splitlines():
        for key, marker in MARKERS.items():
            if marker in raw:
                latest[key] = raw

    rows: list[dict] = []

    line = latest.get("selector")
    if line:
        p = sample_period(line)
        n = plain_n(line)
        add(rows, "FLVER_SELECTOR_TOTAL", number(line, "total_us"), n, p)
        add(rows, "FLVER_PMETAL_SOURCE", number(line, "pmetal_source_us"), n, p,
            "FLVER_SELECTOR_TOTAL")
        add(rows, "FLVER_OWNER_MTD", number(line, "owner_mtd_us"), n, p,
            "FLVER_SELECTOR_TOTAL")
        add(rows, "FLVER_RESOLVE", number(line, "resolve_us"), n, p,
            "FLVER_SELECTOR_TOTAL")

    line = latest.get("pointlight_prep")
    if line:
        p = sample_period(line)
        add(rows, "POINTLIGHT_PRODUCER", number(line, "producer_us"),
            tagged_n(line, "prod"), p)
        add(rows, "POINTLIGHT_PREPARE", number(line, "prepare_us"),
            tagged_n(line, "prep"), p)

    line = latest.get("pointlight_tx")
    if line:
        add(rows, "POINTLIGHT_TRANSACTION", number(line, "total_us"),
            plain_n(line), sample_period(line))

    line = latest.get("dof")
    if line:
        add(rows, "DOF_DRAW_CALLBACK", number(line, "total_us"),
            plain_n(line), sample_period(line))

    rows.sort(
        key=lambda r: r["estimated_cpu_ms_since_launch"],
        reverse=True)

    if args.as_json:
        print(json.dumps({
            "warning": (
                "Estimated CPU totals extrapolate sampled calls. Substages with "
                "overlap_parent are nested and must not be summed with their parent."),
            "rows": rows,
            "markers_found": sorted(latest),
        }, indent=2))
        return 0

    if not rows:
        print("No DSRRL CPU profiler markers found.")
        return 2

    print("DSRRL CPU HOTSPOT RANKING")
    print("NOTE: sampled/extrapolated; nested substages are not additive.")
    print(f"{'#':>2}  {'component':<28} {'avg_us':>10} {'est_calls':>12} {'est_cpu_ms':>12}  overlap")
    for i, row in enumerate(rows, 1):
        overlap = row["overlap_parent"] or "-"
        print(
            f"{i:>2}  {row['component']:<28} "
            f"{row['avg_us']:>10.3f} "
            f"{row['estimated_calls']:>12d} "
            f"{row['estimated_cpu_ms_since_launch']:>12.3f}  "
            f"{overlap}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
