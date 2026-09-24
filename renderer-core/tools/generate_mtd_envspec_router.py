#!/usr/bin/env python3
"""Generate and verify the exact PTDE/DSR 325-record EnvSpec MTD router.

The JSON is the human-auditable source representation. This tool recreates the
historical DSREMR01 binary byte-for-byte and regenerates the C++ header used by
Renderer Core. It intentionally preserves float fields as raw IEEE-754 bits.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

MAGIC=b"DSREMR01"
VERSION=1
COUNT=325
STRIDE=72
CANONICAL_SHA256="0701154f5fa683aae95094a65bad32320baa74a3f8855183b750fd7383d28b8a"
CANONICAL_SIZE=23424
FNV_OFFSET=14695981039346656037
FNV_PRIME=1099511628211


def fnv1a_utf8(text: str) -> int:
    h=FNV_OFFSET
    for b in text.encode("utf-8"):
        h=((h ^ b)*FNV_PRIME) & 0xffffffffffffffff
    return h


def fnv1a_utf16_lower_ascii(text: str) -> int:
    h=FNV_OFFSET
    for ch in text:
        if "A" <= ch <= "Z":
            ch=chr(ord(ch)+32)
        u=ord(ch)
        for b in (u & 0xff,(u >> 8) & 0xff):
            h=((h ^ b)*FNV_PRIME) & 0xffffffffffffffff
    return h


def hex_u32(value: str) -> int:
    return int(value,16)


def serialize(doc: dict) -> bytes:
    records=doc["records"]
    if len(records)!=COUNT:
        raise ValueError(f"expected {COUNT} records, got {len(records)}")
    out=bytearray()
    out += MAGIC
    out += struct.pack("<IIII",VERSION,COUNT,STRIDE,0)
    state_counts={1:0,2:0,3:0}
    safe_count=0
    slots=[0,0,0,0]
    for expected_index,r in enumerate(records):
        if r["index"]!=expected_index:
            raise ValueError(f"record order mismatch at {expected_index}")
        semantic=int(r["semantic_name_hash_fnv1a_utf8"],16)
        legacy=int(r["legacy_name_hash_fnv1a_utf16_lower"],16)
        if semantic!=fnv1a_utf8(r["mtd_name"]):
            raise ValueError(f"UTF-8 semantic hash mismatch: {r['mtd_name']}")
        if legacy!=fnv1a_utf16_lower_ascii(r["mtd_name"]):
            raise ValueError(f"legacy UTF-16 lowercase hash mismatch: {r['mtd_name']}")
        raw=bytes.fromhex(r["dsr_mtd_sha256"])
        if len(raw)!=32:
            raise ValueError(f"bad SHA length: {r['mtd_name']}")
        state=int(r["state_value"]); slot=int(r["envspc_slot"]); safe=1 if r["explicit_none_safe"] else 0
        if state not in state_counts or not 0 <= slot <= 3:
            raise ValueError(f"bad state/slot: {r['mtd_name']}")
        if safe and state!=2:
            raise ValueError(f"safe flag only legal for EXPLICIT_NONE: {r['mtd_name']}")
        state_counts[state]+=1; slots[slot]+=1; safe_count+=safe
        out += struct.pack("<Q",legacy)
        out += raw
        out += struct.pack("<BBBB",state,slot,safe,0)
        out += struct.pack("<III",*(hex_u32(x) for x in r["c101_f32_bits"]))
        out += struct.pack("<I",hex_u32(r["c102_f32_bits"]))
        out += struct.pack("<III",*(hex_u32(x) for x in r["c100_f32_bits"]))
    if len(out)!=CANONICAL_SIZE:
        raise ValueError(f"serialized size mismatch: {len(out)}")
    if state_counts!={1:213,2:87,3:25} or safe_count!=20 or slots!=[179,48,54,44]:
        raise ValueError(f"census counts mismatch: states={state_counts} safe={safe_count} slots={slots}")
    digest=hashlib.sha256(out).hexdigest()
    if digest!=CANONICAL_SHA256:
        raise ValueError(f"canonical SHA mismatch: {digest}")
    return bytes(out)


def cpp_sha_bytes(hex_digest: str) -> str:
    return "{{"+",".join(f"0x{b:02x}u" for b in bytes.fromhex(hex_digest))+"}}"


def generate_header(doc: dict) -> str:
    records=doc["records"]
    state_cpp={1:"envspec_router_state::present",2:"envspec_router_state::explicit_none",3:"envspec_router_state::nospc_host"}
    lines=[
        "#pragma once","", "#include <array>","#include <cstddef>","#include <cstdint>","",
        "namespace dsrrl::operators::material_response::generated {","",
        "enum class envspec_router_state : std::uint8_t { unknown=0, present=1, explicit_none=2, nospc_host=3 };","",
        "struct envspec_router_record {",
        "    const char *mtd_name;",
        "    std::uint64_t semantic_name_hash;",
        "    std::uint64_t legacy_name_hash_utf16_lower;",
        "    std::array<std::uint8_t,32> raw_mtd_sha256;",
        "    envspec_router_state state;",
        "    std::uint8_t envspc_slot;",
        "    bool explicit_none_safe;",
        "    std::array<std::uint32_t,3> c101_bits;",
        "    std::uint32_t c102_bits;",
        "    std::array<std::uint32_t,3> c100_bits;",
        "};","",
        f"inline constexpr std::size_t k_envspec_router_record_count = {len(records)}u;",
        f'inline constexpr char k_envspec_router_source_sha256[] = "{CANONICAL_SHA256}";',
        f"inline constexpr std::size_t k_envspec_router_source_size = {CANONICAL_SIZE}u;",
        "inline constexpr std::array<envspec_router_record,k_envspec_router_record_count> k_envspec_router_v1 = {{",
    ]
    import json as _json
    for r in records:
        c101=",".join(f"0x{hex_u32(x):08x}u" for x in r["c101_f32_bits"])
        c100=",".join(f"0x{hex_u32(x):08x}u" for x in r["c100_f32_bits"])
        lines += [
            "    {",
            f"        {_json.dumps(r['mtd_name'])}, {r['semantic_name_hash_fnv1a_utf8']}ull, {r['legacy_name_hash_fnv1a_utf16_lower']}ull,",
            f"        {cpp_sha_bytes(r['dsr_mtd_sha256'])}, {state_cpp[r['state_value']]}, {r['envspc_slot']}u, {str(r['explicit_none_safe']).lower()},",
            f"        {{{{{c101}}}}}, 0x{hex_u32(r['c102_f32_bits']):08x}u, {{{{{c100}}}}}",
            "    },",
        ]
    lines += ["}};","", "} // namespace dsrrl::operators::material_response::generated",""]
    return "\n".join(lines)


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--json",default="renderer-core/data/census/ptde_mtd_envspec_router_v1.json")
    ap.add_argument("--binary",default="renderer-core/data/census/MATERIAL_ENVSPEC_ROUTER_V1.bin")
    ap.add_argument("--header",default="renderer-core/include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp")
    ap.add_argument("--write",action="store_true")
    args=ap.parse_args()
    doc=json.loads(Path(args.json).read_text(encoding="utf-8"))
    blob=serialize(doc)
    header=generate_header(doc)
    if args.write:
        Path(args.binary).write_bytes(blob)
        Path(args.header).write_text(header,encoding="utf-8")
    else:
        if Path(args.binary).read_bytes()!=blob:
            raise SystemExit("binary differs from deterministic JSON reconstruction")
        if Path(args.header).read_text(encoding="utf-8")!=header:
            raise SystemExit("generated header differs from deterministic JSON reconstruction")
    print(f"MTD EnvSpec router PASS records={COUNT} size={len(blob)} sha256={hashlib.sha256(blob).hexdigest()}")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
