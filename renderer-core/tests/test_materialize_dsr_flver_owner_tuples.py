#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import struct
import sys
import tempfile
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "renderer-core" / "tools"
sys.path.insert(0, str(TOOLS))

import materialize_dsr_flver_owner_tuples as owner  # noqa: E402


def build_flver() -> bytes:
    data = bytearray(0x300)
    data[0:6] = b"FLVER\x00"
    data[6:8] = b"L\x00"
    struct.pack_into("<I", data, 8, 0x2000D)
    struct.pack_into("<I", data, 0x14, 0)  # dummy
    struct.pack_into("<I", data, 0x18, 1)  # material
    struct.pack_into("<I", data, 0x1C, 0)  # bones
    struct.pack_into("<I", data, 0x20, 0)  # meshes
    struct.pack_into("<I", data, 0x24, 0)  # vertex buffers
    struct.pack_into("<I", data, 0x50, 0)  # face sets
    struct.pack_into("<I", data, 0x54, 0)  # layouts
    struct.pack_into("<I", data, 0x58, 2)  # textures
    data[0x49] = 1  # Unicode strings

    cursor = 0xE0

    def put(text: str) -> int:
        nonlocal cursor
        raw = text.encode("utf-16le") + b"\x00\x00"
        off = cursor
        data[off:off + len(raw)] = raw
        cursor += len(raw)
        cursor = (cursor + 3) & ~3
        return off

    mat_name = put("ArmorMetal")
    mtd = put(r"N:\FRPG\data\Material\mtd\P_Metal[DSB].mtd")
    d_path = put(r"N:\FRPG\data\parts\HD_A_0000_d.tpf")
    d_sem = put("g_Diffuse")
    s_path = put(r"N:\FRPG\data\parts\HD_A_0000_s.tpf")
    s_sem = put("g_Specular")

    struct.pack_into("<IIIIIIII", data, 0x80, mat_name, mtd, 2, 0, 0, 0, 0, 0)
    struct.pack_into("<II", data, 0xA0, d_path, d_sem)
    struct.pack_into("<II", data, 0xC0, s_path, s_sem)
    return bytes(data[:cursor])


def write_zip(path: Path, *, manifest_sha: str | None = None) -> str:
    flver = build_flver()
    actual = hashlib.sha256(flver).hexdigest()
    manifest = [{
        "output": "flver/HD_A_0000.flver",
        "sha256_full": manifest_sha or actual,
    }]
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("manifest.json", json.dumps(manifest))
        z.writestr("flver/HD_A_0000.flver", flver)
    return actual


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        fixture = root / "fixture.zip"
        flver_sha = write_zip(fixture)

        rows, summary = owner.materialize_rows(
            fixture,
            require_canonical=False,
        )
        assert len(rows) == 1
        assert summary["tuple_count"] == 1
        assert summary["unique_flver_sha256"] == 1
        assert summary["unique_mtd_basenames"] == 1
        assert summary["source_complete"] is False
        assert summary["runtime_identity_transport"] == "OPEN"
        assert summary["positive_runtime_activation"] == "HOLD_OFF"
        assert rows[0]["flver_sha256"] == flver_sha
        assert rows[0]["material_slot"] == 0
        assert rows[0]["mtd_name"] == "P_Metal[DSB].mtd"
        assert int(rows[0]["semantic_name_hash"], 16) != 0

        header = owner.render_header(rows, summary)
        assert "k_dsr_flver_owner_tuple_source_complete=false" in header
        assert "dsr_flver_owner_tuple_authenticated" in header

        try:
            owner.materialize_rows(fixture, require_canonical=True)
        except ValueError as exc:
            assert "SHA-256 mismatch" in str(exc)
        else:
            raise AssertionError("non-canonical ZIP must not become source-complete")

        tampered = root / "tampered.zip"
        write_zip(tampered, manifest_sha="00" * 32)
        try:
            owner.materialize_rows(tampered, require_canonical=False)
        except ValueError as exc:
            assert "manifest/member SHA mismatch" in str(exc)
        else:
            raise AssertionError("manifest/member mismatch must fail open")

    print("materialize_dsr_flver_owner_tuples: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
