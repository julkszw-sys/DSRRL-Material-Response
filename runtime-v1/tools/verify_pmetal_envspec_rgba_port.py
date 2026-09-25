#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import runpy
import struct
from pathlib import Path

CURRENT = (
    ("70b85d49cea116ff1f72a3fd5bb7726ee72aee5a0655cc81718a83a659ee03f5", 1605, 1720, 7, 72),
    ("ec7f133592462d58c2715c84a398f70f60cecaec8eaaa436ace62b5a262a3ae3", 1514, 1629, 6, 73),
    ("ae69bfe343cb913c4eac4e1d07d1280e4c4dcf02080d1cd55305841e21f76757", 1174, 1289, 5, 74),
)


def parse_cpp_window(text: str) -> list[int]:
    m = re.search(
        r"k_build131_window\s*=\s*\{\{(.*?)\}\};",
        text,
        flags=re.S,
    )
    if not m:
        raise SystemExit("current C++ Build131 window not found")
    vals = [
        int(tok, 0)
        for tok in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", m.group(1))
    ]
    if len(vals) != 100:
        raise SystemExit(f"current C++ window has {len(vals)} words, expected 100")
    return vals


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--provenance", required=True, type=Path)
    ap.add_argument("--transform", required=True, type=Path)
    ap.add_argument("--authority", required=True, type=Path)
    args = ap.parse_args()

    hist = runpy.run_path(str(args.provenance))
    patch = hist.get("PATCH")
    pre = hist.get("PRE")
    post = hist.get("POST")
    if not isinstance(patch, dict) or not isinstance(pre, dict) or not isinstance(post, dict):
        raise SystemExit("historical Build131 provenance is incomplete")

    historical: dict[int, list[int]] = {}
    for shader_id in (72, 73, 74):
        if shader_id not in patch or shader_id not in pre or shader_id not in post:
            raise SystemExit(f"missing Build131 shader {shader_id}")
        word0, hex_window, _checksum = patch[shader_id]
        raw = bytes.fromhex(hex_window)
        if len(raw) != 400:
            raise SystemExit(f"Build131 shader {shader_id} window is not 400 bytes")
        historical[shader_id] = list(struct.unpack("<100I", raw))

    # Historical windows must be identical except for the exact receiver
    # reflection-coordinate register used by source families 9/10/11.
    base = historical[72][:]
    for shader_id, reg in ((72, 7), (73, 6), (74, 5)):
        words = historical[shader_id]
        diffs = [i for i, (a, b) in enumerate(zip(base, words)) if a != b]
        allowed = [] if shader_id == 72 else [6, 38]
        if diffs != allowed:
            raise SystemExit(
                f"Build131 shader {shader_id} unexpected window differences: {diffs}"
            )
        if words[6] != reg or words[38] != reg:
            raise SystemExit(
                f"Build131 shader {shader_id} reflection register mismatch"
            )

    current = parse_cpp_window(args.transform.read_text(encoding="utf-8"))
    expected = base[:]
    expected[6] = 0
    expected[38] = 0
    if current != expected:
        diffs = [i for i, (a, b) in enumerate(zip(current, expected)) if a != b]
        raise SystemExit(f"current C++ Build131 window drift at words {diffs}")

    authority = args.authority.read_text(encoding="utf-8")
    for current_sha, t12, merge, reg, shader_id in CURRENT:
        if current_sha not in authority:
            raise SystemExit(f"missing current V2.11 authority {current_sha}")
        if merge - t12 != 115:
            raise SystemExit(f"current receiver {current_sha} merge span != 115 words")
        if t12 - patch[shader_id][0] != 38:
            raise SystemExit(
                f"current receiver {current_sha} semantic t12 remap is not +38 words"
            )
        marker = f"{t12}u,{merge}u,{reg}u"
        if marker not in authority.replace(" ", ""):
            raise SystemExit(f"authority tuple mismatch for {current_sha}")
        if pre[shader_id] not in authority or post[shader_id] not in authority:
            raise SystemExit(
                f"historical Build131 pre/post provenance missing for shader {shader_id}"
            )

    print(
        "pmetal_envspec_rgba_port: PASS "
        "historical_windows=3 words=100 current_receivers=3"
    )


if __name__ == "__main__":
    main()
