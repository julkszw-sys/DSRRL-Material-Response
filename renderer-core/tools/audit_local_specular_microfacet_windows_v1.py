#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import re
import struct
from collections import Counter
from pathlib import Path

TARGET_RE = re.compile(
    r"^FRPG_Phn_DifSpc.*_(HemEnv|HemEnvLerp)Pnt(S|SS|SSSS)\.fpo$"
)

EXPECTED = {
    "clustered_spc_pnts": {"unique": 24, "aliases": 48, "lights": 1},
    "fixed_spc_pntss": {"unique": 24, "aliases": 48, "lights": 2},
    "fixed_spc_pntssss": {"unique": 24, "aliases": 48, "lights": 4},
}

FLOAT_ANCHORS = {
    "schlick_a": -5.55473,
    "schlick_b": -6.98316,
    "quarter": 0.25,
    "inv_pi": 0.3183098861837907,
    "pi": 3.141592653589793,
}

OP_DP3 = 16
OP_ENDIF = 21
OP_EXP = 25
OP_IF = 31
OP_MAD = 50
OP_CUSTOMDATA = 53
OP_MUL = 56

def fail(message: str) -> None:
    raise SystemExit(message)

def classify(name: str) -> str:
    if name.endswith("PntSSSS.fpo"):
        return "fixed_spc_pntssss"
    if name.endswith("PntSS.fpo"):
        return "fixed_spc_pntss"
    if name.endswith("PntS.fpo"):
        return "clustered_spc_pnts"
    fail(f"unsupported target name: {name}")

def float_bytes(value: float) -> bytes:
    return struct.pack("<f", value)

def float_word(value: float) -> int:
    return struct.unpack("<I", float_bytes(value))[0]

def offsets(blob: bytes, needle: bytes) -> list[int]:
    found: list[int] = []
    start = 0
    while True:
        pos = blob.find(needle, start)
        if pos < 0:
            return found
        found.append(pos)
        start = pos + 1

def shex_words(blob: bytes) -> list[int]:
    if len(blob) < 36 or blob[:4] != b"DXBC":
        fail("invalid DXBC container")
    chunk_count = struct.unpack_from("<I", blob, 28)[0]
    if 32 + 4 * chunk_count > len(blob):
        fail("invalid DXBC chunk table")
    for i in range(chunk_count):
        off = struct.unpack_from("<I", blob, 32 + 4 * i)[0]
        if off + 8 > len(blob):
            continue
        tag = blob[off:off+4]
        size = struct.unpack_from("<I", blob, off + 4)[0]
        if tag not in (b"SHEX", b"SHDR"):
            continue
        if size % 4 or off + 8 + size > len(blob):
            fail("invalid SHEX/SHDR chunk")
        return list(struct.unpack_from(f"<{size // 4}I", blob, off + 8))
    fail("DXBC has no SHEX/SHDR chunk")

def instructions(words: list[int]) -> list[dict]:
    out: list[dict] = []
    p = 2
    while p < len(words):
        start = p
        token = words[p]
        opcode = token & 0x7ff
        if opcode == OP_CUSTOMDATA:
            if p + 1 >= len(words):
                fail("truncated CUSTOMDATA instruction")
            length = words[p + 1]
        else:
            length = (token >> 24) & 0x7f
        if length <= 0 or p + length > len(words):
            fail(f"invalid instruction length at SHEX word {p}")
        raw = words[p:p+length]
        out.append({
            "start": start,
            "end": p + length,
            "opcode": opcode,
            "raw": raw,
        })
        p += length
    if p != len(words):
        fail("SHEX instruction stream did not terminate exactly")
    return out

def valid_window_count(receiver_class: str, count: int) -> bool:
    if receiver_class == "clustered_spc_pnts":
        return count in (34, 37)
    return count in (60, 61, 62, 63)

