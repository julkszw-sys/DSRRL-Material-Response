#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import re
import struct
from collections import Counter
from pathlib import Path

TARGET_RE = re.compile(
    r"^\d+_FRPG_Phn_DifSpc.*_(HemEnv|HemEnvLerp)Pnt(SS|SSSS)\.fpo$"
)

OP_DP3 = 16
OP_ENDIF = 21
OP_IF = 31
OP_MAD = 50
SCHLICK_A = struct.unpack("<I", struct.pack("<f", -5.55473))[0]
SCHLICK_B = struct.unpack("<I", struct.pack("<f", -6.98316))[0]

EXPECTED_ROLE_COUNTS = sorted([48, 48, 20, 20, 4, 4])
EXPECTED_WINDOW_COUNTS = {60: 28, 61: 68, 62: 14, 63: 34}

def fail(message: str) -> None:
    raise SystemExit(message)

def chunks(blob: bytes):
    if len(blob) < 36 or blob[:4] != b"DXBC":
        fail("invalid DXBC")
    count = struct.unpack_from("<I", blob, 28)[0]
    if 32 + 4 * count > len(blob):
        fail("bad DXBC table")
    for i in range(count):
        off = struct.unpack_from("<I", blob, 32 + 4 * i)[0]
        if off + 8 > len(blob):
            fail("bad chunk offset")
        tag = blob[off:off+4]
        size = struct.unpack_from("<I", blob, off + 4)[0]
        if off + 8 + size > len(blob):
            fail("bad chunk size")
        yield tag, blob[off+8:off+8+size]

def shex_words(blob: bytes) -> list[int]:
    hit = None
    for tag, data in chunks(blob):
        if tag not in (b"SHEX", b"SHDR"):
            continue
        if hit is not None or len(data) % 4:
            fail("ambiguous code chunk")
        hit = list(struct.unpack(f"<{len(data)//4}I", data))
    if hit is None or len(hit) < 3 or hit[1] != len(hit):
        fail("missing/invalid SHEX")
    return hit

def parse_operand(words: list[int], at: int, end: int):
    start = at
    token = words[at]
    at += 1
    num = token & 3
    typ = (token >> 12) & 0xff
    dim = (token >> 20) & 3
    while token & 0x80000000:
        token = words[at]
        at += 1
    indices = []
    for d in range(dim):
        rep = (words[start] >> (22 + 3*d)) & 7
        if rep != 0:
            fail("operand audit requires immediate32 register indices")
        indices.append(words[at])
        at += 1
    if typ == 4:
        at += {1: 1, 2: 4}.get(num, 0)
    elif typ == 5:
        at += {1: 2, 2: 8}.get(num, 0)
    if at > end:
        fail("operand overrun")
    return {
        "start": start,
        "end": at,
        "token": words[start],
        "type": typ,
        "indices": indices,
        "pair": tuple(words[start:at]),
    }, at

def instructions(words: list[int]):
    out = []
    at = 2
    while at < len(words):
        start = at
        token = words[at]
        opcode = token & 0x7ff
        length = words[at+1] if opcode == 53 else (token >> 24) & 0x7f
        if length <= 0 or at + length > len(words):
            fail(f"bad instruction at {at}")
        end = at + length
        at += 1
        while token & 0x80000000:
            token = words[at]
            at += 1
        ops = []
        try:
            while at < end:
                op, at = parse_operand(words, at, end)
                ops.append(op)
        except SystemExit:
            raise
        except Exception:
            ops = []
            at = end
        out.append({
            "start": start,
            "end": end,
            "opcode": opcode,
            "raw": words[start:end],
            "ops": ops,
        })
        at = end
    return out

