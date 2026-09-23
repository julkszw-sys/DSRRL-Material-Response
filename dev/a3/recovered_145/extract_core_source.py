#!/usr/bin/env python3
from __future__ import annotations
import argparse, base64, hashlib, io, json, lzma, tarfile
from pathlib import Path

PREFIX="DSRRL_Material_Response_1.45_CORE_SOURCE.tar.xz.b64.part"
EXPECTED_PARTS=10
EXPECTED_ARCHIVE_SIZE=56232
EXPECTED_ARCHIVE_SHA256="f701f11d32fa3a993f787e8c6abe9a14e33267c2e3d94270e50ce32a7feaf25a"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--parts-dir",type=Path,default=Path(__file__).with_name("core_archive"))
    ap.add_argument("--out-dir",type=Path,required=True)
    ap.add_argument("--manifest",type=Path)
    a=ap.parse_args()

    parts=sorted(a.parts_dir.glob(PREFIX+"*"))
    expected=[a.parts_dir/f"{PREFIX}{i:02d}" for i in range(EXPECTED_PARTS)]
    if parts!=expected:
        raise SystemExit(f"FAIL: expected exact parts 00..09, got {[p.name for p in parts]}")

    encoded="".join(p.read_text(encoding="ascii").strip() for p in parts)
    archive=base64.b64decode(encoded,validate=True)
    if len(archive)!=EXPECTED_ARCHIVE_SIZE:
        raise SystemExit(f"FAIL: archive size {len(archive)} != {EXPECTED_ARCHIVE_SIZE}")
    archive_sha=hashlib.sha256(archive).hexdigest()
    if archive_sha!=EXPECTED_ARCHIVE_SHA256:
        raise SystemExit(f"FAIL: archive SHA-256 {archive_sha} != {EXPECTED_ARCHIVE_SHA256}")

    raw_tar=lzma.decompress(archive)
    a.out_dir.mkdir(parents=True,exist_ok=True)
    root=a.out_dir.resolve()
    members=[]
    with tarfile.open(fileobj=io.BytesIO(raw_tar),mode="r:") as tf:
        for m in tf.getmembers():
            target=(a.out_dir/m.name).resolve()
            if target!=root and root not in target.parents:
                raise SystemExit(f"FAIL: unsafe archive member {m.name!r}")
            members.append({"name":m.name,"size":m.size,"type":m.type.decode("latin1") if isinstance(m.type,bytes) else str(m.type)})
        tf.extractall(a.out_dir)

    manifest={
      "schema":"dsrrl.a3.recovered_145.core_source.v2",
      "status":"PASS",
      "archive_size":len(archive),
      "archive_sha256":archive_sha,
      "tar_sha256":hashlib.sha256(raw_tar).hexdigest(),
      "member_count":len(members),
      "parts":[{"name":p.name,"size":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in parts],
      "members":members
    }
    if a.manifest:
        a.manifest.parent.mkdir(parents=True,exist_ok=True)
        a.manifest.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:v for k,v in manifest.items() if k!="members"},indent=2))
    for m in members:
        print(f'{m["size"]:8d} {m["name"]}')

if __name__=="__main__":
    main()
