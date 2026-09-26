#!/usr/bin/env python3
from pathlib import Path
import argparse
import re

ap = argparse.ArgumentParser()
ap.add_argument("--input", required=True)
ap.add_argument("--output", required=True)
args = ap.parse_args()

source = Path(args.input).read_text(encoding="utf-8")

rows = [
    tuple(map(int, match.groups()))
    for match in re.finditer(
        r"\{(\d+)u,(\d+)u,(\d+)u,(\d+)u,(\d+)u\}",
        source,
    )
]

banks = [
    (match.group(1), int(match.group(2)), int(match.group(3)))
    for match in re.finditer(
        r"\{(0x[0-9a-fA-F]+)ULL,(\d+)u,(\d+)u\}",
        source,
    )
]

if len(rows) != 1246 or len(banks) != 20:
    raise SystemExit(
        f"authority count mismatch: rows={len(rows)} banks={len(banks)}"
    )

lines = [
    "#pragma once",
    "#include <array>",
    "#include <cstddef>",
    "#include <cstdint>",
    "",
    "namespace dsrrl::runtime::pmetal_env_source_authority {",
    "struct donor_row { std::uint32_t id; std::uint16_t r,g,b,m; };",
    "struct bank_donor { std::uint64_t signature; std::uint32_t first,count; };",
    f"inline constexpr std::array<donor_row,{len(rows)}> k_rows = {{{{",
]

lines += [
    f"donor_row{{{row[0]}u,{row[1]}u,{row[2]}u,{row[3]}u,{row[4]}u}},"
    for row in rows
]

lines += [
    "}};",
    f"inline constexpr std::array<bank_donor,{len(banks)}> k_banks = {{{{",
]

lines += [
    f"bank_donor{{{sig}ULL,{first}u,{count}u}},"
    for sig, first, count in banks
]

lines += [
    "}};",
    "constexpr const bank_donor *find_bank(std::uint64_t s) noexcept { for (const auto &b : k_banks) if (b.signature == s) return &b; return nullptr; }",
    "constexpr const donor_row *find_row(const bank_donor &b, std::uint32_t id) noexcept { if (b.first > k_rows.size() || b.count > k_rows.size() - b.first) return nullptr; for (std::uint32_t i = 0; i < b.count; ++i) if (k_rows[b.first + i].id == id) return &k_rows[b.first + i]; return nullptr; }",
    "} // namespace dsrrl::runtime::pmetal_env_source_authority",
]

out = Path(args.output)
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"generated rows={len(rows)} banks={len(banks)}")
