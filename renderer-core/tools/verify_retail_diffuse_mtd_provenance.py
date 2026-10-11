#!/usr/bin/env python3
"""V213: exact DSR retail BND3 first-entry MTD identity authority for Diffuse.

Source of truth: owner-original DSR Mtd.mtdbnd.dcx SHA256 pinned in JSON.
No game files are embedded in the repository. Optional --binder verifies raw
BND3 entries; CI always verifies source/header/route and protected negative cases.
No shader, texture, emitter, SpecRGB/PointLight identity or draw gating changes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "data/provenance/dsr_retail_mtd_diffuse_first_entries_20261011.json"
REGISTRY = ROOT / "include/dsrrl/operators/material_response/generated_dsr_mtd_identity_v1.hpp"
ROUTES = ROOT / "include/dsrrl/operators/material_response/generated_routes_v1.hpp"


def fnv1a64(name: str) -> int:
    h = 14695981039346656037
    for b in name.encode("utf-8"):
        h = ((h ^ b) * 1099511628211) & ((1 << 64) - 1)
    return h


def parse_registry(source: str) -> dict[int, tuple[str, bool]]:
    rows = {}
    for match in re.finditer(r"\{0x([0-9a-f]{16})ull,\{([^}]*)\},(true|false)\}", source):
        h = int(match.group(1), 16)
        assert h not in rows, f"Duplicate MTD identity {h:016x}"
        raw = "".join(re.findall(r"0x([0-9a-f]{2})", match.group(2)))
        assert len(raw) == 64
        rows[h] = (raw, match.group(3) == "true")
    return rows


def check_binder(path: Path, pin: dict, entries: list[dict]) -> None:
    compressed = path.read_bytes()
    assert hashlib.sha256(compressed).hexdigest() == pin["binder_sha256"], \
        "Different DSR Mtd.mtdbnd.dcx; no unverified registry promotion"
    assert compressed.startswith(b"DCX\x00")
    zlib_at = compressed.index(b"\x78\xda", 48)
    raw = zlib.decompress(compressed[zlib_at:])
    assert raw.startswith(b"BND3")
    count = struct.unpack_from("<I", raw, 0x10)[0]
    assert count > max(x["bnd_index"] for x in entries)

    def get(index: int) -> tuple[str, bytes]:
        flags, size, offset, file_id, name_offset, uncomp = \
            struct.unpack_from("<6I", raw, 0x20 + index * 24)
        assert flags == 0x40 and size == uncomp and 0 < size < 100_000
        assert 0 <= offset <= len(raw) - size
        assert 0 <= name_offset < len(raw)
        name = raw[name_offset:raw.index(b"\x00", name_offset)].decode("ascii")
        return name, raw[offset:offset + size]

    for record in entries:
        name, payload = get(record["bnd_index"])
        assert name == record["name"]
        assert len(payload) == record["bnd_bytes"]
        assert hashlib.sha256(payload).hexdigest() == record["sha256"]
        prior = [get(i)[0] for i in range(record["bnd_index"])]
        assert record["name"] not in prior, "Not the first effective binder entry"
        if "duplicate_shadow_index" in record:
            shadow_name, shadow_payload = get(record["duplicate_shadow_index"])
            assert shadow_name == record["name"]
            assert hashlib.sha256(shadow_payload).hexdigest() == \
                record["duplicate_shadow_sha256"]
            assert record["duplicate_shadow_sha256"] != record["sha256"]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binder", type=Path,
                        help="Optional owner-original DSR Mtd.mtdbnd.dcx")
    args = parser.parse_args()
    pin = json.loads(MANIFEST.read_text("utf-8"))
    assert pin["schema"] == 1 and len(pin["entries"]) == 3
    assert pin["binder_sha256"] == \
        "7c5a0952c1016a3efa3c6a7180bc60af81ccf7b2a4685c3c89028eda78c2d8b5"

    reg = parse_registry(REGISTRY.read_text("utf-8"))
    route_lines = ROUTES.read_text("utf-8").splitlines()
    assert len(reg) == 43, "No silent widening or truncation of MTD registry"
    for entry in pin["entries"]:
        h = fnv1a64(entry["name"])
        assert h in reg and reg[h] == (entry["sha256"], False), \
            f"MTD identity repair not active: {entry['name']}"
        exact_routes = [line for line in route_lines
                        if f'"{entry["name"]}"' in line]
        assert len(exact_routes) == 1, entry["name"]
        route = exact_routes[0]
        assert f',{entry["route_index"]}u,' in route and \
            f'"{entry["sha256"]}"' in route and '"DifSpcBmp"' in route
        assert '33u,34u,35u' in route, "Receiver family must remain exact"

    # These separate operators MUST NOT be changed to match BND data here:
    for name, digest in pin["negative_contract"].items():
        if not name.endswith(".mtd"):
            continue
        h = fnv1a64(name)
        assert h in reg and reg[h] == (digest, False), \
            f"Protected unrelated MTD identity drift: {name}"
    if args.binder:
        check_binder(args.binder, pin, pin["entries"])
    print(f"V213_RETAIL_DSR_DIFFUSE_MTD_PASS corrected={len(pin['entries'])} "
          f"registry={len(reg)} protected=2 binder={'verified' if args.binder else 'sha_pinned'}")


if __name__ == "__main__":
    main()
