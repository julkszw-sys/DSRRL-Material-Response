#!/usr/bin/env python3
"""Build the public DSRRL Material Response 1.45 addon."""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
from pathlib import Path

EXPECTED_BASE_SHA256 = "cfd1fe585710497a411adad8dc7edd9b46ae187133ee0625abeed2ab1ab96f09"
EXPECTED_OUTPUT_SHA256 = "41690c6212157eb772ae0c75c055d0bb7a842709f65f02c3689b87714151e1f7"
PACK_SIZE = 33_619_968
PACK_SHA256 = "c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3"
PACK_FILE_OFF = 0x1B8400
LOADER_RVA = 0x1BC100
RESOURCE_GUARD_RVA = 0x1BC5AE
LOADER_HEX = Path(__file__).resolve().parents[1] / "reference" / "loader_bytes.hex"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_sections(data: bytes):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    count = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    opt_size = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    sec_off = e_lfanew + 24 + opt_size
    out = []
    for i in range(count):
        off = sec_off + i * 40
        name = data[off:off + 8].rstrip(b"\0").decode(errors="ignore")
        virtual_size, virtual_address, raw_size, raw_ptr = struct.unpack_from(
            "<IIII", data, off + 8
        )
        out.append({
            "header": off,
            "name": name,
            "virtual_size": virtual_size,
            "virtual_address": virtual_address,
            "raw_size": raw_size,
            "raw_ptr": raw_ptr,
        })
    return e_lfanew, out


def rva_to_offset(data: bytes, rva: int) -> int:
    _, sections = parse_sections(data)
    for s in sections:
        va = s["virtual_address"]
        span = max(s["virtual_size"], s["raw_size"])
        if va <= rva < va + span:
            return s["raw_ptr"] + (rva - va)
    raise ValueError(f"RVA not mapped: 0x{rva:X}")


def rel_call(src_rva: int, dst_rva: int) -> bytes:
    return b"\xE8" + struct.pack("<i", dst_rva - (src_rva + 5))


def write_cstr(buf: bytearray, off: int, capacity: int, text: str) -> None:
    raw = text.encode("ascii") + b"\0"
    if len(raw) > capacity:
        raise ValueError((text, len(raw), capacity))
    buf[off:off + capacity] = raw + b"\0" * (capacity - len(raw))


def ascii_strings(data: bytes, min_len: int = 5):
    out = []
    i = 0
    while i < len(data):
        if 32 <= data[i] < 127:
            j = i
            while j < len(data) and 32 <= data[j] < 127:
                j += 1
            if j - i >= min_len and j < len(data) and data[j] == 0:
                out.append((i, bytes(data[i:j])))
            i = j + 1
        else:
            i += 1
    return out


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def build_version_blob() -> bytes:
    key = "VS_VERSION_INFO".encode("utf-16le") + b"\0\0"
    fixed = struct.pack(
        "<13I",
        0xFEEF04BD, 0x00010000,
        (1 << 16) | 45, 0,
        (1 << 16) | 45, 0,
        0x3F, 0,
        0x00040004,
        0x00000002,
        0, 0, 0,
    )
    head = bytearray(struct.pack("<HHH", 0, 52, 0) + key)
    while len(head) % 4:
        head += b"\0"
    blob = head + fixed
    struct.pack_into("<H", blob, 0, len(blob))
    return bytes(blob)


