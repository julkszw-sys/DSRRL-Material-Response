#!/usr/bin/env python3
"""Materialize exact recovered DSRRL Material Response 1.45 source files.

The archive is stored as numbered base64 text parts so the historical source
can be preserved byte-exactly in Git. This script verifies the archive SHA-256
before extraction.
"""
from __future__ import annotations

import base64
import hashlib
import io
import lzma
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PART_DIR = ROOT / "archive" / "core"
PART_GLOB = "DSRRL_Material_Response_1.45_CORE_SOURCE.tar.xz.b64.part*"
EXPECTED_ARCHIVE_SIZE = 56232
EXPECTED_ARCHIVE_SHA256 = "f701f11d32fa3a993f787e8c6abe9a14e33267c2e3d94270e50ce32a7feaf25a"
OUT = ROOT / "materialized"

parts = sorted(PART_DIR.glob(PART_GLOB))
if len(parts) != 10:
    raise SystemExit(f"FAIL: expected 10 archive parts, found {len(parts)}")

encoded = "".join(p.read_text(encoding="ascii").strip() for p in parts)
archive = base64.b64decode(encoded, validate=True)

if len(archive) != EXPECTED_ARCHIVE_SIZE:
    raise SystemExit(f"FAIL: archive size {len(archive)} != {EXPECTED_ARCHIVE_SIZE}")

digest = hashlib.sha256(archive).hexdigest()
if digest != EXPECTED_ARCHIVE_SHA256:
    raise SystemExit(f"FAIL: archive SHA-256 {digest} != {EXPECTED_ARCHIVE_SHA256}")

raw_tar = lzma.decompress(archive)
OUT.mkdir(parents=True, exist_ok=True)
root_resolved = OUT.resolve()

with tarfile.open(fileobj=io.BytesIO(raw_tar), mode="r:") as tf:
    for member in tf.getmembers():
        target = (OUT / member.name).resolve()
        if target != root_resolved and root_resolved not in target.parents:
            raise SystemExit(f"FAIL: unsafe archive member {member.name!r}")
    tf.extractall(OUT)

print(f"PASS: recovered source materialized under {OUT}")
print(f"archive_size={len(archive)}")
print(f"archive_sha256={digest}")
