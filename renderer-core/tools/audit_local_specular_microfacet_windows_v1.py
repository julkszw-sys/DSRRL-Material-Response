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
    "clustered_spc_pnts": {
        "unique": 24,
        "aliases": 48,
        "lights": 1,
    },
    "fixed_spc_pntss": {
        "unique": 24,
        "aliases": 48,
        "lights": 2,
    },
    "fixed_spc_pntssss": {
        "unique": 24,
        "aliases": 48,
        "lights": 4,
    },
}

FLOAT_ANCHORS = {
    "schlick_a": -5.55473,
    "schlick_b": -6.98316,
    "quarter": 0.25,
    "inv_pi": 0.3183098861837907,
    "pi": 3.141592653589793,
}

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

def offsets(blob: bytes, needle: bytes) -> list[int]:
    found: list[int] = []
    start = 0
    while True:
        pos = blob.find(needle, start)
        if pos < 0:
            return found
        found.append(pos)
        start = pos + 1

def main() -> int:
    ap = argparse.ArgumentParser(
        description=(
            "Audit the exact vanilla DSR Phn local-PointLight microfacet "
            "literal signature and emit one row per unique DXBC body."
        )
    )
    ap.add_argument("--shader-dir", required=True)
    ap.add_argument("--output")
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

        output_rows.append(
            {
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
            }
        )

    if args.output:
        out = Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        with out.open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(
                f,
                delimiter="\t",
                fieldnames=list(output_rows[0].keys()),
            )
            writer.writeheader()
            writer.writerows(output_rows)

    print(
        "LOCAL_SPECULAR_MICROFACET_AUDIT_PASS "
        "aliases=144 unique=72 "
        "clustered=24 fixed2=24 fixed4=24 "
        "literal_shape=1_2_4"
    )
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
