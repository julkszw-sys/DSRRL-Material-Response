#!/usr/bin/env python3
"""Generate Renderer 2.0 exact negative MTD/SPX gate.

Input: PTDE_DSR_MTD_SPX_PAIRWISE_V1.json from the canonical
DSRRL_PTDE_DSR_MTD_SPX_PAIRWISE_V1.zip artifact.

Only shared exact identities where DSR ADDS Specular and/or Bump relative to
the PTDE SPX contract are emitted. This is negative routing evidence only.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

EXPECTED_JSON_SHA256="4ef47a68ca3ba6dab009f49f543711702fd971cd5a2e141d3c0b673272742cf4"
EXPECTED_ARTIFACT_SHA256="2f102565a9fd35575bdaccaec5905d5407791f016f2cd4bf94730e86d33a43df"
EXPECTED_PTDE_SOURCE_ZIP_SHA256="2afb7a5947d1249028842d53ebb6c0fc1087f1550f1d406eb8054e89f8edafef"
EXPECTED_DSR_SOURCE_ZIP_SHA256="0ef7886e1122f2ded5008fdf0166381e5cd2b7a3961b3564e02ad46ce533ceb0"

def fnv1a_utf8(text):
    h=14695981039346656037
    for b in text.encode("utf-8"):
        h^=b
        h=(h*1099511628211)&0xffffffffffffffff
    return h

def generate(raw):
    actual=hashlib.sha256(raw).hexdigest()
    if actual!=EXPECTED_JSON_SHA256:
        raise SystemExit(f"input SHA-256 mismatch: expected {EXPECTED_JSON_SHA256}, got {actual}")
    doc=json.loads(raw)
    if doc["sources"]["ptde_mtd_extract_zip_sha256"]!=EXPECTED_PTDE_SOURCE_ZIP_SHA256:
        raise SystemExit("PTDE source ZIP SHA mismatch")
    if doc["sources"]["dsr_mtd_zip_sha256"]!=EXPECTED_DSR_SOURCE_ZIP_SHA256:
        raise SystemExit("DSR source ZIP SHA mismatch")
    if doc["summary"]["shared_names"]!=435 or doc["summary"]["dsr_feature_superset"]!=135:
        raise SystemExit("pairwise census summary mismatch")
    rows=[r for r in doc["records"] if r.get("ptde_sha256") and r.get("dsr_sha256") and (int(r["added_mask"])&0x06)]
    if len(rows)!=118:
        raise SystemExit(f"negative row count mismatch: {len(rows)}")
    out=[
        "#pragma once\n#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::operators::material_response::generated {\n",
        "inline constexpr std::uint8_t mtd_spx_feature_specular=2u;\n",
        "inline constexpr std::uint8_t mtd_spx_feature_bump=4u;\n",
        "struct mtd_spx_negative_record { std::uint64_t semantic_name_hash; const char *raw_dsr_mtd_sha256; std::uint8_t dsr_only_added_mask; };\n",
        f'inline constexpr char k_mtd_spx_pairwise_artifact_sha256[]="{EXPECTED_ARTIFACT_SHA256}";\n',
        f'inline constexpr char k_mtd_spx_pairwise_json_sha256[]="{EXPECTED_JSON_SHA256}";\n',
        f'inline constexpr char k_mtd_spx_pairwise_ptde_source_zip_sha256[]="{EXPECTED_PTDE_SOURCE_ZIP_SHA256}";\n',
        f'inline constexpr char k_mtd_spx_pairwise_dsr_source_zip_sha256[]="{EXPECTED_DSR_SOURCE_ZIP_SHA256}";\n',
        f"inline constexpr std::size_t k_mtd_spx_negative_record_count={len(rows)}u;\n",
        "inline constexpr std::array<mtd_spx_negative_record,k_mtd_spx_negative_record_count> k_mtd_spx_negative_v1 = {{\n",
    ]
    for r in rows:
        mask=int(r["added_mask"])&0x06
        out.append(f'    {{0x{fnv1a_utf8(r["mtd_name"]):016x}ull,"{r["dsr_sha256"]}",{mask}u}}, // {r["mtd_name"]}\n')
    out.append("}};\n} // namespace dsrrl::operators::material_response::generated\n")
    return "".join(out)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("input_json",type=Path)
    ap.add_argument("output_header",type=Path)
    args=ap.parse_args()
    text=generate(args.input_json.read_bytes())
    args.output_header.parent.mkdir(parents=True,exist_ok=True)
    args.output_header.write_text(text,encoding="utf-8",newline="\n")
    print(f"generated {args.output_header}")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
