#!/usr/bin/env python3
"""Generate exact semantic-MTD -> raw-MTD-SHA registry from a DSR install root.

This registry is identity evidence only. A semantic basename hash resolves only
when every DSR .mtd carrying that basename has one identical raw SHA-256.
Collisions are emitted explicitly and runtime must fail open on them.
"""
from __future__ import annotations
import argparse, hashlib
from collections import defaultdict
from pathlib import Path


def fnv1a64(text: str) -> int:
    h=14695981039346656037
    for b in text.encode("utf-8"):
        h ^= b; h=(h*1099511628211)&0xffffffffffffffff
    return h

def sha256_file(path: Path) -> bytes:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda:f.read(8<<20),b""): h.update(block)
    return h.digest()

def arr(d: bytes) -> str:
    return "{{"+",".join(f"0x{x:02x}u" for x in d)+"}}"

def main()->int:
    ap=argparse.ArgumentParser(); ap.add_argument("root",type=Path); ap.add_argument("--output",type=Path,required=True); a=ap.parse_args()
    by_hash=defaultdict(lambda:defaultdict(list)); errors=[]
    for p in a.root.rglob("*"):
        if not p.is_file() or p.suffix.casefold()!=".mtd": continue
        try: d=sha256_file(p)
        except OSError as e: errors.append((str(p),str(e))); continue
        by_hash[fnv1a64(p.name)][d].append(p.name)
    if errors: raise SystemExit(f"MTD scan incomplete: {len(errors)} read errors")
    rows=[]
    for semantic, variants in sorted(by_hash.items()):
        if len(variants)==1:
            d=next(iter(variants)); rows.append((semantic,d,False))
        else:
            rows.append((semantic,bytes(32),True))
    o=["#pragma once\n#include <array>\n#include <cstdint>\n\nnamespace dsrrl::operators::material_response::generated {\n",
       "struct dsr_mtd_identity_record { std::uint64_t semantic_name_hash; std::array<std::uint8_t,32> raw_mtd_sha256; bool ambiguous; };\n",
       "inline constexpr bool k_dsr_mtd_identity_source_complete=true;\n",
       f"inline constexpr std::array<dsr_mtd_identity_record,{len(rows)}u> k_dsr_mtd_identity_registry={{{{\n"]
    for h,d,amb in rows: o.append(f"{{0x{h:016x}ull,{arr(d)},{str(amb).lower()}}},\n")
    o += ["}};\n",
          "constexpr bool dsr_mtd_identity_resolve(std::uint64_t h,std::array<std::uint8_t,32>&out) noexcept{out={};std::size_t lo=0,hi=k_dsr_mtd_identity_registry.size();while(lo<hi){auto m=lo+(hi-lo)/2u;if(k_dsr_mtd_identity_registry[m].semantic_name_hash<h)lo=m+1u;else hi=m;}if(lo>=k_dsr_mtd_identity_registry.size())return false;const auto&r=k_dsr_mtd_identity_registry[lo];if(r.semantic_name_hash!=h||r.ambiguous)return false;out=r.raw_mtd_sha256;return true;}\n",
          "constexpr bool dsr_mtd_identity_ambiguous(std::uint64_t h) noexcept{for(const auto&r:k_dsr_mtd_identity_registry)if(r.semantic_name_hash==h)return r.ambiguous;return false;}\n",
          "} // namespace dsrrl::operators::material_response::generated\n"]
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text("".join(o),encoding="utf-8",newline="\n")
    print(f"DSR_MTD_IDENTITY_PASS records={len(rows)} ambiguous={sum(x[2] for x in rows)}")
    return 0
if __name__=="__main__": raise SystemExit(main())