def has_cb0_index(ins: dict, index: int) -> bool:
    for op in ins["ops"]:
        if op["type"] == 8 and op["indices"] == [0, index]:
            return True
    return False

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--shader-dir", required=True)
    args = ap.parse_args()

    root = Path(args.shader_dir)
    unique = {}
    for path in root.glob("*.fpo"):
        if not TARGET_RE.fullmatch(path.name):
            continue
        blob = path.read_bytes()
        unique.setdefault(hashlib.sha256(blob).hexdigest(), (path, blob))

    if len(unique) != 48:
        fail(f"expected 48 unique fixed Spc bodies, got {len(unique)}")

    role_shapes = Counter()
    window_counts = Counter()
    total_windows = 0

    for digest, (path, blob) in sorted(unique.items()):
        words = shex_words(blob)
        ins = instructions(words)

        stack = []
        if_end = {}
        for i, item in enumerate(ins):
            if item["opcode"] == OP_IF:
                stack.append(i)
            elif item["opcode"] == OP_ENDIF:
                if not stack:
                    fail(f"{path.name}: ENDIF without IF")
                if_end[stack.pop()] = i
        if stack:
            fail(f"{path.name}: unclosed IF")

        anchors = [
            i for i, item in enumerate(ins)
            if item["opcode"] == OP_MAD
            and SCHLICK_A in item["raw"]
            and SCHLICK_B in item["raw"]
        ]
        expected = 4 if path.name.endswith("PntSSSS.fpo") else 2
        if len(anchors) != expected:
            fail(f"{path.name}: expected {expected} anchors, got {len(anchors)}")

        for ordinal, anchor in enumerate(anchors):
            containing = [
                (start, end)
                for start, end in if_end.items()
                if start < anchor < end
            ]
            if not containing:
                fail(f"{path.name}: anchor outside IF")
            start, end = max(containing, key=lambda pair: pair[0])
            count = end - start + 1
            window_counts[count] += 1
            total_windows += 1

            trio = ins[anchor-3:anchor]
            if len(trio) != 3 or [x["opcode"] for x in trio] != [OP_DP3]*3:
                fail(f"{path.name}: missing DP3 triplet")
            if any((x["end"]-x["start"]) != 7 or len(x["ops"]) != 3 for x in trio):
                fail(f"{path.name}: unexpected DP3 operand shape")

            # DP3(V,H), DP3(N,H), DP3(N,L)
            view = trio[0]["ops"][1]["pair"]
            h0 = trio[0]["ops"][2]["pair"]
            normal0 = trio[1]["ops"][1]["pair"]
            h1 = trio[1]["ops"][2]["pair"]
            normal1 = trio[2]["ops"][1]["pair"]
            light = trio[2]["ops"][2]["pair"]
            if h0 != h1 or normal0 != normal1:
                fail(f"{path.name}: N/H semantic identity mismatch")

            for pair in (view, h0, normal0, light):
                token = pair[0]
                typ = (token >> 12) & 0xff
                dim = (token >> 20) & 3
                if len(pair) != 2 or typ != 0 or dim != 1:
                    fail(f"{path.name}: N/V/L is not a direct temp register")

            role_shapes[(normal0, view, light, h0)] += 1

            position = 112 + ordinal
            color = 116 + ordinal
            window = ins[start:end+1]
            if not any(has_cb0_index(x, position) for x in window):
                fail(f"{path.name}: missing cb0[{position}] fixed position/begin")
            if not any(has_cb0_index(x, color) for x in window):
                fail(f"{path.name}: missing cb0[{color}] fixed color/end")

            after_switch = False
            tail_color = False
            for item in window:
                if item["opcode"] == 23:
                    after_switch = True
                elif after_switch and item["opcode"] == 56 and has_cb0_index(item, color):
                    tail_color = True
            if not tail_color:
                fail(f"{path.name}: missing stock fixed light-color tail")

    if total_windows != 144:
        fail(f"expected 144 fixed per-light windows, got {total_windows}")
    if dict(sorted(window_counts.items())) != EXPECTED_WINDOW_COUNTS:
        fail(f"unexpected window distribution: {dict(window_counts)}")
    if len(role_shapes) != 6:
        fail(f"expected 6 N/V/L/H register-allocation shapes, got {len(role_shapes)}")
    if sorted(role_shapes.values()) != EXPECTED_ROLE_COUNTS:
        fail(f"unexpected role-shape counts: {sorted(role_shapes.values())}")

    print(
        "FIXED_LOCAL_SPECULAR_OPERAND_AUDIT_PASS "
        "unique=48 windows=144 role_shapes=6 "
        "window_counts=60:28,61:68,62:14,63:34 "
        "operand_mismatch=0 fixed_cb_mismatch=0"
    )
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
