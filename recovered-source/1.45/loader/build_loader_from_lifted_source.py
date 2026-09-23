#!/usr/bin/env python3
from __future__ import annotations
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "loader_sha_lifted.S"
OUT = ROOT / "loader_sha.bin"

EXPECTED_SIZE = 1504
EXPECTED_SHA256 = "6dad44e0b8beef0420fe2d9c88d5004a52d81906ea176a4884a0bb8f1a9d6a60"

def materialize() -> bytes:
    data = bytearray()
    for line in SRC.read_text(encoding="utf-8").splitlines():
        code = line.split("/*", 1)[0]
        if ".byte" not in code:
            continue
        payload = code.split(".byte", 1)[1]
        for token in payload.split(","):
            token = token.strip()
            if token:
                data.append(int(token, 0))
    out = bytes(data)
    if len(out) != EXPECTED_SIZE:
        raise SystemExit(f"size mismatch: {len(out)} != {EXPECTED_SIZE}")
    sha = hashlib.sha256(out).hexdigest()
    if sha != EXPECTED_SHA256:
        raise SystemExit(f"SHA mismatch: {sha} != {EXPECTED_SHA256}")
    return out

if __name__ == "__main__":
    out = materialize()
    OUT.write_bytes(out)
    print(f"PASS size={len(out)} sha256={hashlib.sha256(out).hexdigest()}")
