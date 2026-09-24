#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

EXPECTED_PLANS = 144
EXPECTED_OPS = 312
EXPECTED_OWNER_OPS = {
    "terminal_sat_rgb": 144,
    "diffuse_material_domain": 108,
    "pointlight_pnts_attenuation": 24,
    "envspec_nospc_delete": 12,
    "fixed_postfog_identity": 24,
}

def parse_ops(field: str):
    out = []
    for item in field.split(";"):
        off, old, new = (int(v) for v in item.split(","))
        out.append((off, old, new))
    return out

def owner_for_words(old: int, new: int) -> str:
    if (old & 0x00002000) == 0 and new == (old | 0x00002000):
        return "terminal_sat_rgb"
    if old == 0x400CCCCD and new == 0x3F800000:
        return "diffuse_material_domain"
    if (old, new) in {
        (0x07000038, 0x07000033),
        (0x07002038, 0x07002034),
    }:
        return "pointlight_pnts_attenuation"
    if old == 0x07000038 and new == 0x07000031:
        return "envspec_nospc_delete"
    if old == 0x00000002 and new == 0x00000001:
        return "fixed_postfog_identity"
    raise SystemExit(
        f"unmapped recovered DWORD translation: 0x{old:08x} -> 0x{new:08x}"
    )

def q(value: str) -> str:
    return json.dumps(value)

def hx(value: int) -> str:
    return f"0x{value:08x}u"

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--index", required=True)
    ap.add_argument("--part", action="append", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    if len(args.part) != 3:
        raise SystemExit(f"expected 3 recipe parts, got {len(args.part)}")

    rows = []
    for path in args.part:
        for line_no, line in enumerate(
            Path(path).read_text(encoding="utf-8").splitlines(),
            start=1,
        ):
            fields = line.split("\t")
            if len(fields) != 5:
                raise SystemExit(f"{path}:{line_no}: expected 5 TSV fields")
            original_sha, replacement_sha, code_size, mask, op_field = fields
            rows.append({
                "original_sha256": original_sha,
                "replacement_sha256": replacement_sha,
                "code_size": int(code_size),
                "mask": int(mask),
                "ops": parse_ops(op_field),
            })

    index = json.loads(Path(args.index).read_text(encoding="utf-8"))
    plans = index["plans"]

    if len(rows) != EXPECTED_PLANS or len(plans) != EXPECTED_PLANS:
        raise SystemExit(
            f"expected {EXPECTED_PLANS} recipe/index plans, got "
            f"{len(rows)}/{len(plans)}"
        )

    flat_ops = []
    plan_rows = []
    owner_counts = {}

    for pos, (row, plan) in enumerate(zip(rows, plans)):
        checks = {
            "original_sha256": plan["original_sha256"],
            "replacement_sha256": plan["replacement_sha256"],
            "code_size": int(plan["code_size"]),
            "mask": int(plan["plan_mask"]),
        }
        for key, expected in checks.items():
            if row[key] != expected:
                raise SystemExit(
                    f"row {pos}: {key} mismatch: "
                    f"{row[key]!r} != {expected!r}"
                )

        first = len(flat_ops)
        declared_owners = set(plan["owners"])

        for off, old, new in row["ops"]:
            owner = owner_for_words(old, new)
            if owner not in declared_owners:
                raise SystemExit(
                    f"row {pos}: op owner {owner} not declared by identity index"
                )
            owner_counts[owner] = owner_counts.get(owner, 0) + 1
            flat_ops.append({
                "byte_offset": off,
                "old": old,
                "new": new,
                "owner": owner,
            })

        plan_rows.append({
            "original_sha256": row["original_sha256"],
            "replacement_sha256": row["replacement_sha256"],
            "code_size": row["code_size"],
            "legacy_mask": row["mask"],
            "representative": plan["representative"],
            "first_op": first,
            "op_count": len(row["ops"]),
        })

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
            f"    {{{op['byte_offset']}u, {hx(op['old'])}, "
            f"{hx(op['new'])}, core::operator_id::{op['owner']}}},\n"
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
