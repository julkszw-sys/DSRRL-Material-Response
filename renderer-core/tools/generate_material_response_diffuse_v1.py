#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path

def q(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    with Path(args.input).open("r", encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))

    if len(rows) != 48:
        raise SystemExit(f"expected 48 rows, got {len(rows)}")

    families = {"HEMENV": [], "HEMENV_LERP": []}
    for row in rows:
        family = row["family"]
        if family not in families:
            raise SystemExit(f"unknown family {family}")
        if len(row["stock_sha256"]) != 64:
            raise SystemExit(f"bad stock sha for {row['label']}")
        index = int(row["index"])
        receiver_id = int(row["receiver_id"])
        if receiver_id != 24 + index:
            raise SystemExit(f"receiver/index mismatch for {row['label']}")
        for key in ("stock_size", "c100_site0", "c100_site1", "diffuse_pow_site"):
            if int(row[key]) <= 0:
                raise SystemExit(f"invalid {key} for {row['label']}")
        families[family].append(row)

    for family, records in families.items():
        records.sort(key=lambda r: int(r["index"]))
        if len(records) != 24:
            raise SystemExit(f"expected 24 {family} rows, got {len(records)}")
        if [int(r["index"]) for r in records] != list(range(24)):
            raise SystemExit(f"non-contiguous {family} indices")

    lines = [
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstdint>\n",
        "#include <string_view>\n\n",
        "namespace dsrrl::operators::material_response::generated_diffuse_v1 {\n\n",
        "enum class receiver_family : std::uint8_t { stable_hemenv = 0, hemenvlerp = 1 };\n\n",
        "struct plan {\n",
        "    receiver_family family;\n",
        "    std::uint8_t family_index;\n",
        "    std::uint32_t receiver_id;\n",
        "    std::string_view label;\n",
        "    std::uint32_t stock_size;\n",
        "    std::string_view stock_sha256;\n",
        "    std::array<std::uint32_t,2> c100_sites;\n",
        "    std::uint32_t diffuse_pow_site;\n",
        "};\n\n",
        "inline constexpr std::array<plan,48> k_plans = {{\n",
    ]

    for family_name in ("HEMENV", "HEMENV_LERP"):
        enum_name = "stable_hemenv" if family_name == "HEMENV" else "hemenvlerp"
        for row in families[family_name]:
            lines.append(
                "    {receiver_family::" + enum_name + "," +
                f"{int(row['index'])}u,{int(row['receiver_id'])}u," +
                q(row["label"]) + "," +
                f"{int(row['stock_size'])}u," + q(row["stock_sha256"]) + "," +
                "{{" + f"{int(row['c100_site0'])}u,{int(row['c100_site1'])}u" + "}}," +
                f"{int(row['diffuse_pow_site'])}u" +
                "},\n"
            )

    lines += [
        "}};\n\n",
        "} // namespace dsrrl::operators::material_response::generated_diffuse_v1\n",
    ]

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("".join(lines), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
