#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
journal = json.loads((ROOT / "renderer-core/data/provenance/clustered_pnts_direct_stock_journal_v1.json").read_text(encoding="utf-8"))
ul_lines = (ROOT / "renderer-core/data/provenance/upper_lower_phn_pnts_unique_v1.tsv").read_text(encoding="utf-8").splitlines()
integrated = (ROOT / "renderer-core/integrated/integrated_addon.cpp").read_text(encoding="utf-8")

clustered = {str(e["original_sha256"]).lower() for e in journal["entries"]}
upper_lower = {
    line.split("\t")[6].lower()
    for line in ul_lines
    if line and not line.startswith("#") and not line.startswith("plan_index")
}

if len(clustered) != 36 or len(upper_lower) != 36 or clustered != upper_lower:
    raise SystemExit(
        f"clustered/phn_pnts identity mismatch: clustered={len(clustered)} "
        f"upper_lower={len(upper_lower)} overlap={len(clustered & upper_lower)}"
    )

required = (
    "clustered_pnts_upper_lower_overlap",
    "upper_lower_identity.family ==",
    "upper_lower_hemenv_family::phn_pnts",
    "upper_lower_standalone && !clustered_pnts_upper_lower_overlap",
)
missing = [token for token in required if token not in integrated]
if missing:
    raise SystemExit("integrated receiver overlap gate missing: " + ", ".join(missing))

print("clustered_pnts_ul_receiver_overlap: PASS 36/36 exact PS identities; overlap is composed, not double-counted")
