#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

HEADER = """#pragma once

#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {

struct route_seed {
    const char *binding_key;
    const char *mtd_name;
    std::uint32_t route_index;
    float c101;
    std::uint8_t lod_min;
    std::uint8_t lod_max;
    const char *material_family;
    std::uint32_t receiver0;
    std::uint32_t receiver1;
    std::uint32_t receiver2;
    const char *sha256;
    bool known_hash_name_collision;
};

inline constexpr route_seed k_material_routes_v1[] = {
"""

FOOTER = """};

inline constexpr std::size_t k_material_route_count_v1 =
    sizeof(k_material_routes_v1) / sizeof(k_material_routes_v1[0]);

} // namespace dsrrl::operators::material_response::generated
"""

def q(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    doc = json.loads(Path(args.input).read_text(encoding="utf-8"))
    routes = doc["routes"]

    lines = [HEADER]
    for route in routes:
        rx = route["receiver_triplet"]
        if len(rx) != 3:
            raise ValueError(f"receiver_triplet must contain exactly 3 receivers: {route['mtd_name']}")
        sha = route["sha256"].lower()
        if len(sha) != 64 or any(c not in "0123456789abcdef" for c in sha):
            raise ValueError(f"invalid SHA-256: {route['mtd_name']}")
        lines.append(
            "    {" +
            ",".join([
                q(route["binding_key"]),
                q(route["mtd_name"]),
                f"{int(route['route_index'])}u",
                f"{float(route['c101']):.6f}f",
                f"{int(route['lod_min'])}u",
                f"{int(route['lod_max'])}u",
                q(route["material_family"]),
                f"{int(rx[0])}u",
                f"{int(rx[1])}u",
                f"{int(rx[2])}u",
                q(sha),
                "true" if route.get("hash_name_collision", False) else "false",
            ]) +
            "},\n"
        )
    lines.append(FOOTER)
    Path(args.output).write_text("".join(lines), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
