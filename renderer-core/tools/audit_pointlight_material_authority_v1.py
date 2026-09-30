#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--router", type=Path, required=True)
    ap.add_argument("--authority", type=Path, required=True)
    ns = ap.parse_args()

    router = json.loads(ns.router.read_text(encoding="utf-8"))
    authority = json.loads(ns.authority.read_text(encoding="utf-8"))

    if authority.get("schema") != "DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_V1":
        raise SystemExit("unexpected authority schema")
    if len(router.get("records", [])) != 325:
        raise SystemExit("unexpected router record count")

    active = authority.get("active_records", [])
    deferred = authority.get("deferred_records", [])

    spc = [x for x in active if x.get("material_mode") == "SPC"]
    nospc = [x for x in active if x.get("material_mode") == "NOSPC"]
    non_scalar = [
        x for x in spc
        if len(set(x.get("c101_f32_bits", []))) > 1
    ]

    if len(active) != 205 or len(spc) != 180 or len(nospc) != 25:
        raise SystemExit(
            f"authority count drift: total={len(active)} spc={len(spc)} "
            f"nospc={len(nospc)}"
        )
    if len(non_scalar) != 12:
        raise SystemExit(
            f"expected 12 authored RGB-c101 Spc rows, got {len(non_scalar)}"
        )

    seen_semantic: dict[str, str] = {}
    for row in active:
        if row.get("authority") != "POINTLIGHT_MATERIAL_CONSTANTS_ONLY":
            raise SystemExit("active row widened beyond PointLight constants")
        if row.get("dsr_spx") != row.get("ptde_spx"):
            raise SystemExit(
                f"active row is not exact-SPX: {row.get('mtd_name')}"
            )
        if "FRPG_Phn_" not in str(row.get("dsr_spx", "")):
            raise SystemExit(
                f"active row escaped Phn family: {row.get('mtd_name')}"
            )
        semantic = str(row.get("semantic_name_hash_fnv1a_utf8", ""))
        sha = str(row.get("dsr_mtd_sha256", ""))
        if not semantic or len(sha) != 64:
            raise SystemExit("missing exact semantic/raw-MTD identity")
        old = seen_semantic.get(semantic)
        if old is not None and old != sha:
            raise SystemExit(
                f"ambiguous semantic active: {semantic}"
            )
        seen_semantic[semantic] = sha
        if len(row.get("c100_f32_bits", [])) != 3:
            raise SystemExit("invalid c100 width")
        if len(row.get("c101_f32_bits", [])) != 3:
            raise SystemExit("invalid c101 width")
        if not str(row.get("c102_f32_bits", "")).startswith("0x"):
            raise SystemExit("invalid c102 bits")

    variants = [
        x for x in deferred
        if x.get("reason") ==
        "SPC_BOTH_FAMILY_VARIANT_REQUIRES_RECEIVER_FAMILY_PROOF"
    ]
    ambiguous = [
        x for x in deferred
        if x.get("reason") ==
        "AMBIGUOUS_SEMANTIC_NAME_HAS_MULTIPLE_DSR_RAW_MTD_SHA"
    ]
    if len(variants) != 31:
        raise SystemExit(f"family-variant deferred drift: {len(variants)}")
    if len(ambiguous) != 2:
        raise SystemExit(f"ambiguous deferred drift: {len(ambiguous)}")
    ambiguous_semantics = {
        str(x.get("semantic_name_hash_fnv1a_utf8", ""))
        for x in ambiguous
    }
    if any(x in seen_semantic for x in ambiguous_semantics):
        raise SystemExit(
            "ambiguous semantic leaked into active PointLight authority"
        )

    policy = authority.get("policy", {})
    forbidden = set(policy.get("explicitly_not_authorized", []))
    required_forbidden = {
        "PTDE Diffuse replacement outside equipment",
        "PTDE Normal replacement outside equipment",
        "PTDE SpecRGB replacement outside equipment",
        "receiver-family widening",
        "V4/V5 PointLight amplitude or range heuristics",
    }
    if not required_forbidden.issubset(forbidden):
        raise SystemExit("scope guard drift")

    print(
        "POINTLIGHT_MATERIAL_AUTHORITY_PASS "
        f"active={len(active)} spc={len(spc)} nospc={len(nospc)} "
        f"rgb_c101={len(non_scalar)} deferred_variant={len(variants)} "
        f"deferred_ambiguous={len(ambiguous)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
