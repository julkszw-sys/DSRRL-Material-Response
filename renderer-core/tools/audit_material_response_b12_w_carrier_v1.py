#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

TARGET_RE = re.compile(
    r"^FRPG_Phn_DifSpc.*_(HemEnv|HemEnvLerp)\.fpo$"
)

EXPECTED_ALIASES = 48
EXPECTED_STABLE = 24
EXPECTED_LERP = 24

def fail(message: str) -> None:
    raise SystemExit(message)

def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]

def code_words(blob: bytes) -> list[int]:
    if len(blob) < 32:
        fail("invalid DXBC header")
    count = u32(blob, 28)
    if count == 0 or count > 64 or 32 + 4 * count > len(blob):
        fail("invalid DXBC chunk table")

    payload = None
    for i in range(count):
        off = u32(blob, 32 + 4 * i)
        if off > len(blob) or len(blob) - off < 8:
            fail("invalid DXBC chunk offset")
        tag = blob[off : off + 4]
        size = u32(blob, off + 4)
        if size > len(blob) - off - 8:
            fail("invalid DXBC chunk size")
        if tag not in (b"SHEX", b"SHDR"):
            continue
        if payload is not None:
            fail("multiple shader-code chunks")
        payload = blob[off + 8 : off + 8 + size]

    if payload is None or len(payload) % 4:
        fail("missing or malformed shader-code chunk")

    words = list(struct.unpack("<%dI" % (len(payload) // 4), payload))
    if len(words) < 3 or words[1] != len(words):
        fail("invalid shader word count")
    return words

def uses_w(swizzle: int) -> bool:
    return any(((swizzle >> (2 * lane)) & 3) == 3 for lane in range(4))

def cb0_material_refs(words: list[int]) -> list[tuple[int, int, int, int]]:
    refs: list[tuple[int, int, int, int]] = []
    for i in range(len(words) - 2):
        token = words[i]
        operand_type = (token >> 12) & 0xFF
        dimension = (token >> 20) & 0x3
        rep0 = (token >> 22) & 0x7
        rep1 = (token >> 25) & 0x7
        if (
            operand_type == 8
            and dimension == 2
            and rep0 == 0
            and rep1 == 0
            and words[i + 1] == 0
            and words[i + 2] in (9, 10)
        ):
            mode = (token >> 2) & 0x3
            swizzle = (token >> 4) & 0xFF
            refs.append((words[i + 2], mode, swizzle, token))
    return refs

def main() -> int:
    ap = argparse.ArgumentParser(
        description=(
            "Prove that the 48 exact Material Response HemEnv/HemEnvLerp "
            "stock hosts never consume .w from the cb0[9]/cb0[10] operands "
            "rewired by V2.11 to b12[1]/b12[0]. This certifies b12[0].w as "
            "an orthogonal PTDE g_SpecularPower carrier."
        )
    )
    ap.add_argument("--shader-dir", required=True)
    args = ap.parse_args()

    root = Path(args.shader_dir)
    if not root.is_dir():
        fail(f"shader directory does not exist: {root}")

    targets = [
        p for p in sorted(root.rglob("*.fpo"))
        if TARGET_RE.fullmatch(p.name)
    ]
    if len(targets) != EXPECTED_ALIASES:
        fail(f"expected {EXPECTED_ALIASES} exact MR hosts, got {len(targets)}")

    stable = 0
    lerp = 0
    refs_total = 0
    for path in targets:
        if path.name.endswith("_HemEnvLerp.fpo"):
            lerp += 1
        elif path.name.endswith("_HemEnv.fpo"):
            stable += 1
        else:
            fail(f"unexpected target family: {path.name}")

        refs = cb0_material_refs(code_words(path.read_bytes()))
        if not refs:
            fail(f"{path.name}: no cb0[9]/cb0[10] material refs found")

        for index, mode, swizzle, token in refs:
            if mode != 1:
                fail(
                    f"{path.name}: cb0[{index}] non-swizzle operand "
                    f"token=0x{token:08x}"
                )
            if uses_w(swizzle):
                fail(
                    f"{path.name}: cb0[{index}] consumes .w "
                    f"token=0x{token:08x}"
                )
        refs_total += len(refs)

    if stable != EXPECTED_STABLE or lerp != EXPECTED_LERP:
        fail(f"family count mismatch stable={stable} lerp={lerp}")

    print(
        "MR_B12_W_CARRIER_AUDIT_PASS "
        f"hosts={len(targets)} stable={stable} lerp={lerp} "
        f"material_refs={refs_total} w_consumers=0 carrier=b12[0].w"
    )
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
