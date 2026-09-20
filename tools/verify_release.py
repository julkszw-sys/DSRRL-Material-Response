#!/usr/bin/env python3
"""Static identity verifier for DSRRL Material Response 1.45."""

import argparse
import hashlib
from pathlib import Path

EXPECTED_SIZE = 1_803_264
EXPECTED_SHA256 = "41690c6212157eb772ae0c75c055d0bb7a842709f65f02c3689b87714151e1f7"

FORBIDDEN_RELEASE_MARKERS = [
    b"[DSRRL]",
    b"ENVSPEC_DIAG",
    b"ASSET_DIAG",
    b"CAPTURE_NORMAL",
    b"CAPTURE_DIFFUSE",
    b"CAPTURE_SPEC",
    b"T12_T14_BIND PASS",
    b"RESTORE PASS",
    b"GPU_IDENTITY",
    b"active BSS-safe",
]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("addon", type=Path)
    args = p.parse_args()

    data = args.addon.read_bytes()
    digest = hashlib.sha256(data).hexdigest()

    print(f"file={args.addon}")
    print(f"size={len(data)}")
    print(f"sha256={digest}")

    if len(data) != EXPECTED_SIZE:
        raise SystemExit(f"FAIL: size != {EXPECTED_SIZE}")
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"FAIL: sha256 != {EXPECTED_SHA256}")

    remaining = [m.decode(errors="replace") for m in FORBIDDEN_RELEASE_MARKERS if m in data]
    if remaining:
        raise SystemExit(f"FAIL: development diagnostics remain: {remaining}")

    if b"DSRRL Material Response 1.45\0" not in data:
        raise SystemExit("FAIL: 1.45 registration string missing")

    print("PASS: exact Material Response 1.45 binary identity")


if __name__ == "__main__":
    main()
