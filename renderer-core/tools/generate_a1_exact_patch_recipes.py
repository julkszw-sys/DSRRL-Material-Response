#!/usr/bin/env python3
import argparse
import gzip
import hashlib
import json
from pathlib import Path

EXPECTED_SOURCE_SHA256 = "5cc15f8084fb75cb33c27be3f0e94be7fd186f07c16274b33ee09735b747a1ec"
EXPECTED_PLANS = 144
EXPECTED_OPS = 312
EXPECTED_OWNER_OPS = {
    "terminal_sat_rgb": 144,
    "diffuse_material_domain": 108,
    "pointlight_pnts_attenuation": 24,
    "envspec_nospc_delete": 12,
    "fixed_postfog_identity": 24,
}

def owner_for_reason(reason: str) -> str:
    if "terminal RGB SAT" in reason:
        return "terminal_sat_rgb"
    if reason in {
        "DIFFUSE x^2.2 -> x (SPEC stock DSR)",
        "FIXED PHN DIFFUSE material x^2.2 -> x",
    }:
        return "diffuse_material_domain"
    if reason.startswith("PntS "):
        return "pointlight_pnts_attenuation"
    if reason.startswith("no-Spc DSR-only EnvSpec lane OFF"):
        return "envspec_nospc_delete"
    if reason.startswith("FIXED PHN post-Fog root OFF"):
        return "fixed_postfog_identity"
    raise SystemExit(f"unmapped closed-op reason: {reason!r}")

def q(value: str) -> str:
    return json.dumps(value)

def hx(value: int) -> str:
    return f"0x{value:08x}u"

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-gzip", required=True)
    ap.add_argument("--index", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    raw = gzip.decompress(Path(args.source_gzip).read_bytes())
    source_sha = hashlib.sha256(raw).hexdigest()
    if source_sha != EXPECTED_SOURCE_SHA256:
        raise SystemExit(f"source PORT_PLAN SHA mismatch: {source_sha}")

    source = json.loads(raw.decode("utf-8"))
    index = json.loads(Path(args.index).read_text(encoding="utf-8"))
    by_sha = {p["original_sha256"]: p for p in source["plans"]}

    flat_ops = []
    plan_rows = []
    owner_counts = {}

    for ip in index["plans"]:
        sha = ip["original_sha256"]
        sp = by_sha.get(sha)
        if sp is None:
            raise SystemExit(f"selected identity missing from recovered source: {sha}")
        if int(sp["mask"]) != int(ip["plan_mask"]):
            raise SystemExit(f"{sha}: mask mismatch")
        if int(sp["code_size"]) != int(ip["code_size"]):
            raise SystemExit(f"{sha}: code size mismatch")
        if sp["replacement_sha256"] != ip["replacement_sha256"]:
            raise SystemExit(f"{sha}: replacement SHA mismatch")
        if sp["representative"] != ip["representative"]:
            raise SystemExit(f"{sha}: representative mismatch")

        first = len(flat_ops)
        for op in sp["ops"]:
            owner = owner_for_reason(op.get("reason", ""))
            owner_counts[owner] = owner_counts.get(owner, 0) + 1
            flat_ops.append({
                "byte_offset": int(op["byte_offset"]),
                "old": int(op["old"]),
                "new": int(op["new"]),
                "owner": owner,
            })

        plan_rows.append({
            "original_sha256": sha,
            "replacement_sha256": sp["replacement_sha256"],
            "code_size": int(sp["code_size"]),
            "legacy_mask": int(sp["mask"]),
            "representative": sp["representative"],
            "first_op": first,
            "op_count": len(sp["ops"]),
        })

    if len(plan_rows) != EXPECTED_PLANS:
        raise SystemExit(f"expected {EXPECTED_PLANS} plans, got {len(plan_rows)}")
    if len(flat_ops) != EXPECTED_OPS:
        raise SystemExit(f"expected {EXPECTED_OPS} ops, got {len(flat_ops)}")
    if owner_counts != EXPECTED_OWNER_OPS:
        raise SystemExit(f"owner counts mismatch: {owner_counts}")

    out = [
        "#pragma once\n\n",
        '#include "dsrrl/core/types.hpp"\n\n',
        "#include <cstddef>\n",
        "#include <cstdint>\n\n",
        "namespace dsrrl::operators::legacy_plan::generated {\n\n",
        "struct a1_exact_patch_op {\n",
        "    std::uint32_t byte_offset;\n",
        "    std::uint32_t expected_old_word;\n",
        "    std::uint32_t replacement_word;\n",
        "    core::operator_id owner;\n",
        "};\n\n",
        "struct a1_exact_patch_plan {\n",
        "    const char *original_sha256;\n",
        "    const char *replacement_sha256;\n",
        "    std::uint32_t code_size;\n",
        "    std::uint32_t legacy_mask;\n",
        "    const char *representative;\n",
        "    std::uint16_t first_op;\n",
        "    std::uint16_t op_count;\n",
        "};\n\n",
        "inline constexpr a1_exact_patch_op k_a1_exact_patch_ops_v1[] = {\n",
    ]

    for op in flat_ops:
        out.append(
            f"    {{{op['byte_offset']}u, {hx(op['old'])}, {hx(op['new'])}, "
            f"core::operator_id::{op['owner']}}},\n"
        )

    out += [
        "};\n\n",
        "inline constexpr a1_exact_patch_plan k_a1_exact_patch_plans_v1[] = {\n",
    ]

    for p in plan_rows:
        out.append(
            "    {" +
            ", ".join([
                q(p["original_sha256"]),
                q(p["replacement_sha256"]),
                f"{p['code_size']}u",
                f"{p['legacy_mask']}u",
                q(p["representative"]),
                f"{p['first_op']}u",
                f"{p['op_count']}u",
            ]) +
            "},\n"
        )

    out += [
        "};\n\n",
        "inline constexpr std::size_t k_a1_exact_patch_op_count_v1 =\n",
        "    sizeof(k_a1_exact_patch_ops_v1) / sizeof(k_a1_exact_patch_ops_v1[0]);\n",
        "inline constexpr std::size_t k_a1_exact_patch_plan_count_v1 =\n",
        "    sizeof(k_a1_exact_patch_plans_v1) / sizeof(k_a1_exact_patch_plans_v1[0]);\n\n",
        "} // namespace dsrrl::operators::legacy_plan::generated\n",
    ]

    Path(args.output).write_text("".join(out), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
