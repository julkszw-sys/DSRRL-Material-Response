#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

REQUIRED_IDS = {
    "material_response",
    "upper_lower",
    "hemdir3",
    "spec_rgb",
    "env_spec",
    "envspec_nospc_delete",
    "envspec_pmetal_diagnostic",
    "env_diffuse",
    "point_light",
    "pointlight_pnts_attenuation",
    "local_specular_legacy",
    "subsurface",
    "diffuse",
    "normal",
    "diffuse_material_domain",
    "terminal_sat_rgb",
    "terminal_sat_rgba",
    "fixed_postfog_identity",
    "faceeye_shadow_legacy",
    "post_bloom",
    "post_hdr",
    "dsr_native_sfx",
    "dsr_sfx_inverse_tonemap",
    "pmetal_black_safe_source",
    "pmetal_black_safe_v10",
}

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--stamp", required=True)
    args = p.parse_args()

    doc = json.loads(Path(args.input).read_text(encoding="utf-8"))
    rows = doc["operators"]
    ids = [row["operator_id"] for row in rows]

    if len(ids) != len(set(ids)):
        raise SystemExit("duplicate operator_id in catalog")

    missing = REQUIRED_IDS - set(ids)
    extra = set(ids) - REQUIRED_IDS
    if missing or extra:
        raise SystemExit(f"catalog mismatch missing={sorted(missing)} extra={sorted(extra)}")

    keys = [row["operator_key"] for row in rows]
    if len(keys) != len(set(keys)):
        raise SystemExit("duplicate operator_key in catalog")

    stamp = {
        "schema": 1,
        "operator_count": len(rows),
        "operator_ids": sorted(ids),
        "operator_keys": sorted(keys),
        "architecture_contract": doc.get("architecture_contract", {}).get("operator_key"),
    }
    Path(args.stamp).write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
