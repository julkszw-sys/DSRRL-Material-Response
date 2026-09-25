#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

SHA_RE = re.compile(r"^[0-9a-f]{64}$")


def rows(path: str):
    with Path(path).open("r", encoding="utf-8", newline="") as fh:
        filtered=(line for line in fh if line.strip() and not line.startswith("#"))
        return list(csv.DictReader(filtered, delimiter="\t"))


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    a=ap.parse_args()

    src=rows(a.input)
    if len(src)!=24:
        raise SystemExit(f"expected 24 HemDir3 Spc rows, got {len(src)}")

    src.sort(key=lambda r:int(r["shader_index"]))
    seen_idx=set()
    seen_sha=set()
    for r in src:
        idx=int(r["shader_index"])
        sha=r["code_sha256"]
        if idx in seen_idx or sha in seen_sha:
            raise SystemExit("duplicate shader index/SHA")
        seen_idx.add(idx); seen_sha.add(sha)
        if not SHA_RE.fullmatch(sha) or not SHA_RE.fullmatch(r["replacement_sha256"]):
            raise SystemExit(f"bad SHA in row {idx}")
        if int(r["c101_slot_word"])<=0 or int(r["c102_slot_word"])<=0:
            raise SystemExit(f"bad operand site in row {idx}")
        if r["c101_operand_token"].lower()!="0x00208246":
            raise SystemExit(f"unexpected c101 operand token in row {idx}")
        if r["c102_operand_token"].lower()!="0x00208006":
            raise SystemExit(f"unexpected c102 operand token in row {idx}")
        if int(r["replacement_size"])<=0:
            raise SystemExit(f"bad replacement size in row {idx}")

    out=[
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstdint>\n",
        "#include <string_view>\n\n",
        "namespace dsrrl::operators::lightbank::generated {\n\n",
        "struct hemdir3_spc_b12_plan {\n",
        "    std::uint16_t shader_index;\n",
        "    std::string_view stock_sha256;\n",
        "    std::uint32_t c101_slot_word;\n",
        "    std::uint32_t c102_slot_word;\n",
        "    std::uint32_t replacement_size;\n",
        "    std::string_view replacement_sha256;\n",
        "};\n\n",
        "inline constexpr std::uint32_t k_hemdir3_spc_c101_operand_token = 0x00208246u;\n",
        "inline constexpr std::uint32_t k_hemdir3_spc_c102_operand_token = 0x00208006u;\n\n",
        "inline constexpr std::array<hemdir3_spc_b12_plan,24> k_hemdir3_spc_b12_plans = {{\n",
    ]
    for r in src:
        out.append(
            f'    {{{int(r["shader_index"])}u,"{r["code_sha256"]}",'
            f'{int(r["c101_slot_word"])}u,{int(r["c102_slot_word"])}u,'
            f'{int(r["replacement_size"])}u,"{r["replacement_sha256"]}"}},\n'
        )
    out += [
        "}};\n\n",
        "constexpr const hemdir3_spc_b12_plan *find_hemdir3_spc_b12_plan(\n",
        "    std::uint16_t shader_index) noexcept\n",
        "{\n",
        "    for (const auto &row : k_hemdir3_spc_b12_plans)\n",
        "        if (row.shader_index == shader_index) return &row;\n",
        "    return nullptr;\n",
        "}\n\n",
        "} // namespace dsrrl::operators::lightbank::generated\n",
    ]

    Path(a.output).write_text("".join(out),encoding="utf-8")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
