#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path

MAGIC=b"DSRENP01"
EXPECTED_SHA="ba996b8b6c4851466ff2106a1f01f17e2711aca1a4fba85efc87873123663cbc"
COUNT=342
STRIDE=36
SIZE=24+COUNT*STRIDE

def parse(path: Path):
    blob=path.read_bytes()
    if len(blob)!=SIZE: raise SystemExit(f"size mismatch: {len(blob)}")
    if hashlib.sha256(blob).hexdigest()!=EXPECTED_SHA: raise SystemExit("SHA mismatch")
    if blob[:8]!=MAGIC: raise SystemExit("magic mismatch")
    version,count,stride,reserved=struct.unpack_from("<4I",blob,8)
    if (version,count,stride,reserved)!=(1,COUNT,STRIDE,0):
        raise SystemExit("header mismatch")
    records=[]
    seen_sha=set(); seen_ord=set()
    off=24
    for _ in range(COUNT):
        sha=blob[off:off+32]
        ordinal,res=struct.unpack_from("<HH",blob,off+32)
        off+=STRIDE
        if res!=0 or ordinal>=COUNT or sha in seen_sha or ordinal in seen_ord:
            raise SystemExit("invalid/duplicate record")
        seen_sha.add(sha); seen_ord.add(ordinal)
        records.append((sha,ordinal))
    if seen_ord!=set(range(COUNT)): raise SystemExit("ordinal coverage mismatch")
    return records

def header(records):
    out=[
        "#pragma once","","#include <array>","#include <cstddef>","#include <cstdint>","",
        "namespace dsrrl::operators::env_spec::generated {","",
        "struct native_probe_hash_record {",
        "    std::array<std::uint8_t,32> sha256;",
        "    std::uint16_t probe_ordinal;",
        "};","",
        "inline constexpr std::size_t k_native_probe_hash_record_count = 342u;",
        f'inline constexpr char k_native_probe_hash_source_sha256[] = "{EXPECTED_SHA}";',
        "inline constexpr std::size_t k_native_probe_hash_source_size = 12336u;","",
        "inline constexpr std::array<native_probe_hash_record,k_native_probe_hash_record_count> k_native_probe_hash_v1 = {{"
    ]
    for sha,ordinal in records:
        raw=",".join(f"0x{x:02x}u" for x in sha)
        out.append(f"    {{{{{raw}}}, {ordinal}u}},")
    out += ["}};","","} // namespace dsrrl::operators::env_spec::generated",""]
    return "\n".join(out)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",default="renderer-core/data/census/DSR_NATIVE_ENVSPEC_PROBE_HASH_V1.bin")
    ap.add_argument("--output",required=True)
    args=ap.parse_args()
    records=parse(Path(args.input))
    text=header(records)
    out=Path(args.output)
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(text,encoding="utf-8")
    print(f"native EnvSpec probes: PASS count={len(records)} sha256={EXPECTED_SHA}")

if __name__=="__main__":
    main()
