#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROUTE_RE = re.compile(
    r'\{"[^"]+","(?P<mtd>[^"]+)",(?P<route>\d+)u,[-0-9.]+f,\d+u,\d+u,'
    r'"[^"]+",\d+u,\d+u,\d+u,"(?P<sha>[0-9a-f]{64})",(?:true|false)\}'
)
DONOR_RE = re.compile(
    r'\{"(?P<sha>[0-9a-f]{64})", \{(?P<c100>[^}]+)\}, \d+, '
    r'\{(?P<c101_ptde>[^}]+)\}, \{(?P<c101_f0q>[^}]+)\}, \d+, '
    r'(?P<c102>[^,]+), (?P<slot>-?\d+), (?P<has>true|false)\}'
)

# Exact donor authority. The direct PTDE local-specular island consumes the
# authored c102 value associated with the exact MTD SHA. Do not infer c102 from
# a broad material-family name: e.g. P_Wet and C_Wet variants legitimately
# carry different authored exponents.
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--routes", required=True)
    ap.add_argument("--donors", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    route_text = Path(args.routes).read_text(encoding="utf-8")
    donor_text = Path(args.donors).read_text(encoding="utf-8")

    donors = {}
    for m in DONOR_RE.finditer(donor_text):
        donors[m.group("sha")] = {
            "c100": m.group("c100"),
            "c101_f0q": m.group("c101_f0q"),
            "c102": m.group("c102"),
            "has": m.group("has") == "true",
        }

    by_route = {}
    for m in ROUTE_RE.finditer(route_text):
        route = int(m.group("route"))
        sha = m.group("sha")
        if sha not in donors or not donors[sha]["has"]:
            raise SystemExit(f"missing active donor constants for route={route} sha={sha}")

        specular_power = donors[sha]["c102"]
        specular_power_verified = donors[sha]["has"]
        value = (
            donors[sha]["c100"],
            donors[sha]["c101_f0q"],
            specular_power,
            specular_power_verified,
        )
        if route in by_route and by_route[route] != value:
            raise SystemExit(f"route collision has divergent b12 constants: {route}")
        by_route[route] = value

    if len(by_route) != 34:
        raise SystemExit(f"expected 34 distinct active routes, got {len(by_route)}")

    lines = [
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstdint>\n\n",
        "namespace dsrrl::operators::material_response::generated {\n\n",
        "struct material_response_constants {\n",
        "    std::uint32_t route_index;\n",
        "    std::array<float,3> c100;\n",
        "    std::array<float,3> c101_f0q;\n",
        "    float ptde_specular_power;\n",
        "    bool ptde_specular_power_verified;\n",
        "};\n\n",
        f"inline constexpr std::array<material_response_constants,{len(by_route)}> "
        "k_material_response_constants_v1 = {{\n",
    ]

    for route in sorted(by_route):
        c100, c101, specular_power, verified = by_route[route]
        verified_text = "true" if verified else "false"
        lines.append(
            f"    {{{route}u,{{{{{c100}}}}},{{{{{c101}}}}},"
            f"{specular_power},{verified_text}}},\n"
        )

    lines += [
        "}};\n\n",
        "constexpr const material_response_constants *find_material_response_constants(\n",
        "    std::uint32_t route_index) noexcept\n",
        "{\n",
        "    const material_response_constants *hit = nullptr;\n",
        "    for (const auto &row : k_material_response_constants_v1) {\n",
        "        if (row.route_index != route_index)\n",
        "            continue;\n",
        "        if (hit != nullptr)\n",
        "            return nullptr;\n",
        "        hit = &row;\n",
        "    }\n",
        "    return hit;\n",
        "}\n\n",
        "} // namespace dsrrl::operators::material_response::generated\n",
    ]

    Path(args.out).write_text("".join(lines), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
