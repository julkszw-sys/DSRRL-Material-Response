#!/usr/bin/env python3
"""V276 exact original owner DSR DX11 RX donor extraction.

Never check extracted DXBC into GitHub or upload to Actions: local use only.
Run with user-owned DSRRL_FINAL_EVIDENCE_PART02(2).zip.
"""
import argparse
import hashlib
import struct
import zipfile
import zlib
from pathlib import Path

OWNER = "DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx"
RX = {
    "RX33": ("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85", 20380),
    "RX34": ("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db", 20080),
    "RX35": ("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8", 18580),
}
def extract(src: Path, dst: Path) -> dict:
    with zipfile.ZipFile(src) as z:
        dcx = z.read(OWNER)
    if dcx[:4] != b"DCX\0":
        raise ValueError("wrong original DSR DCX")
    binder = zlib.decompress(dcx[76:])
    if binder[:4] != b"BND3":
        raise ValueError("wrong original DSR shader binder")
    sha_to_rx = {digest: name for name, (digest, _) in RX.items()}
    blobs = {}
    i = 0
    while (i := binder.find(b"DXBC", i)) != -1:
        if i + 32 > len(binder):
            break
        length = struct.unpack_from("<I", binder, i + 24)[0]
        if length < 128 or length > 256000 or i + length > len(binder):
            i += 4
            continue
        blob = binder[i:i + length]
        digest = hashlib.sha256(blob).hexdigest()
        if digest in sha_to_rx:
            name = sha_to_rx[digest]
            if name in blobs:
                raise ValueError(f"duplicate exact original {name}")
            if length != RX[name][1]:
                raise ValueError(f"wrong original shader size {name}")
            blobs[name] = blob
        i += 4
    if set(blobs) != set(RX):
        raise ValueError("missing SHA-authorized original RX donor(s)")
    dst.mkdir(parents=True, exist_ok=True)
    results = {}
    for name in RX:
        path = dst / (name.lower() + "_retail_owner.dxbc")
        if path.exists():
            raise ValueError(f"refusing to overwrite existing donor: {path}")
        path.write_bytes(blobs[name])
        results[name] = {"size": len(blobs[name]), "sha256": RX[name][0],
                         "path": str(path)}
    return results
if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--part02", type=Path, required=True)
    p.add_argument("--outdir", type=Path, required=True)
    args = p.parse_args()
    for name, row in extract(args.part02, args.outdir).items():
        print(f"V276_OWNER_{name}_SHA256_PASS {row['size']} {row['sha256']}")
    print("V276_RETAIL_RX33_RX34_RX35_EXACT_INPUTS_PASS")
