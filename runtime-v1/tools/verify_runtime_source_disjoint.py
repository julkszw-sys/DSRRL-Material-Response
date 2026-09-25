#!/usr/bin/env python3
from __future__ import annotations
import argparse,re
from pathlib import Path

SHA=re.compile(r"\b[a-f0-9]{64}\b")

def hashes(path:Path)->set[str]:
    return set(SHA.findall(path.read_text(encoding="utf-8").lower()))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--a1",required=True,type=Path)
    ap.add_argument("--mr",required=True,type=Path)
    ap.add_argument("--lerp",required=True,type=Path)
    ns=ap.parse_args()
    a1=hashes(ns.a1)
    mr=hashes(ns.mr)
    lerp=hashes(ns.lerp)
    a1_mr=sorted(a1 & mr)
    a1_lerp=sorted(a1 & lerp)
    if a1_mr or a1_lerp:
        raise SystemExit(
            "A1/MR exact-shader authority overlap: "
            f"a1_vs_mr={a1_mr} a1_vs_lerp={a1_lerp}")
    print(
        "runtime_source_disjoint: PASS "
        f"a1_hashes={len(a1)} mr_hashes={len(mr)} lerp_hashes={len(lerp)}")

if __name__=="__main__":
    main()