def add_version_resource(data: bytearray) -> None:
    e_lfanew, _ = parse_sections(data)
    opt = e_lfanew + 24
    rva = 0x1BE800
    raw = rva_to_offset(data, rva)
    version_blob = build_version_blob()
    data_off = align_up(88, 4)
    body = bytearray(data_off + len(version_blob))

    struct.pack_into("<IIHHHH", body, 0, 0, 0, 0, 0, 0, 1)
    struct.pack_into("<II", body, 16, 16, 0x80000000 | 24)
    struct.pack_into("<IIHHHH", body, 24, 0, 0, 0, 0, 0, 1)
    struct.pack_into("<II", body, 40, 1, 0x80000000 | 48)
    struct.pack_into("<IIHHHH", body, 48, 0, 0, 0, 0, 0, 1)
    struct.pack_into("<II", body, 64, 0x0409, 72)
    struct.pack_into("<IIII", body, 72, rva + data_off, len(version_blob), 1200, 0)
    body[data_off:data_off + len(version_blob)] = version_blob

    if any(data[raw:raw + len(body)]):
        raise RuntimeError("VERSIONINFO cave is not zero")
    data[raw:raw + len(body)] = body

    resource_dd = opt + 112 + 2 * 8
    old = struct.unpack_from("<II", data, resource_dd)
    if old != (0, 0):
        raise RuntimeError(f"unexpected existing resource directory: {old}")
    struct.pack_into("<II", data, resource_dd, rva, len(body))


def read_loader_hex(path: Path) -> bytes:
    loader = bytes.fromhex("".join(path.read_text(encoding="ascii").split()))
    digest = sha256_bytes(loader)
    if len(loader) != 1501:
        raise RuntimeError(f"loader size mismatch: {len(loader)}")
    if digest != "819f9cda297b09a24cb3eb2a9f6699dc5bb7807ac47ba2a94dbbdf50419d9405":
        raise RuntimeError(f"loader SHA-256 mismatch: {digest}")
    return loader


def patch_rva(buf: bytearray, rva: int, expected: bytes, new: bytes, label: str) -> None:
    off = rva_to_offset(buf, rva)
    got = bytes(buf[off:off + len(expected)])
    if got != expected:
        raise RuntimeError(
            f"{label}: got {got.hex()} expected {expected.hex()} at RVA 0x{rva:X}"
        )
    if len(new) != len(expected):
        raise RuntimeError(f"{label}: patch length mismatch")
    buf[off:off + len(new)] = new


