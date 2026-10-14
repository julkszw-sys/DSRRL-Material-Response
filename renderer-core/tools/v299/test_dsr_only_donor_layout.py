#!/usr/bin/env python3
"""R43 original bank layout provenance: never extrapolate PTDE beyond donor_count."""
import csv
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
TABLE = ROOT / "data/provenance/pmetal_env_source_layout_v1.tsv"
AUTHORITY = ROOT / "data/provenance/envdiffuse_cross_version_homology_gate_v1.json"
def main():
    with TABLE.open(newline="", encoding="utf-8") as f:
        lines=(x for x in f if x.strip() and not x.lstrip().startswith("#"))
        rows=list(csv.DictReader(lines,delimiter="\t"))
    assert len(rows)==20
    assert {int(x["bank_index"]) for x in rows}==set(range(20))
    live=0;donors=0;unpaired=[]
    for r in rows:
        a=int(r["live_count"]);b=int(r["donor_count"])
        assert 0<b<=a<=256
        live+=a;donors+=b
        for row in range(b,a):
            unpaired.append((r["bank_name"],row))
    assert (live,donors)==(1249,1246)
    assert sorted(unpaired)==sorted([
        ("m14_LightBank.param",64),
        ("s14_LightBank.param",64),
        ("m18_LightBank.param",64),
    ])
    homology=json.loads(AUTHORITY.read_text())
    assert homology["policy"]["classic_assignment_homology_verified"] is False
    assert homology["cross_version_homology"]["state"]=="OPEN"
    print("V299_DSR_ONLY_ROW_PROVENANCE_LAYOUT_PASS banks=20 native=1249 ptde=1246 dsr_only=3")
if __name__=="__main__":
    main()
