#!/usr/bin/env python3
"""Materialize exact DSR FLVER/material owner tuples.

Canonical input is DSR_FLVER_CENSUS_COMPACT.zip. The input is content-addressed
and must reproduce the already-audited full DSR corpus before a source-complete
header can be emitted:

  ZIP SHA-256      a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891
  unique FLVERs    4161
  material slots   19985
  MTD basenames    367

The output tuple is exact and collision-free at the FLVER component:
(raw FLVER SHA-256, material slot, semantic MTD-name hash).

This tool deliberately does not infer runtime identity. A source-complete static
tuple corpus is only one half of the authorization contract; the live DSR
selector must transport the same exact FLVER SHA/material slot before positive
Diffuse/Normal routing can be armed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path
from typing import Iterable

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_flver_pairwise_material_census as pairwise  # noqa: E402

CANONICAL_DSR_ZIP_SHA256 = (
    "a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891"
)
EXPECTED_SOURCE_FLVER = 4161
EXPECTED_OWNER_FLVER = 3944
EXPECTED_MATERIAL_SLOTS = 19985
EXPECTED_MTD_BASENAMES = 367


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(8 << 20)
            if not chunk:
                return h.hexdigest()
            h.update(chunk)


def _verify_manifest_hashes(zip_path: Path) -> None:
    """Verify every materialized FLVER member against manifest sha256_full."""
    with zipfile.ZipFile(zip_path) as z:
        try:
            manifest = json.loads(z.read("manifest.json"))
        except KeyError as exc:
            raise ValueError("DSR compact census is missing manifest.json") from exc
        names = set(z.namelist())
        seen: set[str] = set()
        for index, row in enumerate(manifest):
            member = str(row.get("output", ""))
            # Compact census members are *.flver.struct payloads. Their bytes
            # are authenticated by sha256_struct; sha256_full identifies the
            # original complete FLVER and is intentionally used by owner tuples.
            expected = str(
                row.get("sha256_struct") or row.get("sha256_full", "")
            ).lower()
            if not member or member in seen:
                continue
            seen.add(member)
            if member not in names:
                raise ValueError(
                    f"manifest row {index} references missing member {member!r}"
                )
            if len(expected) != 64:
                raise ValueError(
                    f"manifest row {index} has invalid member SHA-256 {expected!r}"
                )
            try:
                int(expected, 16)
            except ValueError as exc:
                raise ValueError(
                    f"manifest row {index} has non-hex member SHA-256 {expected!r}"
                ) from exc
            actual = hashlib.sha256(z.read(member)).hexdigest()
            if actual != expected:
                raise ValueError(
                    f"manifest/member SHA mismatch for {member}: "
                    f"expected {expected}, got {actual}"
                )


def materialize_rows(
    zip_path: Path,
    *,
    require_canonical: bool = True,
) -> tuple[list[dict], dict]:
    source_sha = sha256_file(zip_path)
    _verify_manifest_hashes(zip_path)
    if require_canonical and source_sha != CANONICAL_DSR_ZIP_SHA256:
        raise ValueError(
            "DSR compact census SHA-256 mismatch: "
            f"expected {CANONICAL_DSR_ZIP_SHA256}, got {source_sha}"
        )

    materials = pairwise.dsr_materials(zip_path)
    rows = []
    mtd_names = set()
    flver_shas = set()
    for (flver_sha, material_slot), (mtd_name, _present, _resource) in materials.items():
        if len(flver_sha) != 64:
            raise ValueError(f"invalid FLVER SHA-256: {flver_sha!r}")
        int(flver_sha, 16)
        if material_slot < 0:
            raise ValueError(f"negative material slot: {material_slot}")
        if not mtd_name:
            raise ValueError("empty MTD basename in exact owner tuple")
        flver_shas.add(flver_sha)
        mtd_names.add(mtd_name.casefold())
        rows.append({
            "flver_sha256": flver_sha.lower(),
            "material_slot": int(material_slot),
            "mtd_name": mtd_name,
            "semantic_name_hash": f"0x{pairwise.fnv(mtd_name):016x}",
        })

    rows.sort(key=lambda r: (
        r["flver_sha256"],
        r["material_slot"],
        int(r["semantic_name_hash"], 16),
    ))

    # Exact tuple uniqueness is mandatory. Any duplicate means the source layer
    # failed to define one unambiguous owner identity.
    keys = {
        (r["flver_sha256"], r["material_slot"], r["semantic_name_hash"])
        for r in rows
    }
    if len(keys) != len(rows):
        raise ValueError("duplicate exact FLVER/material/MTD owner tuple")

    summary = {
        "schema": "dsrrl.dsr_flver_owner_tuple_corpus.v1",
        "source_zip_sha256": source_sha,
        "source_complete": False,
        "tuple_count": len(rows),
        "unique_flver_sha256": len(flver_shas),
        "unique_mtd_basenames": len(mtd_names),
        "identity": ["raw_flver_sha256", "material_slot", "semantic_mtd_name_hash"],
        "runtime_identity_transport": "OPEN",
        "positive_runtime_activation": "HOLD_OFF",
        "pixel_equivalence": "OPEN",
    }

    # 4161 is the complete source FLVER population, while only 3944 FLVERs
    # own one or more material slots. The owner tuple table must therefore
    # contain 3944 unique FLVER SHA-256 values, not all 4161 source FLVERs.
    # Source completeness is independently attested by the canonical ZIP hash
    # and manifest/member integrity above.
    canonical_counts = (
        summary["tuple_count"] == EXPECTED_MATERIAL_SLOTS
        and summary["unique_flver_sha256"] == EXPECTED_OWNER_FLVER
        and summary["unique_mtd_basenames"] == EXPECTED_MTD_BASENAMES
    )
    summary["source_flver_count"] = EXPECTED_SOURCE_FLVER
    if require_canonical and not canonical_counts:
        raise ValueError(
            "source-completeness invariant mismatch: "
            f"tuples={summary['tuple_count']} "
            f"owner_flver={summary['unique_flver_sha256']} "
            f"mtd={summary['unique_mtd_basenames']}"
        )
    summary["source_complete"] = bool(
        source_sha == CANONICAL_DSR_ZIP_SHA256 and canonical_counts
    )
    return rows, summary


def _digest_initializer(hex_digest: str) -> str:
    b = bytes.fromhex(hex_digest)
    return "{{" + ",".join(f"0x{x:02x}u" for x in b) + "}}"


def render_header(rows: Iterable[dict], summary: dict) -> str:
    rows = list(rows)
    complete = bool(summary.get("source_complete"))
    out = [
        "#pragma once\n",
        "#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::operators::material_response::generated {\n",
        "struct flver_owner_tuple_record {\n",
        "    std::array<std::uint8_t,32> flver_sha256;\n",
        "    std::uint32_t material_slot;\n",
        "    std::uint64_t semantic_name_hash;\n",
        "};\n",
        f'inline constexpr char k_dsr_flver_owner_tuple_source_sha256[]="{summary["source_zip_sha256"]}";\n',
        f"inline constexpr bool k_dsr_flver_owner_tuple_source_complete={'true' if complete else 'false'};\n",
        f"inline constexpr std::size_t k_dsr_flver_owner_tuple_count={len(rows)}u;\n",
        "inline constexpr std::array<flver_owner_tuple_record,k_dsr_flver_owner_tuple_count> "
        "k_dsr_flver_owner_tuples = {{\n",
    ]
    for row in rows:
        out.append(
            "    {"
            + _digest_initializer(row["flver_sha256"])
            + f',{row["material_slot"]}u,{row["semantic_name_hash"]}ull'
            + "},\n"
        )
    out.extend([
        "}};\n",
        "constexpr int compare_digest(const std::array<std::uint8_t,32>&a,"
        "const std::array<std::uint8_t,32>&b) noexcept{"
        "for(std::size_t i=0;i<a.size();++i){if(a[i]<b[i])return -1;if(a[i]>b[i])return 1;}return 0;}\n",
        "constexpr bool dsr_flver_owner_tuple_authenticated("
        "const std::array<std::uint8_t,32>&sha,std::uint32_t slot,std::uint64_t mtd) noexcept{"
        "if(!k_dsr_flver_owner_tuple_source_complete||mtd==0u)return false;"
        "std::size_t lo=0,hi=k_dsr_flver_owner_tuples.size();"
        "while(lo<hi){const auto mid=lo+(hi-lo)/2u;const auto&r=k_dsr_flver_owner_tuples[mid];"
        "const int dc=compare_digest(r.flver_sha256,sha);"
        "if(dc<0||(dc==0&&(r.material_slot<slot||(r.material_slot==slot&&r.semantic_name_hash<mtd))))lo=mid+1u;"
        "else hi=mid;}"
        "if(lo>=k_dsr_flver_owner_tuples.size())return false;"
        "const auto&r=k_dsr_flver_owner_tuples[lo];"
        "return compare_digest(r.flver_sha256,sha)==0&&r.material_slot==slot&&r.semantic_name_hash==mtd;}\n",
        "} // namespace dsrrl::operators::material_response::generated\n",
    ])
    return "".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("dsr_compact_zip", type=Path)
    ap.add_argument("--jsonl", type=Path, required=True)
    ap.add_argument("--manifest", type=Path, required=True)
    ap.add_argument("--header", type=Path, required=True)
    args = ap.parse_args()

    rows, summary = materialize_rows(args.dsr_compact_zip, require_canonical=True)
    if not summary["source_complete"]:
        raise SystemExit("refusing to emit runtime header from incomplete owner corpus")

    for path in (args.jsonl, args.manifest, args.header):
        path.parent.mkdir(parents=True, exist_ok=True)

    with args.jsonl.open("w", encoding="utf-8", newline="\n") as f:
        for row in rows:
            f.write(json.dumps(row, sort_keys=True, separators=(",", ":")) + "\n")
    args.manifest.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    args.header.write_text(render_header(rows, summary), encoding="utf-8", newline="\n")
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
