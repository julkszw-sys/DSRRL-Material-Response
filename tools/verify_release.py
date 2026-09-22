#!/usr/bin/env python3
"""Static verifier for the DSRRL Material Response 1.45 clean release."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

EXPECTED_SIZE = 1_803_264
EXPECTED_SHA256 = "13e722f9472e00c1922baecefe121bf1b7b9dd6129f1d2028d64568d74bb5ab7"
LOADER_RVA = 0x1BC100
LOADER_SIZE = 1501

EXPECTED_PATCHES = {
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
    "PTDE_GI_ENVSPEC".encode("utf-16le"),
)


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
    return sections


def rva_to_offset(data: bytes, rva: int) -> int:
    for virtual_size, virtual_address, raw_size, raw_ptr in parse_sections(data):
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

    if b"DSRRL Material Response 1.45\0" not in data:
        raise SystemExit("FAIL: registration string missing")
    if b"SpecRGB" not in data or b"Subsurf" not in data:
        raise SystemExit("FAIL: clean release metadata missing expected active features")

    print("PASS: exact Material Response 1.45 clean release identity/invariants")


if __name__ == "__main__":
    main()
