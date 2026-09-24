#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,hashlib
from pathlib import Path
EXPECTED_CANONICAL_DATA_SHA256="977a38a3a28228a81a836c238282b34fd718428a98cb5e5008f56892e537bbd9"
EXPECTED_PAIRS=528
EXPECTED_SAFE_ROWS=3446
EXPECTED_TARGETS=524

def load(path:Path):
    lines=[x for x in path.read_text(encoding="utf-8").splitlines() if x and not x.startswith("#")]
    canonical=("\n".join(lines)+"\n").encode("utf-8")
    actual=hashlib.sha256(canonical).hexdigest()
    if actual!=EXPECTED_CANONICAL_DATA_SHA256:
        raise SystemExit(f"canonical data SHA256 mismatch: {actual} != {EXPECTED_CANONICAL_DATA_SHA256}")
    rows=list(csv.DictReader(lines,delimiter="\t"))
    pairs=[]; safe=0
    for r in rows:
        pairs.append((int(r["spec_hash"],16),int(r["diffuse_hash"],16)));safe+=int(r["row_uses"])
    if pairs!=sorted(pairs) or len(pairs)!=EXPECTED_PAIRS or len(set(pairs))!=EXPECTED_PAIRS:
        raise SystemExit("diffuse pair corpus order/count mismatch")
    targets=sorted({d for _,d in pairs})
    members=sorted({x for p in pairs for x in p})
    if safe!=EXPECTED_SAFE_ROWS or len(targets)!=EXPECTED_TARGETS:
        raise SystemExit("diffuse safe-row/target count mismatch")
    return pairs,members,targets

def emit_array(out,typename,name,values,fmt):
    out.append(f"inline constexpr std::array<{typename}, {len(values)}> {name} = {{{{\n")
    for v in values: out.append("    "+fmt(v)+",\n")
    out.append("}};\n\n")

def render(pairs,members,targets):
    out=["#pragma once\n","#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
         "namespace dsrrl::runtime::generated {\n\n",
         "struct diffuse_pair_v12 { std::uint64_t spec_hash; std::uint64_t diffuse_hash; };\n\n"]
    emit_array(out,"diffuse_pair_v12","k_diffuse_pairs_v12",pairs,
               lambda x:f"{{0x{x[0]:016x}ull, 0x{x[1]:016x}ull}}")
    emit_array(out,"std::uint64_t","k_diffuse_pair_member_hashes_v12",members,lambda x:f"0x{x:016x}ull")
    emit_array(out,"std::uint64_t","k_diffuse_target_hashes_v12",targets,lambda x:f"0x{x:016x}ull")
    out.append("""template <std::size_t N>
inline bool diffuse_sorted_hash_contains(const std::array<std::uint64_t,N>&v,std::uint64_t x) noexcept
{std::size_t lo=0,hi=N;while(lo<hi){const auto m=lo+((hi-lo)>>1u),a=v[m];if(x<a)hi=m;else if(x>a)lo=m+1u;else return true;}return false;}
inline bool diffuse_name_hash_allowed_v12(std::uint64_t v) noexcept
{return diffuse_sorted_hash_contains(k_diffuse_pair_member_hashes_v12,v);}
inline bool diffuse_target_hash_allowed_v12(std::uint64_t v) noexcept
{return diffuse_sorted_hash_contains(k_diffuse_target_hashes_v12,v);}
inline bool diffuse_pair_allowed_v12(std::uint64_t spec,std::uint64_t diffuse) noexcept
{
 std::size_t lo=0,hi=k_diffuse_pairs_v12.size();
 while(lo<hi){const auto m=lo+((hi-lo)>>1u);const auto &a=k_diffuse_pairs_v12[m];
  if(spec!=a.spec_hash){if(spec<a.spec_hash)hi=m;else lo=m+1u;continue;}
  if(diffuse!=a.diffuse_hash){if(diffuse<a.diffuse_hash)hi=m;else lo=m+1u;continue;}
  return true;}return false;
}
inline constexpr std::size_t k_diffuse_pair_count_v12=k_diffuse_pairs_v12.size();
inline constexpr std::size_t k_diffuse_target_count_v12=k_diffuse_target_hashes_v12.size();
inline constexpr std::size_t k_diffuse_safe_row_count_v12=3446u;
} // namespace dsrrl::runtime::generated
""")
    return "".join(out)

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--input",required=True,type=Path);ap.add_argument("--output",required=True,type=Path)
    ns=ap.parse_args();p,m,t=load(ns.input);ns.output.write_text(render(p,m,t),encoding="utf-8")
if __name__=="__main__":main()
