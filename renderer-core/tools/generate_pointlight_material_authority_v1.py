#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path


def parse_u64(value: str) -> int:
    return int(str(value), 16)


def parse_u32(value: str) -> int:
    return int(str(value), 16)


def digest_bytes(value: str) -> list[int]:
    raw = bytes.fromhex(value)
    if len(raw) != 32:
        raise ValueError("SHA-256 must be 32 bytes")
    return list(raw)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ns = ap.parse_args()

    payload = json.loads(ns.input.read_text(encoding="utf-8"))
    if payload.get("schema") != "DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_V1":
        raise SystemExit("unexpected PointLight material authority schema")

    rows = list(payload.get("active_records", []))
    if len(rows) != 205:
        raise SystemExit(f"expected 205 active PointLight material rows, got {len(rows)}")

    cooked = []
    seen = set()
    for row in rows:
        semantic = parse_u64(row["semantic_name_hash_fnv1a_utf8"])
        if semantic in seen:
            raise SystemExit(f"duplicate active semantic hash 0x{semantic:016x}")
        seen.add(semantic)

        mode = row.get("material_mode")
        if mode not in {"SPC", "NOSPC"}:
            raise SystemExit(f"unexpected material mode {mode!r}")

        cooked.append({
            "semantic": semantic,
            "raw": digest_bytes(row["dsr_mtd_sha256"]),
            "c100": [parse_u32(x) for x in row["c100_f32_bits"]],
            "c101": [parse_u32(x) for x in row["c101_f32_bits"]],
            "c102": parse_u32(row["c102_f32_bits"]),
            "spc": mode == "SPC",
            "c101_scalar": bool(row.get("c101_scalar", False)),
            "router_index": int(row["router_index"]),
        })

    cooked.sort(key=lambda x: x["semantic"])

    out = [
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstddef>\n",
        "#include <cstdint>\n\n",
        "namespace dsrrl::operators::material_response::generated {\n\n",
        "struct pointlight_material_authority_record_v1 {\n",
        "    std::uint64_t semantic_name_hash;\n",
        "    std::array<std::uint8_t,32> raw_mtd_sha256;\n",
        "    std::array<std::uint32_t,3> c100_bits;\n",
        "    std::array<std::uint32_t,3> c101_bits;\n",
        "    std::uint32_t c102_bits;\n",
        "    std::uint32_t router_index;\n",
        "    bool spc;\n",
        "    bool c101_scalar;\n",
        "};\n\n",
        f"inline constexpr std::array<pointlight_material_authority_record_v1,{len(cooked)}u> "
        "k_pointlight_material_authority_v1 = {{\n",
    ]

    for row in cooked:
        raw = ",".join(f"0x{x:02x}u" for x in row["raw"])
        c100 = ",".join(f"0x{x:08x}u" for x in row["c100"])
        c101 = ",".join(f"0x{x:08x}u" for x in row["c101"])
        out.append(
            "    {"
            f"0x{row['semantic']:016x}ull,"
            "{{" + raw + "}},"
            "{{" + c100 + "}},"
            "{{" + c101 + "}},"
            f"0x{row['c102']:08x}u,"
            f"{row['router_index']}u,"
            + ("true" if row["spc"] else "false") + ","
            + ("true" if row["c101_scalar"] else "false")
            + "},\n"
        )

    out += [
        "}};\n\n",
        "constexpr const pointlight_material_authority_record_v1 *\n",
        "find_pointlight_material_authority_v1(std::uint64_t semantic_name_hash) noexcept\n",
        "{\n",
        "    std::size_t lo = 0u;\n",
        "    std::size_t hi = k_pointlight_material_authority_v1.size();\n",
        "    while (lo < hi) {\n",
        "        const auto mid = lo + (hi - lo) / 2u;\n",
        "        if (k_pointlight_material_authority_v1[mid].semantic_name_hash < semantic_name_hash)\n",
        "            lo = mid + 1u;\n",
        "        else\n",
        "            hi = mid;\n",
        "    }\n",
        "    if (lo >= k_pointlight_material_authority_v1.size() ||\n",
        "        k_pointlight_material_authority_v1[lo].semantic_name_hash != semantic_name_hash)\n",
        "        return nullptr;\n",
        "    return &k_pointlight_material_authority_v1[lo];\n",
        "}\n\n",
        "constexpr bool pointlight_material_behavior_equal_v1(\n",
        "    const pointlight_material_authority_record_v1 &a,\n",
        "    const pointlight_material_authority_record_v1 &b) noexcept\n",
        "{\n",
        "    return a.c100_bits == b.c100_bits &&\n",
        "           a.c101_bits == b.c101_bits &&\n",
        "           a.c102_bits == b.c102_bits &&\n",
        "           a.spc == b.spc &&\n",
        "           a.c101_scalar == b.c101_scalar;\n",
        "}\n\n",
        "constexpr const pointlight_material_authority_record_v1 *\n",
        "find_pointlight_material_authority_by_raw_mtd_v1(\n",
        "    const std::array<std::uint8_t,32> &raw_mtd_sha256) noexcept\n",
        "{\n",
        "    const pointlight_material_authority_record_v1 *match = nullptr;\n",
        "    for (const auto &record : k_pointlight_material_authority_v1) {\n",
        "        if (record.raw_mtd_sha256 != raw_mtd_sha256)\n",
        "            continue;\n",
        "        if (match != nullptr &&\n",
        "            !pointlight_material_behavior_equal_v1(*match, record))\n",
        "            return nullptr;\n",
        "        if (match == nullptr)\n",
        "            match = &record;\n",
        "    }\n",
        "    return match;\n",
        "}\n\n",
        "constexpr bool resolve_pointlight_material_raw_mtd_v1(\n",
        "    std::uint64_t semantic_name_hash,\n",
        "    std::array<std::uint8_t,32> &raw_mtd_sha256) noexcept\n",
        "{\n",
        "    raw_mtd_sha256 = {};\n",
        "    const auto *record = find_pointlight_material_authority_v1(semantic_name_hash);\n",
        "    if (record == nullptr)\n",
        "        return false;\n",
        "    raw_mtd_sha256 = record->raw_mtd_sha256;\n",
        "    return true;\n",
        "}\n\n",
        "} // namespace dsrrl::operators::material_response::generated\n",
    ]

    ns.output.parent.mkdir(parents=True, exist_ok=True)
    ns.output.write_text("".join(out), encoding="utf-8", newline="\n")
    print(
        "POINTLIGHT_MATERIAL_AUTHORITY_HEADER_PASS "
        f"records={len(cooked)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
