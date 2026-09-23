#!/usr/bin/env python3
"""Static verifier for the exact DSRRL Material Response 1.45 Nexus release."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from patch_pmetal_terminal_sat import TARGETS, SAT_BIT, MOV_TOKEN, scan_dxbc, shex_offset

EXPECTED_SIZE = 1_803_264
EXPECTED_SHA256 = "e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342"
EXPECTED_PE_CHECKSUM = 0x001BC666

LOADER_RVA = 0x1BC100
LOADER_SIZE = 1501

EXPECTED_PATCHES = {
    0x006A67: bytes.fromhex("90 90 90 90 90"),
    0x006EF0: bytes.fromhex("c3 90 90 90 90"),
    0x008E37: bytes.fromhex("e9 0d 00 00 00 90 90"),
    0x10B245: bytes.fromhex("e9 3f 00 00 00"),
    0x1142C9: bytes.fromhex("e9 3e 00 00 00 90 90"),
    0x19BD03: bytes.fromhex("e9 44 00 00 00"),
    0x19BD87: bytes.fromhex("e9 44 00 00 00"),
    0x1B919C: bytes.fromhex("e9 3c 00 00 00 90 90"),
    0x1B827F: bytes.fromhex("45 31 ed 90 90 90 90"),
}

FORBIDDEN = (
    b"[DSRRL 1.45 TELEMETRY]",
    b"PTDE_GI_ENVSPEC",
    b"exact-slot EnvSpec",
    b"/24 c101 shaders ",
    b": diffuse shaders ",
    "PTDE_GI_ENVSPEC".encode("utf-16le"),
)

EXPECTED_DXBC_SHA256 = {
    33: "ee2511b2c3c6a822c921ad0ca5ef9eaf78b05d1992ce88851b5ecae95601c872",
    34: "3cb53c033f61ef7664be097373c87d1a5f0933c2d3342feb6c50a63b5e3b49dd",
    35: "71b973e36cb2ebbabc05455c1f882653ad39532644051d7d1e2e045874d427d3",
    72: "159e9bbcb36c110e0e6e223f740986844cc8f0882d472afaf5b65abee4301e98",
    73: "726461e5308788f75e7dba4e0e7c85e9fcc801faec38c060635dff40fb9ae877",
    74: "11a405536a0600037b11817579a29cf5663bc62e8b17955af255d279e03417c7",
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_sections(data: bytes):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    count = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    opt_size = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    sec_off = e_lfanew + 24 + opt_size
    sections = []
    for i in range(count):
        off = sec_off + i * 40
        virtual_size, virtual_address, raw_size, raw_ptr = struct.unpack_from(
            "<IIII", data, off + 8
        )
        sections.append((virtual_size, virtual_address, raw_size, raw_ptr))
    return e_lfanew, sections


def rva_to_offset(data: bytes, rva: int) -> int:
    _, sections = parse_sections(data)
    for virtual_size, virtual_address, raw_size, raw_ptr in sections:
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            return raw_ptr + (rva - virtual_address)
    raise SystemExit(f"FAIL: RVA not mapped: 0x{rva:X}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("addon", type=Path)
    args = parser.parse_args()

    data = args.addon.read_bytes()
    digest = sha256_bytes(data)

    print(f"file={args.addon}")
    print(f"size={len(data)}")
    print(f"sha256={digest}")

    if len(data) != EXPECTED_SIZE:
        raise SystemExit(f"FAIL: size != {EXPECTED_SIZE}")
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"FAIL: sha256 != {EXPECTED_SHA256}")

    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    checksum_offset = e_lfanew + 24 + 64
    pe_checksum = struct.unpack_from("<I", data, checksum_offset)[0]
    if pe_checksum != EXPECTED_PE_CHECKSUM:
        raise SystemExit(
            f"FAIL: PE checksum 0x{pe_checksum:08X} != 0x{EXPECTED_PE_CHECKSUM:08X}"
        )

    for marker in FORBIDDEN:
        if marker in data:
            raise SystemExit(f"FAIL: forbidden release marker remains: {marker!r}")

    for rva, expected in EXPECTED_PATCHES.items():
        off = rva_to_offset(data, rva)
        if data[off:off + len(expected)] != expected:
            raise SystemExit(f"FAIL: release patch mismatch at RVA 0x{rva:X}")

    loader_off = rva_to_offset(data, LOADER_RVA)
    if any(data[loader_off:loader_off + LOADER_SIZE]):
        raise SystemExit("FAIL: EnvSpec loader cave not cleared")

    embedded = scan_dxbc(data)
    if len(embedded) != 78:
        raise SystemExit(f"FAIL: embedded DXBC count is {len(embedded)}, expected 78")

    for index, expected_hash in EXPECTED_DXBC_SHA256.items():
        off, size = embedded[index]
        blob = data[off:off + size]
        actual_hash = sha256_bytes(blob)
        if actual_hash != expected_hash:
            raise SystemExit(
                f"FAIL: DXBC {index} SHA-256 {actual_hash} != {expected_hash}"
            )

    for index, terminal_word in TARGETS.items():
        off, size = embedded[index]
        blob = data[off:off + size]
        token_offset = shex_offset(blob) + terminal_word * 4
        token = struct.unpack_from("<I", blob, token_offset)[0]
        if (token & ~SAT_BIT) != MOV_TOKEN or (token & SAT_BIT) == 0:
            raise SystemExit(
                f"FAIL: DXBC {index} terminal RGB write is not MOV_SAT: 0x{token:08X}"
            )

    if b"DSRRL Material Response 1.45\0" not in data:
        raise SystemExit("FAIL: registration string missing")
    if b"SpecRGB" not in data or b"Subsurf" not in data:
        raise SystemExit("FAIL: release metadata missing expected active features")

    print("PASS: exact Nexus Material Response 1.45 identity/invariants")
    print("PASS: EnvSpec replacement disabled")
    print("PASS: terminal RGB SAT present in DXBC 33/34/35")


if __name__ == "__main__":
    main()
