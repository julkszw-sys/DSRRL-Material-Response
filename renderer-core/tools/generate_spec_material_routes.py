#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv
from pathlib import Path

EXPECTED_COUNT=34

def load(path:Path):
    lines=[x for x in path.read_text(encoding="utf-8").splitlines() if x and not x.startswith("#")]
    rows=list(csv.DictReader(lines,delimiter="\t"))
    if len(rows)!=EXPECTED_COUNT: raise SystemExit("expected 34 distinct SpecRGB material routes")
    if len({r["sha256"] for r in rows})!=EXPECTED_COUNT: raise SystemExit("duplicate SpecRGB material route SHA")
    for r in rows:
        h=r["sha256"].lower()
        if len(h)!=64 or any(c not in "0123456789abcdef" for c in h): raise SystemExit("bad route SHA")
        rec=[int(r[f"receiver{i}"]) for i in range(3)]
        if any(x<24 or x>47 for x in rec): raise SystemExit("receiver outside stable 24..47")
    return rows

def render(rows):
    out=["#pragma once\n","#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <string_view>\n\n",
         "namespace dsrrl::runtime::generated {\n\n",
         "struct spec_material_route { std::string_view sha256; std::uint16_t route_index; std::array<std::uint32_t,3> receivers; };\n\n",
         f"inline constexpr std::array<spec_material_route, {len(rows)}> k_spec_material_routes = {{{{\n"]
    for r in rows:
        rec=",".join(r[f"receiver{i}"]+"u" for i in range(3))
        out.append(f'    {{"{r["sha256"].lower()}",{int(r["route_index"])}u,{{{{{rec}}}}}}},\n')
    out.append("}};\n\n")
    out.append("""inline bool spec_material_route_allowed(std::string_view sha256,std::uint32_t receiver) noexcept
{
    for(const auto &r:k_spec_material_routes){
        if(r.sha256!=sha256) continue;
        return r.receivers[0]==receiver || r.receivers[1]==receiver || r.receivers[2]==receiver;
    }
    return false;
}
inline constexpr std::size_t k_spec_material_route_count=k_spec_material_routes.size();

} // namespace dsrrl::runtime::generated
""")
    return "".join(out)

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--input",required=True,type=Path);ap.add_argument("--output",required=True,type=Path)
    ns=ap.parse_args();ns.output.write_text(render(load(ns.input)),encoding="utf-8")
if __name__=="__main__":main()
