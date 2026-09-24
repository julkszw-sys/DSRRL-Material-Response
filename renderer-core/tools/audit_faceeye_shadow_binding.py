#!/usr/bin/env python3
import argparse
import hashlib
import json
import struct
from pathlib import Path

VARIANTS = [
    ("Csd", "1636_FRPG_Phn_FaceEye_Csd.fpo", "no-point", "comparison"),
    ("CsdPntS", "1637_FRPG_Phn_FaceEye_CsdPntS.fpo", "PntS", "comparison"),
    ("CsdPntSS", "1638_FRPG_Phn_FaceEye_CsdPntSS.fpo", "PntSS", "regular"),
    ("CsdPntSSSS", "1639_FRPG_Phn_FaceEye_CsdPntSSSS.fpo", "PntSSSS", "regular"),
    ("Sdw", "1640_FRPG_Phn_FaceEye_Sdw.fpo", "no-point", "comparison"),
    ("SdwPntS", "1641_FRPG_Phn_FaceEye_SdwPntS.fpo", "PntS", "comparison"),
    ("SdwPntSS", "1642_FRPG_Phn_FaceEye_SdwPntSS.fpo", "PntSS", "regular"),
    ("SdwPntSSSS", "1643_FRPG_Phn_FaceEye_SdwPntSSSS.fpo", "PntSSSS", "regular"),
]

def chunks(data):
    if data[:4] != b"DXBC":
        raise ValueError("not DXBC")
    count = struct.unpack_from("<I", data, 28)[0]
    offsets = struct.unpack_from("<" + "I" * count, data, 32)
    out = {}
    for offset in offsets:
        tag = data[offset:offset + 4].decode("ascii")
        size = struct.unpack_from("<I", data, offset + 4)[0]
        out[tag] = data[offset + 8:offset + 8 + size]
    return out

def cstr(blob, offset):
    end = blob.find(b"\0", offset)
    if end < 0:
        raise ValueError("unterminated RDEF string")
    return blob[offset:end].decode("ascii")

def resources(rdef):
    _, _, count, offset = struct.unpack_from("<4I", rdef, 0)
    rows = []
    for index in range(count):
        v = struct.unpack_from("<8I", rdef, offset + index * 32)
        rows.append({
            "name": cstr(rdef, v[0]),
            "type": v[1],
            "return_type": v[2],
            "dimension": v[3],
            "samples": v[4],
            "bind_point": v[5],
            "bind_count": v[6],
            "flags": v[7],
        })
    return rows

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--shader-dir", required=True)
    parser.add_argument("--mtd")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = Path(args.shader_dir)
    rows = []
    for label, filename, suffix, expected in VARIANTS:
        data = (root / filename).read_bytes()
        rdef = resources(chunks(data)["RDEF"])
        textures = [x for x in rdef if x["bind_point"] == 7 and x["type"] == 2]
        samplers = [x for x in rdef if x["bind_point"] == 7 and x["type"] == 3]
        if len(textures) != 1 or len(samplers) != 1:
            raise SystemExit(f"{filename}: expected exactly one t7 and one s7")
        mode = "comparison" if (samplers[0]["flags"] & 2) else "regular"
        rows.append({
            "variant": label,
            "suffix": suffix,
            "sha256": hashlib.sha256(data).hexdigest(),
            "t7": textures[0],
            "s7": samplers[0],
            "sampler_mode_from_rdef_flags": mode,
            "expected_mode": expected,
            "mode_matches_expected": mode == expected,
        })

    t7_set = {
        (
            r["t7"]["name"],
            r["t7"]["type"],
            r["t7"]["return_type"],
            r["t7"]["dimension"],
            r["t7"]["bind_point"],
            r["t7"]["bind_count"],
            r["t7"]["flags"],
        )
        for r in rows
    }

    mtd = {}
    if args.mtd:
        data = Path(args.mtd).read_bytes()
        mtd = {
            "sha256": hashlib.sha256(data).hexdigest(),
            "references_faceeye_spx": b"FRPG_Phn_FaceEye.spx" in data,
            "contains_gSMP_7": b"gSMP_7" in data,
            "contains_diffuse5": b"g_Diffuse5" in data,
            "contains_specular5": b"g_Specular5" in data,
        }

    passed = len(t7_set) == 1 and all(r["mode_matches_expected"] for r in rows)
    report = {
        "schema": 1,
        "audit": "DSR FaceEye shadow t7/s7 declaration partition",
        "pass": passed,
        "shader_count": len(rows),
        "t7_descriptor_unique_count": len(t7_set),
        "rows": rows,
        "ps_eye_mtd": mtd,
        "conclusion": {
            "resource_declaration": "same t7 gSMP_7 descriptor in all eight shadow variants",
            "sampler_declaration": "comparison exactly on no-point/PntS; regular exactly on PntSS/PntSSSS",
            "resource_sidecar_requirement": "not established; static resource ABI does not split",
            "remaining_blocker": "actual runtime s7 sampler-state object/descriptor route",
        },
    }
    Path(args.output).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if passed else 1

if __name__ == "__main__":
    raise SystemExit(main())
