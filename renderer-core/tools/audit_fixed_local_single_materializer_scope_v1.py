#!/usr/bin/env python3
"""Static source-corpus audit for the fixed single-endpoint direct-PTDE island.

Input is an extracted vanilla DSR FRPG_FlverPBL_fpo_DX11 shader directory.
The audit never patches game files. It verifies the exact structural surface
consumed by fixed_local_specular_single_materializer.cpp.
"""
from __future__ import annotations
import argparse
import collections
import hashlib
import os
import re
import struct

NAME = re.compile(
    r"^\d+_(FRPG_Phn_DifSpc.*_HemEnv(?:Lerp)?PntSS(?:SS)?\.fpo)$"
)
SCHLICK = 0xC0B1C059

def u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]

def chunks(data: bytes):
    if data[:4] != b"DXBC":
        raise ValueError("not DXBC")
    for i in range(u32(data, 28)):
        off = u32(data, 32 + 4 * i)
        size = u32(data, off + 4)
        yield data[off:off+4], data[off+8:off+8+size]

def code_words(data: bytes):
    hits = []
    for tag, payload in chunks(data):
        if tag in (b"SHEX", b"SHDR"):
            hits.append(list(struct.unpack(
                "<%dI" % (len(payload) // 4), payload)))
    if len(hits) != 1:
        raise ValueError("code chunk count")
    return hits[0]

def instructions(words):
    out = []
    at = 2
    while at < len(words):
        opcode = words[at] & 0x7FF
        length = words[at+1] if opcode == 53 else ((words[at] >> 24) & 0x7F)
        if not length or at + length > len(words):
            raise ValueError("bad instruction")
        out.append((at, at + length, opcode, words[at:at+length]))
        at += length
    if at != len(words):
        raise ValueError("trailing words")
    return out

def sample_resource(iw):
    for k, token in enumerate(iw[:-1]):
        if ((token >> 12) & 0xFF) == 7 and ((token >> 20) & 3) == 1:
            return iw[k+1]
    return None

def color0_register(data: bytes):
    hits = []
    for tag, payload in chunks(data):
        if tag != b"ISGN":
            continue
        count = u32(payload, 0)
        for i in range(count):
            at = 8 + 24 * i
            noff = u32(payload, at)
            semantic_index = u32(payload, at + 4)
            end = payload.find(b"\0", noff)
            if end < 0:
                raise ValueError("unterminated semantic")
            name = payload[noff:end].decode("ascii")
            if name == "COLOR" and semantic_index == 0:
                hits.append(u32(payload, at + 16))
    return hits

def audit_one(path: str):
    data = open(path, "rb").read()
    words = code_words(data)
    ins = instructions(words)

    stack = []
    pairs = {}
    anchors = []
    for i, item in enumerate(ins):
        if item[2] == 31:
            stack.append(i)
        elif item[2] == 21:
            pairs[stack.pop()] = i
        if SCHLICK in item[3]:
            anchors.append(i)

    expected_lights = 4 if "PntSSSS" in path else 2
    if len(anchors) != expected_lights:
        raise ValueError("light anchor count")

    first_if = max(i for i in pairs if i < anchors[0] and pairs[i] > anchors[0])
    pre = ins[:first_if]
    slots = collections.Counter(
        sample_resource(x[3])
        for x in pre
        if 69 <= x[2] <= 74 and sample_resource(x[3]) is not None
    )

    blended = slots[3] == 1 and slots[4] == 1
    if blended:
        return "blended", expected_lights

    t0 = [
        i for i, x in enumerate(pre)
        if 69 <= x[2] <= 74 and sample_resource(x[3]) == 0
    ]
    t1 = [
        i for i, x in enumerate(pre)
        if 69 <= x[2] <= 74 and sample_resource(x[3]) == 1
    ]
    if len(t0) != 1 or len(t1) != 1:
        raise ValueError("single endpoint count")
    if pre[t1[0]][3][3] != 0x001000F2:
        raise ValueError("t1 destination token")
    if t0[0] + 1 >= len(pre):
        raise ValueError("missing c156 add")
    c156 = pre[t0[0] + 1]
    if c156[2] != 0 or 156 not in c156[3]:
        raise ValueError("t0+c156 seam")
    if len(color0_register(data)) != 1:
        raise ValueError("COLOR0 signature")

    for ordinal, anchor in enumerate(anchors):
        start = max(i for i in pairs if i < anchor and pairs[i] > anchor)
        end = pairs[start]
        if start < 4 or [ins[start-k][2] for k in (4,3,2,1)] != [0,16,75,49]:
            raise ValueError("geometry prefix")

        found_linear = False
        for j in range(start, end - 3):
            q = ins[j:j+4]
            if [x[2] for x in q] != [0,0,14,0]:
                continue
            if [x[1]-x[0] for x in q] != [9,10,7,8]:
                continue
            if not (q[3][3][0] & 0x2000):
                continue
            if (112 + ordinal) not in q[0][3]:
                continue
            if (112 + ordinal) not in q[1][3] or (116 + ordinal) not in q[1][3]:
                continue
            found_linear = True
            break
        if not found_linear:
            raise ValueError("linear attenuation template")

    final_start = max(i for i in pairs if i < anchors[-1] and pairs[i] > anchors[-1])
    final_end = pairs[final_start]
    cut = ins[final_end + 1]
    if cut[2] != 0 or cut[1] - cut[0] != 7 or cut[3][5] != 0x00100246:
        raise ValueError("immediate post-ENDIF output cut")

    return "single", expected_lights

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("directory")
    args = ap.parse_args()

    unique = {}
    for name in os.listdir(args.directory):
        if not NAME.match(name):
            continue
        path = os.path.join(args.directory, name)
        data = open(path, "rb").read()
        unique.setdefault(hashlib.sha256(data).hexdigest(), path)

    if len(unique) != 48:
        raise SystemExit(f"expected 48 unique fixed Spc bodies, got {len(unique)}")

    classes = collections.Counter()
    lights = collections.Counter()
    failures = []
    for sha, path in sorted(unique.items()):
        try:
            cls, count = audit_one(path)
            classes[cls] += 1
            if cls == "single":
                lights[count] += 1
        except Exception as exc:
            failures.append((os.path.basename(path), str(exc)))

    print("unique_fixed_spc=48")
    print(f"single={classes['single']} blended={classes['blended']}")
    print(f"single_pntss={lights[2]} single_pntssss={lights[4]}")
    print(f"failures={len(failures)}")
    for name, reason in failures:
        print(f"FAIL\t{name}\t{reason}")

    if failures or classes["single"] != 24 or classes["blended"] != 24:
        raise SystemExit(1)
    if lights[2] != 12 or lights[4] != 12:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
