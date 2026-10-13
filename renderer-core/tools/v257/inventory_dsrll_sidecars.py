#!/usr/bin/env python3
"""V257 offline inventory of physically present DSRRL sidecars.

The output is explicitly UNVERIFIED_CANDIDATE. Never automatically
admit a draw merely because this inventory includes a matching basename.
"""
from pathlib import Path
import argparse,hashlib,json

def scan(root:Path):
    if not root.is_dir():
        raise FileNotFoundError(f"DSRRL root missing: {root}")
    result=[]
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        rel=path.relative_to(root).as_posix()
        low=rel.lower()
        if not (low.endswith(".dds") or low.endswith("_pack_rgba.bin")):
            continue
        sha=hashlib.sha256()
        with path.open("rb") as f:
            magic=f.read(4)
            if low.endswith(".dds") and magic!=b"DDS ":
                raise ValueError(f"DDS signature missing: {rel}")
            f.seek(0)
            for block in iter(lambda:f.read(1<<20),b""):
                sha.update(block)
        result.append({"relative_path":rel,
            "sha256":sha.hexdigest(),"size_bytes":path.stat().st_size,
            "mapping_status":"UNVERIFIED_CANDIDATE",
            "authorizes_runtime_bridge":False})
    return result

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--root",required=True,type=Path)
    ap.add_argument("--out",type=Path,required=True)
    args=ap.parse_args()
    result={"schema":"dsrrl.v257.sidecar_inventory.v1",
      "scope":"equipment resource discovery only; no texture/slot/MTD/FLVER authority",
      "all_entries_require_explicit_mapping_review":True,
      "files":scan(args.root)}
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",
                        encoding="utf-8")
    print("V257_UNVERIFIED_SIDECAR_INVENTORY_COUNT",len(result["files"]))
if __name__=="__main__":
    main()
