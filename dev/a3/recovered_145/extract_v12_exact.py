#!/usr/bin/env python3
from __future__ import annotations
import argparse, base64, hashlib, json, zlib
from pathlib import Path

PREFIX="asset_integrated_v12.c.gz.b64.part"
EXPECTED_SOURCE_SHA256="879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--parts-dir",type=Path,default=Path(__file__).with_name("v12_exact"))
    ap.add_argument("--out",type=Path,required=True)
    ap.add_argument("--manifest",type=Path)
    ap.add_argument("--allow-partial",action="store_true")
    a=ap.parse_args()

    parts=sorted(a.parts_dir.glob(PREFIX+"*"))
    expected=[a.parts_dir/f"{PREFIX}{i:02d}" for i in range(len(parts))]
    if not parts or parts!=expected:
        raise SystemExit("missing/non-contiguous V12 source parts")

    joined="".join(p.read_text(encoding="utf-8").strip() for p in parts)
    packed=base64.b64decode(joined,validate=True)

    dz=zlib.decompressobj(16+zlib.MAX_WBITS)
    src=dz.decompress(packed)
    complete=bool(dz.eof)
    if complete:
        src+=dz.flush()

    got=hashlib.sha256(src).hexdigest()
    if complete and got!=EXPECTED_SOURCE_SHA256:
        raise SystemExit(f"V12 source SHA mismatch: {got}")
    if not complete and not a.allow_partial:
        raise SystemExit("V12 gzip stream incomplete; add remaining parts or use --allow-partial for inspection only")

    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_bytes(src)

    manifest={
      "schema":"dsrrl.a3.recovered_145.v12_exact.v2",
      "complete":complete,
      "expected_source_sha256":EXPECTED_SOURCE_SHA256,
      "source_sha256":got,
      "source_size":len(src),
      "gzip_prefix_sha256":hashlib.sha256(packed).hexdigest(),
      "parts":[{"name":p.name,"size":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in parts]
    }
    if a.manifest:
        a.manifest.parent.mkdir(parents=True,exist_ok=True)
        a.manifest.write_text(json.dumps(manifest,indent=2)+"\n")
    print(json.dumps(manifest,indent=2))
    if not complete:
        print("WARNING: PARTIAL V12 SOURCE — inspection only; never use for a build")

if __name__=="__main__":
    main()
