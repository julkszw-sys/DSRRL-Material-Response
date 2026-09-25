#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import struct
import zlib
from pathlib import Path

EXPECTED_DCX_SHA256 = "ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394"
EXPECTED_BND3_SHA256 = "9ad94df9e282e6fd559605506aa16d74eeb1ef3ebb1ed3002c5affee80018464"
DCX_ZLIB_OFFSET = 76
BND3_FILE_HEADER_OFFSET = 0x20
BND3_FILE_HEADER_STRIDE = 24
B13_DECL = [0x04000059, 0x00208E46, 13, 8]
RDEF_NAME = b"DSRRL_LightBankCarrier\0"

MD5_K = [
0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391,
]
MD5_SHIFT = [
7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21,
]

def die(msg: str) -> None:
    raise SystemExit(msg)

def sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()

def rows(path: str):
    with Path(path).open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader((x for x in f if not x.startswith("#")), delimiter="\t"))

def rol(v: int, s: int) -> int:
    return ((v << s) | (v >> (32 - s))) & 0xFFFFFFFF

def checksum_transform(state, block: bytes):
    words = struct.unpack("<16I", block)
    a, b, c, d = state
    aa, bb, cc, dd = a, b, c, d

    for i in range(64):
        if i < 16:
            f = (b & c) | ((~b) & d)
            g = i
        elif i < 32:
            f = (d & b) | ((~d) & c)
            g = (5 * i + 1) % 16
        elif i < 48:
            f = b ^ c ^ d
            g = (3 * i + 5) % 16
        else:
            f = c ^ (b | (~d))
            g = (7 * i) % 16

        old_d = d
        d = c
        c = b
        b = (b + rol((a + f + MD5_K[i] + words[g]) & 0xFFFFFFFF, MD5_SHIFT[i])) & 0xFFFFFFFF
        a = old_d

    return [
        (aa + a) & 0xFFFFFFFF,
        (bb + b) & 0xFFFFFFFF,
        (cc + c) & 0xFFFFFFFF,
        (dd + d) & 0xFFFFFFFF,
    ]

def fix_dxbc_checksum(data: bytearray) -> None:
    payload = bytes(data[0x14:])
    bit_count = len(payload) * 8
    state = [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476]

    full = len(payload) & ~63
    for offset in range(0, full, 64):
        state = checksum_transform(state, payload[offset:offset + 64])

    tail = payload[full:]
    block = bytearray(64)

    if len(tail) >= 56:
        block[:len(tail)] = tail
        block[len(tail)] = 0x80
        state = checksum_transform(state, block)
        block = bytearray(64)
        struct.pack_into("<I", block, 0, bit_count & 0xFFFFFFFF)
        struct.pack_into("<I", block, 60, ((bit_count >> 2) | 1) & 0xFFFFFFFF)
        state = checksum_transform(state, block)
    else:
        struct.pack_into("<I", block, 0, bit_count & 0xFFFFFFFF)
        block[4:4 + len(tail)] = tail
        block[4 + len(tail)] = 0x80
        struct.pack_into("<I", block, 60, ((bit_count >> 2) | 1) & 0xFFFFFFFF)
        state = checksum_transform(state, block)

    for i, value in enumerate(state):
        struct.pack_into("<I", data, 4 + 4 * i, value)

def parse_chunks(dx: bytes):
    if dx[:4] != b"DXBC" or len(dx) < 32:
        die("invalid DXBC")
    count = struct.unpack_from("<I", dx, 28)[0]
    offsets = struct.unpack_from("<" + "I" * count, dx, 32)
    chunks = []
    for off in offsets:
        tag = dx[off:off + 4]
        size = struct.unpack_from("<I", dx, off + 4)[0]
        chunks.append([tag, bytearray(dx[off + 8:off + 8 + size])])
    return chunks

def patch_rdef(payload: bytearray) -> bytearray:
    if len(payload) < 32:
        die("RDEF too short")

    cb_count, cb_offset, resource_count, resource_offset = struct.unpack_from("<4I", payload, 0)
    cb_bytes = cb_count * 24
    resource_bytes = resource_count * 32

    if cb_count == 0 or resource_count == 0:
        die("RDEF missing expected tables")
    if cb_offset + cb_bytes > len(payload) or resource_offset + resource_bytes > len(payload):
        die("RDEF table bounds invalid")

    for i in range(resource_count):
        base = resource_offset + i * 32
        _, rtype, _, _, _, bind_point, bind_count, _ = struct.unpack_from("<8I", payload, base)
        if rtype == 0 and bind_count and bind_point <= 13 < bind_point + bind_count:
            die("stock RDEF already binds b13")

    out = bytearray(payload)
    while len(out) % 4:
        out.append(0)

    name_offset = len(out)
    out += RDEF_NAME
    while len(out) % 4:
        out.append(0)

    new_cb_offset = len(out)
    out += payload[cb_offset:cb_offset + cb_bytes]
    out += struct.pack("<6I", name_offset, 0, 0, 128, 0, 0)

    new_resource_offset = len(out)
    out += payload[resource_offset:resource_offset + resource_bytes]
    out += struct.pack("<8I", name_offset, 0, 0, 0, 0, 13, 1, 0)

    struct.pack_into("<4I", out, 0, cb_count + 1, new_cb_offset, resource_count + 1, new_resource_offset)
    return out

