#!/usr/bin/env python3
"""Read-only log timeline: NEVER treat gaps between INFO lines as frame time.

Emits objective log events plus whether the binary logged QPC Present/frame
samples. This parser contains no hooks, graphics state, texture cache, or
shader behavior; it does not ship private ReShade logs into the repository.
"""
from __future__ import annotations
import argparse
import collections
import json
import re
from pathlib import Path

HEAD = re.compile(r"^(\d{2}):(\d{2}):(\d{2}):(\d{3})\s+\[\s*(\d+)\]")
TAG = re.compile(r"\[DSRRL ([^]]+)\]")
PRESENT_PREFIX = "[DSRRL STUTTER R43]"
HITCH_PREFIX = "[DSRRL HITCH FRAME]"
SIDECAR_PREFIX = "[DSRRL SIDECAR 1295] stage=resource_view_init"

def audit(text: str) -> dict:
    timeline = []
    groups = collections.Counter()
    per_second = collections.Counter()
    threads = collections.Counter()
    first_dds = []
    probes = []
    for line_number, line in enumerate(text.splitlines(), 1):
        m = HEAD.match(line)
        if not m:
            continue
        h, minutes, seconds, ms, thread = map(int, m.groups())
        stamp = (((h * 60) + minutes) * 60 + seconds) * 1000 + ms
        tag = TAG.search(line)
        if tag:
            groups[tag.group(1)] += 1
        per_second[line[:8]] += 1
        threads[str(thread)] += 1
        if SIDECAR_PREFIX in line:
            first_dds.append({"line": line_number, "timestamp": line[:12]})
        if "[DSRRL V229 GLOSS CUT]" in line:
            pm = re.search(r"\bprobe=(\d+),(\d+)", line)
            if pm:
                probes.append({"line": line_number, "timestamp": line[:12],
                               "probe_a": int(pm.group(1)), "probe_b": int(pm.group(2))})
        timeline.append((stamp, line_number, line[:12], tag.group(1) if tag else "reshade"))
    gaps = [
        {"gap_ms": timeline[i][0] - timeline[i-1][0],
         "after_line": timeline[i-1][1], "before_line": timeline[i][1],
         "from": timeline[i-1][2], "to": timeline[i][2],
         "is_frame_hitch": False}
        for i in range(1, len(timeline))
        if timeline[i][0] - timeline[i-1][0] >= 1000
    ]
    measured = sum(PRESENT_PREFIX in line for line in text.splitlines())
    hitches = sum(HITCH_PREFIX in line for line in text.splitlines())
    return {
        "schema": "DSRRL_V233_RESHADE_LOG_ACTIVITY_ONLY_v1",
        "total_timestamped_lines": len(timeline),
        "time_first": timeline[0][2] if timeline else None,
        "time_last": timeline[-1][2] if timeline else None,
        "present_profile_windows": measured,
        "hitch_frame_samples": hitches,
        "can_attribute_stutter_cause": bool(measured and hitches),
        "largest_log_silence_gaps": sorted(gaps,key=lambda x:x["gap_ms"],reverse=True)[:12],
        "peak_logged_seconds": per_second.most_common(12),
        "events_by_thread": dict(threads),
        "events_by_tag": dict(groups),
        "sidecar_init_logged": len(first_dds),
        "last_sidecar_inits": first_dds[-12:],
        "distinct_logged_gloss_probes": sorted({e["probe_a"] for e in probes}),
        "warning": "A gap between ReShade INFO lines is never a measured Present/CPU/GPU stall."
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--output", type=Path)
    args = ap.parse_args()
    result = json.dumps(audit(args.log.read_text(encoding="utf-8-sig",errors="replace")),
                        indent=2,sort_keys=True)+"\n"
    if args.output:
        args.output.write_text(result,encoding="utf-8")
    print(result)

if __name__ == "__main__":
    main()
