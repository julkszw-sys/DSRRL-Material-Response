#!/usr/bin/env python3
"""Generate the exact V12 Normal route corpus header.

The TSV is a compact, reviewable extraction of k_normal_triples from the
recovered source-complete V12 bridge. No route inference is performed here.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
from pathlib import Path

EXPECTED_INPUT_SHA256 = "8f81a5da4d65db85a1a99846a5c918709933eafc0cb9c4e3cf5400bee7f91380"
EXPECTED_TUPLES = 557
EXPECTED_SAFE_ROWS = 3872
EXPECTED_UNIQUE_HASHES = 1664


def load(path: Path):
    raw = path.read_bytes()
    actual = hashlib.sha256(raw).hexdigest()
    if actual != EXPECTED_INPUT_SHA256:
        raise SystemExit(f"input SHA256 mismatch: {actual} != {EXPECTED_INPUT_SHA256}")

    lines = [
        line for line in raw.decode("utf-8").splitlines()
        if line and not line.startswith("#")
    ]
    rows = list(csv.DictReader(lines, delimiter="\t"))

    tuples = []
    safe_rows = 0
    for row in rows:
        triple = tuple(int(row[key], 16) for key in ("diffuse_hash", "spec_hash", "normal_hash"))
        tuples.append(triple)
        safe_rows += int(row["row_uses"])

    if tuples != sorted(tuples):
        raise SystemExit("normal tuple corpus is not lexicographically sorted")
    if len(tuples) != EXPECTED_TUPLES or len(set(tuples)) != EXPECTED_TUPLES:
        raise SystemExit("normal tuple count/uniqueness mismatch")
    if safe_rows != EXPECTED_SAFE_ROWS:
        raise SystemExit("safe row count mismatch")

    members = sorted({value for triple in tuples for value in triple})
    if len(members) != EXPECTED_UNIQUE_HASHES:
        raise SystemExit("normal logical-name hash count mismatch")
    return tuples, members


def render(tuples, members):
    out = []
    out.append("#pragma once\n")
    out.append("#include <array>\n#include <cstddef>\n#include <cstdint>\n\n")
    out.append("namespace dsrrl::runtime::generated {\n\n")
    out.append("struct normal_tuple_v12 {\n")
    out.append("    std::uint64_t diffuse_hash;\n    std::uint64_t spec_hash;\n    std::uint64_t normal_hash;\n};\n\n")
    out.append(f"inline constexpr std::array<normal_tuple_v12, {len(tuples)}> k_normal_tuples_v12 = {{{{\n")
    for d, s, n in tuples:
        out.append(f"    {{0x{d:016x}ull, 0x{s:016x}ull, 0x{n:016x}ull}},\n")
    out.append("}};\n\n")
    out.append(f"inline constexpr std::array<std::uint64_t, {len(members)}> k_normal_tuple_member_hashes_v12 = {{{{\n")
    for value in members:
        out.append(f"    0x{value:016x}ull,\n")
    out.append("}};\n\n")
    out.append("inline bool normal_name_hash_allowed_v12(std::uint64_t value) noexcept\n{\n")
    out.append("    std::size_t lo = 0, hi = k_normal_tuple_member_hashes_v12.size();\n")
    out.append("    while (lo < hi) {\n        const auto mid = lo + ((hi - lo) >> 1u);\n        const auto at = k_normal_tuple_member_hashes_v12[mid];\n        if (value < at) hi = mid; else if (value > at) lo = mid + 1u; else return true;\n    }\n    return false;\n}\n\n")
    out.append("inline bool normal_tuple_allowed_v12(std::uint64_t diffuse, std::uint64_t spec, std::uint64_t normal) noexcept\n{\n")
    out.append("    std::size_t lo = 0, hi = k_normal_tuples_v12.size();\n")
    out.append("    while (lo < hi) {\n        const auto mid = lo + ((hi - lo) >> 1u);\n        const auto &at = k_normal_tuples_v12[mid];\n")
    out.append("        if (diffuse != at.diffuse_hash) { if (diffuse < at.diffuse_hash) hi = mid; else lo = mid + 1u; continue; }\n")
    out.append("        if (spec != at.spec_hash) { if (spec < at.spec_hash) hi = mid; else lo = mid + 1u; continue; }\n")
    out.append("        if (normal != at.normal_hash) { if (normal < at.normal_hash) hi = mid; else lo = mid + 1u; continue; }\n")
    out.append("        return true;\n    }\n    return false;\n}\n\n")
    out.append("inline constexpr std::size_t k_normal_tuple_count_v12 = k_normal_tuples_v12.size();\n")
    out.append("inline constexpr std::size_t k_normal_tuple_member_count_v12 = k_normal_tuple_member_hashes_v12.size();\n")
    out.append("inline constexpr std::size_t k_normal_safe_row_count_v12 = 3872u;\n\n")
    out.append("} // namespace dsrrl::runtime::generated\n")
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ns = ap.parse_args()
    tuples, members = load(ns.input)
    ns.output.write_text(render(tuples, members), encoding="utf-8")


if __name__ == "__main__":
    main()
