#!/usr/bin/env python3
"""Guard the PTDE FLVER texture-semantic surface used by the MTD census.

This is a construction/static-evidence check only. Presence may support USE.
Absence must remain UNKNOWN/fail-open while source coverage is partial.
"""
from __future__ import annotations

import ast
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "tools" / "generate_ptde_flver_texture_semantics.py"
SRC = ROOT / "data" / "census" / "ptde_flver_texture_semantics_v1_source.json"

REQUIRED = {
    "g_Diffuse", "g_Bumpmap", "g_DetailBumpmap", "g_Specular",
    "g_Lightmap", "g_Diffuse_2", "g_Bumpmap_2", "g_Specular_2",
}


def generator_bits() -> set[str]:
    tree = ast.parse(GEN.read_text(encoding="utf-8"), filename=str(GEN))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "BITS" for t in node.targets):
            value = ast.literal_eval(node.value)
            if not isinstance(value, dict):
                raise SystemExit("BITS is not a dict")
            return set(value)
    raise SystemExit("BITS assignment not found")


def main() -> int:
    bits = generator_bits()
    missing = REQUIRED - bits
    if missing:
        raise SystemExit(f"missing texture semantic consumers: {sorted(missing)}")

    src = json.loads(SRC.read_text(encoding="utf-8"))
    if src.get("coverage_state") == "PARTIAL_SOURCE_COVERAGE":
        if src.get("positive_use_only") is not True or src.get("absence_is_unknown") is not True:
            raise SystemExit("partial FLVER coverage must remain positive-only with absence UNKNOWN")
    if int(src.get("source", {}).get("scan_error_count", 0)) > 0 and src.get("absence_is_unknown") is not True:
        raise SystemExit("scan errors forbid negative authority")

    observed = src.get("observed", {}).get("positive_semantic_identity_counts", {})
    unknown_keys = set(observed) - bits
    if unknown_keys:
        raise SystemExit(f"source manifest reports semantics outside generator surface: {sorted(unknown_keys)}")

    # Do not require every supported semantic to be present in the old manifest:
    # that manifest was produced from a partial run. A future source-complete rerun
    # can populate Lightmap/secondary-slot counts without changing this authority rule.
    print(
        "PASS texture semantic surface: "
        f"supported={len(bits)} reported={len(observed)} "
        f"coverage={src.get('coverage_state')} absence=UNKNOWN"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
