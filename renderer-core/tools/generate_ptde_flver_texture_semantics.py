#!/usr/bin/env python3
"""Generate positive-only PTDE FLVER texture semantic runtime data.

Input is the owner-side canonical TSV emitted by ptde_full_flver_ownership_export.py.
The generator deliberately makes no negative semantic claims. If the scan is not
source-complete, observed semantic presence may still be emitted as USE evidence,
but every absent bit remains UNKNOWN and --require-source-complete refuses output.
Unknown non-empty texture semantics are also rejected: silently dropping one from
positive_mask would turn observed evidence into an unrepresented runtime carrier.
The TSV and error log must match the hashes recorded by the scan summary so files
from different owner runs can never be combined into false provenance/completeness.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

BITS = {
    "g_Diffuse": 1 << 0,
    "g_Bumpmap": 1 << 1,
    "g_DetailBumpmap": 1 << 2,
    "g_Specular": 1 << 3,
    "g_Lightmap": 1 << 4,
    "g_Diffuse_2": 1 << 5,
    "g_Bumpmap_2": 1 << 6,
    "g_Specular_2": 1 << 7,
}

REQ = {"game", "flver_identity", "material_slot", "mtd_name", "mtd_sha256", "texture_semantic"}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(8 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fnv1a_utf8(text: str) -> int:
    h = 14695981039346656037
    for b in text.encode("utf-8"):
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def fnv1a_utf16_lower(text: str) -> int:
    h = 14695981039346656037
    for ch in text.lower():
        u = ord(ch)
        for b in (u & 0xFF, (u >> 8) & 0xFF):
            h ^= b
            h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def load_summary(path: Path) -> dict:
    obj = json.loads(path.read_text(encoding="utf-8"))
    if obj.get("game") != "PTDE":
        raise SystemExit("summary is not PTDE")
    return obj


def require_scan_member(summary: dict, path: Path, actual_sha: str, output_key: str) -> None:
    outputs = summary.get("outputs")
    hashes = summary.get("sha256")
    if not isinstance(outputs, dict) or not isinstance(hashes, dict):
        raise SystemExit("scan summary lacks output provenance")
    recorded_name = outputs.get(output_key)
    if not isinstance(recorded_name, str) or not recorded_name:
        raise SystemExit(f"scan summary lacks output name for {output_key}")
    expected_sha = hashes.get(recorded_name)
    if not isinstance(expected_sha, str) or len(expected_sha) != 64:
        raise SystemExit(f"scan summary lacks SHA-256 for {recorded_name}")
    if path.name != recorded_name:
        raise SystemExit(
            f"scan provenance filename mismatch for {output_key}: "
            f"{path.name!r} != {recorded_name!r}"
        )
    if actual_sha.lower() != expected_sha.lower():
        raise SystemExit(
            f"scan provenance SHA mismatch for {recorded_name}: "
            f"{actual_sha.lower()} != {expected_sha.lower()}"
        )


def load_error_count(path: Path) -> int:
    return sum(1 for line in path.read_text(encoding="utf-8").splitlines() if line.strip())


def load_records(path: Path) -> tuple[list[dict], int]:
    slot_semantics: dict[tuple[str, str, str, int], set[str]] = defaultdict(set)
    with path.open(encoding="utf-8", newline="") as f:
        r = csv.DictReader(f, delimiter="\t")
        if not r.fieldnames or not REQ.issubset(r.fieldnames):
            raise SystemExit(f"input TSV missing columns: {sorted(REQ - set(r.fieldnames or []))}")
        for row in r:
            if row["game"] != "PTDE":
                raise SystemExit(f"non-PTDE row: {row['game']!r}")
            name = row["mtd_name"].strip()
            sha = row["mtd_sha256"].strip().lower()
            if not name or len(sha) != 64 or any(c not in "0123456789abcdef" for c in sha):
                raise SystemExit("invalid exact MTD identity")
            try:
                slot = int(row["material_slot"])
            except ValueError as e:
                raise SystemExit("invalid material_slot") from e
            semantic = row["texture_semantic"].strip()
            if semantic and semantic not in BITS:
                raise SystemExit(
                    f"unknown PTDE texture_semantic {semantic!r} for "
                    f"{name} {sha} slot={slot}; extend the semantic ABI before import"
                )
            slot_semantics[(name, sha, row["flver_identity"], slot)].add(semantic)

    identity_signatures: dict[tuple[str, str], set[tuple[str, ...]]] = defaultdict(set)
    observed_slots: dict[tuple[str, str], int] = defaultdict(int)
    for (name, sha, _flver, _slot), semantics in slot_semantics.items():
        identity_signatures[(name, sha)].add(tuple(sorted(x for x in semantics if x)))
        observed_slots[(name, sha)] += 1

    by_name: dict[str, set[str]] = defaultdict(set)
    records = []
    for (name, sha), signatures in identity_signatures.items():
        if len(signatures) != 1:
            raise SystemExit(f"semantic signature varies for exact PTDE MTD: {name} {sha}")
        by_name[name].add(sha)
        semantics = set(next(iter(signatures)))
        mask = 0
        for semantic, bit in BITS.items():
            if semantic in semantics:
                mask |= bit
        records.append({
            "mtd_name": name,
            "ptde_mtd_sha256": sha,
            "semantic_name_hash": fnv1a_utf8(name),
            "legacy_name_hash_utf16_lower": fnv1a_utf16_lower(name),
            "positive_mask": mask,
            "observed_material_slots": observed_slots[(name, sha)],
            "semantics": sorted(semantics),
        })

    aliases = {name: shas for name, shas in by_name.items() if len(shas) != 1}
    if aliases:
        raise SystemExit(f"exact MTD name has multiple PTDE hashes: {aliases}")

    semantic_hashes: dict[int, str] = {}
    legacy_hashes: dict[int, str] = {}
    for rec in records:
        for key, seen in (
            ("semantic_name_hash", semantic_hashes),
            ("legacy_name_hash_utf16_lower", legacy_hashes),
        ):
            hv = rec[key]
            previous = seen.get(hv)
            if previous is not None and previous != rec["mtd_name"]:
                raise SystemExit(f"{key} collision: {previous!r} vs {rec['mtd_name']!r}")
            seen[hv] = rec["mtd_name"]

    records.sort(key=lambda x: (x["mtd_name"].casefold(), x["ptde_mtd_sha256"]))
    return records, len(slot_semantics)


def render_header(records: list[dict], tsv_sha: str, summary_sha: str, errors_sha: str,
                  error_count: int, source_complete: bool, owner_zip_sha: str) -> str:
    rows = []
    for r in records:
        name = r["mtd_name"].replace("\\", "\\\\").replace('"', '\\"')
        rows.append(
            f'    {{"{name}", 0x{r["semantic_name_hash"]:016x}ull, '
            f'0x{r["legacy_name_hash_utf16_lower"]:016x}ull, {r["positive_mask"]}u}}'
        )
    body = ",\n".join(rows)
    return f"""#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {{