def transform(base_path: Path, pack_path: Path, out_path: Path) -> None:
    base = bytearray(base_path.read_bytes())
    base_sha = sha256_bytes(base)
    if base_sha != EXPECTED_BASE_SHA256:
        raise RuntimeError(
            f"development basis SHA-256 mismatch: {base_sha}\n"
            f"expected: {EXPECTED_BASE_SHA256}"
        )

    pack = pack_path.read_bytes()
    if len(pack) != PACK_SIZE or sha256_bytes(pack) != PACK_SHA256:
        raise RuntimeError("PackedGI resource size/SHA-256 mismatch")

    if bytes(base[PACK_FILE_OFF:PACK_FILE_OFF + PACK_SIZE]) != pack:
        raise RuntimeError("embedded PackedGI data does not match the supplied resource")
    if PACK_FILE_OFF + PACK_SIZE != len(base):
        raise RuntimeError("unexpected development basis layout")

    loader = read_loader_hex(LOADER_HEX)
    loader_off = rva_to_offset(base, LOADER_RVA)
    if any(base[loader_off:loader_off + len(loader)]):
        raise RuntimeError("loader injection area is not zero")
    base[loader_off:loader_off + len(loader)] = loader

    patch_rva(
        base, 0x10C374,
        bytes.fromhex("488905955c0000"),
        b"\x90" * 7,
        "disable development log callback",
    )
    patch_rva(
        base, 0x1B919C,
        bytes.fromhex("488d3d71520000"),
        rel_call(0x1B919C, LOADER_RVA) + bytes.fromhex("eb3a"),
        "install external PackedGI loader",
    )
    patch_rva(
        base, 0x1B827F,
        bytes.fromhex("4c8ba988221100"),
        rel_call(0x1B827F, RESOURCE_GUARD_RVA) + b"\x90\x90",
        "install PackedGI ready guard",
    )

    write_cstr(base, 0xDC58, 0x48, "DSRRL Material Response 1.45")
    write_cstr(
        base, 0xDCA0, 0x153,
        "DSRRL Material Response 1.45: PTDE material response, SpecRGB, "
        "Subsurf, equipment Normal/Diffuse and exact-slot EnvSpec resource "
        "bridge for Dark Souls Remastered. Unsupported or unmapped routes "
        "fail open to stock DSR.",
    )
    write_cstr(base, 0x10387A, 0x2A, "DSRRL_Material_Response_1.45.addon64")

    telemetry_re = [
        re.compile(br"^\[DSRRL\]\[(?:ASSET|ASSET_DIAG|PTDE_SPEC|ENVSPEC|FAILOPEN|ACTIVE)"),
        re.compile(br"^DSRRL Material Response 1\.3(?: active|$)"),
    ]
    telemetry_regexes = [
        re.compile(br"^C\\d+_"),
        re.compile(br"^TIER\\d+="),
        re.compile(br"^B\\d+_(?:CREATE|HIT)="),
    ]
    telemetry_fragments = (
        b"UNMAPPED=", b"SELECTOR=", b"DONOR_SELECTOR=", b"TARGET_BINDS=",
        b"PTDE_DIFFUSE_DRAWS=", b"PTDE_C",
        b"C", b"LERP_BYPASS=", b"FAILOPEN=",
    )
    protected = {
        (0xDC58, 0xDC58 + 0x48),
        (0xDCA0, 0xDCA0 + 0x153),
        (0x10387A, 0x10387A + 0x2A),
    }

    def is_protected(i: int, j: int) -> bool:
        return any(i < end and j > start for start, end in protected)

    for i, s in ascii_strings(base):
        j = i + len(s)
        if is_protected(i, j):
            continue
        match = (
            any(rx.search(s) for rx in telemetry_re)
            or any(rx.search(s) for rx in telemetry_regexes)
            or any(fragment in s for fragment in telemetry_fragments)
            or b"active BSS-safe" in s
        )
        if match:
            base[i:j] = b"\0" * (j - i)

    _, sections = parse_sections(base)
    srgbmt = next(s for s in sections if s["name"] == ".srgbmt")
    if srgbmt["raw_ptr"] != 0x12D400 or srgbmt["virtual_address"] != 0x134000:
        raise RuntimeError("unexpected .srgbmt layout")

    new_raw_size = PACK_FILE_OFF - srgbmt["raw_ptr"]
    if new_raw_size != 0x8B000:
        raise RuntimeError("unexpected externalized raw size")

    struct.pack_into("<I", base, srgbmt["header"] + 16, new_raw_size)
    base = base[:PACK_FILE_OFF]
    add_version_resource(base)

    leftover = b"[DSRRL][FAILOPEN] PTDE EnvSpec LightBank packer bytes mismatch"
    pos = base.find(leftover)
    if pos < 0:
        raise RuntimeError("expected development diagnostic string not found")
    base[pos:pos + len(leftover)] = b"\0" * len(leftover)

    out_path.write_bytes(base)
    output_sha = sha256_bytes(base)

    if len(base) != 1_803_264:
        raise RuntimeError(f"unexpected output size: {len(base)}")
    if output_sha != EXPECTED_OUTPUT_SHA256:
        raise RuntimeError(
            f"output SHA-256 mismatch: {output_sha}\n"
            f"expected: {EXPECTED_OUTPUT_SHA256}"
        )

    print(f"PASS: {out_path}")
    print(f"size={len(base)}")
    print(f"sha256={output_sha}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", type=Path, required=True,
                        help="exact development basis addon")
    parser.add_argument("--pack", type=Path, required=True,
                        help="PTDE_GI_ENVSPEC_PACK_RGBA.bin")
    parser.add_argument("--out", type=Path,
                        default=Path("DSRRL_Material_Response_1.45.addon64"))
    args = parser.parse_args()
    transform(args.base, args.pack, args.out)


if __name__ == "__main__":
    main()
