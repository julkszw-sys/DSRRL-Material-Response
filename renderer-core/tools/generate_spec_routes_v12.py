#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path

def load(path: Path):
    rows=[]
    for raw in path.read_text(encoding="utf-8").splitlines():
        line=raw.strip()
        if not line or line.startswith("#") or line=="hash\tlogical_name":
            continue
        h,name=line.split("\t",1)
        if len(h)!=16:
            raise SystemExit(f"bad hash: {h}")
        rows.append((int(h,16),name))
    rows=sorted(set(rows),key=lambda x:x[0])
    if len(rows)!=769:
        raise SystemExit(f"expected 769 spec names, got {len(rows)}")
    if len({h for h,_ in rows})!=len(rows):
        raise SystemExit("duplicate/colliding spec hash")
    return rows

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True)
    ap.add_argument("--output",required=True)
    a=ap.parse_args()
    rows=load(Path(a.input))
    out=[
        "#pragma once\n#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::runtime::generated {\n\n",
        f"inline constexpr std::array<std::uint64_t, {len(rows)}> k_spec_name_hashes_v12 = {{{{\n",
    ]
    for h,name in rows:
        out.append(f"    0x{h:016x}ull, // {name}\n")
    out += [
        "}};\n\n",
        "inline bool spec_name_hash_allowed_v12(std::uint64_t value) noexcept\n",
        "{\n",
        "    std::size_t lo=0,hi=k_spec_name_hashes_v12.size();\n",
        "    while(lo<hi){const auto mid=lo+((hi-lo)>>1u);const auto at=k_spec_name_hashes_v12[mid];\n",
        "        if(value<at)hi=mid;else if(value>at)lo=mid+1u;else return true;}\n",
        "    return false;\n",
        "}\n",
        "inline constexpr std::size_t k_spec_name_count_v12=k_spec_name_hashes_v12.size();\n\n",
        "} // namespace dsrrl::runtime::generated\n",
    ]
    Path(a.output).write_text("".join(out),encoding="utf-8")

if __name__=="__main__":
    main()
