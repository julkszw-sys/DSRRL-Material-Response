#!/usr/bin/env python3
"""V232 exact nonmetal-vs-P_Metal HemEnvLerp MATERIAL input census.

SOURCE ONLY. It does not claim consumer or PTDE pixel equivalence.
Reads source-controlled raw-MTD router + exact FLVER/slot manifest + R19
shader rewrite, and reports the *three distinct* material domains admitted
by V203. No D3D hook, shader, constant-buffer or resource mutation.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ROUTER = "renderer-core/include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
MANIFEST = "renderer-core/include/dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp"
LERP = "renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"
DRAW = "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"
GATE = "renderer-core/include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp"

EXPECTED = {
    "P_DullLeather[DSB].mtd": {"slot": 0, "c101": 1.0, "c102": 2.0, "count": 12},
    "P_Leather[DSB].mtd": {"slot": 1, "c101": 1.5, "c102": 4.0, "count": 12},
    "P_Metal[DSB].mtd": {"slot": 2, "c101": 2.5, "c102": 8.5, "count": 12},
}

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8").replace("\r\n", "\n")

def float_bits(word: str) -> float:
    return struct.unpack("<f", struct.pack("<I", int(word.rstrip("u"), 16)))[0]

def parse_router(text: str, name: str) -> dict:
    lines = text.splitlines()
    matches = [i for i, line in enumerate(lines)
               if re.search(r'^\s*"' + re.escape(name) + r'",', line)]
    if len(matches) != 1:
        raise AssertionError(f"{name}: expected 1 exact raw-MTD row, got {len(matches)}")
    i = matches[0]
    if i + 2 >= len(lines):
        raise AssertionError(f"{name}: truncated raw-MTD record")
    hdr = lines[i+1]
    values = lines[i+2]
    slot = re.search(r'envspec_router_state::present,\s*(\d+)u,\s*false', hdr)
    constants = re.fullmatch(
        r'\s*\{\{(0x[0-9a-fA-F]+u),(0x[0-9a-fA-F]+u),(0x[0-9a-fA-F]+u)\}\},'
        r'\s*(0x[0-9a-fA-F]+u),\s*'
        r'\{\{(0x[0-9a-fA-F]+u),(0x[0-9a-fA-F]+u),(0x[0-9a-fA-F]+u)\}\}',
        values.strip().rstrip(","),
    )
    if not slot or not constants:
        raise AssertionError(f"{name}: raw-MTD parser shape drift")
    c101 = [float_bits(constants.group(i)) for i in (1,2,3)]
    c100 = [float_bits(constants.group(i)) for i in (5,6,7)]
    return {
        "slot": int(slot.group(1)),
        "c101_rgb": c101,
        "c102": float_bits(constants.group(4)),
        "c100_rgb": c100,
    }

def parse_manifest(text: str) -> list:
    rows = re.findall(
        r'^\s*\{\s*"([0-9a-f]{64})",\s*(\d+)u,\s*"([^"]+)",\s*"([^"]+)"\s*\},',
        text, flags=re.M
    )
    if len(rows) != 36 or len(set(rows)) != 36:
        raise AssertionError(f"exact FLVER material owner manifest drift: {len(rows)}")
    return rows

def assert_operator_cut(lerp: str, draw: str, gate: str) -> None:
    begin = lerp.index("bool apply_exact_ptde_lerp_materialworkflow_r15_r16_r17(")
    end = lerp.index("bool pmetal_lerp_materialworkflow_postcondition(", begin)
    cut = lerp[begin:end]
    # R19 edits a shared shader DXBC, without material-specific c101/c102.
    if "material_identity" in cut or "envspc_slot" in cut:
        raise AssertionError("R19 cut is no longer shader-local: re-audit consumers")
    needles = [
        "additive_weight_hits != 1u",
        "diffuse_split_hits != 1u",
        "workflow_material_hits != 1u",
        "words[additive_gate_word + 3u] = 0x00004001u",
        "words[alpha_mul_word + 3u] = 0x00004001u",
        "words[diffuse_weight_mul_word + 3u] = 0x00004001u",
        "words[workflow_mad_word + 3u] = 0x00004001u",
        "postmerge_gate_hits != 1u",
    ]
    for marker in needles:
        if marker not in cut:
            raise AssertionError("R19 MaterialWorkflow site changed: " + marker)
    if "env_semantics.envspc_slot" not in draw or (
        "prepared.material_resources.spec_rgb" not in draw
    ):
        raise AssertionError("resource/SpecRGB prepare route changed")
    if "material.route_index == 345u" not in gate or (
        "exact_envspec36_draw" not in gate
    ):
        raise AssertionError("P_Metal/nonmetal exact-route separation changed")

def census() -> dict:
    router = read(ROUTER)
    manifest = read(MANIFEST)
    lerp, draw, gate = read(LERP), read(DRAW), read(GATE)
    assert_operator_cut(lerp, draw, gate)
    rows = parse_manifest(manifest)
    counts = collections.Counter(row[2] for row in rows)
    domains = {}
    for mtd, want in EXPECTED.items():
        record = parse_router(router, mtd)
        if counts[mtd] != want["count"] or (
            record["slot"] != want["slot"]
        ) or record["c101_rgb"] != [want["c101"]]*3 or (
            record["c102"] != want["c102"]
        ):
            raise AssertionError(f"{mtd}: nonmetal material domain drift: {record}")
        domains[mtd] = {
            **record, "exact_owner_rows": counts[mtd],
            "authority": "raw-MTD SHA + exact FLVER SHA + material slot",
        }
    if set(counts) != set(EXPECTED):
        raise AssertionError("unexpected material family admitted")
    # A shared Lerp beta/probe ordinal does NOT imply shared PTDE material.
    return {
        "schema": "dsrrl.v232.nonmetal_lerp_material_cut.v1",
        "source_sha256": {
            path: hashlib.sha256(read(path).encode()).hexdigest()
            for path in (ROUTER, MANIFEST, LERP, DRAW, GATE)
        },
        "exact_owner_rows": len(rows),
        "material_domains": domains,
        "shared_R19_DXBC_materialworkflow_rewrite": True,
        "PTDE_material_consumer_equivalence": "OPEN",
        "nonmetal_stable_pixel": "FAIL_OWNER_REPORTED_OVERGLOSS",
        "nonmetal_lerp_pixel": "FAIL_OWNER_REPORTED_LAMINATE",
        "P_Metal_lerp_control": "OWNER_REPORTED_PASS",
        "shader_or_addon_modified": False,
        "next_cut": "separately prove PTDE nonmetal material response at t1/t10/c100/c101/c102 and postmerge SpecTex-alpha/F0 path",
    }

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = census()
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered)

if __name__ == "__main__":
    main()
