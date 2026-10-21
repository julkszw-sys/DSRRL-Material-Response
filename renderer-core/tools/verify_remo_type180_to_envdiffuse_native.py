#!/usr/bin/env python3
"""SHA-pinned, read-only static DSR REMO Type180/0x804D -> LightBank + GI join.
Runtime model identity and PTDE pixels are NOT established.
Usage: python verify_remo_type180_to_envdiffuse_native.py DarkSoulsRemastered.exe
"""
import hashlib, pathlib, struct, sys

EXPECTED = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
if len(sys.argv) != 2:
    sys.exit("Pass exact original DarkSoulsRemastered.exe path")
blob = pathlib.Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(blob).hexdigest() == EXPECTED, "unknown EXE, fail-closed"
START, OFFSET, SIZE = 0x140001000, 0x400, 0x129b400
def read(va, count):
    assert START <= va < START + SIZE and va + count <= START + SIZE
    at = OFFSET + va - START
    return blob[at:at + count]
def exact(va, expected, label):
    assert read(va, len(bytes.fromhex(expected))) == bytes.fromhex(expected), label
def relative(va, target, label):
    code = read(va, 5)
    assert code[0] in (0xe8, 0xe9), label
    assert va + 5 + struct.unpack("<i", code[1:])[0] == target, label

EXACT = [
    [5374421136,"0f b7 81 de 00 00 00 66 89 02","Type180 actor default getter"],
    [5370640000,"0f b7 02 66 89 41 08","0x804D selector A restored"],
    [5370640007,"0f b7 42 02 66 89 41 0a","0x804D selector B restored"],
    [5370898349,"4c 8d 87 08 01 00 00","REMO first model controller"],
    [5370938900,"48 8d 9f 08 01 00 00","REMO second model controller"],
    [5370571131,"41 b8 05 00 00 00","Map LightBank source type5"],
    [5370571963,"41 b8 06 00 00 00","Character LightBank source type6"],
    [5370864406,"41 0f b7 47 4e","GI source selector A"],
    [5370864424,"41 0f b7 47 4c","GI source selector B"],
    [5370864491,"41 8b 87 dc 00 00 00","GI probe B packed"],
    [5370864512,"41 8b 87 d8 00 00 00","GI probe A packed"],
    [5370721931,"ba 0b 00 00 00","EnvDiffuse A t11"],
    [5370721967,"ba 0d 00 00 00","EnvDiffuse B t13"],
    [5370722112,"ba 0c 00 00 00","EnvSpec A t12"],
    [5370722147,"ba 0e 00 00 00","EnvSpec B t14"]]
RELATIVE = [
    [5374438470,5370948560,"first 0x804D serialize"],
    [5374438558,5370911552,"second 0x804D serialize"],
    [5370948812,5370640000,"first controller deserialize"],
    [5370911804,5370640000,"second controller deserialize"],
    [5370898362,5370547168,"first model map lightbank"],
    [5370938937,5370547168,"second model map lightbank"],
    [5370938930,5370547216,"second model alternate lightbank"],
    [5370547189,5370571008,"map source resolved"],
    [5370547237,5370571840,"character source resolved"],
    [5370571262,5374360448,"map single native hook"],
    [5370571717,5374360624,"map blended native hook"],
    [5370572117,5374360448,"chr single native hook"],
    [5370572618,5374360624,"chr blended native hook"],
    [5370864523,5370721056,"packed GI binder"],
    [5370721826,5374361488,"EnvDif A name"],
    [5370721898,5374361488,"EnvDif B name"],
    [5370722011,5374361600,"EnvSpc A name"],
    [5370722081,5374361600,"EnvSpc B name"]]
for va, expected, label in EXACT: exact(va, expected, label)
for va, target, label in RELATIVE: relative(va, target, label)
print("STATIC_RE_PASS", len(EXACT), "exact byte windows,", len(RELATIVE),
      "native xrefs. Receiver/pixel=OPEN.")
