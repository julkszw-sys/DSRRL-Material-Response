#!/usr/bin/env python3
"""PR292: compare complete observed native b0 CPU-upload register hashes.

Every record is 129 contiguous, epoch-coherent 16-byte FNV-1a hashes.
Pair only neighboring *sampled* bank changes with same owner SHA prefix,
slot, receiver, native PS and D3D context. These are correlations,
NOT independent trials, proof of identical camera, or PTDE pixel authority.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import sys

TAG = "[DSRRL PMETAL B0 REGHASH]"
M10 = "4c594553d201d80c"
M18 = "1ecfd1e617c59071"
TOKEN = re.compile(r"([a-z_]+)=([^\s]+)")


def parse(lines):
    samples = {}
    rejected = Counter()
    for line in lines:
        if TAG not in line:
            continue
        data = dict(TOKEN.findall(line.split(TAG, 1)[1]))
        try:
            sample = int(data["sample"])
            part = int(data["part"])
            first = int(data["first"])
            end = int(data["end_exclusive"])
            hashes = data["hashes"].split(",")
            expected_count = end - first
            assert 0 <= part <= 8
            assert first == part * 16
            assert 0 < expected_count <= 16
            assert expected_count == len(hashes)
            assert all(re.fullmatch(r"[0-9a-f]{16}", h) for h in hashes)
            assert data["count"] == "129"
            assert data["domain"] == "CPU_MAP_WRITE_ONLY"
            assert data["bank"] in (M10, M18)
            assert len(data["whole"]) == 16
        except (AssertionError, ValueError, KeyError):
            rejected["malformed_chunk"] += 1
            continue
        metadata_keys = (
            "sample", "ms", "rx", "slot", "owner_sha0", "bank",
            "row", "producer_rva", "ctx", "ps", "b0", "epoch", "whole"
        )
        metadata = tuple(data[k] for k in metadata_keys)
        s = samples.setdefault(sample, {"metadata": metadata, "parts": {}})
        if s["metadata"] != metadata or part in s["parts"]:
            rejected["inconsistent_or_duplicate_part"] += 1
            s["invalid"] = True
            continue
        s["parts"][part] = (first, hashes)

    completed = []
    for s in samples.values():
        if s.get("invalid") or len(s["parts"]) != 9:
            rejected["incomplete_snapshot"] += 1
            continue
        sequence = []
        for part in range(9):
            first, hashes = s["parts"][part]
            if len(sequence) != first:
                sequence = []
                break
            sequence.extend(hashes)
        if len(sequence) != 129:
            rejected["noncontiguous_snapshot"] += 1
            continue
        m = dict(zip(metadata_keys, s["metadata"]))
        m["ms"] = int(m["ms"])
        m["epoch"] = int(m["epoch"])
        m["reg_hashes"] = sequence
        completed.append(m)
    return completed, rejected


def compare(records, window_ms=2000):
    groups = defaultdict(list)
    for rec in records:
        # Stable shader+context+owner prefix+slot+receiver; no claim that
        # camera, material instance or GPU draw epoch is identical.
        key = tuple(rec[k] for k in
                    ("owner_sha0", "slot", "rx", "ps", "ctx"))
        groups[key].append(rec)
    switches = []
    changed = Counter()
    unchanged = Counter()
    for key, group in groups.items():
        group.sort(key=lambda r: (r["ms"], r["epoch"]))
        for before, after in zip(group, group[1:]):
            if before["bank"] == after["bank"]:
                continue
            gap = after["ms"] - before["ms"]
            if gap < 0 or gap > window_ms:
                continue
            ids = [i for i, (a, b) in enumerate(zip(
                before["reg_hashes"], after["reg_hashes"])) if a != b]
            for i in range(129):
                (changed if i in ids else unchanged)[i] += 1
            switches.append({
                "owner_sha0": key[0], "slot": key[1], "rx": key[2],
                "ps": key[3], "ctx": key[4],
                "from": "m10" if before["bank"] == M10 else "m18",
                "to": "m10" if after["bank"] == M10 else "m18",
                "ms_gap": gap,
                "writer_epochs": [before["epoch"], after["epoch"]],
                "same_cb_pointer": before["b0"] == after["b0"],
                "changed_count": len(ids), "changed_indices": ids,
            })
    summary = {
        "scope": "sampled CPU Map/Unmap b0 16-byte fingerprints, not shader ABI/pixel proof",
        "complete_samples": len(records),
        "by_bank": dict(Counter("m10" if r["bank"] == M10 else "m18"
                                for r in records)),
        "same_shader_context_sampled_bank_switches": len(switches),
        "switch_groups_are_not_independent_events": True,
        "total_changed_per_register": [changed[i] for i in range(129)],
        "total_unchanged_per_register": [unchanged[i] for i in range(129)],
        "switches": switches,
    }
    return summary


def selftest():
    def nine(sample, bank, changed_at=None):
        a = ["0123456789abcdef"] * 129
        if changed_at is not None:
            a[changed_at] = "abcdef0123456789"
        lines = []
        for part in range(9):
            first = part * 16
            chunk = a[first:first+16]
            lines.append(
                f"{TAG} sample={sample} part={part} first={first} "
                f"end_exclusive={first+len(chunk)} ms={100+sample*10} "
                f"rx=33 slot=0 owner_sha0=abcdef12 bank={bank} row=32 "
                f"producer_rva=20e019 ctx=ab ps=cd b0=ef "
                f"epoch={sample} whole=1234567890abcdef count=129 "
                f"domain=CPU_MAP_WRITE_ONLY hashes={','.join(chunk)}")
        return lines
    records, invalid = parse(nine(1, M10) + nine(2, M18, 64))
    result = compare(records)
    assert not invalid and len(records) == 2
    assert result["same_shader_context_sampled_bank_switches"] == 1
    assert result["switches"][0]["changed_indices"] == [64]
    print("PMETAL_B0_REGHASH_ANALYZER_SELFTEST_PASS")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("log", nargs="?", type=Path)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--window-ms", type=int, default=2000)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return 0
    if args.log is None:
        ap.error("provide a ReShade.log or --selftest")
    with args.log.open("r", encoding="utf-8", errors="replace") as f:
        records, invalid = parse(f)
    result = compare(records, args.window_ms)
    result["invalid_chunks_or_samples"] = dict(invalid)
    payload = json.dumps(result, indent=2, ensure_ascii=False)
    if args.output:
        args.output.write_text(payload+"\n", encoding="utf-8")
    else:
        print(payload)
    return 0

if __name__ == "__main__":
    sys.exit(main())
