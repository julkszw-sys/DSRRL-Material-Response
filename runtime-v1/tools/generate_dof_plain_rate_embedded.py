#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path

EXPECTED = {
    "vertex": {
        "size": 804,
        "sha256": "6045a062806520145f2f40571ba53c5b1bdd1d638a465cb7a86a42ebc841d6af",
    },
    "pixel": {
        "size": 4904,
        "sha256": "c75c8fc3bda694536ae9c4edec025ffc1b1c04297bf54d67c51b4b9fee782897",
    },
}

def load_exact(path: Path, kind: str) -> bytes:
    data = path.read_bytes()
    expected = EXPECTED[kind]
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != expected["size"]:
        raise SystemExit(
            f"{kind} size mismatch: {len(data)} != {expected['size']}")
    if digest != expected["sha256"]:
        raise SystemExit(
            f"{kind} sha256 mismatch: {digest} != {expected['sha256']}")
    return data

def array_text(data: bytes) -> str:
    rows = []
    for offset in range(0, len(data), 16):
        chunk = data[offset:offset + 16]
        rows.append(
            "    " + ", ".join(f"0x{value:02X}u" for value in chunk)
        )
    return ",\n".join(rows)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vertex", required=True, type=Path)
    parser.add_argument("--pixel", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    vertex = load_exact(args.vertex, "vertex")
    pixel = load_exact(args.pixel, "pixel")

    text = f"""#pragma once

#include <array>
#include <cstdint>

namespace dsrrl::runtime::dof::embedded_plain_rate {{

inline constexpr std::array<std::uint8_t, {len(vertex)}> vertex = {{{{
{array_text(vertex)}
}}}};

inline constexpr std::array<std::uint8_t, {len(pixel)}> pixel = {{{{
{array_text(pixel)}
}}}};

inline constexpr const char *vertex_sha256 =
    "{EXPECTED['vertex']['sha256']}";
inline constexpr const char *pixel_sha256 =
    "{EXPECTED['pixel']['sha256']}";

}} // namespace dsrrl::runtime::dof::embedded_plain_rate
"""

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text, encoding="utf-8", newline="\n")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
