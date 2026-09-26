#!/usr/bin/env python3
from pathlib import Path
import argparse, re

ap=argparse.ArgumentParser()
ap.add_argument("--input",required=True)
ap.add_argument("--output",required=True)
a=ap.parse_args()
s=Path(a.input).read_text(encoding="utf-8")
rows=[tuple(map(int,m.groups())) for m in re.finditer(r"\\{(\\d+)u,(\\d+)u,(\\d+)u,(\\d+)u,(\\d+)u\\}",s)]
banks=[(m.group(1),int(m.group(2)),int(m.group(3))) for m in re.finditer(r"\\{(0x[0-9a-fA-F]+)ULL,(\\d+)u,(\\d+)u\\}",s)]
if len(rows)!=1246 or len(banks)!=20: raise SystemExit("authority count mismatch")
l=["#pragma once","#include <array>","#include <cstddef>","#include <cstdint>","","namespace dsrrl::runtime::pmetal_env_source_authority {","struct donor_row { std::uint32_t id; std::uint16_t r,g,b,m; };","struct bank_donor { std::uint64_t signature; std::uint32_t first,count; };",f"inline constexpr std::array<donor_row,{len(rows)}> k_rows = {{{{"]
l += [f"donor_row{{{x[0]}u,{x[1]}u,{x[2]}u,{x[3]}u,{x[4]}u}}," for x in rows]
l += ["}};",f"inline constexpr std::array<bank_donor,{len(banks)}> k_banks = {{{{"]
l += [f"bank_donor{{{x[0]}ULL,{x[1]}u,{x[2]}u}}," for x in banks]
l += ["}};","constexpr const bank_donor *find_bank(std::uint64_t s) noexcept { for(const auto &b:k_banks) if(b.signature==s) return &b; return nullptr; }","constexpr const donor_row *find_row(const bank_donor &b,std::uint32_t id) noexcept { if(b.first>k_rows.size()||b.count>k_rows.size()-b.first) return nullptr; for(std::uint32_t i=0;i<b.count;++i) if(k_rows[b.first+i].id==id) return &k_rows[b.first+i]; return nullptr; }","}"]
o=Path(a.output); o.parent.mkdir(parents=True,exist_ok=True); o.write_text("\\n".join(l)+"\\n",encoding="utf-8")
print(f"generated rows={len(rows)} banks={len(banks)}")
