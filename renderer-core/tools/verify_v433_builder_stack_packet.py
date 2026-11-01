#!/usr/bin/env python3
"""V433 static-only DSR builder-local packet provenance verifier.
No executable mutation, no runtime model->LightBank->RX34 proof.
Usage: python verify_v433_builder_stack_packet.py DarkSoulsRemastered.exe
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

SHA = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"

SITES = {
    "default_virtual": {
        "packet": "rbp-0x30", "target": None,
        "opcodes": {
            0x220102: "488bf2", 0x2201A6: "488975d0",
            0x220CE6: "488d55d0", 0x220CED: "ff5018",
            0x20DDE4: "4c8bfa", 0x20DDFF: "4d8b37",
        }
    },
    "bit0_or_4": {
        "packet": "rbp-0x10", "target": (0x22119B, 0x20E9D0),
        "opcodes": {
            0x220D90: "488bf2", 0x220E2B: "488975f0",
            0x22117D: "488d55f0", 0x22119B: "e830d8feff",
            0x20EA1D: "4c8bf2", 0x20E9FF: "488b2a",
            0x20EB35: "410fb7464e", 0x20EB43: "664139464c",
        }
    },
    "bit30_or_31": {
        "packet": "rsp+0x60", "target": (0x221412, 0x20E9D0),
        "opcodes": {
            0x221211: "488bfa", 0x221267: "48897c2460",
            0x2213F3: "488d542460", 0x221412: "e8b9d5feff",
            0x20EA1D: "4c8bf2", 0x20E9FF: "488b2a",
            0x20EB35: "410fb7464e", 0x20EB43: "664139464c",
        }
    },
    "bit18": {
        "packet": "rbp-0x10", "target": (0x222033, 0x20F9F0),
        "opcodes": {
            0x221C48: "488bf2", 0x221CDB: "488975f0",
            0x222015: "488d55f0", 0x222033: "e8b8d9feff",
            0x20FA4B: "4c8bfa", 0x20FA23: "488b2a",
            0x20FB58: "410fb7474e", 0x20FB62: "664139474c",
        }
    },
}

def main(exe: str) -> None:
    data = Path(exe).read_bytes()
    assert hashlib.sha256(data).hexdigest() == SHA, "Wrong original PE SHA256"
    off = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[off:off + 4] == b"PE\x00\x00"
    assert struct.unpack_from("<H", data, off + 24)[0] == 0x20B
    assert struct.unpack_from("<Q", data, off + 48)[0] == 0x140000000
    sections = []
    sec = off + 24 + struct.unpack_from("<H", data, off + 20)[0]
    for i in range(struct.unpack_from("<H", data, off + 6)[0]):
        o = sec + i * 40
        _, rva, length, raw = struct.unpack_from("<IIII", data, o + 8)
        sections.append((rva, length, raw))
    def read(rva: int, size: int) -> bytes:
        for base, length, raw in sections:
            if base <= rva and rva + size <= base + length:
                return data[raw + rva - base:raw + rva - base + size]
        raise AssertionError("Missing RVA: %x" % rva)
    expected = {}
    for name, entry in SITES.items():
        for rva, op in entry["opcodes"].items():
            if rva in expected:
                assert expected[rva] == op
            expected[rva] = op
            assert read(rva, len(bytes.fromhex(op))) == bytes.fromhex(op), (name, hex(rva))
        if entry["target"]:
            call, dst = entry["target"]
            b = read(call, 5)
            assert b[0] == 0xE8
            assert call + 5 + struct.unpack_from("<i", b, 1)[0] == dst
    result = {
        "original_sha256": SHA, "status": "STATIC_ABI_PASS",
        "exact_instruction_preimages": len(expected), "direct_calls": 3,
        "conditional_virtual_paths": 1,
        "builders": {name: {"packet": e["packet"], "direct_target": hex(e["target"][1]) if e["target"] else None}
                     for name, e in SITES.items()},
        "open": ["virtual dispatch", "packet post-mutation A/B beta E0/F0",
                 "Source1 native_producer_sequence", "per-RX34 draw",
                 "actor asset identity", "runtime", "bridge activation", "pixel"],
    }
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: python verify_v433_builder_stack_packet.py original_DSR.exe")
    main(sys.argv[1])
