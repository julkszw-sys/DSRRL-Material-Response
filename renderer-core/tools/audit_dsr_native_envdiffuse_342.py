#!/usr/bin/env python3
"""Reconstruct DSR's EXACT native EnvDiffuse 64x64 BC6H resource hashes.
No DDS decode, decompression of BC6H, rescale, pixel transform or on-disk EXE patch.
Sources: original owner DSR evidence Part01/02 and canonical packed GI binder map.
"""
import csv
import hashlib
import io
import json
import struct
import sys
import zipfile
import zlib
from collections import defaultdict
from pathlib import Path

SRC_SHA = (
    "3ead31d0063d910da1879762502ba3bc61cd799a87a17138549bf04797d87f1b",
    "57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076",
    "1cf41fe591c998b47ef2d7b7c8bcc5292113f736a510ed3b3149045a81bc3a61",
)
SPEC_AUTHORITY_SHA = "ba996b8b6c4851466ff2106a1f01f17e2711aca1a4fba85efc87873123663cbc"
DIFF_AUTHORITY_SHA = "cd3549ee84410f932909e818f07d5009347314f991bf6b0c69bffa46fceb747c"

def authority_blob(magic, rows, key):
    assert len(rows) == 342 and {r["ordinal"] for r in rows} == set(range(342))
    return magic + struct.pack("<4I", 1, 342, 36, 0) + b"".join(
        bytes.fromhex(r[key]) + struct.pack("<HH", r["ordinal"], 0)
        for r in rows
    )

def run(part1, part2, binder, output):
    paths = [Path(part1), Path(part2), Path(binder)]
    for path, sha in zip(paths, SRC_SHA):
        assert hashlib.sha256(path.read_bytes()).hexdigest() == sha, f"source SHA mismatch: {path}"
    with zipfile.ZipFile(paths[2]) as z:
        binder_rows = list(csv.DictReader(
            io.StringIO(z.read("DSR_GI_PACKED_PROBE_ID_BINDER_MAP.csv").decode())
        ))
    assert len(binder_rows) == 342
    results = []
    with zipfile.ZipFile(paths[0]) as z1, zipfile.ZipFile(paths[1]) as z2:
        for m in sorted({r["map"] for r in binder_rows}):
            z = z1 if int(m[1:]) <= 15 else z2
            bdt = z.read(f"DSR/map/{m}/GI_EnvM_{m}.tpfbdt")
            for row in (r for r in binder_rows if r["map"] == m):
                pos, sz = int(row["bdt_offset"]), int(row["dcx_size"])
                dcx = bdt[pos:pos+sz]
                assert len(dcx) == sz and dcx[:4] == b"DCX\\0".replace(b"\\0",b"\x00")
                tpf = zlib.decompress(dcx[76:])
                assert tpf[:4] == b"TPF\\0".replace(b"\\0",b"\x00") and struct.unpack_from("<I",tpf,8)[0] == 2
                d0,s0 = struct.unpack_from("<II",tpf,16)
                d1,s1 = struct.unpack_from("<II",tpf,36)
                diffuse,spec = tpf[d0:d0+s0],tpf[d1:d1+s1]
                for d,dim,size in ((diffuse,64,24576),(spec,256,524352)):
                    assert d[:4] == b"DDS " and struct.unpack_from("<I",d,128)[0] == 95
                    assert struct.unpack_from("<II",d,12) == (dim,dim)
                    assert len(d) == size+148
                names = tpf[56:d0].split(b"\x00")
                assert names[0].decode() == row["runtime_envdif_name"]
                assert names[1].decode() == row["runtime_envspc_name"]
                results.append({
                    "ordinal": len(results), "map": m,
                    "packed_id": int(row["packed_id"]),
                    "diffuse_name": names[0].decode(),
                    "spec_name": names[1].decode(),
                    "diffuse_sha256": hashlib.sha256(diffuse[148:]).hexdigest(),
                    "spec_sha256": hashlib.sha256(spec[148:]).hexdigest(),
                })
    spec = authority_blob(b"DSRENP01",results,"spec_sha256")
    diff = authority_blob(b"DSREND01",results,"diffuse_sha256")
    assert hashlib.sha256(spec).hexdigest() == SPEC_AUTHORITY_SHA, "ordinal/spec canonical mismatch"
    assert hashlib.sha256(diff).hexdigest() == DIFF_AUTHORITY_SHA, "diffuse corpus drift"
    duplicates = defaultdict(list)
    for r in results: duplicates[r["diffuse_sha256"]].append(r["ordinal"])
    repeated = [v for v in duplicates.values() if len(v)>1]
    assert repeated == [[321,322]], f"unexpected duplicate EnvDiffuse fingerprint: {repeated}"
    out=Path(output);out.mkdir(parents=True,exist_ok=True)
    (out/"DSR_NATIVE_ENVDIFFUSE_PROBE_HASH_V1.bin").write_bytes(diff)
    (out/"DSR_NATIVE_ENVDIFFUSE_PROBE_HASH_V1.json").write_text(json.dumps(results,indent=2))
    print("PASS 342 exact native DSR EnvDiffuse; sha256",DIFF_AUTHORITY_SHA)
    print("PASS EnvSpec canonical corpus order",SPEC_AUTHORITY_SHA)
    print("PASS unique diffuse 341, one duplicate ordinals 321/322 (m17)")

if __name__=="__main__":
    if len(sys.argv)!=5:
        sys.exit("Usage: audit_dsr_native_envdiffuse_342.py PART01.zip PART02.zip BINDER.zip OUTPUT_DIR")
    run(*sys.argv[1:])
