#!/usr/bin/env python3
"""Build the DSRRL Material Response 1.45 clean release.

This is an auditable hardening transform over the exact integrated 1.45 basis.
EnvSpec/cubemap replacement and release telemetry are intentionally disabled.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

EXPECTED_BASE_SHA256 = "db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966"
EXPECTED_OUTPUT_SIZE = 1_803_264
EXPECTED_OUTPUT_SHA256 = "13e722f9472e00c1922baecefe121bf1b7b9dd6129f1d2028d64568d74bb5ab7"

LOADER_RVA = 0x1BC100
LOADER_SIZE = 1501

PATCHES = {
    0x10B245: (
        bytes.fromhex("b8 01 00 00 00"),
        bytes.fromhex("e9 3f 00 00 00"),
        "bypass Subsurf activation telemetry",
    ),
    0x1142C9: (
        bytes.fromhex("80 3d 30 e0 ff ff 00"),
        bytes.fromhex("e9 3e 00 00 00 90 90"),
        "bypass SpecRGB activation telemetry",
    ),
    0x19BD03: (
        bytes.fromhex("48 85 f6 74 44"),
        bytes.fromhex("e9 44 00 00 00"),
        "bypass Normal activation telemetry",
    ),
    0x19BD87: (
        bytes.fromhex("48 85 f6 74 44"),
        bytes.fromhex("e9 44 00 00 00"),
        "bypass Diffuse activation telemetry",
    ),
    0x1B919C: (
        bytes.fromhex("e8 5f 2f 00 00 eb 3a"),
        bytes.fromhex("e9 3c 00 00 00 90 90"),
        "skip external EnvSpec loader",
    ),
    0x1B827F: (
        bytes.fromhex("e8 2a 43 00 00 90 90"),
        bytes.fromhex("45 31 ed 90 90 90 90"),
        "force EnvSpec resource null / fail-open",
    ),
}

TELEMETRY_STRINGS = (
    b"[DSRRL 1.45 TELEMETRY] SUBSURF ROUTE3 ACTIVE",
    b"[DSRRL 1.45 TELEMETRY] SPECRGB t10 ACTIVE",
    b"[DSRRL 1.45 TELEMETRY] NORMAL t2 ACTIVE",
    b"[DSRRL 1.45 TELEMETRY] DIFFUSE t0 ACTIVE",
    b"[DSRRL 1.45 TELEMETRY] ENVSPEC t12+t14 ACTIVE",
)

TITLE_OFF, TITLE_CAP = 0xDC58, 0x48
DESC_OFF, DESC_CAP = 0xDCA0, 0x153
NAME_OFF, NAME_CAP = 0x10387A, 0x2A

TITLE = "DSRRL Material Response 1.45"
DESCRIPTION = (
    "DSRRL Material Response 1.45: PTDE material response, SpecRGB, Subsurf, "
    "and equipment Normal/Diffuse bridges for Dark Souls Remastered. Unsupported "
    "or unmapped routes fail open to stock DSR."
)
FILENAME = "DSRRL_Material_Response_1.45.addon64"


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
        name = data[off:off + 8].rstrip(b"\0").decode(errors="ignore")
        virtual_size, virtual_address, raw_size, raw_ptr = struct.unpack_from(
            "<IIII", data, off + 8
        )
        sections.append((name, virtual_size, virtual_address, raw_size, raw_ptr))
    return e_lfanew, sections


def rva_to_offset(data: bytes, rva: int) -> int:
    _, sections = parse_sections(data)
    for _, virtual_size, virtual_address, raw_size, raw_ptr in sections:
        span = max(virtual_size, raw_size)
        if virtual_address <= rva < virtual_address + span:
            return raw_ptr + (rva - virtual_address)
    raise RuntimeError(f"RVA not mapped: 0x{rva:X}")


def patch_rva(buf: bytearray, rva: int, expected: bytes, new: bytes, label: str) -> None:
    if len(expected) != len(new):
        raise RuntimeError(f"{label}: patch length mismatch")
    off = rva_to_offset(buf, rva)
    got = bytes(buf[off:off + len(expected)])
    if got != expected:
        raise RuntimeError(
            f"{label}: got {got.hex()} expected {expected.hex()} at RVA 0x{rva:X}"
        )
    buf[off:off + len(new)] = new


def write_cstr(buf: bytearray, off: int, capacity: int, text: str) -> None:
    raw = text.encode("ascii") + b"\0"
    if len(raw) > capacity:
        raise RuntimeError(f"metadata string exceeds capacity: {text}")
    buf[off:off + capacity] = raw + b"\0" * (capacity - len(raw))


def update_pe_checksum(buf: bytearray) -> int:
    e_lfanew = struct.unpack_from("<I", buf, 0x3C)[0]
    checksum_off = e_lfanew + 24 + 64
    buf[checksum_off:checksum_off + 4] = b"\0\0\0\0"

    checksum = 0
    for i in range(0, len(buf) - 1, 2):
        checksum += buf[i] | (buf[i + 1] << 8)
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    if len(buf) & 1:
        checksum += buf[-1]
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    checksum = (checksum & 0xFFFF) + (checksum >> 16)
    checksum = (checksum + len(buf)) & 0xFFFFFFFF

    buf[checksum_off:checksum_off + 4] = struct.pack("<I", checksum)
    return checksum


def assert_postconditions(data: bytes) -> None:
    if len(data) != EXPECTED_OUTPUT_SIZE:
        raise RuntimeError(f"unexpected output size: {len(data)}")

    for marker in TELEMETRY_STRINGS:
        if marker in data:
            raise RuntimeError(f"telemetry marker remains: {marker!r}")

    if b"PTDE_GI_ENVSPEC" in data or b"exact-slot EnvSpec" in data:
        raise RuntimeError("EnvSpec release marker remains")
    if "PTDE_GI_ENVSPEC".encode("utf-16le") in data:
        raise RuntimeError("UTF-16 EnvSpec path remains")

    loader_off = rva_to_offset(data, LOADER_RVA)
    if any(data[loader_off:loader_off + LOADER_SIZE]):
        raise RuntimeError("EnvSpec loader cave is not cleared")

    digest = sha256_bytes(data)
    if digest != EXPECTED_OUTPUT_SHA256:
        raise RuntimeError(
            f"output SHA-256 mismatch: {digest}\nexpected: {EXPECTED_OUTPUT_SHA256}"
        )


def transform(base_path: Path, out_path: Path) -> None:
    buf = bytearray(base_path.read_bytes())
    base_sha = sha256_bytes(buf)
    if base_sha != EXPECTED_BASE_SHA256:
        raise RuntimeError(
            f"integrated basis SHA-256 mismatch: {base_sha}\n"
            f"expected: {EXPECTED_BASE_SHA256}"
        )

    for rva, (expected, new, label) in PATCHES.items():
        patch_rva(buf, rva, expected, new, label)

    loader_off = rva_to_offset(buf, LOADER_RVA)
    buf[loader_off:loader_off + LOADER_SIZE] = b"\0" * LOADER_SIZE

    for marker in TELEMETRY_STRINGS:
        pos = buf.find(marker)
        if pos < 0:
            raise RuntimeError(f"expected telemetry marker not found: {marker!r}")
        if buf.find(marker, pos + 1) >= 0:
            raise RuntimeError(f"duplicate telemetry marker: {marker!r}")
        buf[pos:pos + len(marker)] = b"\0" * len(marker)

    write_cstr(buf, TITLE_OFF, TITLE_CAP, TITLE)
    write_cstr(buf, DESC_OFF, DESC_CAP, DESCRIPTION)
    write_cstr(buf, NAME_OFF, NAME_CAP, FILENAME)

    checksum = update_pe_checksum(buf)
    assert_postconditions(bytes(buf))
    out_path.write_bytes(buf)

    print(f"PASS: {out_path}")
    print(f"size={len(buf)}")
    print(f"sha256={sha256_bytes(buf)}")
    print(f"pe_checksum=0x{checksum:08X}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", type=Path, required=True, help="exact integrated 1.45 addon")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("DSRRL_Material_Response_1.45.addon64"),
    )
    args = parser.parse_args()
    transform(args.base, args.out)


if __name__ == "__main__":
    main()
