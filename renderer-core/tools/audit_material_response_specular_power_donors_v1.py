#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
import re
from pathlib import Path

ROUTE_RE = re.compile(
    r'\{"[^"]+","[^"]+",(?P<route>\d+)u,[-0-9.]+f,\d+u,\d+,'
    r'"[^"]+",\d+u,\d+u,\d+u,"(?P<sha>[0-9a-f]{64})",(?:true|false)\}'
)
DONOR_RE = re.compile(
    r'\{"(?P<sha>[0-9a-f]{64})", \{[^}]+\}, \d+, '
    r'\{[^}]+\}, \{[^}]+\}, \d+, '
    r'(?P<c102>[^,]+), [^,]+, (?P<has>true|false)\}'
)
GENERATED_RE = re.compile(
    r'\{(?P<route>\d+)u,\{\{[^}]+\}\},\{\{[^}]+\}\},'
    r'(?P<c102>[-0-9.]+)f,(?P<verified>true|false)\}'
)

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--routes", required=True)
    ap.add_argument("--donors", required=True)
    ap.add_argument("--generated", required=True)
    args = ap.parse_args()

    routes = Path(args.routes).read_text(encoding="utf-8")
    donors_text = Path(args.donors).read_text(encoding="utf-8")
    generated = Path(args.generated).read_text(encoding="utf-8")

    donor = {}
    for m in DONOR_RE.finditer(donors_text):
        donor[m.group("sha")] = (
            float(m.group("c102").rstrip("f")),
            m.group("has") == "true",
        )

    expected = {}
    for m in ROUTE_RE.finditer(routes):
        route = int(m.group("route"))
        sha = m.group("sha")
        if sha not in donor:
            raise SystemExit(f"route {route}: donor SHA missing: {sha}")
        value = donor[sha]
        prior = expected.get(route)
        if prior is not None and prior != value:
            raise SystemExit(f"route {route}: divergent exact donor c102")
        expected[route] = value

    actual = {
        int(m.group("route")): (
            float(m.group("c102")),
            m.group("verified") == "true",
        )
        for m in GENERATED_RE.finditer(generated)
    }

    if len(expected) != 34 or len(actual) != 34:
        raise SystemExit(
            f"expected/generated route count mismatch: {len(expected)}/{len(actual)}"
        )

    for route, (want, verified) in expected.items():
        got = actual.get(route)
        if got is None:
            raise SystemExit(f"route {route}: generated entry missing")
        if got[1] != verified or not math.isclose(got[0], want, rel_tol=0.0, abs_tol=1e-6):
            raise SystemExit(
                f"route {route}: generated c102 {got} != exact donor {(want, verified)}"
            )

    print(
        "MR_SPECULAR_POWER_DONOR_AUDIT_PASS "
        f"routes={len(expected)} source=exact_mtd_sha"
    )
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
