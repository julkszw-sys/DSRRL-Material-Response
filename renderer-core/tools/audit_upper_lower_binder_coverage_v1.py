#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import zlib
from pathlib import Path


def unpack_dcx(path: Path) -> bytes:
    data = path.read_bytes()
    if data[0x18:0x1C] != b"DCS\0" or data[0x24:0x28] != b"DCP\0" or data[0x44:0x48] != b"DCA\0":
        raise SystemExit("unsupported DCX header")
    unpacked = zlib.decompress(data[0x4C:])
    expected = struct.unpack_from(">I", data, 0x1C)[0]
    if len(unpacked) != expected:
        raise SystemExit("DCX size mismatch")
    return unpacked


def entries(bnd: bytes):
    if bnd[:4] != b"BND3":
        raise SystemExit("not BND3")
    count = struct.unpack_from("<I", bnd, 0x10)[0]
    for index in range(count):
        data_offset, _, name_offset, size, _, _ = struct.unpack_from("<6I", bnd, 0x28 + index * 24)
        end = bnd.find(b"\0", name_offset)
        name = bnd[name_offset:end].decode("shift_jis")
        payload = bnd[data_offset:data_offset + size]
        if payload[:4] != b"DXBC" or struct.unpack_from("<I", payload, 24)[0] != size:
            raise SystemExit(f"invalid DXBC entry {index}")
        yield index, Path(name.replace("\\", "/")).name, payload


def shex_words(payload: bytes):
    count = struct.unpack_from("<I", payload, 28)[0]
    hit = None
    for i in range(count):
        off = struct.unpack_from("<I", payload, 32 + i * 4)[0]
        tag = payload[off:off + 4]
        size = struct.unpack_from("<I", payload, off + 4)[0]
        if tag not in (b"SHEX", b"SHDR"):
            continue
        if hit is not None:
            raise SystemExit("multiple code chunks")
        code = payload[off + 8:off + 8 + size]
        hit = list(struct.unpack("<%dI" % (len(code) // 4), code))
    return hit


def exact_ul_consumer(payload: bytes) -> bool:
    if b"gFC_HemAmbCol_u" not in payload or b"gFC_HemAmbCol_d" not in payload:
        return False
    words = shex_words(payload)
    if words is None:
        return False
    c7 = sum(words[i] == 0 and words[i + 1] == 7 for i in range(len(words) - 1))
    c8 = sum(words[i] == 0 and words[i + 1] == 8 for i in range(len(words) - 1))
    return c7 == 1 and c8 == 2


def authority_shas(paths):
    shas = set()
    plans = set()
    for path in paths:
        lines = [line for line in path.read_text(encoding="utf-8").splitlines() if not line.startswith("#")]
        for row in csv.DictReader(lines, delimiter="\t"):
            sha = row["stock_sha256"].lower()
            plan = int(row["plan_index"])
            if sha in shas:
                raise SystemExit(f"duplicate authority SHA {sha}")
            if plan in plans:
                raise SystemExit(f"duplicate authority plan {plan}")
            shas.add(sha)
            plans.add(plan)
    return shas, plans


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--binder", type=Path, required=True)
    ap.add_argument("--authority", type=Path, action="append", required=True)
    ap.add_argument("--expected-names", type=int, default=603)
    ap.add_argument("--expected-unique", type=int, default=344)
    args = ap.parse_args()

    bnd = unpack_dcx(args.binder)
    exact_names = []
    exact_shas = set()
    for _, name, payload in entries(bnd):
        if not exact_ul_consumer(payload):
            continue
        exact_names.append(name)
        exact_shas.add(hashlib.sha256(payload).hexdigest())

    auth_shas, plans = authority_shas(args.authority)

    if len(exact_names) != args.expected_names:
        raise SystemExit(f"expected {args.expected_names} exact names, got {len(exact_names)}")
    if len(exact_shas) != args.expected_unique:
        raise SystemExit(f"expected {args.expected_unique} exact SHA, got {len(exact_shas)}")
    if plans != set(range(args.expected_unique)):
        raise SystemExit("authority plan_index is not contiguous 0..343")

    missing = exact_shas - auth_shas
    extra = auth_shas - exact_shas
    if missing or extra:
        raise SystemExit(f"coverage mismatch missing={len(missing)} extra={len(extra)}")

    print(json.dumps({
        "status": "UPPER_LOWER_BINDER_COVERAGE_PASS",
        "binder_names": len(exact_names),
        "unique_executable_sha256": len(exact_shas),
        "authority_plans": len(plans),
        "missing": 0,
        "extra": 0,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
