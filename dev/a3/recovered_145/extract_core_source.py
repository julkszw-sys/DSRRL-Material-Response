#!/usr/bin/env python3
from __future__ import annotations
import argparse, base64, hashlib, io, json, lzma, tarfile
from pathlib import Path

PREFIX="DSRRL_Material_Response_1.45_CORE_SOURCE.tar.xz.b64.part"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--parts-dir", type=Path, default=Path(__file__).with_name("core_archive"))
    ap.add_argument("--out-dir", type=Path, required=True)
    ap.add_argument("--manifest", type=Path)
    a=ap.parse_args()

    parts=sorted(a.parts_dir.glob(PREFIX+"*"))
    if not parts:
        raise SystemExit("no core archive parts")
    expected=[a.parts_dir/f"{PREFIX}{i:02d}" for i in range(len(parts))]
    if parts!=expected:
        raise SystemExit(f"non-contiguous core archive parts: {[p.name for p in parts]}")

    b64="".join(p.read_text(encoding="utf-8").strip() for p in parts)
    packed=base64.b64decode(b64, validate=True)
    tar_bytes=lzma.decompress(packed)

    a.out_dir.mkdir(parents=True, exist_ok=True)
    members=[]
    with tarfile.open(fileobj=io.BytesIO(tar_bytes), mode="r:") as tf:
        for m in tf.getmembers():
            if m.name.startswith("/") or ".." in Path(m.name).parts:
                raise SystemExit(f"unsafe archive member: {m.name}")
            members.append({"name":m.name,"size":m.size,"type":m.type.decode("latin1") if isinstance(m.type,bytes) else str(m.type)})
        tf.extractall(a.out_dir)

    manifest={
        "schema":"dsrrl.a3.recovered_145.core_source.v1",
        "parts":[{"name":p.name,"sha256":hashlib.sha256(p.read_bytes()).hexdigest(),"size":p.stat().st_size} for p in parts],
        "base64_concat_sha256":hashlib.sha256(b64.encode()).hexdigest(),
        "tar_xz_sha256":hashlib.sha256(packed).hexdigest(),
        "tar_sha256":hashlib.sha256(tar_bytes).hexdigest(),
        "member_count":len(members),
        "members":members,
    }
    if a.manifest:
        a.manifest.parent.mkdir(parents=True,exist_ok=True)
        a.manifest.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:v for k,v in manifest.items() if k!="members"},indent=2))
    for m in members:
        print(f'{m["size"]:8d} {m["name"]}')

if __name__=="__main__":
    main()
