#!/usr/bin/env python3
from pathlib import Path
import hashlib

ROOT = Path(__file__).resolve().parents[1]
HEX = ROOT / "reference" / "loader_bytes.hex"
OUT = ROOT / "reference" / "loader_bytes.bin"

EXPECTED_SIZE = 1501
EXPECTED_SHA256 = "819f9cda297b09a24cb3eb2a9f6699dc5bb7807ac47ba2a94dbbdf50419d9405"

raw = bytes.fromhex("".join(HEX.read_text(encoding="ascii").split()))
if len(raw) != EXPECTED_SIZE:
    raise SystemExit(f"loader size mismatch: {len(raw)} != {EXPECTED_SIZE}")

digest = hashlib.sha256(raw).hexdigest()
if digest != EXPECTED_SHA256:
    raise SystemExit(f"loader SHA-256 mismatch: {digest}")

OUT.write_bytes(raw)
print(f"wrote {OUT} ({len(raw)} bytes, sha256={digest})")
