#!/usr/bin/env python3
from __future__ import annotations

import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXPECTED = {
    "v12/asset_integrated_v12.c": "879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2",
    "v12/asset_stubs_v12.s": "c7e8634e3fd37730f8ee052c2cdc763448dfd408cd023492a6d472ab285c89a9",
    "v12/build_v12.py": "c330b710ed4cede81dcb873f6ad56f7ef798c31f667e3d61bf90edcc412061a3",
    "v12/audit_v12_independent.py": "f6e10a914e5313e1cc28501bcde6ec1783a8f6c5ab03ef64c9024330430f9f2c",
    "v15_7/build_v15_7.py": "99f9108c2eea2b90632506d46d47ec4180ac657269970e6193f8a2a2305e151e",
    "v15_7/V12_ROUTE_ENVSPC_SLOT_MAP_COMPLETE.json": "046fcbc32781f37475fb9a101b0483bb2fb4b969bf14438201a128c01b45440f",
    "client145/build_client145.py": "e0fec5dca605f94ea0c532cff5e555d7fdb07824a67e89fecc791b4f87f23db3",
    "client145/build_client145_final.py": "09c17dfefe536ee2bf10c2ef51ec2652bb2cba672ab98fe921ff2e00d02ccea6",
}

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def main() -> int:
    failed = False
    for rel, expected in EXPECTED.items():
        path = ROOT / rel
        if not path.is_file():
            print(f"FAIL missing {rel}")
            failed = True
            continue
        actual = sha256(path)
        if actual != expected:
            print(f"FAIL {rel} expected={expected} actual={actual}")
            failed = True
        else:
            print(f"PASS {rel} {actual}")

    if failed:
        print("RECOVERY-INTEGRITY FAIL")
        return 1

    print("RECOVERY-INTEGRITY PASS")
    print("source_complete=false")
    print("note=integrity pass protects recovered provenance; it does not close the missing historical source chain")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
