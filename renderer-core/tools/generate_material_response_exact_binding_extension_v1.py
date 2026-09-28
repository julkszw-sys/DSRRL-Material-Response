#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path

DONOR_RE = re.compile(
    r'\{"(?P<sha>[0-9a-f]{64})", \{(?P<c100>[^}]+)\}, (?P<c100_tier>\d+), '
    r'\{(?P<c101_ptde>[^}]+)\}, \{(?P<c101_f0q>[^}]+)\}, (?P<c101_tier>\d+), '
    r'(?P<c102>[^,]+), (?P<slot>-?\d+), (?P<has>true|false)\}'
)

ROUTE_TAG_BASE = 0x40000000


def parse_vec(text: str) -> tuple[float, float, float]:
    values = tuple(float(x.strip().rstrip("f")) for x in text.split(","))
    if len(values) != 3:
        raise SystemExit(f"expected float3, got: {text}")
    return values  # type: ignore[return-value]


def f32_text(value: float) -> str:
    return f"{value:.9g}f"


def bytes_cpp(sha: str) -> str:
    values = [f"0x{sha[i:i+2]}u" for i in range(0, 64, 2)]
    return "{{" + ",".join(values) + "}}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bindings", required=True)
    ap.add_argument("--routes", required=True)
    ap.add_argument("--donors", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    bindings_doc = json.loads(Path(args.bindings).read_text(encoding="utf-8"))
    routes_doc = json.loads(Path(args.routes).read_text(encoding="utf-8"))
    donor_text = Path(args.donors).read_text(encoding="utf-8")

    bindings = bindings_doc.get("bindings") or bindings_doc.get("rows") or bindings_doc.get("routes")
    if not isinstance(bindings, list):
        raise SystemExit("exact-binding JSON has no bindings/rows/routes array")

    routes = routes_doc.get("routes")
    if not isinstance(routes, list):
        raise SystemExit("material-response routes JSON has no routes array")

    donors: dict[str, dict[str, object]] = {}
    for m in DONOR_RE.finditer(donor_text):
        donors[m.group("sha")] = {
            "c100": parse_vec(m.group("c100")),
            "c101_ptde": parse_vec(m.group("c101_ptde")),
            "c101_f0q": parse_vec(m.group("c101_f0q")),
            "c102": float(m.group("c102").rstrip("f")),
            "slot": int(m.group("slot")),
            "has": m.group("has") == "true",
        }

    route_shas = {str(r["sha256"]) for r in routes}
    records = []

    for binding in bindings:
        operators = binding.get("operators", {})
        if operators.get("material_response") != "USE":
            continue
        if operators.get("pointlight") != "NO_USE":
            raise SystemExit(
                f"{binding.get('mtd_name')}: exact MR extension must remain PointLight NO_USE"
            )

        sha = str(binding["raw_sha256"])
        if sha in route_shas:
            raise SystemExit(
                f"{binding['mtd_name']}: exact-binding SHA already belongs to ordinary MR route"
            )

        donor = donors.get(sha)
        if donor is None or not donor["has"]:
            raise SystemExit(
                f"{binding['mtd_name']}: exact donor constants missing for {sha}"
            )

        receiver_scope = list(binding["receiver_scope"])
        if len(receiver_scope) != 3 or len(set(receiver_scope)) != 3:
            raise SystemExit(
                f"{binding['mtd_name']}: expected exact receiver triplet"
            )

        c101 = float(binding["c101"])
        donor_c101 = donor["c101_ptde"]
        assert isinstance(donor_c101, tuple)
        if any(not math.isclose(x, c101, rel_tol=0.0, abs_tol=1e-6) for x in donor_c101):
            raise SystemExit(
                f"{binding['mtd_name']}: binding c101 disagrees with exact donor"
            )

        # Derive only the legacy LOD metadata from an already-certified ordinary
        # MR behavior tuple. The actual extension constants remain keyed by the
        # extension's exact raw-MTD SHA. If the certified cohort does not agree
        # on one LOD interval, generation fails instead of guessing.
        reference_lods = set()
        for route in routes:
            if route["material_family"] != binding["material_family"]:
                continue
            if list(route["receiver_triplet"]) != receiver_scope:
                continue
            if not math.isclose(float(route["c101"]), c101, rel_tol=0.0, abs_tol=1e-6):
                continue

            route_donor = donors.get(str(route["sha256"]))
            if route_donor is None or not route_donor["has"]:
                continue

            same_constants = (
                route_donor["c100"] == donor["c100"]
                and route_donor["c101_f0q"] == donor["c101_f0q"]
                and math.isclose(
                    float(route_donor["c102"]),
                    float(donor["c102"]),
                    rel_tol=0.0,
                    abs_tol=1e-6,
                )
            )
            if same_constants:
                reference_lods.add((int(route["lod_min"]), int(route["lod_max"])))

        if len(reference_lods) != 1:
            raise SystemExit(
                f"{binding['mtd_name']}: exact extension has no unique certified LOD behavior: "
                f"{sorted(reference_lods)}"
            )

        lod_min, lod_max = next(iter(reference_lods))
        bridge_binding_id = int(binding["bridge_binding_id"])
        if bridge_binding_id <= 0 or bridge_binding_id >= ROUTE_TAG_BASE:
            raise SystemExit(f"invalid bridge_binding_id: {bridge_binding_id}")

        policy = str(binding["gate_policy"])
        if policy not in {"DIRECT_EXACT", "PTDE_COMPANION_REQUIRED"}:
            raise SystemExit(
                f"{binding['mtd_name']}: unsupported gate policy {policy}"
            )

        records.append(
            {
                "bridge_binding_id": bridge_binding_id,
                "route_tag": ROUTE_TAG_BASE | bridge_binding_id,
                "mtd_name": str(binding["mtd_name"]),
                "raw_sha256": sha,
                "material_family": str(binding["material_family"]),
                "receiver_scope": receiver_scope,
                "runtime_mtd_allowed": policy == "DIRECT_EXACT",
                "c101": c101,
                "c100": donor["c100"],
                "c101_f0q": donor["c101_f0q"],
                "c102": float(donor["c102"]),
                "lod_min": lod_min,
                "lod_max": lod_max,
            }
        )

    if len(records) != 8:
        raise SystemExit(f"expected 8 exact MR binding extensions, got {len(records)}")

    if len({r["route_tag"] for r in records}) != len(records):
        raise SystemExit("exact MR extension route-tag collision")
    if len({r["raw_sha256"] for r in records}) != len(records):
        raise SystemExit("exact MR extension raw-SHA collision")
    if len({r["mtd_name"] for r in records}) != len(records):
        raise SystemExit("exact MR extension semantic-name collision")

    records.sort(key=lambda r: int(r["bridge_binding_id"]))

    lines = [
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstdint>\n\n",
        "namespace dsrrl::operators::material_response::generated {\n\n",
        "inline constexpr std::uint32_t k_exact_binding_mr_route_tag_base = 0x40000000u;\n\n",
        "struct exact_binding_mr_record {\n",
        "    std::uint32_t bridge_binding_id;\n",
        "    std::uint32_t route_tag;\n",
        "    const char *mtd_name;\n",
        "    std::array<std::uint8_t,32> raw_mtd_sha256;\n",
        "    const char *material_family;\n",
        "    std::uint32_t receiver0;\n",
        "    std::uint32_t receiver1;\n",
        "    std::uint32_t receiver2;\n",
        "    bool runtime_mtd_allowed;\n",
        "    float c101;\n",
        "    std::array<float,3> c100;\n",
        "    std::array<float,3> c101_f0q;\n",
        "    float ptde_specular_power;\n",
        "    std::uint8_t lod_min;\n",
        "    std::uint8_t lod_max;\n",
        "};\n\n",
        f"inline constexpr std::array<exact_binding_mr_record,{len(records)}> "
        "k_exact_binding_mr_v1 = {{\n",
    ]

    for r in records:
        c100 = r["c100"]
        c101_f0q = r["c101_f0q"]
        assert isinstance(c100, tuple)
        assert isinstance(c101_f0q, tuple)
        lines.append(
            "    {"
            f"{r['bridge_binding_id']}u,"
            f"0x{int(r['route_tag']):08x}u,"
            f"\"{r['mtd_name']}\","
            f"{bytes_cpp(str(r['raw_sha256']))},"
            f"\"{r['material_family']}\","
            f"{r['receiver_scope'][0]}u,{r['receiver_scope'][1]}u,{r['receiver_scope'][2]}u,"
            f"{str(bool(r['runtime_mtd_allowed'])).lower()},"
            f"{f32_text(float(r['c101']))},"
            "{{" + ",".join(f32_text(float(x)) for x in c100) + "}},"
            "{{" + ",".join(f32_text(float(x)) for x in c101_f0q) + "}},"
            f"{f32_text(float(r['c102']))},"
            f"{r['lod_min']}u,{r['lod_max']}u"
            "},\n"
        )

    lines += [
        "}};\n\n",
        "constexpr bool is_exact_binding_mr_route_tag(std::uint32_t route_tag) noexcept\n",
        "{\n",
        "    return (route_tag & 0xff000000u) == k_exact_binding_mr_route_tag_base;\n",
        "}\n\n",
        "} // namespace dsrrl::operators::material_response::generated\n",
    ]

    Path(args.output).write_text("".join(lines), encoding="utf-8")
    print(
        "MR_EXACT_BINDING_EXTENSION_GENERATION_PASS "
        f"records={len(records)} direct_exact="
        f"{sum(1 for r in records if r['runtime_mtd_allowed'])} "
        f"companion_required="
        f"{sum(1 for r in records if not r['runtime_mtd_allowed'])}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
