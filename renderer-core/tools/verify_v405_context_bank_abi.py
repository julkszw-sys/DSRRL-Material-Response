#!/usr/bin/env python3
"""V405 original-EXE static RE regression. No executable patch or GPU claims."""
import hashlib
import struct
import sys
from pathlib import Path

EXPECTED = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
data = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() == EXPECTED, "Wrong EXE (fail-open)"
pe = struct.unpack_from("<I", data, 0x3c)[0]
header = pe + 24 + struct.unpack_from("<H", data, pe + 20)[0]
sections = []
for i in range(struct.unpack_from("<H", data, pe + 6)[0]):
    o = header + i * 40
    mem_size, rva, disk_size, raw = struct.unpack_from("<IIII", data, o + 8)
    sections.append((rva, min(mem_size, disk_size), raw))

def read(rva, count):
    for start, span, disk in sections:
        if start <= rva and rva + count <= start + span:
            return data[disk + rva - start: disk + rva - start + count]
    raise ValueError(hex(rva))

def verify(rva, hex_bytes):
    want = bytes.fromhex(hex_bytes)
    assert read(rva, len(want)) == want, hex(rva)

# Explicit native model-context vtable and adjacent bank carriers.
for rva, bs in [
    (0x1ea48c,"48 8d 05 cd c6 0f 01"),
    (0x1ea493,"48 89 06"),
    (0x1ea4c4,"48 89 8e 18 23 00 00"),
    (0x1ea4cb,"48 8b 48 28"),
    (0x1ea4cf,"48 89 8e 20 23 00 00"),
    (0x1ea4ee,"48 89 be 38 23 00 00"),
    (0x1eabf9,"41 b8 06 00 00 00"),
    (0x1eabff,"48 8b 89 20 23 00 00"),
    (0x1eac06,"e8 65 ef 37 00"),
    (0x1eac0f,"8b 83 38 23 00 00"),
    (0x569b90,"8b 04 d0"),      # type6 dword at +0
    (0x569b60,"48 8b 44 d0 08"), # source pointer at +8
    (0x4ff60b,"c7 46 6c fe ff ff ff"),
    (0x4ff612,"c7 46 70 fe ff ff ff"),
    (0x4fffdf,"0f 84 df 00 00 00"),
    (0x5001c7,"74 36"),
    (0x50019b,"ff 90 a8 00 00 00"),
]:
    verify(rva, bs)

base = 0x140000000
col = struct.unpack("<Q", read(0x12e6b58, 8))[0]
assert col == base + 0x163ab48
assert struct.unpack("<I", read(0x163ab48, 4))[0] == 1
td = struct.unpack("<I", read(0x163ab54, 4))[0]
assert read(td + 16, 30).split(b"\0")[0] == b".?AVFrpgDrawContext@NS_FRPG@@"
assert struct.unpack("<Q", read(0x136b068, 8))[0] == base + 0x501330
assert struct.unpack("<Q", read(0x136c5c8, 8))[0] == base + 0x4fff10
assert struct.unpack("<Q", read(0x136c5d0, 8))[0] == base + 0x5000e0
print("V405 FRPGDRAWCONTEXT BANK ABI STATIC RE PASS; RUNTIME/GPU/PIXEL OPEN")
