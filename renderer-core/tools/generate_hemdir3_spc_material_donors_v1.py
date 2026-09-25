#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def digest_init(value: str) -> str:
    if len(value) != 64:
        raise ValueError("invalid SHA256")
    bs = [f"0x{value[i:i+2]}" for i in range(0, 64, 2)]
    return "{{" + ",".join(bs) + "}}"


def ff(value: str) -> str:
    value = value.strip()
    float(value)
    if value.lower().endswith("f"):
        return value
    return value + "f"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    with Path(args.input).open("r", encoding="utf-8", newline="") as fh:
        data = [line for line in fh if line.strip() and not line.startswith("#")]

    rows = list(csv.DictReader(data, delimiter="\t"))
    if len(rows) != 306:
        raise SystemExit(f"expected 306 active donor rows, got {len(rows)}")

    rows.sort(key=lambda r: r["raw_mtd_sha256"])

    shas = [r["raw_mtd_sha256"] for r in rows]
    if len(set(shas)) != len(shas):
        raise SystemExit("duplicate raw MTD SHA")

    for r in rows:
        if len(r["raw_mtd_sha256"]) != 64:
            raise SystemExit("invalid raw MTD SHA length")
        try:
            bytes.fromhex(r["raw_mtd_sha256"])
            c101 = [float(r[f"c101_{c}"]) for c in "rgb"]
            c102 = float(r["c102"])
            int(r["slot"])
        except Exception as exc:
            raise SystemExit(f"invalid donor row {r}: {exc}") from exc
        if any(v < 0.0 for v in c101) or c102 < 0.0:
            raise SystemExit(f"negative donor value for {r['raw_mtd_sha256']}")

    out = [
        "#pragma once\n",
        "#include \"dsrrl/core/types.hpp\"\n",
        "#include <array>\n",
        "#include <cstddef>\n",
        "#include <cstdint>\n\n",
        "namespace dsrrl::operators::lightbank::generated {\n\n",
        "struct hemdir3_spc_material_donor {\n",
        "    core::sha256_digest raw_mtd_sha256{};\n",
        "    std::array<float,3> c101_ptde{};\n",
        "    float c102 = 0.0f;\n",
        "    std::int32_t slot = 0;\n",
        "};\n\n",
        f"inline constexpr std::array<hemdir3_spc_material_donor,{len(rows)}> k_hemdir3_spc_material_donors_v1 = {{{{\n",
    ]

    for r in rows:
        out.append(
            "    {"
            + digest_init(r["raw_mtd_sha256"]) + ","
            + "{{"
            + ",".join(ff(r[f"c101_{c}"]) for c in "rgb")
            + "}},"
            + ff(r["c102"]) + ","
            + str(int(r["slot"])) + "},\n"
        )

    out += [
        "}};\n\n",
        "constexpr int compare_digest(const core::sha256_digest &a, const core::sha256_digest &b) noexcept\n",
        "{\n",
        "    for (std::size_t i=0;i<a.size();++i) {\n",
        "        if (a[i] < b[i]) return -1;\n",
        "        if (a[i] > b[i]) return 1;\n",
        "    }\n",
        "    return 0;\n",
        "}\n\n",
        "constexpr const hemdir3_spc_material_donor *find_hemdir3_spc_material_donor(\n",
        "    const core::sha256_digest &sha) noexcept\n",
        "{\n",
        "    std::size_t lo=0u, hi=k_hemdir3_spc_material_donors_v1.size();\n",
        "    while (lo < hi) {\n",
        "        const std::size_t mid=lo+(hi-lo)/2u;\n",
        "        const int cmp=compare_digest(k_hemdir3_spc_material_donors_v1[mid].raw_mtd_sha256,sha);\n",
        "        if (cmp < 0) lo=mid+1u; else hi=mid;\n",
        "    }\n",
        "    if (lo >= k_hemdir3_spc_material_donors_v1.size()) return nullptr;\n",
        "    return compare_digest(k_hemdir3_spc_material_donors_v1[lo].raw_mtd_sha256,sha)==0\n",
        "        ? &k_hemdir3_spc_material_donors_v1[lo] : nullptr;\n",
        "}\n\n",
        "} // namespace dsrrl::operators::lightbank::generated\n",
    ]

    Path(args.output).write_text("".join(out), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