// Positive PTDE texture-capability evidence recovered from owner FLVER slots.
// Missing bits are UNKNOWN, never NO_USE.
enum ptde_texture_semantic_bit : std::uint8_t {{
    ptde_tex_diffuse       = 1u << 0u,
    ptde_tex_bump          = 1u << 1u,
    ptde_tex_detail_bump   = 1u << 2u,
    ptde_tex_specular      = 1u << 3u,
    ptde_tex_lightmap      = 1u << 4u,
    ptde_tex_diffuse_2     = 1u << 5u,
    ptde_tex_bump_2        = 1u << 6u,
    ptde_tex_specular_2    = 1u << 7u,
}};

struct ptde_flver_texture_semantic_record {{
    const char *mtd_name;
    std::uint64_t semantic_name_hash;
    std::uint64_t legacy_name_hash_utf16_lower;
    std::uint8_t positive_mask;
}};

inline constexpr char k_ptde_flver_texture_semantics_owner_zip_sha256[] =
    "{owner_zip_sha}";
inline constexpr char k_ptde_flver_texture_semantics_input_tsv_sha256[] =
    "{tsv_sha}";
inline constexpr char k_ptde_flver_texture_semantics_scan_summary_sha256[] =
    "{summary_sha}";
inline constexpr char k_ptde_flver_texture_semantics_scan_errors_sha256[] =
    "{errors_sha}";
