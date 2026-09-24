#!/usr/bin/env python3
"""Generate exact V12 Normal route tables from pinned recovered provenance.

The input TSV is an exact extraction of k_normal_triples from recovered
asset_integrated_v12.c. Comment lines carry provenance and are deliberately
excluded from the canonical data digest, so provenance comments can be
extended without changing route identity.
"""
from __future__ import annotations
import argparse, csv, hashlib
from pathlib import Path

EXPECTED_CANONICAL_DATA_SHA256 = "8f81a5da4d65db85a1a99846a5c918709933eafc0cb9c4e3cf5400bee7f91380"
EXPECTED_TUPLES = 557
EXPECTED_SAFE_ROWS = 3872
EXPECTED_UNIQUE_HASHES = 1664
EXPECTED_NORMAL_TARGETS = 555

def load(path: Path):
    raw = path.read_text(encoding="utf-8").splitlines()
    lines = [line for line in raw if line and not line.startswith("#")]
    canonical = ("\n".join(lines) + "\n").encode("utf-8")
    actual = hashlib.sha256(canonical).hexdigest()
    if actual != EXPECTED_CANONICAL_DATA_SHA256:
        raise SystemExit(f"canonical data SHA256 mismatch: {actual} != {EXPECTED_CANONICAL_DATA_SHA256}")
    rows = list(csv.DictReader(lines, delimiter="\t"))
    tuples, safe_rows = [], 0
    for row in rows:
        triple = tuple(int(row[k], 16) for k in ("diffuse_hash","spec_hash","normal_hash"))
        tuples.append(triple)
        safe_rows += int(row["row_uses"])
    if tuples != sorted(tuples):
        raise SystemExit("normal tuple corpus is not lexicographically sorted")
    if len(tuples) != EXPECTED_TUPLES or len(set(tuples)) != EXPECTED_TUPLES:
        raise SystemExit("normal tuple count/uniqueness mismatch")
    if safe_rows != EXPECTED_SAFE_ROWS:
        raise SystemExit("safe row count mismatch")
    members = sorted({v for triple in tuples for v in triple})
    targets = sorted({triple[2] for triple in tuples})
    if len(members) != EXPECTED_UNIQUE_HASHES:
        raise SystemExit("normal logical-name hash count mismatch")
    if len(targets) != EXPECTED_NORMAL_TARGETS:
        raise SystemExit("normal target hash count mismatch")
    return tuples, members, targets

def emit_array(out, typename, name, values, formatter):
    out.append(f"inline constexpr std::array<{typename}, {len(values)}> {name} = {{{{\n")
    for value in values:
        out.append("    " + formatter(value) + ",\n")
    out.append("}};\n\n")

def render(tuples, members, targets):
    out=["#pragma once\n","#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
         "namespace dsrrl::runtime::generated {\n\n",
         "struct normal_tuple_v12 { std::uint64_t diffuse_hash; std::uint64_t spec_hash; std::uint64_t normal_hash; };\n\n"]
    emit_array(out,"normal_tuple_v12","k_normal_tuples_v12",tuples,
               lambda x:f"{{0x{x[0]:016x}ull, 0x{x[1]:016x}ull, 0x{x[2]:016x}ull}}")
    emit_array(out,"std::uint64_t","k_normal_tuple_member_hashes_v12",members,
               lambda x:f"0x{x:016x}ull")
    emit_array(out,"std::uint64_t","k_normal_target_hashes_v12",targets,
               lambda x:f"0x{x:016x}ull")
    out.append("""template <std::size_t N>
inline bool sorted_hash_contains(const std::array<std::uint64_t,N> &values, std::uint64_t value) noexcept
{
    std::size_t lo=0, hi=N;
    while(lo<hi){const auto mid=lo+((hi-lo)>>1u);const auto at=values[mid];
        if(value<at)hi=mid;else if(value>at)lo=mid+1u;else return true;}
    return false;
}
inline bool normal_name_hash_allowed_v12(std::uint64_t value) noexcept
{ return sorted_hash_contains(k_normal_tuple_member_hashes_v12,value); }
inline bool normal_target_hash_allowed_v12(std::uint64_t value) noexcept
{ return sorted_hash_contains(k_normal_target_hashes_v12,value); }
inline bool normal_tuple_allowed_v12(std::uint64_t diffuse,std::uint64_t spec,std::uint64_t normal) noexcept
{
    std::size_t lo=0,hi=k_normal_tuples_v12.size();
    while(lo<hi){const auto mid=lo+((hi-lo)>>1u);const auto &at=k_normal_tuples_v12[mid];
        if(diffuse!=at.diffuse_hash){if(diffuse<at.diffuse_hash)hi=mid;else lo=mid+1u;continue;}
        if(spec!=at.spec_hash){if(spec<at.spec_hash)hi=mid;else lo=mid+1u;continue;}
        if(normal!=at.normal_hash){if(normal<at.normal_hash)hi=mid;else lo=mid+1u;continue;}
        return true;}
    return false;
}
inline constexpr std::size_t k_normal_tuple_count_v12=k_normal_tuples_v12.size();
inline constexpr std::size_t k_normal_tuple_member_count_v12=k_normal_tuple_member_hashes_v12.size();
inline constexpr std::size_t k_normal_target_count_v12=k_normal_target_hashes_v12.size();
inline constexpr std::size_t k_normal_safe_row_count_v12=3872u;

} // namespace dsrrl::runtime::generated
""")
    return "".join(out)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True,type=Path)
    ap.add_argument("--output",required=True,type=Path)
    ns=ap.parse_args()
    tuples,members,targets=load(ns.input)
    ns.output.write_text(render(tuples,members,targets),encoding="utf-8")

if __name__=="__main__": main()
