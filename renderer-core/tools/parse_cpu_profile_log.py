#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

MARKERS = {
    "selector_r50": "[DSRRL PERF R50] SELECTOR",
    "selector_r45": "[DSRRL PERF R45] SELECTOR",
    "selector_r32": "[DSRRL PERF R32] SELECTOR",
    "pointlight_capture_r49": "[DSRRL PERF R49] POINTLIGHT_CAPTURE",
    "pointlight_prep_r47": "[DSRRL PERF R47] POINTLIGHT_PREP",
    "pointlight_prep_r46": "[DSRRL PERF R46] POINTLIGHT_PREP",
    "pointlight_prep_r32": "[DSRRL PERF R32] POINTLIGHT_PREP",
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

    selector_line = latest.get("selector_r50") or latest.get("selector_r45") or latest.get("selector_r32")
    line = selector_line
    if selector_line:
        p = sample_period(selector_line)
        n = plain_n(selector_line)
        stage_keys = [
            ("FLVER_PREFIX", "prefix_us"),
            ("FLVER_RESOLVE", "resolve_us"),
            ("FLVER_CACHE_LOOKUP", "cache_lookup_us"),
            ("FLVER_CACHE_PUBLISH", "cache_publish_us"),
            ("FLVER_OWNER_LOOKUP", "owner_lookup_us"),
            ("FLVER_OWNER_MTD", "owner_mtd_us"),
            ("FLVER_SELECTION_PUBLISH", "selection_publish_us"),
            ("FLVER_POINTLIGHT_BRIDGE", "pointlight_bridge_us"),
            ("FLVER_PMETAL_SOURCE", "pmetal_source_us"),
            ("FLVER_RUNTIME_MTD", "runtime_mtd_us"),
            ("FLVER_RUNTIME_PUBLISH", "runtime_publish_us"),
        ]
        accounted = number(selector_line, "accounted_us")
        if accounted is None:
            values = [number(line, key) for _, key in stage_keys]
            accounted = sum(v for v in values if v is not None)
        add(rows, "FLVER_SELECTOR_ACCOUNTED", accounted, n, p)
        for component, key in stage_keys:
            add(rows, component, number(line, key), n, p,
                "FLVER_SELECTOR_ACCOUNTED")

    line = latest.get("pointlight_prep_r47") or latest.get("pointlight_prep_r46") or latest.get("pointlight_prep_r32")
    pointlight_wall_envelope = None
    if line:
        p = sample_period(line)
        if "POINTLIGHT_PREP sample=1/" in line and "producer_wall_us=" in line:
            pointlight_wall_envelope = {
                "avg_wall_us": number(line, "producer_wall_us"),
                "max_wall_us": number(line, "producer_wall_max_us"),
                "select_avg_cycles": number(line, "select_cycles"),
                "select_max_cycles": number(line, "select_cycles_max"),
                "capture_avg_cycles": number(line, "capture_cycles"),
                "capture_max_cycles": number(line, "capture_cycles_max"),
                "note": "Outer QPC wall is scheduler-sensitive. R47 thread-cycle counters are the preferred CPU ranking signal for select/capture."
            }
            add(rows, "POINTLIGHT_SELECT", number(line, "select_us"),
                tagged_n(line, "select"), p, "POINTLIGHT_PRODUCER_WALL")
            add(rows, "POINTLIGHT_CAPTURE_SOURCES",
                number(line, "capture_sources_us"),
                tagged_n(line, "capture"), p, "POINTLIGHT_PRODUCER_WALL")
            add(rows, "POINTLIGHT_SIDECAR_BUILD",
                number(line, "sidecar_build_us"),
                tagged_n(line, "build"), p)
            add(rows, "POINTLIGHT_AUTHORITY", number(line, "authority_us"),
                tagged_n(line, "auth"), p)
            add(rows, "POINTLIGHT_PREPARE", number(line, "prepare_us"),
                tagged_n(line, "prep"), p)
            add(rows, "POINTLIGHT_GPU_CACHE", number(line, "gpu_cache_us"),
                tagged_n(line, "gpu"), p)
            add(rows, "POINTLIGHT_UPLOAD", number(line, "upload_us"),
                tagged_n(line, "upload"), p)
        else:
            add(rows, "POINTLIGHT_PRODUCER_WALL", number(line, "producer_us"),
                tagged_n(line, "prod"), p)
            add(rows, "POINTLIGHT_PREPARE", number(line, "prepare_us"),
                tagged_n(line, "prep"), p)

    capture_detail = latest.get("pointlight_capture_r49")
    if capture_detail and pointlight_wall_envelope is not None:
        pointlight_wall_envelope["capture_subpaths"] = {
            "preamble_avg_cycles": number(capture_detail, "preamble_cycles"),
            "preamble_max_cycles": number(capture_detail, "preamble_max"),
            "semantic_avg_cycles": number(capture_detail, "semantic_cycles"),
            "semantic_max_cycles": number(capture_detail, "semantic_max"),
            "semantic_bank_avg_cycles": number(capture_detail, "semantic_bank_cycles"),
            "semantic_bank_max_cycles": number(capture_detail, "semantic_bank_max"),
            "semantic_lerp_avg_cycles": number(capture_detail, "semantic_lerp_cycles"),
            "semantic_lerp_max_cycles": number(capture_detail, "semantic_lerp_max"),
            "donor_avg_cycles": number(capture_detail, "donor_cycles"),
            "donor_max_cycles": number(capture_detail, "donor_max"),
            "donor_bank_avg_cycles": number(capture_detail, "donor_bank_cycles"),
            "donor_bank_max_cycles": number(capture_detail, "donor_bank_max"),
            "donor_direct_avg_cycles": number(capture_detail, "donor_direct_cycles"),
            "donor_direct_max_cycles": number(capture_detail, "donor_direct_max"),
        }

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
            "pointlight_wall_envelope": pointlight_wall_envelope,
            "selector_wall_envelope": (
                {
                    "avg_wall_us": number(selector_line, "total_wall_us") or number(selector_line, "total_us"),
                    "max_wall_us": number(selector_line, "max_total_wall_us") or number(selector_line, "max_total_us"),
                    "unaccounted_wall_us": number(selector_line, "unaccounted_wall_us"),
                    "note": "Outer selector wall envelope includes any unbucketed addon work. R50 explicitly accounts the PointLight bridge; residual only is excluded from CPU ranking.",
                }
                if selector_line else None
            ),
            "markers_found": sorted(latest),
        }, indent=2))
        return 0

    if not rows:
        print("No DSRRL CPU profiler markers found.")
        return 2

    print("DSRRL CPU HOTSPOT RANKING")
    print("NOTE: sampled/extrapolated; nested substages are not additive.")
    if selector_line:
        wall = number(selector_line, "total_wall_us") or number(selector_line, "total_us")
        max_wall = number(selector_line, "max_total_wall_us") or number(selector_line, "max_total_us")
        unaccounted = number(selector_line, "unaccounted_wall_us")
        if wall is not None:
            suffix = (
                f", unaccounted={unaccounted:.3f} us"
                if unaccounted is not None else "")
            print(
                f"Selector outer wall envelope: avg={wall:.3f} us "
                f"max={max_wall:.3f} us{suffix}; excluded from CPU ranking.")
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
