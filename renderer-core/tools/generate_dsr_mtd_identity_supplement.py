#!/usr/bin/env python3
"""Generate evidence-certified exact DSR semantic-MTD -> raw-SHA supplement.

This does not make the generic MTD identity registry source-complete. It only
materializes explicitly certified special-route identities. Duplicate semantic
hashes with different SHA-256 values are rejected.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def fnv1a64(text: str) -> int:
    h = 14695981039346656037
    for b in text.encode("utf-8"):
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def digest_array(hex_digest: str) -> str:
    raw = bytes.fromhex(hex_digest)
    if len(raw) != 32:
        raise ValueError("SHA-256 must be 32 bytes")
    return "{{" + ",".join(f"0x{x:02x}u" for x in raw) + "}}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()

    payload = json.loads(args.input.read_text(encoding="utf-8"))
    rows: dict[int, tuple[str, str]] = {}

    for record in payload.get("records", []):
        if record.get("status") != "CONFIRMED":
            continue
        name = str(record["semantic_name"])
        sha = str(record["raw_mtd_sha256"]).lower()
        bytes.fromhex(sha)
        if len(sha) != 64:
            raise SystemExit(f"invalid SHA-256 for {name}")
        h = fnv1a64(name)
        previous = rows.get(h)
        if previous is not None and previous[1] != sha:
            raise SystemExit(
                f"ambiguous certified semantic hash 0x{h:016x}: "
                f"{previous[0]} vs {name}"
            )
        rows[h] = (name, sha)

    ordered = sorted(rows.items())
    out = [
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstddef>\n",
        "#include <cstdint>\n\n",
        "namespace dsrrl::operators::material_response::generated {\n",
        "struct dsr_mtd_identity_supplement_record { "
        "std::uint64_t semantic_name_hash; "
        "std::array<std::uint8_t,32> raw_mtd_sha256; };\n",
        "inline constexpr bool k_dsr_mtd_identity_supplement_source_complete=false;\n",
        f"inline constexpr std::array<dsr_mtd_identity_supplement_record,{len(ordered)}u> "
        "k_dsr_mtd_identity_supplement={{\n",
    ]
    for h, (_, sha) in ordered:
        out.append(
            f"{{0x{h:016x}ull,{digest_array(sha)}}},\n"
        )
    out += [
        "}};\n",
        "constexpr bool dsr_mtd_identity_supplement_resolve("
        "std::uint64_t h,std::array<std::uint8_t,32>&out) noexcept{"
        "out={};"
        "std::size_t lo=0,hi=k_dsr_mtd_identity_supplement.size();"
        "while(lo<hi){auto m=lo+(hi-lo)/2u;"
        "if(k_dsr_mtd_identity_supplement[m].semantic_name_hash<h)lo=m+1u;else hi=m;}"
        "if(lo>=k_dsr_mtd_identity_supplement.size())return false;"
        "const auto&r=k_dsr_mtd_identity_supplement[lo];"
        "if(r.semantic_name_hash!=h)return false;"
        "out=r.raw_mtd_sha256;return true;}\n",
        "} // namespace dsrrl::operators::material_response::generated\n",
    ]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("".join(out), encoding="utf-8", newline="\n")
    print(
        "DSR_MTD_IDENTITY_SUPPLEMENT_PASS "
        f"records={len(ordered)} source_complete=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
