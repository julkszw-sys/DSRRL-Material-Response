#!/usr/bin/env python3
from __future__ import annotations
import argparse, base64, gzip, hashlib, json
from pathlib import Path

PREFIX="asset_integrated_v12.c.gz.b64.part"
EXPECTED_SOURCE_SHA256="879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--parts-dir",type=Path,default=Path(__file__).with_name("v12_exact"))
    ap.add_argument("--out",type=Path,required=True)
    ap.add_argument("--manifest",type=Path)
    a=ap.parse_args()
    parts=sorted(a.parts_dir.glob(PREFIX+"*"))
    expected=[a.parts_dir/f"{PREFIX}{i:02d}" for i in range(len(parts))]
    if not parts or parts!=expected:
        raise SystemExit("missing/non-contiguous V12 source parts")
    joined="".join(p.read_text(encoding="utf-8").strip() for p in parts)
    packed=base64.b64decode(joined,validate=True)
    src=gzip.decompress(packed)
    got=hashlib.sha256(src).hexdigest()
    if got!=EXPECTED_SOURCE_SHA256:
        raise SystemExit(f"V12 source SHA mismatch: {got}")
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_bytes(src)
    manifest={
      "schema":"dsrrl.a3.recovered_145.v12_exact.v1",
      "source_sha256":got,
      "source_size":len(src),
      "gzip_sha256":hashlib.sha256(packed).hexdigest(),
      "parts":[{"name":p.name,"size":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in parts]
    }
    if a.manifest:
        a.manifest.parent.mkdir(parents=True,exist_ok=True)
        a.manifest.write_text(json.dumps(manifest,indent=2)+"\n")
    print(json.dumps(manifest,indent=2))
if __name__=="__main__":
    main()
