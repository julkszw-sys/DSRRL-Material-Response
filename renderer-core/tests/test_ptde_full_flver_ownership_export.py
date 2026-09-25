#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "renderer-core" / "tools"
TESTS = REPO / "renderer-core" / "tests"
sys.path.insert(0, str(TESTS))

from test_dsr_flver_zip_ownership_census import build_fixture


def load_tool():
    path = TOOLS / "ptde_full_flver_ownership_export.py"
    spec = importlib.util.spec_from_file_location("ptde_full_flver_ownership_export", path)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


def build_variable_bnd3(payload: bytes) -> bytes:
    # IDS + NAMES1, bit-big-endian format byte. This produces a 0x14-byte
    # member header, proving the parser must not assume the old fixed 0x18.
    fmt = 0x02 | 0x04
    name = b"model.flver\x00"
    name_off = 0x40
    payload_off = 0x60
    out = bytearray(payload_off + len(payload))
    out[0:4] = b"BND3"
    out[0x0C] = fmt
    out[0x0D] = 0
    out[0x0E] = 1
    out[0x0F] = 0
    struct.pack_into("<i", out, 0x10, 1)
    struct.pack_into("<i", out, 0x14, 0x34)

    cursor = 0x20
    out[cursor:cursor + 4] = b"\x00\x00\x00\x00"
    cursor += 4
    struct.pack_into("<i", out, cursor, len(payload))
    cursor += 4
    struct.pack_into("<I", out, cursor, payload_off)
    cursor += 4
    struct.pack_into("<i", out, cursor, 7)
    cursor += 4
    struct.pack_into("<i", out, cursor, name_off)
    cursor += 4
    assert cursor == 0x34

    out[name_off:name_off + len(name)] = name
    out[payload_off:payload_off + len(payload)] = payload
    return bytes(out)


def main() -> int:
    tool = load_tool()

    flver = bytearray(build_fixture())
    struct.pack_into("<i", flver, 8, 0x2000E)
    version, materials = tool.parse_flver2_materials(bytes(flver))
    assert version == 0x2000E
    assert len(materials) == 1
    assert materials[0].mtd_path.endswith("P_Metal[DSB].mtd")

    binder = build_variable_bnd3(bytes(flver))
    entries = tool.parse_bnd3_ptde(binder)
    assert len(entries) == 1
    assert entries[0].file_id == 7
    assert entries[0].name == "model.flver"
    version2, materials2 = tool.parse_flver2_materials(entries[0].payload)
    assert version2 == 0x2000E
    assert len(materials2) == 1

    too_new = bytearray(flver)
    struct.pack_into("<i", too_new, 8, 0x2000F)
    try:
        tool.parse_flver2_materials(bytes(too_new))
    except ValueError as exc:
        assert "post-PTDE" in str(exc)
    else:
        raise AssertionError("0x2000F must fail closed")

    print("ptde_full_flver_ownership_export: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
