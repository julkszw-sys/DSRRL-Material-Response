#!/usr/bin/env python3
"""Export source-complete DSR FLVER/material ownership from an extracted root.

This is construction evidence only. It reuses the canonical FLVER2/DCX parser
from dsr_flver_zip_ownership_census.py so extracted-root and ZIP scans cannot
drift on binary layout. Exact PTDE homology, runtime activation, and pixel
equivalence remain separate evidence layers.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path

from dsr_flver_zip_ownership_census import (
    DCX_MAGIC,
    FLVER_MAGIC,
    basename_any,
    norm_slashes,
    parse_flver2_materials,
    sha256_bytes,
    unwrap_dcx,
)

SCHEMA=(
    "game","flver_identity","material_slot","mtd_name","mtd_sha256",
    "spx_sha256","receiver_index","consumer_family","material_family",
    "texture_semantic","resource_hash","srv_slot","sampler_slot",
)


def sha256_file(path:Path,chunk:int=8<<20)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        while True:
            block=f.read(chunk)
            if not block:
                return h.hexdigest()
            h.update(block)


def resolve_mtd(
    mtd_index:dict[str,list[tuple[Path,str]]],
    mtd_path:str,
)->tuple[str|None,list[tuple[Path,str]]]:
    """Resolve a basename only when every matching file has one content hash."""
    name=basename_any(mtd_path)
    hits=mtd_index.get(name.casefold(),[])
    by_hash=defaultdict(list)
    for path,digest in hits:
        by_hash[digest].append(path)
    if len(by_hash)!=1:
        return None,hits
    return next(iter(by_hash)),hits


def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("root",type=Path)
    ap.add_argument("--out",type=Path,default=Path("dsr_flver_ownership.tsv"))
    ap.add_argument(
        "--unresolved",
        type=Path,
        default=Path("dsr_flver_ownership_unresolved.tsv"),
    )
    ap.add_argument(
        "--summary",
        type=Path,
        default=Path("dsr_flver_ownership_summary.json"),
    )
    a=ap.parse_args()

    root=a.root.resolve()
    if not root.is_dir():
        ap.error(f"root is not a directory: {root}")

    mtd_index=defaultdict(list)
    scan_errors=[]
    for p in root.rglob("*"):
        if not p.is_file() or p.suffix.casefold()!=".mtd":
            continue
        try:
            digest=sha256_file(p)
        except OSError as exc:
            scan_errors.append((
                p.relative_to(root).as_posix(),
                f"MTD_READ: {exc}",
            ))
            continue
        mtd_index[p.name.casefold()].append((p,digest))

    rows=[]
    unresolved=[]
    counts=Counter()
    versions=Counter()

    for p in root.rglob("*"):
        if not p.is_file() or p.suffix.casefold()==".mtd":
            continue
        rel=p.relative_to(root).as_posix()
        try:
            raw=p.read_bytes()
        except OSError as exc:
            if p.suffix.casefold() in {".flver",".flver2",".dcx"}:
                unresolved.append((rel,f"FILE_READ: {exc}"))
            continue

        try:
            data,layers=unwrap_dcx(raw)
        except Exception as exc:
            if raw.startswith(DCX_MAGIC) or p.suffix.casefold()==".dcx":
                unresolved.append((rel,f"DCX: {exc}"))
            continue
        if not data.startswith(FLVER_MAGIC):
            continue

        counts["flver_files"]+=1
        counts["dcx_layers"]+=layers
        digest=sha256_bytes(data)
        ident=f"DSR|{norm_slashes(rel)}#{digest}"

        try:
            version,materials=parse_flver2_materials(data)
        except Exception as exc:
            unresolved.append((ident,f"FLVER_PARSE: {exc}"))
            continue

        counts["flver_parse_ok"]+=1
        counts["material_instances"]+=len(materials)
        versions[f"0x{version:X}"]+=1

        for material in materials:
            mtd_hash,hits=resolve_mtd(mtd_index,material.mtd_path)
            if mtd_hash is None:
                unresolved.append((
                    ident,
                    (
                        f"slot={material.slot};mtd={material.mtd_path};"
                        f"distinct_hashes={len({h for _,h in hits})};"
                        f"matches={len(hits)}"
                    ),
                ))
                continue

            bindings=material.textures or (None,)
            for binding in bindings:
                semantic="" if binding is None else binding.semantic
                rows.append((
                    "DSR",
                    ident,
                    material.slot,
                    basename_any(material.mtd_path),
                    mtd_hash,
                    "","","","",
                    semantic,
                    "","","",
                ))
                counts["ownership_rows"]+=1
                if binding is not None:
                    counts["texture_bindings"]+=1

    unresolved.extend(scan_errors)
    rows.sort(key=lambda r:(
        r[1],
        int(r[2]),
        str(r[3]).casefold(),
        str(r[9]).casefold(),
    ))
    unresolved.sort(key=lambda r:(r[0],r[1]))

    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.unresolved.parent.mkdir(parents=True,exist_ok=True)
    a.summary.parent.mkdir(parents=True,exist_ok=True)

    with a.out.open("w",newline="",encoding="utf-8") as f:
        w=csv.writer(f,delimiter="\t",lineterminator="\n")
        w.writerow(SCHEMA)
        w.writerows(rows)

    with a.unresolved.open("w",newline="",encoding="utf-8") as f:
        w=csv.writer(f,delimiter="\t",lineterminator="\n")
        w.writerow(("flver_identity","reason"))
        w.writerows(unresolved)

    summary={
        "schema":2,
        "game":"DSR",
        "claim_scope":
            "FLVER_MATERIAL_SLOT_MTD_TEXTURE_SEMANTIC_CONSTRUCTION_EVIDENCE",
        "counts":{
            **{k:int(v) for k,v in sorted(counts.items())},
            "mtd_basenames":len(mtd_index),
            "unresolved_rows":len(unresolved),
        },
        "flver_versions":dict(sorted(versions.items())),
        "source_complete":
            counts["flver_files"]>0 and len(unresolved)==0,
        "policy":{
            "duplicate_mtd_basename":
                "RESOLVE_ONLY_IF_ALL_MATCHES_HAVE_ONE_CONTENT_SHA256",
            "missing_or_ambiguous_mtd":"UNKNOWN_FAIL_OPEN",
            "cross_version_homology":"OPEN",
            "runtime_activation":"OPEN",
            "pixel_equivalence":"OPEN",
        },
    }
    a.summary.write_text(
        json.dumps(summary,indent=2,sort_keys=True)+"\n",
        encoding="utf-8",
    )
    print(json.dumps(summary,indent=2,sort_keys=True))
    return 0 if summary["source_complete"] else 2


if __name__=="__main__":
    raise SystemExit(main())
