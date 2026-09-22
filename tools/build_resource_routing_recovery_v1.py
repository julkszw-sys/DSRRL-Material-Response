#!/usr/bin/env python3
"""Build DSRRL 1.45 Resource Routing Recovery V1 from Telemetry V2.

Clean-room project patch based solely on DSRRL's own V12 runtime/RE evidence.
It restores the proven ordinary DifSpcBmp local receiver-domain gate at the
common Diffuse PREPARE cut. It does not alter renderer equations or final binders.
"""
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path

EXPECTED_BASIS_SHA256 = "d02af39ae6c0dc38525dac930ded3030a3440491028072cb8db111316c4a4442"
EXPECTED_OUTPUT_SHA256 = "45c7023cb6d2c1680712ca32dae6af25c3c406e7ce233d2555a441be67653928"
PATCH_RVA = 0x19ABD3
EXPECTED = bytes.fromhex("8b4914e8251b02000f84bcffffff")
REPLACEMENT = bytes.fromhex("8b491483f9170f83beffffff9090")

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def rva_to_offset(image: bytes, rva: int) -> int:
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    section_count = struct.unpack_from("<H", image, pe + 6)[0]
    opt_size = struct.unpack_from("<H", image, pe + 20)[0]
    section_table = pe + 24 + opt_size
    for i in range(section_count):
        off = section_table + i * 40
        virtual_size, virtual_address, raw_size, raw_ptr = struct.unpack_from("<IIII", image, off + 8)
        span = max(virtual_size, raw_size)
        if virtual_address <= rva < virtual_address + span:
            return raw_ptr + (rva - virtual_address)
    raise ValueError(f"RVA 0x{rva:X} is not mapped")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("basis", type=Path, help="Telemetry V2 .addon64 basis")
    ap.add_argument("output", type=Path, help="output .addon64")
    args = ap.parse_args()

    original = args.basis.read_bytes()
    actual = sha256(original)
    if actual != EXPECTED_BASIS_SHA256:
        raise SystemExit(f"refusing unknown basis: {actual}")

    image = bytearray(original)
    patch_off = rva_to_offset(image, PATCH_RVA)
    found = bytes(image[patch_off:patch_off + len(EXPECTED)])
    if found != EXPECTED:
        raise SystemExit(f"guard mismatch at RVA 0x{PATCH_RVA:X}: {found.hex()}")

    image[patch_off:patch_off + len(REPLACEMENT)] = REPLACEMENT
    result = bytes(image)
    out_sha = sha256(result)
    if out_sha != EXPECTED_OUTPUT_SHA256:
        raise SystemExit(f"unexpected output SHA-256: {out_sha}")

    args.output.write_bytes(result)
    print(f"PASS: {args.output}")
    print(f"SHA256: {out_sha}")
    print("Patch: common Diffuse PREPARE gate restored to local TLS ordinal 0..22")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