def build_replacement(dx: bytes, sites):
    chunks = parse_chunks(dx)
    code_hits = 0
    rdef_hits = 0

    for chunk in chunks:
        tag, payload = chunk
        if tag in (b"SHEX", b"SHDR"):
            code_hits += 1
            words = list(struct.unpack("<" + "I" * (len(payload) // 4), payload))
            expected = [0x0100086A, 0x04000059, 0x00208E46, 0, 0xB7, 0x04000059, 0x00208E46, 1, 1]
            if words[2:11] != expected:
                die("unexpected native HemDir3 cbuffer declarations")

            for site, source_reg in sites:
                if site + 1 >= len(words) or words[site] != 0 or words[site + 1] != source_reg:
                    die(f"operand precondition failed word={site} reg={source_reg}")
                words[site] = 13
                words[site + 1] = source_reg - 92

            words[11:11] = B13_DECL
            words[1] += len(B13_DECL)
            chunk[1] = bytearray(struct.pack("<" + "I" * len(words), *words))

        elif tag == b"RDEF":
            rdef_hits += 1
            chunk[1] = patch_rdef(payload)

    if code_hits != 1 or rdef_hits != 1:
        die(f"expected one code and one RDEF chunk, got code={code_hits} rdef={rdef_hits}")

    count = len(chunks)
    header_size = 32 + 4 * count
    out = bytearray(dx[:header_size])
    offsets = []

    for tag, payload in chunks:
        offsets.append(len(out))
        out += tag
        out += struct.pack("<I", len(payload))
        out += payload

    struct.pack_into("<I", out, 24, len(out))
    struct.pack_into("<I", out, 28, count)
    for i, off in enumerate(offsets):
        struct.pack_into("<I", out, 32 + 4 * i, off)

    out[4:20] = b"\0" * 16
    fix_dxbc_checksum(out)
    return out

def bnd_entry(bnd: bytes, shader_index: int):
    off = BND3_FILE_HEADER_OFFSET + shader_index * BND3_FILE_HEADER_STRIDE
    if off + 24 > len(bnd):
        die(f"BND3 index out of range: {shader_index}")

    flags, size, data_offset, file_id, name_offset, stored_size = struct.unpack_from("<6I", bnd, off)
    if file_id != shader_index or size != stored_size or data_offset + size > len(bnd):
        die(f"BND3 record mismatch for index {shader_index}")

    end = bnd.find(b"\0", name_offset)
    if end < 0:
        die(f"BND3 name missing terminator for index {shader_index}")

    return bnd[name_offset:end].decode("ascii"), bnd[data_offset:data_offset + size]

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--shaderbnd-dcx", required=True)
    ap.add_argument("--receivers", required=True)
    ap.add_argument("--patch-sites", required=True)
    ap.add_argument("--replacements", required=True)
    args = ap.parse_args()

    dcx = Path(args.shaderbnd_dcx).read_bytes()
    if sha256(dcx) != EXPECTED_DCX_SHA256:
        die("retail FlverPBL shaderbnd DCX SHA mismatch")

    try:
        bnd = zlib.decompress(dcx[DCX_ZLIB_OFFSET:])
    except zlib.error as exc:
        die(f"DCX zlib payload failed: {exc}")

    if sha256(bnd) != EXPECTED_BND3_SHA256 or bnd[:4] != b"BND3":
        die("decompressed retail BND3 provenance mismatch")

    receivers = rows(args.receivers)
    patch_rows = {r["code_sha256"]: r for r in rows(args.patch_sites)}
    replacement_rows = {r["source_sha256"]: r for r in rows(args.replacements)}

    if len(receivers) != 48 or len(patch_rows) != 48 or len(replacement_rows) != 48:
        die("authority tables must contain exactly 48 unique HemDir3 receivers")

    for row in receivers:
        shader_index = int(row["shader_index"])
        expected_name = row["shader_name"]
        expected_size = int(row["code_size"])
        expected_sha = row["code_sha256"]

        name, dx = bnd_entry(bnd, shader_index)
        if name != expected_name or len(dx) != expected_size or sha256(dx) != expected_sha:
            die(f"stock receiver provenance mismatch for index {shader_index}")

        patch = patch_rows.get(expected_sha)
        replacement = replacement_rows.get(expected_sha)
        if patch is None or replacement is None:
            die(f"authority join missing for {expected_sha}")

        sites = []
        for item in patch["patch_word_sites"].split(","):
            word_s, reg_s = item.split(":", 1)
            sites.append((int(word_s), int(reg_s)))

        output = build_replacement(dx, sites)
        if len(output) != int(replacement["replacement_size"]):
            die(f"replacement size mismatch for index {shader_index}")
        if sha256(output) != replacement["replacement_sha256"]:
            die(f"replacement SHA mismatch for index {shader_index}")

    print("HEMDIR3_RETAIL_AUTHORITY_PASS receivers=48 nospc=24 spc=24 rdef_b13=1")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