def microfacet_windows(blob: bytes, receiver_class: str) -> list[dict]:
    words = shex_words(blob)
    ins = instructions(words)
    schlick_a = float_word(FLOAT_ANCHORS["schlick_a"])
    schlick_b = float_word(FLOAT_ANCHORS["schlick_b"])

    stack: list[int] = []
    if_end: dict[int, int] = {}
    for i, inst in enumerate(ins):
        if inst["opcode"] == OP_IF:
            stack.append(i)
        elif inst["opcode"] == OP_ENDIF:
            if not stack:
                fail("ENDIF without IF")
            if_end[stack.pop()] = i
    if stack:
        fail("unclosed IF in target shader")

    anchors: list[int] = []
    for i, inst in enumerate(ins):
        if (
            inst["opcode"] == OP_MAD
            and schlick_a in inst["raw"]
            and schlick_b in inst["raw"]
        ):
            anchors.append(i)

    expected = EXPECTED[receiver_class]["lights"]
    if len(anchors) != expected:
        fail(f"expected {expected} Schlick anchors, got {len(anchors)}")

    result: list[dict] = []
    for ordinal, anchor in enumerate(anchors):
        if anchor < 3 or anchor + 2 >= len(ins):
            fail("Schlick anchor too close to stream boundary")
        shape = [
            ins[anchor-3]["opcode"],
            ins[anchor-2]["opcode"],
            ins[anchor-1]["opcode"],
            ins[anchor]["opcode"],
            ins[anchor+1]["opcode"],
            ins[anchor+2]["opcode"],
        ]
        if shape != [OP_DP3, OP_DP3, OP_DP3, OP_MAD, OP_MUL, OP_EXP]:
            fail(f"unexpected Schlick opcode shape: {shape}")

        containing = [
            (start, end)
            for start, end in if_end.items()
            if start < anchor < end
        ]
        if not containing:
            fail("Schlick anchor is not inside an IF/ENDIF window")
        start, end = max(containing, key=lambda pair: pair[0])
        count = end - start + 1
        if not valid_window_count(receiver_class, count):
            fail(
                f"unexpected {receiver_class} window instruction count: {count}"
            )
        local_anchors = sum(
            1
            for i in range(start, end + 1)
            if (
                ins[i]["opcode"] == OP_MAD
                and schlick_a in ins[i]["raw"]
                and schlick_b in ins[i]["raw"]
            )
        )
        if local_anchors != 1:
            fail("per-light IF window does not own exactly one microfacet anchor")

        opcode_bytes = b"".join(
            struct.pack("<H", ins[i]["opcode"])
            for i in range(start, end + 1)
        )
        result.append({
            "light_ordinal": ordinal,
            "start_word": ins[start]["start"],
            "schlick_word": ins[anchor]["start"],
            "end_word_exclusive": ins[end]["end"],
            "instruction_count": count,
            "opcode_fingerprint": hashlib.sha256(opcode_bytes).hexdigest(),
        })
    return result

