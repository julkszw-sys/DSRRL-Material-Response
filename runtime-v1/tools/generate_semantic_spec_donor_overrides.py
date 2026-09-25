#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,math
from pathlib import Path

EXPECTED=8
EXPECTED_GROUPS=4

def f(v:str)->str:
    x=float(v)
    if x.is_integer(): return f"{int(x)}.0f"
    return f"{v}f"

def load(path:Path):
    lines=[x for x in path.read_text(encoding="utf-8").splitlines() if x and not x.startswith("#")]
    rows=list(csv.DictReader(lines,delimiter="\t"))
    if len(rows)!=EXPECTED: raise SystemExit("expected 8 semantic spec donor rows")
    if len({r["name"] for r in rows})!=EXPECTED: raise SystemExit("duplicate semantic MTD name")
    groups={}
    for r in rows:
        h=r["raw_sha256"].lower()
        if len(h)!=64 or any(c not in "0123456789abcdef" for c in h):
            raise SystemExit("bad raw SHA")
        groups.setdefault(h,[]).append(r)
        q=float(r["c101_f0q"]); expected=max(float(r["c101"]),0.0)**(1.0/2.2)
        if abs(q-expected)>2e-7:
            raise SystemExit(f"bad c101_f0q for {r['name']}")
        slot=int(r["slot"])
        if slot<0 or slot>3: raise SystemExit("EnvSpc slot outside 0..3")
    if len(groups)!=EXPECTED_GROUPS or any(len(v)!=2 for v in groups.values()):
        raise SystemExit("expected four 2-name ambiguity groups")
    return rows

def render(rows):
    out=[
      "#pragma once\n\n",
      "#include <array>\n#include <cstddef>\n#include <string_view>\n\n",
      "namespace dsrrl::runtime {\n\n",
      "struct semantic_spec_donor_override {\n",
      "    std::wstring_view mtd_basename;\n",
      "    std::string_view raw_sha256;\n",
      "    std::array<float,3> c101{};\n",
      "    std::array<float,3> c101_f0q{};\n",
      "    float c102=0.0f;\n",
      "    int slot=0;\n",
      "};\n\n",
      f"inline constexpr std::array<semantic_spec_donor_override,{len(rows)}> k_semantic_spec_donor_overrides = {{{{\n"
    ]
    for r in rows:
        c=f(r["c101"]); q=f(r["c101_f0q"]); c102=f(r["c102"])
        out.append(
          f'    {{L"{r["name"]}","{r["raw_sha256"].lower()}",'
          f'{{{c},{c},{c}}},{{{q},{q},{q}}},{c102},{int(r["slot"])} }},\n')
    out += [
      "}};\n\n",
      "inline bool semantic_spec_raw_hash_is_ambiguous(std::string_view raw_sha256) noexcept\n",
      "{\n    for(const auto &entry:k_semantic_spec_donor_overrides)\n",
      "        if(entry.raw_sha256==raw_sha256) return true;\n",
      "    return false;\n}\n\n",
      "inline int find_semantic_spec_donor_override(std::wstring_view semantic_name,std::string_view raw_sha256) noexcept\n",
      "{\n    const auto slash=semantic_name.find_last_of(L\"\\\\/\");\n",
      "    const auto basename=slash==std::wstring_view::npos ? semantic_name : semantic_name.substr(slash+1u);\n",
      "    for(std::size_t i=0;i<k_semantic_spec_donor_overrides.size();++i){\n",
      "        const auto &entry=k_semantic_spec_donor_overrides[i];\n",
      "        if(entry.raw_sha256==raw_sha256 && entry.mtd_basename==basename) return static_cast<int>(i);\n",
      "    }\n    return -1;\n}\n\n",
      "} // namespace dsrrl::runtime\n"
    ]
    return "".join(out)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True,type=Path)
    ap.add_argument("--output",required=True,type=Path)
    ns=ap.parse_args()
    rows=load(ns.input)
    ns.output.parent.mkdir(parents=True,exist_ok=True)
    ns.output.write_text(render(rows),encoding="utf-8",newline="\n")
    print(f"semantic_spec_donor_overrides: PASS rows={len(rows)} groups=4")

if __name__=="__main__": main()
