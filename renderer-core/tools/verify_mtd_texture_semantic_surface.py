#!/usr/bin/env python3
"""Guard the PTDE FLVER texture-semantic surface used by the MTD census.

Construction/static evidence only. Presence may support USE; absence remains
UNKNOWN/fail-open while source coverage is partial.
"""
from __future__ import annotations

import ast
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "tools" / "generate_ptde_flver_texture_semantics.py"
SRC = ROOT / "data" / "census" / "ptde_flver_texture_semantics_v1_source.json"

# This mapping is a serialized ABI: generated positive_mask values are consumed
# by C++ with these exact bit positions. Checking names alone is insufficient.
EXPECTED_BITS = {
    "g_Diffuse": 1 << 0,
    "g_Bumpmap": 1 << 1,
    "g_DetailBumpmap": 1 << 2,
    "g_Specular": 1 << 3,
    "g_Lightmap": 1 << 4,
    "g_Diffuse_2": 1 << 5,
    "g_Bumpmap_2": 1 << 6,
    "g_Specular_2": 1 << 7,
}


def _literal_int(node: ast.AST) -> int:
    if isinstance(node, ast.Constant) and isinstance(node.value, int) and not isinstance(node.value, bool):
        return node.value
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.LShift):
        left = _literal_int(node.left)
        right = _literal_int(node.right)
        if left < 0 or right < 0 or right > 63:
            raise SystemExit("BITS contains an invalid literal shift")
        return left << right
    raise SystemExit("BITS values must be integer literals or literal left shifts")


def generator_bits() -> dict[str, int]:
    tree = ast.parse(GEN.read_text(encoding="utf-8"), filename=str(GEN))
    for node in tree.body:
        value = None
        if isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "BITS" for t in node.targets
        ):
            value = node.value
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) and node.target.id == "BITS":
            value = node.value
        if value is None:
            continue
        if not isinstance(value, ast.Dict):
            raise SystemExit("BITS is not a dict literal")
        parsed: dict[str, int] = {}
        for key, raw_value in zip(value.keys, value.values):
            if not isinstance(key, ast.Constant) or not isinstance(key.value, str):
                raise SystemExit("BITS contains a non-string key")
            if key.value in parsed:
                raise SystemExit(f"BITS contains duplicate key: {key.value}")
            parsed[key.value] = _literal_int(raw_value)
        return parsed
    raise SystemExit("BITS assignment not found")


def main() -> int:
    bits = generator_bits()
    if bits != EXPECTED_BITS:
        missing = sorted(set(EXPECTED_BITS) - set(bits))
        extra = sorted(set(bits) - set(EXPECTED_BITS))
        wrong = sorted(
            key for key in set(bits) & set(EXPECTED_BITS)
            if bits[key] != EXPECTED_BITS[key]
        )
        raise SystemExit(
            "texture semantic bit ABI mismatch: "
            f"missing={missing} extra={extra} wrong_values={wrong}"
        )
    if len(set(bits.values())) != len(bits):
        raise SystemExit("texture semantic bit ABI contains duplicate bit values")

    src = json.loads(SRC.read_text(encoding="utf-8"))
    if src.get("coverage_state") == "PARTIAL_SOURCE_COVERAGE" and (
        src.get("positive_use_only") is not True
        or src.get("absence_is_unknown") is not True
    ):
        raise SystemExit(
            "partial FLVER coverage must remain positive-only with absence UNKNOWN"
        )
    if (
        int(src.get("source", {}).get("scan_error_count", 0)) > 0
        and src.get("absence_is_unknown") is not True
    ):
        raise SystemExit("scan errors forbid negative authority")

    observed = src.get("observed", {}).get("positive_semantic_identity_counts", {})
    unknown = set(observed) - set(bits)
    if unknown:
        raise SystemExit(
            f"source manifest reports semantics outside generator surface: {sorted(unknown)}"
        )

    print(
        "PASS texture semantic surface: "
        f"supported={len(bits)} reported={len(observed)} "
        f"coverage={src.get('coverage_state')} absence=UNKNOWN abi=EXACT"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
