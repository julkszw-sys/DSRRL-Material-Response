#!/usr/bin/env python3
"""Apply the exact P_Metal terminal RGB SAT delta used by the Nexus 1.45 release.

Input:
  clean 1.45 binary SHA-256
  3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000

Output:
  Nexus shipping binary SHA-256
  e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342

The only shader-body mutation is MOV -> MOV_SAT on the final RGB output write
for embedded DXBC indices 33, 34 and 35. DXBC checksums and the PE checksum
are recomputed afterwards.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

INPUT_SHA256 = "3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000"
OUTPUT_SHA256 = "e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342"
OUTPUT_SIZE = 1_803_264
EXPECTED_PE_CHECKSUM = 0x001BC666

TARGETS = {
    33: 2822,
    34: 2741,
    35: 2394,
}
MOV_TOKEN = 0x05000036
SAT_BIT = 0x00002000

SHIFTS = [
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21,
]
K = [
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391,
]
MASK = 0xFFFFFFFF


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def rol(value: int, shift: int) -> int:
    return ((value << shift) | (value >> (32 - shift))) & MASK


def _transform(state: list[int], block: bytes) -> None:
    words = struct.unpack("<16I", block)
    a, b, c, d = state
    for i in range(64):
        if i < 16:
            f = (b & c) | ((~b) & d)
            g = i
        elif i < 32:
            f = (b & d) | (c & (~d))
            g = (5 * i + 1) & 15
        elif i < 48:
            f = b ^ c ^ d
            g = (3 * i + 5) & 15
        else:
            f = c ^ (b | (~d))
            g = (7 * i) & 15

        previous_d = d
        d = c
        c = b
        b = (b + rol((a + (f & MASK) + K[i] + words[g]) & MASK, SHIFTS[i])) & MASK
        a = previous_d

    state[0] = (state[0] + a) & MASK
    state[1] = (state[1] + b) & MASK
    state[2] = (state[2] + c) & MASK
    state[3] = (state[3] + d) & MASK


def dxbc_checksum(blob: bytes) -> bytes:
    if blob[:4] != b"DXBC" or len(blob) < 20:
        raise RuntimeError("not a DXBC container")

    data = blob[20:]
    state = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476]
    full = len(data) & ~63
    for offset in range(0, full, 64):
        _transform(state, data[offset:offset + 64])

    remainder = data[full:]
    bitcount = (len(data) * 8) & MASK
    block = bytearray(64)

    if len(remainder) >= 56:
        block[:len(remainder)] = remainder
        block[len(remainder)] = 0x80
        _transform(state, bytes(block))
        block = bytearray(64)
        struct.pack_into("<I", block, 0, bitcount)
    else:
        struct.pack_into("<I", block, 0, bitcount)
        block[4:4 + len(remainder)] = remainder
        block[4 + len(remainder)] = 0x80

    struct.pack_into("<I", block, 60, ((bitcount >> 2) | 1) & MASK)
    _transform(state, bytes(block))
    return struct.pack("<4I", *state)


def scan_dxbc(data: bytes) -> list[tuple[int, int]]:
    result: list[tuple[int, int]] = []
    position = 0
    while True:
        offset = data.find(b"DXBC", position)
        if offset < 0:
            break
        if offset + 32 <= len(data):
            size = struct.unpack_from("<I", data, offset + 24)[0]
            chunks = struct.unpack_from("<I", data, offset + 28)[0]
            if 32 <= size <= 200_000 and offset + size <= len(data) and 1 <= chunks <= 32:
                result.append((offset, size))
        position = offset + 4
    return result


def shex_offset(blob: bytes) -> int:
    chunks = struct.unpack_from("<I", blob, 28)[0]
    for index in range(chunks):
        chunk_offset = struct.unpack_from("<I", blob, 32 + 4 * index)[0]
        if blob[chunk_offset:chunk_offset + 4] in (b"SHEX", b"SHDR"):
            return chunk_offset + 8
    raise RuntimeError("DXBC has no SHEX/SHDR chunk")


def update_pe_checksum(buf: bytearray) -> int:
    pe_offset = struct.unpack_from("<I", buf, 0x3C)[0]
    checksum_offset = pe_offset + 24 + 64
    buf[checksum_offset:checksum_offset + 4] = b"\0\0\0\0"

    checksum = 0
    for i in range(0, len(buf) - 1, 2):
        checksum += buf[i] | (buf[i + 1] << 8)
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    if len(buf) & 1:
        checksum += buf[-1]
        checksum = (checksum & 0xFFFF) + (checksum >> 16)

    checksum = (checksum & 0xFFFF) + (checksum >> 16)
    checksum = (checksum + len(buf)) & 0xFFFFFFFF
    struct.pack_into("<I", buf, checksum_offset, checksum)
    return checksum


def apply_terminal_sat(data: bytes, require_input_identity: bool = True) -> bytes:
    if require_input_identity and sha256_bytes(data) != INPUT_SHA256:
        raise RuntimeError("refusing unknown clean-release input")

    result = bytearray(data)
    embedded = scan_dxbc(result)
    if len(embedded) != 78:
        raise RuntimeError(f"unexpected embedded DXBC count: {len(embedded)}")

    for index, terminal_word in TARGETS.items():
        file_offset, size = embedded[index]
        blob = bytearray(result[file_offset:file_offset + size])
        code_offset = shex_offset(blob)
        token_offset = code_offset + terminal_word * 4
        token = struct.unpack_from("<I", blob, token_offset)[0]
        if token != MOV_TOKEN:
            raise RuntimeError(
                f"DXBC {index}: terminal token 0x{token:08X} != expected MOV 0x{MOV_TOKEN:08X}"
            )

        struct.pack_into("<I", blob, token_offset, token | SAT_BIT)
        blob[4:20] = dxbc_checksum(bytes(blob))
        result[file_offset:file_offset + size] = blob

    pe_checksum = update_pe_checksum(result)
    if pe_checksum != EXPECTED_PE_CHECKSUM:
        raise RuntimeError(
            f"unexpected PE checksum 0x{pe_checksum:08X}; expected 0x{EXPECTED_PE_CHECKSUM:08X}"
        )

    output = bytes(result)
    if len(output) != OUTPUT_SIZE:
        raise RuntimeError(f"unexpected output size: {len(output)}")
    if sha256_bytes(output) != OUTPUT_SHA256:
        raise RuntimeError(
            f"unexpected output SHA-256: {sha256_bytes(output)}\nexpected: {OUTPUT_SHA256}"
        )
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="3dcb50... clean 1.45 addon")
    parser.add_argument("output", type=Path, help="e44183... Nexus shipping addon")
    args = parser.parse_args()

    output = apply_terminal_sat(args.input.read_bytes(), require_input_identity=True)
    args.output.write_bytes(output)
    print(f"PASS: {args.output}")
    print(f"size={len(output)}")
    print(f"sha256={sha256_bytes(output)}")


if __name__ == "__main__":
    main()
