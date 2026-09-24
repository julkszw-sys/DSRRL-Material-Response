#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv
from pathlib import Path

EXPECTED_COUNT=24

def load(path:Path):
    lines=[x for x in path.read_text(encoding="utf-8").splitlines() if x and not x.startswith("#")]
    rows=list(csv.DictReader(lines,delimiter="\t"))
    if len(rows)!=EXPECTED_COUNT: raise SystemExit("expected 24 U/L stable rows")
    for i,r in enumerate(rows):
        if int(r["host_index"])!=i: raise SystemExit("host index mismatch")
        for key in ("v29_sha256","v29_ul_sha256"):
            v=r[key].lower()
            if len(v)!=64 or any(c not in "0123456789abcdef" for c in v):
                raise SystemExit(f"bad SHA in {key}")
    return rows

def render(rows):
    out=["#pragma once\n","#include <array>\n#include <cstddef>\n#include <string_view>\n\n",
         "namespace dsrrl::runtime::generated {\n\n",
         "struct ul_stable_hash_pair { std::string_view v29; std::string_view v29_ul; };\n\n",
         f"inline constexpr std::array<ul_stable_hash_pair, {len(rows)}> k_ul_stable_hashes = {{{{\n"]
    for r in rows:
        out.append(f'    {{"{r["v29_sha256"].lower()}","{r["v29_ul_sha256"].lower()}"}},\n')
    out.append("}};\n\n")
    out.append("inline constexpr std::size_t k_ul_stable_hash_count=k_ul_stable_hashes.size();\n\n")
    out.append("} // namespace dsrrl::runtime::generated\n")
    return "".join(out)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--input",required=True,type=Path); ap.add_argument("--output",required=True,type=Path)
    ns=ap.parse_args(); ns.output.write_text(render(load(ns.input)),encoding="utf-8")
if __name__=="__main__":main()
