#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

SHA_RE = re.compile(r"^[0-9a-f]{64}$")
EXPECTED_CLASSES = {
    "clustered_spc_pnts": 24,
    "fixed_spc_pntss": 24,
    "fixed_spc_pntssss": 24,
}
CLASS_ENUM = {
    "clustered_spc_pnts": "local_specular_generated_class::clustered_spc_pnts",
    "fixed_spc_pntss": "local_specular_generated_class::fixed_spc_pntss",
    "fixed_spc_pntssss": "local_specular_generated_class::fixed_spc_pntssss",
}

def fail(message: str) -> None:
    raise SystemExit(message)

def read_rows(path: str):
    with Path(path).open("r", encoding="utf-8", newline="") as f:
        filtered = (line for line in f if not line.startswith("#"))
        return list(csv.DictReader(filtered, delimiter="\t"))

def digest_bytes(sha: str) -> str:
    return ",".join(f"0x{sha[i:i+2]}u" for i in range(0, 64, 2))

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    rows = read_rows(args.input)
    if len(rows) != 72:
        fail(f"expected 72 unique local-specular receivers, got {len(rows)}")

    seen_sha = set()
    class_counts = {key: 0 for key in EXPECTED_CLASSES}
    alias_total = 0
    entries = []

    for row in rows:
        cls = row["receiver_class"]
        if cls not in EXPECTED_CLASSES:
            fail(f"unsupported receiver class: {cls}")

        sha = row["code_sha256"]
        if not SHA_RE.fullmatch(sha):
            fail(f"invalid sha256: {sha}")
        if sha in seen_sha:
            fail(f"duplicate sha256 across receiver classes: {sha}")
        seen_sha.add(sha)

        code_size = int(row["code_size"])
        shader_index = int(row["representative_shader_index"])
        alias_count = int(row["alias_count"])
        if code_size <= 0 or shader_index < 0:
            fail(f"invalid size/index for {sha}")
        if alias_count != 2:
            fail(f"expected HemEnv/HemEnvLerp alias pair for {sha}, got {alias_count}")

        class_counts[cls] += 1
        alias_total += alias_count
        entries.append((cls, sha, code_size, shader_index, alias_count))

    if class_counts != EXPECTED_CLASSES:
        fail(f"unexpected class counts: {class_counts}")
    if alias_total != 144:
        fail(f"expected 144 aliases, got {alias_total}")

    candidate_sizes = sorted({entry[2] for entry in entries})

    out = [
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstddef>\n",
        "#include <cstdint>\n\n",
        "namespace dsrrl::operators::point_light::generated {\n\n",
        "enum class local_specular_generated_class : std::uint8_t {\n",
        "    clustered_spc_pnts = 1,\n",
        "    fixed_spc_pntss = 2,\n",
        "    fixed_spc_pntssss = 3\n",
        "};\n\n",
        "struct local_specular_receiver_record {\n",
        "    local_specular_generated_class receiver_class;\n",
        "    std::array<std::uint8_t,32> sha256;\n",
        "    std::uint32_t code_size;\n",
        "    std::uint16_t representative_shader_index;\n",
        "    std::uint8_t alias_count;\n",
        "};\n\n",
        f"inline constexpr std::array<std::uint32_t,{len(candidate_sizes)}> k_local_specular_candidate_sizes = {{{{\n",
    ]
    for size in candidate_sizes:
        out.append(f"    {size}u,\n")
    out += [
        "}};\n\n",
        "inline constexpr bool local_specular_candidate_size(std::size_t size) noexcept\n",
        "{\n",
        "    for (const auto candidate : k_local_specular_candidate_sizes)\n",
        "        if (candidate == size) return true;\n",
        "    return false;\n",
        "}\n\n",
        "inline constexpr std::array<local_specular_receiver_record,72> k_local_specular_receivers = {{\n",
    ]
    for cls, sha, size, shader_index, alias_count in entries:
        out.append(
            f"    {{{CLASS_ENUM[cls]},{{{digest_bytes(sha)}}},{size}u,{shader_index}u,{alias_count}u}},\n"
        )
    out += [
        "}};\n\n",
        "} // namespace dsrrl::operators::point_light::generated\n",
    ]

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("".join(out), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