inline constexpr std::size_t k_ptde_flver_texture_semantics_scan_error_count = {error_count}u;
inline constexpr bool k_ptde_flver_texture_semantics_source_complete = {"true" if source_complete else "false"};
inline constexpr std::size_t k_ptde_flver_texture_semantics_record_count = {len(records)}u;

inline constexpr std::array<ptde_flver_texture_semantic_record,
    k_ptde_flver_texture_semantics_record_count>
k_ptde_flver_texture_semantics_v1 = {{{{
{body}
}}}};

}} // namespace dsrrl::operators::material_response::generated
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input_tsv", type=Path)
    ap.add_argument("--scan-summary", type=Path, required=True)
    ap.add_argument("--scan-errors", type=Path, required=True)
    ap.add_argument("--out-header", type=Path, required=True)
    ap.add_argument("--out-json", type=Path)
    ap.add_argument("--owner-zip-sha256", default="")
    ap.add_argument("--expect-input-sha256")
    ap.add_argument("--require-source-complete", action="store_true")
    args = ap.parse_args()

    tsv_sha = sha256_file(args.input_tsv)
    if args.expect_input_sha256 and tsv_sha != args.expect_input_sha256.lower():
        raise SystemExit(f"input SHA mismatch: {tsv_sha}")

    summary = load_summary(args.scan_summary)
    errors_sha = sha256_file(args.scan_errors)
    require_scan_member(summary, args.input_tsv, tsv_sha, "canonical_tsv")
    require_scan_member(summary, args.scan_errors, errors_sha, "errors_jsonl")
    error_count = load_error_count(args.scan_errors)
    unresolved = int(summary.get("unresolved_rows", 0))
    source_complete = error_count == 0 and unresolved == 0
    if args.require_source_complete and not source_complete:
        raise SystemExit(
            f"source-incomplete scan: errors={error_count} unresolved={unresolved}"
        )

    records, slot_count = load_records(args.input_tsv)
    summary_sha = sha256_file(args.scan_summary)
    header = render_header(
        records, tsv_sha, summary_sha, errors_sha, error_count,
        source_complete, args.owner_zip_sha256.lower(),
    )
    args.out_header.parent.mkdir(parents=True, exist_ok=True)
    args.out_header.write_text(header, encoding="utf-8", newline="\n")

    if args.out_json:
        payload = {
            "schema": 1,
            "name": "PTDE FLVER positive texture semantics V1",
            "coverage_state": "SOURCE_COMPLETE" if source_complete else "PARTIAL_SOURCE_COVERAGE",
            "positive_use_only": True,
            "absence_is_unknown": True,
            "source": {
                "owner_zip_sha256": args.owner_zip_sha256.lower(),
                "input_tsv_sha256": tsv_sha,
                "scan_summary_sha256": summary_sha,
                "scan_errors_sha256": errors_sha,
                "scan_error_count": error_count,
                "unresolved_rows": unresolved,
            },
            "counts": {
                "records": len(records),
                "observed_material_slots": slot_count,
            },
            "bits": BITS,
            "records": records,
        }
        args.out_json.parent.mkdir(parents=True, exist_ok=True)
        args.out_json.write_text(
            json.dumps(payload, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

    print(
        f"records={len(records)} slots={slot_count} "
        f"errors={error_count} unresolved={unresolved} "
        f"source_complete={int(source_complete)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
