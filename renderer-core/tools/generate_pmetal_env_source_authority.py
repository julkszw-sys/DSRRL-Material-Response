#!/usr/bin/env python3
from pathlib import Path
import argparse
import csv
import re

ap = argparse.ArgumentParser()
ap.add_argument("--input", required=True)
ap.add_argument("--layout-input", required=True)
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
    (match.group(1).lower(), int(match.group(2)), int(match.group(3)))
    for match in re.finditer(
        r"\{(0x[0-9a-fA-F]+)ULL,(\d+)u,(\d+)u\}",
        source,
    )
]

if len(rows) != 1246 or len(banks) != 20:
    raise SystemExit(
        f"authority count mismatch: rows={len(rows)} banks={len(banks)}"
    )

# The recovered donor corpus is stronger than a generic sparse row table:
# every donor bank occupies one contiguous slice and row IDs are exactly
# 0..count-1. Validate that source fact before emitting an O(1) resolver.
for signature, first, count in banks:
    if first > len(rows) or count > len(rows) - first:
        raise SystemExit(
            f"bank slice out of range: signature={signature} first={first} count={count}"
        )
    ids = [row[0] for row in rows[first:first + count]]
    expected = list(range(count))
    if ids != expected:
        raise SystemExit(
            f"bank row IDs are not dense 0..count-1: signature={signature}"
        )

layout_lines = [
    line
    for line in Path(args.layout_input).read_text(encoding="utf-8").splitlines()
    if line.strip() and not line.lstrip().startswith("#")
]
layout_rows = list(csv.DictReader(layout_lines, delimiter="\t"))

required = {
    "bank_index",
    "area",
    "bank_name",
    "v13_signature",
    "layout_signature",
    "live_count",
    "donor_count",
    "param_sha256",
    "dcx_sha256",
}
if len(layout_rows) != 20:
    raise SystemExit(f"layout authority count mismatch: rows={len(layout_rows)}")
if not layout_rows or not required.issubset(layout_rows[0].keys()):
    raise SystemExit("layout authority schema mismatch")

layout_by_v13 = {}
seen_layouts = set()
seen_indices = set()
for row in layout_rows:
    index = int(row["bank_index"])
    v13 = row["v13_signature"].lower()
    layout = row["layout_signature"].lower()
    live_count = int(row["live_count"])
    donor_count = int(row["donor_count"])

    if index < 0 or index >= 20 or index in seen_indices:
        raise SystemExit(f"invalid/duplicate layout bank_index: {index}")
    if v13 in layout_by_v13:
        raise SystemExit(f"duplicate V13 signature in layout authority: {v13}")
    if layout in seen_layouts:
        raise SystemExit(f"duplicate layout signature: {layout}")
    if not re.fullmatch(r"[0-9a-f]{16}", v13):
        raise SystemExit(f"invalid V13 signature: {v13}")
    if not re.fullmatch(r"[0-9a-f]{16}", layout):
        raise SystemExit(f"invalid layout signature: {layout}")
    if live_count <= 0 or live_count > 256:
        raise SystemExit(f"invalid live_count for {row['bank_name']}: {live_count}")
    if donor_count <= 0 or donor_count > live_count:
        raise SystemExit(f"invalid donor_count for {row['bank_name']}: {donor_count}")
    if not re.fullmatch(r"[0-9a-f]{64}", row["param_sha256"].lower()):
        raise SystemExit(f"invalid PARAM SHA256 for {row['bank_name']}")
    if not re.fullmatch(r"[0-9a-f]{64}", row["dcx_sha256"].lower()):
        raise SystemExit(f"invalid DCX SHA256 for {row['bank_name']}")

    seen_indices.add(index)
    seen_layouts.add(layout)
    layout_by_v13[v13] = {
        "index": index,
        "layout": layout,
        "live_count": live_count,
        "donor_count": donor_count,
        "bank_name": row["bank_name"],
    }

if seen_indices != set(range(20)):
    raise SystemExit("layout bank_index set must be exactly 0..19")

resolved_banks = []
for ordinal, (signature, first, count) in enumerate(banks):
    key = signature[2:] if signature.startswith("0x") else signature
    info = layout_by_v13.get(key)
    if info is None:
        raise SystemExit(f"missing layout authority for V13 signature {signature}")
    if info["index"] != ordinal:
        raise SystemExit(
            f"layout bank_index mismatch for {signature}: "
            f"{info['index']} != {ordinal}"
        )
    if info["donor_count"] != count:
        raise SystemExit(
            f"donor_count mismatch for {signature}: "
            f"{info['donor_count']} != {count}"
        )
    resolved_banks.append(
        (
            signature,
            "0x" + info["layout"],
            info["live_count"],
            first,
            count,
            info["bank_name"],
        )
    )

