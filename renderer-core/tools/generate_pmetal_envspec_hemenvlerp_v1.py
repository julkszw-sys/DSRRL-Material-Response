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
expected_terminal_rgb_word={9:2904,10:2823,11:2476}
expected_ul_sites={
    9:(1831,1835,1845),
    10:(1740,1744,1754),
    11:(1393,1397,1407),
}
required_offsets={
    "mul_a_word":13,
    "t14_word":21,
    "mad_b_minus_a_word":34,
    "mad_lerp_word":45,
    "postblend_word":55,
    "t9_word":93,
    "t11_word":129,
    "t13_word":150,
    "envdiff_b_minus_a_word":163,
    "envdiff_lerp_word":174,
    "envdiff_gain_word":184,
    "merge_word":192,
}
for i,r in enumerate(rows):
    pair=int(r["pair_index"])
    receiver=int(r["semantic_receiver_id"])
    t12=int(r["t12_word"])
    if pair!=expected[i]:
        raise SystemExit("unexpected pair order")
    if receiver!=24+pair:
        raise SystemExit("semantic receiver mismatch")
    if len(r["v211_sha256"])!=64:
        raise SystemExit("bad v211 sha")
    if int(r["reflection_coord_register"]) not in (5,6,7):
        raise SystemExit("unexpected reflection coordinate register")
    for key,delta in required_offsets.items():
        if int(r[key]) != t12 + delta:
            raise SystemExit(f"{key} is not t12+{delta} for pair {pair}")
    if int(r["terminal_rgb_word"]) != expected_terminal_rgb_word[pair]:
        raise SystemExit(f"unexpected terminal RGB word for pair {pair}")
    ul_sites=(
        int(r["ul_u_slot_word"]),
        int(r["ul_d_slot_word_0"]),
        int(r["ul_d_slot_word_1"]),
    )
    if ul_sites != expected_ul_sites[pair]:
        raise SystemExit(f"unexpected UpperLower operand sites for pair {pair}")

fields=[
    "pair_index","semantic_receiver_id","label","v211_sha256",
    "reflection_coord_register","t12_word","mul_a_word","t14_word",
    "mad_b_minus_a_word","mad_lerp_word","postblend_word","t9_word",
    "t11_word","t13_word","envdiff_b_minus_a_word","envdiff_lerp_word",
    "envdiff_gain_word","merge_word","terminal_rgb_word",
    "ul_u_slot_word","ul_d_slot_word_0","ul_d_slot_word_1"
]

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
"    std::uint32_t reflection_coord_register;",
"    std::uint32_t t12_word;",
"    std::uint32_t mul_a_word;",
"    std::uint32_t t14_word;",
"    std::uint32_t mad_b_minus_a_word;",
"    std::uint32_t mad_lerp_word;",
"    std::uint32_t postblend_word;",
"    std::uint32_t t9_word;",
"    std::uint32_t t11_word;",
"    std::uint32_t t13_word;",
"    std::uint32_t envdiff_b_minus_a_word;",
"    std::uint32_t envdiff_lerp_word;",
"    std::uint32_t envdiff_gain_word;",
"    std::uint32_t merge_word;",
"    std::uint32_t terminal_rgb_word;",
"    std::uint32_t ul_u_slot_word;",
"    std::uint32_t ul_d_slot_word_0;",
"    std::uint32_t ul_d_slot_word_1;",
"};",
"inline constexpr std::array<pmetal_hemenvlerp_site,3> k_pmetal_hemenvlerp_sites = {{"
]
for r in rows:
    label=r["label"].replace("\\","\\\\").replace('"','\\"')
    vals=[
        f'{int(r["pair_index"])}u',
        f'{int(r["semantic_receiver_id"])}u',
        f'"{label}"',
        f'"{r["v211_sha256"]}"'
    ]
    vals += [f'{int(r[k])}u' for k in fields[4:]]
    lines.append("    {"+",".join(vals)+"},")
lines += [
"}};",
"} // namespace dsrrl::operators::env_spec::generated",
""
]
out=Path(a.output)
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text("\n".join(lines),encoding="utf-8")
