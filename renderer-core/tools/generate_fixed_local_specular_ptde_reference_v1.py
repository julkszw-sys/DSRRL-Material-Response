#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,re
from pathlib import Path

SHA=re.compile(r"^[0-9a-f]{64}$")
CLASSES={"fixed_spc_pntss":2,"fixed_spc_pntssss":4}

def digest(s:str)->str:
    return ",".join(f"0x{s[i:i+2]}u" for i in range(0,64,2))

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True)
    ap.add_argument("--output",required=True)
    a=ap.parse_args()
    with Path(a.input).open("r",encoding="utf-8",newline="") as f:
        rows=list(csv.DictReader(f,delimiter="\t"))
    if len(rows)!=48:
        raise SystemExit(f"expected 48 rows, got {len(rows)}")
    seen=set(); counts={k:0 for k in CLASSES}; entries=[]
    for r in rows:
        cls=r["receiver_class"]
        if cls not in CLASSES: raise SystemExit(f"bad class {cls}")
        for k in ("dsr_sha256","ptde_hemenv_sha256","ptde_hemenvlerp_sha256","source_zip_sha256"):
            if not SHA.fullmatch(r[k]): raise SystemExit(f"bad sha {k}")
        if r["dsr_sha256"] in seen: raise SystemExit("duplicate DSR SHA")
        seen.add(r["dsr_sha256"]); counts[cls]+=1
        entries.append(r)
    if counts!={"fixed_spc_pntss":24,"fixed_spc_pntssss":24}:
        raise SystemExit(f"bad counts {counts}")

    o=["#pragma once\n","#include <array>\n","#include <cstddef>\n","#include <cstdint>\n\n",
       "namespace dsrrl::operators::point_light::generated {\n\n",
       "struct fixed_ptde_reference_record {\n",
       "    std::array<std::uint8_t,32> dsr_sha256;\n",
       "    std::uint32_t dsr_size;\n",
       "    std::uint8_t light_count;\n",
       "    std::array<std::uint8_t,32> ptde_hemenv_sha256;\n",
       "    std::uint32_t ptde_hemenv_size;\n",
       "    std::array<std::uint8_t,32> ptde_hemenvlerp_sha256;\n",
       "    std::uint32_t ptde_hemenvlerp_size;\n",
       "};\n\n",
       "inline constexpr std::array<fixed_ptde_reference_record,48> k_fixed_ptde_references = {{\n"]
    for r in entries:
        o.append("    {{{{{}}},{},{}u,{{{}}},{}u,{{{}}},{}u}},\n".format(
            digest(r["dsr_sha256"]),r["dsr_size"],CLASSES[r["receiver_class"]],
            digest(r["ptde_hemenv_sha256"]),r["ptde_hemenv_size"],
            digest(r["ptde_hemenvlerp_sha256"]),r["ptde_hemenvlerp_size"]))
    o+=["}};\n\n","} // namespace dsrrl::operators::point_light::generated\n"]
    out=Path(a.output); out.parent.mkdir(parents=True,exist_ok=True); out.write_text("".join(o),encoding="utf-8")
    return 0
if __name__=="__main__": raise SystemExit(main())
