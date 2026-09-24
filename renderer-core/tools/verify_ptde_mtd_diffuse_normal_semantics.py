#!/usr/bin/env python3
import csv
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
census = json.loads((root / "data/census/ptde_mtd_diffuse_normal_semantics_v1.json").read_text(encoding="utf-8"))
assert census["schema"] == 1
assert set(census["states"]) == {"USE", "NO_USE", "UNKNOWN"}
assert census["runtime_activation"] == "OPEN"
assert census["pixel_equivalence"] == "OPEN"

def read_tsv(path):
    lines = [x for x in path.read_text(encoding="utf-8").splitlines() if x and not x.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))

diff = read_tsv(root / "data/provenance/diffuse_routes_v12.tsv")
norm = read_tsv(root / "data/provenance/normal_routes_v12.tsv")
assert len(diff) == 528, len(diff)
assert len(norm) == 557, len(norm)
assert len({r["diffuse_hash"] for r in diff}) == 524
assert sum(int(r["row_uses"]) for r in diff) == 3446
assert sum(int(r["row_uses"]) for r in norm) == 3872
assert all(r["spec_hash"] and r["diffuse_hash"] for r in diff)
assert all(r["diffuse_hash"] and r["spec_hash"] and r["normal_hash"] for r in norm)
assert census["default"]["diffuse"] == "UNKNOWN"
assert census["default"]["normal_bump"] == "UNKNOWN"
assert census["classifications"]["diffuse_exact_resource_route"]["state"] == "USE"
assert census["classifications"]["normal_bump_exact_resource_route"]["state"] == "USE"
print(f"ptde_mtd_diffuse_normal_semantics_v1: PASS diffuse={len(diff)} normal={len(norm)}")
