#!/usr/bin/env python3
"""Canonicalize nondeterministic MSVC PE metadata for DSRRL addon builds.

This does not alter executable sections, imports, relocations or resource data.
It normalizes only link-tool metadata that varies across GitHub Windows images:
- DOS/Rich header area,
- COFF TimeDateStamp,
- export TimeDateStamp,
- debug-directory TimeDateStamp,
- IMAGE_DEBUG_TYPE_REPRO record/payload.

The normalized PE header is placed at 0x100 inside the existing 0x400
SizeOfHeaders block; section raw offsets/RVAs remain unchanged.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

PE_FIXED_OFFSET = 0x100
HEADER_LIMIT = 0x400
IMAGE_DEBUG_TYPE_REPRO = 16


def u16(buf: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def u32(buf: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def p32(buf: bytearray, off: int, value: int) -> None:
    struct.pack_into("<I", buf, off, value)


def parse(path: Path) -> tuple[bytearray, int, int, int]:
    data = bytearray(path.read_bytes())
    if len(data) < HEADER_LIMIT or data[:2] != b"MZ":
        raise ValueError("not an MZ/PE image")
    pe_off = u32(data, 0x3C)
    if pe_off + 24 > len(data) or data[pe_off : pe_off + 4] != b"PE\0\0":
        raise ValueError("PE signature missing")
    sections = u16(data, pe_off + 6)
    optional_size = u16(data, pe_off + 20)
    pe_size = 4 + 20 + optional_size + sections * 40
    if pe_off + pe_size > len(data):
        raise ValueError("truncated PE header")
    return data, pe_off, pe_size, sections


def canonicalize(src: Path, dst: Path) -> str:
    data, old_pe, pe_size, sections = parse(src)
    pe = bytearray(data[old_pe : old_pe + pe_size])

    # COFF TimeDateStamp.
    p32(pe, 8, 0)

    optional_rel = 24
    if u16(pe, optional_rel) != 0x20B:
        raise ValueError("expected PE32+")
    optional_size = u16(pe, 20)
    dir_rel = optional_rel + 112

    export_rva, export_size = struct.unpack_from("<II", pe, dir_rel + 0 * 8)
    debug_rva, debug_size = struct.unpack_from("<II", pe, dir_rel + 6 * 8)

    section_rel = 4 + 20 + optional_size
    sec = []
    for i in range(sections):
        off = section_rel + i * 40
        _, vsize, vaddr, raw_size, raw_off = struct.unpack_from("<8sIIII", pe, off)
        sec.append((vaddr, max(vsize, raw_size), raw_off))

    def rva_to_offset(rva: int) -> int:
        for va, size, raw in sec:
            if va <= rva < va + size:
                return raw + (rva - va)
        raise ValueError(f"RVA is not backed by a section: {rva:#x}")

    if export_rva and export_size >= 8:
        p32(data, rva_to_offset(export_rva) + 4, 0)

    if debug_rva and debug_size:
        if debug_size % 28:
            raise ValueError("debug directory size is not a multiple of 28")
        debug_off = rva_to_offset(debug_rva)
        kept = []
        for i in range(debug_size // 28):
            entry = bytearray(data[debug_off + i * 28 : debug_off + (i + 1) * 28])
            debug_type = u32(entry, 12)
            payload_size = u32(entry, 16)
            payload_off = u32(entry, 24)
            p32(entry, 4, 0)
            if debug_type == IMAGE_DEBUG_TYPE_REPRO:
                if payload_off and payload_size:
                    if payload_off + payload_size > len(data):
                        raise ValueError("REPRO payload exceeds file")
                    data[payload_off : payload_off + payload_size] = b"\0" * payload_size
                continue
            kept.append(entry)

        compact = b"".join(kept)
        data[debug_off : debug_off + debug_size] = (
            compact + b"\0" * (debug_size - len(compact))
        )
        struct.pack_into("<II", pe, dir_rel + 6 * 8, debug_rva, len(compact))

    # Preserve only the 64-byte DOS header, drop Rich/stub metadata, and place
    # normalized PE/section headers at a fixed position within SizeOfHeaders.
    if PE_FIXED_OFFSET + len(pe) > HEADER_LIMIT:
        raise ValueError("normalized PE headers exceed fixed header block")
    dos = bytes(data[:0x40])
    data[:HEADER_LIMIT] = b"\0" * HEADER_LIMIT
    data[:0x40] = dos
    p32(data, 0x3C, PE_FIXED_OFFSET)
    data[PE_FIXED_OFFSET : PE_FIXED_OFFSET + len(pe)] = pe

    dst.write_bytes(data)
    return hashlib.sha256(data).hexdigest()


def check(path: Path) -> str:
    data, pe_off, pe_size, sections = parse(path)
    if pe_off != PE_FIXED_OFFSET:
        raise ValueError(f"noncanonical e_lfanew: {pe_off:#x}")
    if any(data[0x40:PE_FIXED_OFFSET]):
        raise ValueError("nonzero DOS/Rich metadata remains")
    if u32(data, pe_off + 8) != 0:
        raise ValueError("COFF TimeDateStamp is nonzero")

    pe = data[pe_off : pe_off + pe_size]
    optional_rel = 24
    optional_size = u16(pe, 20)
    dir_rel = optional_rel + 112
    export_rva, export_size = struct.unpack_from("<II", pe, dir_rel + 0 * 8)
    debug_rva, debug_size = struct.unpack_from("<II", pe, dir_rel + 6 * 8)

    section_rel = 4 + 20 + optional_size
    sec = []
    for i in range(sections):
        off = section_rel + i * 40
        _, vsize, vaddr, raw_size, raw_off = struct.unpack_from("<8sIIII", pe, off)
        sec.append((vaddr, max(vsize, raw_size), raw_off))

    def rva_to_offset(rva: int) -> int:
        for va, size, raw in sec:
            if va <= rva < va + size:
                return raw + (rva - va)
        raise ValueError(f"RVA is not backed by a section: {rva:#x}")

    if export_rva and export_size >= 8 and u32(data, rva_to_offset(export_rva) + 4) != 0:
        raise ValueError("export TimeDateStamp is nonzero")

    if debug_rva and debug_size:
        if debug_size % 28:
            raise ValueError("debug directory size is not canonical")
        debug_off = rva_to_offset(debug_rva)
        for i in range(debug_size // 28):
            entry = data[debug_off + i * 28 : debug_off + (i + 1) * 28]
            if u32(entry, 4) != 0:
                raise ValueError("debug TimeDateStamp is nonzero")
            if u32(entry, 12) == IMAGE_DEBUG_TYPE_REPRO:
                raise ValueError("IMAGE_DEBUG_TYPE_REPRO remains")

    return hashlib.sha256(data).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("src", type=Path)
    ap.add_argument("dst", nargs="?", type=Path)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.check:
        print(check(args.src))
        return 0

    dst = args.dst or args.src
    print(canonicalize(args.src, dst))
    check(dst)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
