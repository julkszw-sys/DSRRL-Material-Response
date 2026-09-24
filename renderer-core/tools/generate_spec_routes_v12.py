#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,hashlib
from pathlib import Path

EXPECTED_CANONICAL_DATA_SHA256="ca9825ea04f82ae5449aad58a42e4d84cf6b34eded15eef3a6afdfe715e15c95"
EXPECTED_COUNT=769

def load(path:Path):
    lines=[x for x in path.read_text(encoding="utf-8").splitlines() if x and not x.startswith("#")]
    canonical=("\n".join(lines)+"\n").encode("utf-8")
    actual=hashlib.sha256(canonical).hexdigest()
    if actual!=EXPECTED_CANONICAL_DATA_SHA256:
        raise SystemExit(f"canonical data SHA256 mismatch: {actual} != {EXPECTED_CANONICAL_DATA_SHA256}")
    rows=list(csv.DictReader(lines,delimiter="\t"))
    values=[int(r["spec_hash"],16) for r in rows]
    if len(values)!=EXPECTED_COUNT or len(set(values))!=EXPECTED_COUNT:
        raise SystemExit("SpecRGB hash count/uniqueness mismatch")
    if values!=sorted(values):
        raise SystemExit("SpecRGB corpus is not sorted")
    return values

def render(values):
    out=["#pragma once\n","#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
         "namespace dsrrl::runtime::generated {\n\n",
         f"inline constexpr std::array<std::uint64_t, {len(values)}> k_spec_name_hashes_v12 = {{{{\n"]
    out.extend(f"    0x{x:016x}ull,\n" for x in values)
    out.append("}};\n\n")
    out.append("""inline bool spec_name_hash_allowed_v12(std::uint64_t value) noexcept
{
    std::size_t lo=0,hi=k_spec_name_hashes_v12.size();
    while(lo<hi){
        const auto mid=lo+((hi-lo)>>1u);
        const auto at=k_spec_name_hashes_v12[mid];
        if(value<at)hi=mid;else if(value>at)lo=mid+1u;else return true;
    }
    return false;
}
inline constexpr std::size_t k_spec_name_count_v12=k_spec_name_hashes_v12.size();

} // namespace dsrrl::runtime::generated
""")
    return "".join(out)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True,type=Path)
    ap.add_argument("--output",required=True,type=Path)
    ns=ap.parse_args()
    ns.output.write_text(render(load(ns.input)),encoding="utf-8")
if __name__=="__main__":main()
