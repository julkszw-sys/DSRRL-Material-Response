#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

EXPECTED_MASK_COUNTS = {8: 72, 39: 12, 193: 24, 256: 36}
EXPECTED_PLANS = 144
EXPECTED_ALIASES = 252

def q(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    doc = json.loads(Path(args.input).read_text(encoding="utf-8"))
    plans = doc["plans"]

    if len(plans) != EXPECTED_PLANS:
        raise SystemExit(f"expected {EXPECTED_PLANS} plans, got {len(plans)}")

    aliases = sum(len(p["aliases"]) for p in plans)
    if aliases != EXPECTED_ALIASES:
        raise SystemExit(f"expected {EXPECTED_ALIASES} aliases, got {aliases}")

    counts = {}
    seen = set()
    for p in plans:
        key = (p["original_sha256"], p["plan_mask"])
        if key in seen:
            raise SystemExit(f"duplicate identity {key}")
        seen.add(key)
        counts[p["plan_mask"]] = counts.get(p["plan_mask"], 0) + 1
        if len(p["original_sha256"]) != 64 or len(p["replacement_sha256"]) != 64:
            raise SystemExit("invalid SHA-256 length")
        if not p.get("owners"):
            raise SystemExit(f"missing island owners for {p['representative']}")

    if counts != EXPECTED_MASK_COUNTS:
        raise SystemExit(f"mask counts mismatch: {counts}")

    lines = [
        "#pragma once\n\n",
        "#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::operators::legacy_plan::generated {\n\n",
        "struct a1_plan_identity {\n",
        "    const char *original_sha256;\n",
        "    const char *replacement_sha256;\n",
        "    std::uint32_t code_size;\n",
        "    std::uint32_t legacy_mask;\n",
        "    const char *representative;\n",
        "    std::uint16_t alias_count;\n",
        "};\n\n",
        "inline constexpr a1_plan_identity k_a1_plan_index_v1[] = {\n",
    ]
    for p in plans:
        lines.append(
            "    {" +
            ",".join([
                q(p["original_sha256"]),
                q(p["replacement_sha256"]),
                f"{int(p['code_size'])}u",
                f"{int(p['plan_mask'])}u",
                q(p["representative"]),
                f"{len(p['aliases'])}u",
            ]) +
            "},\n"
        )
    lines += [
        "};\n\n",
        "inline constexpr std::size_t k_a1_plan_count_v1 =\n",
        "    sizeof(k_a1_plan_index_v1) / sizeof(k_a1_plan_index_v1[0]);\n\n",
        "} // namespace dsrrl::operators::legacy_plan::generated\n",
    ]
    Path(args.output).write_text("".join(lines), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