lines = [
    "#pragma once",
    "#include \"dsrrl/runtime/dsr_only_lightbank_rows_v1.hpp\"",
    "#include <array>",
    "#include <cstddef>",
    "#include <cstdint>",
    "",
    "namespace dsrrl::runtime::pmetal_env_source_authority {",
    "struct donor_row { std::uint32_t id; std::uint16_t r,g,b,m; };",
    "struct bank_donor { std::uint64_t signature; std::uint64_t layout_signature; std::uint32_t live_count; std::uint32_t first,count; };",
    f"inline constexpr std::array<donor_row,{len(rows)}> k_rows = {{{{",
]

lines += [
    f"donor_row{{{row[0]}u,{row[1]}u,{row[2]}u,{row[3]}u,{row[4]}u}},"
    for row in rows
]

lines += [
    "}};",
    f"inline constexpr std::array<bank_donor,{len(resolved_banks)}> k_banks = {{{{",
]

lines += [
    (
        f"bank_donor{{{sig}ULL,{layout}ULL,{live_count}u,"
        f"{first}u,{count}u}}, // {name}"
    )
    for sig, layout, live_count, first, count, name in resolved_banks
]

lines += [
    "}};",
    "inline constexpr std::array<bank_donor,3> k_dsr_common_banks = {{",
    "bank_donor{0x96ece3bed03eed01ULL,0x96ece3bed03eed01ULL,64u,0u,0u}, // default_LightBank DSR_ONLY",
    "bank_donor{0x34bdd6493ca1a91aULL,0x34bdd6493ca1a91aULL,64u,0u,0u}, // m99_LightBank DSR_ONLY",
    "bank_donor{0x91aa11098cee0fe2ULL,0x91aa11098cee0fe2ULL,64u,0u,0u}, // s99_LightBank DSR_ONLY",
    "}};",
    "constexpr const bank_donor *find_bank(std::uint64_t s) noexcept { for (const auto &b : k_banks) if (b.signature == s) return &b; for(const auto &b:k_dsr_common_banks)if(b.signature==s)return &b; return nullptr; }",
    "constexpr const bank_donor *find_bank_by_layout(std::uint64_t s, std::uint32_t live_count) noexcept { for (const auto &b : k_banks) if (b.layout_signature == s && b.live_count == live_count) return &b; for(const auto &b:k_dsr_common_banks)if(b.layout_signature==s && b.live_count==live_count)return &b; return nullptr; }",
    "inline constexpr std::array<donor_row,3> k_dsr_extra = {{",
    "donor_row{dsr_only_lightbank_rows_v1::rows[0].id,dsr_only_lightbank_rows_v1::rows[0].r,dsr_only_lightbank_rows_v1::rows[0].g,dsr_only_lightbank_rows_v1::rows[0].b,dsr_only_lightbank_rows_v1::rows[0].m},",
    "donor_row{dsr_only_lightbank_rows_v1::rows[1].id,dsr_only_lightbank_rows_v1::rows[1].r,dsr_only_lightbank_rows_v1::rows[1].g,dsr_only_lightbank_rows_v1::rows[1].b,dsr_only_lightbank_rows_v1::rows[1].m},",
    "donor_row{dsr_only_lightbank_rows_v1::rows[2].id,dsr_only_lightbank_rows_v1::rows[2].r,dsr_only_lightbank_rows_v1::rows[2].g,dsr_only_lightbank_rows_v1::rows[2].b,dsr_only_lightbank_rows_v1::rows[2].m},",
    "}};",
    "constexpr const donor_row *find_row(const bank_donor &b, std::uint32_t id) noexcept { if (b.first > k_rows.size() || b.count > k_rows.size() - b.first || id >= b.live_count) return nullptr; if (id >= b.count) { for (std::size_t i=0;i<k_dsr_extra.size();++i) if (dsr_only_lightbank_rows_v1::rows[i].signature==b.signature && k_dsr_extra[i].id==id) return &k_dsr_extra[i]; return nullptr; } const auto &row = k_rows[b.first + id]; return row.id == id ? &row : nullptr; }",
    "} // namespace dsrrl::runtime::pmetal_env_source_authority",
]

out = Path(args.output)
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(
    f"generated rows={len(rows)} banks={len(resolved_banks)} "
    f"layout_unique={len(seen_layouts)}"
)
