#!/usr/bin/env python3
import argparse
import base64
import hashlib
import json
from pathlib import Path


def load_payload(path: Path):
    obj = json.loads(path.read_text(encoding="utf-8"))
    out = {}
    for kind in ("vertex", "pixel"):
        entry = obj[kind]
        data = base64.b64decode(entry["base64"], validate=True)
        expected_size = int(entry["size"])
        expected_sha = str(entry["sha256"]).lower()
        actual_sha = hashlib.sha256(data).hexdigest()
        if len(data) != expected_size:
            raise SystemExit(
                f"{kind} size mismatch: {len(data)} != {expected_size}")
        if actual_sha != expected_sha:
            raise SystemExit(
                f"{kind} sha256 mismatch: {actual_sha} != {expected_sha}")
        out[kind] = (data, expected_sha)
    return out


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
    parser.add_argument("--payload", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    payload = load_payload(args.payload)
    vertex, vertex_sha = payload["vertex"]
    pixel, pixel_sha = payload["pixel"]

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
    "{vertex_sha}";
inline constexpr const char *pixel_sha256 =
    "{pixel_sha}";

}} // namespace dsrrl::runtime::dof::embedded_plain_rate
"""

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
