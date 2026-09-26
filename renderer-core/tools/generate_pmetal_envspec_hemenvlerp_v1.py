#!/usr/bin/env python3
from pathlib import Path
import argparse,csv

ap=argparse.ArgumentParser()
ap.add_argument("--input",required=True)
ap.add_argument("--output",required=True)
a=ap.parse_args()

with Path(a.input).open("r",encoding="utf-8",newline="") as f:
    rows=list(csv.DictReader(f,delimiter="\t"))
if len(rows)!=3:
    raise SystemExit(f"expected 3 P_Metal HemEnvLerp rows, got {len(rows)}")
expected=[9,10,11]
for i,r in enumerate(rows):
    if int(r["pair_index"])!=expected[i]:
        raise SystemExit("unexpected pair order")
    if int(r["semantic_receiver_id"])!=24+expected[i]:
        raise SystemExit("semantic receiver mismatch")
    if len(r["v211_sha256"])!=64:
        raise SystemExit("bad v211 sha")

lines=[
"#pragma once",
"#include <array>",
"#include <cstdint>",
"#include <string_view>",
"",
"namespace dsrrl::operators::env_spec::generated {",
"struct pmetal_hemenvlerp_site {",
"    std::uint8_t pair_index;",
"    std::uint32_t semantic_receiver_id;",
"    std::string_view label;",
"    std::string_view v211_sha256;",
"    std::uint32_t t12_word;",
"    std::uint32_t mul_a_word;",
"    std::uint32_t t14_word;",
"    std::uint32_t mad_b_minus_a_word;",
"    std::uint32_t mad_lerp_word;",
"    std::uint32_t t9_word;",
"};",
"inline constexpr std::array<pmetal_hemenvlerp_site,3> k_pmetal_hemenvlerp_sites = {{"
]
for r in rows:
    label=r["label"].replace("\\","\\\\").replace('"','\\"')
    lines.append(
        "    {"+
        f'{int(r["pair_index"])}u,{int(r["semantic_receiver_id"])}u,"{label}",'+
        f'"{r["v211_sha256"]}",{int(r["t12_word"])}u,{int(r["mul_a_word"])}u,'+
        f'{int(r["t14_word"])}u,{int(r["mad_b_minus_a_word"])}u,'+
        f'{int(r["mad_lerp_word"])}u,{int(r["t9_word"])}u'+
        "},"
    )
lines += [
"}};",
"} // namespace dsrrl::operators::env_spec::generated",
""
]
out=Path(a.output); out.parent.mkdir(parents=True,exist_ok=True)
out.write_text("\n".join(lines),encoding="utf-8")