def main() -> int:
    ap = argparse.ArgumentParser(
        description=(
            "Audit the exact vanilla DSR Phn local-PointLight microfacet "
            "literal signature and token-level per-light operator windows."
        )
    )
    ap.add_argument("--shader-dir", required=True)
    ap.add_argument("--output")
    ap.add_argument("--windows-output")
    args = ap.parse_args()

    root = Path(args.shader_dir)
    if not root.is_dir():
        fail(f"shader directory does not exist: {root}")

    aliases: list[tuple[str, str, bytes]] = []
    for path in sorted(root.rglob("*.fpo")):
        if not TARGET_RE.fullmatch(path.name):
            continue
        blob = path.read_bytes()
        aliases.append((classify(path.name), path.name, blob))

    if len(aliases) != 144:
        fail(f"expected 144 exact HemEnv/HemEnvLerp aliases, got {len(aliases)}")

    unique: dict[tuple[str, str], dict] = {}
    alias_counts = Counter()
    for receiver_class, name, blob in aliases:
        digest = hashlib.sha256(blob).hexdigest()
        alias_counts[receiver_class] += 1
        key = (receiver_class, digest)
        row = unique.setdefault(
            key,
            {
                "receiver_class": receiver_class,
                "sha256": digest,
                "code_size": len(blob),
                "names": [],
                "blob": blob,
            },
        )
        if row["code_size"] != len(blob) or row["blob"] != blob:
            fail(f"SHA collision or inconsistent payload: {digest}")
        row["names"].append(name)

    class_unique = Counter(k[0] for k in unique)
    for receiver_class, expected in EXPECTED.items():
        if alias_counts[receiver_class] != expected["aliases"]:
            fail(
                f"{receiver_class}: expected {expected['aliases']} aliases, "
                f"got {alias_counts[receiver_class]}"
            )
        if class_unique[receiver_class] != expected["unique"]:
            fail(
                f"{receiver_class}: expected {expected['unique']} unique bodies, "
                f"got {class_unique[receiver_class]}"
            )

    if len(unique) != 72:
        fail(f"expected 72 unique DXBC bodies, got {len(unique)}")

    output_rows = []
    window_rows = []
    fingerprint_counts = Counter()

    for (receiver_class, digest), row in sorted(
        unique.items(), key=lambda item: (item[0][0], min(item[1]["names"]))
    ):
        blob = row["blob"]
        light_count = EXPECTED[receiver_class]["lights"]
        anchor_offsets = {
            key: offsets(blob, float_bytes(value))
            for key, value in FLOAT_ANCHORS.items()
        }
        expected_counts = {
            "schlick_a": light_count,
            "schlick_b": light_count,
            "quarter": light_count,
            "inv_pi": 4 * light_count,
            "pi": 3 * light_count,
        }
        actual_counts = {key: len(value) for key, value in anchor_offsets.items()}
        if actual_counts != expected_counts:
            fail(
                f"{digest}: microfacet signature mismatch: "
                f"expected {expected_counts}, got {actual_counts}"
            )
        if len(row["names"]) != 2:
            fail(
                f"{digest}: expected exact HemEnv/HemEnvLerp alias pair, "
                f"got {len(row['names'])}"
            )

        windows = microfacet_windows(blob, receiver_class)
        if len(windows) != light_count:
            fail(f"{digest}: wrong per-light window count")

        output_rows.append({
            "receiver_class": receiver_class,
            "code_sha256": digest,
            "code_size": row["code_size"],
            "alias_count": len(row["names"]),
            "representative_shader_name": min(row["names"]),
            "schlick_a_offsets": ",".join(map(str, anchor_offsets["schlick_a"])),
            "schlick_b_offsets": ",".join(map(str, anchor_offsets["schlick_b"])),
            "quarter_offsets": ",".join(map(str, anchor_offsets["quarter"])),
            "inv_pi_offsets": ",".join(map(str, anchor_offsets["inv_pi"])),
            "pi_offsets": ",".join(map(str, anchor_offsets["pi"])),
        })

        for window in windows:
            fingerprint_counts[
                (receiver_class, window["opcode_fingerprint"])
            ] += 1
            window_rows.append({
                "receiver_class": receiver_class,
                "code_sha256": digest,
                "code_size": row["code_size"],
                "representative_shader_name": min(row["names"]),
                **window,
            })

    expected_window_total = 24 * (1 + 2 + 4)
    if len(window_rows) != expected_window_total:
        fail(
            f"expected {expected_window_total} unique per-light windows, "
            f"got {len(window_rows)}"
        )

    fingerprints_by_class = {
        receiver_class: {
            fingerprint
            for (klass, fingerprint), count in fingerprint_counts.items()
            if klass == receiver_class and count > 0
        }
        for receiver_class in EXPECTED
    }
    if len(fingerprints_by_class["clustered_spc_pnts"]) != 2:
        fail("expected exactly 2 clustered PntS opcode shapes")
    if len(fingerprints_by_class["fixed_spc_pntss"]) != 4:
        fail("expected exactly 4 fixed PntSS opcode shapes")
    if len(fingerprints_by_class["fixed_spc_pntssss"]) != 4:
        fail("expected exactly 4 fixed PntSSSS opcode shapes")

    if args.output:
        out = Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        with out.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(
                handle,
                delimiter="\t",
                fieldnames=list(output_rows[0].keys()),
            )
            writer.writeheader()
            writer.writerows(output_rows)

    if args.windows_output:
        out = Path(args.windows_output)
        out.parent.mkdir(parents=True, exist_ok=True)
        with out.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(
                handle,
                delimiter="\t",
                fieldnames=list(window_rows[0].keys()),
            )
            writer.writeheader()
            writer.writerows(window_rows)

    print(
        "LOCAL_SPECULAR_MICROFACET_AUDIT_PASS "
        "aliases=144 unique=72 windows=168 "
        "clustered=24 fixed2=24 fixed4=24 "
        "literal_shape=1_2_4 opcode_shapes=2_4_4"
    )
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
