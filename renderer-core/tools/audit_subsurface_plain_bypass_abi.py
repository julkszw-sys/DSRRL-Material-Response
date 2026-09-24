#!/usr/bin/env python3
import argparse
import glob
import hashlib
import json
import struct
from pathlib import Path

PAIRS = [
    {
        "variant": "Csd",
        "source_suffix": "FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo",
        "source_sha256": "0b8288d686c8f349ad87352946be51ffd007462f25357326bf47e736e690e511",
        "target_suffix": "FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo",
        "target_sha256": "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",
        "target_receiver_id": 33,
    },
    {
        "variant": "Sdw",
        "source_suffix": "FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf.fpo",
        "source_sha256": "885337e50f3d29f086fd18e1f7524f28712031d0264964aef5f37037df7d7bcb",
        "target_suffix": "FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo",
        "target_sha256": "d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",
        "target_receiver_id": 34,
    },
    {
        "variant": "Plain",
        "source_suffix": "FRPG_Phn_DifSpcBmp__________HemEnvSubsurf.fpo",
        "source_sha256": "3002cfb9aee6835412399c3be267ab94c5706d7d5030fc4cafc82bc54c55a860",
        "target_suffix": "FRPG_Phn_DifSpcBmp__________HemEnv.fpo",
        "target_sha256": "7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701",
        "target_receiver_id": 35,
    },
]

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def find_one(root, suffix):
    hits = [Path(p) for p in glob.glob(str(root / ("*" + suffix)))]
    if len(hits) != 1:
        raise SystemExit("expected exactly one *%s, found %d" % (suffix, len(hits)))
    return hits[0]

def chunks(data):
    if data[:4] != b"DXBC":
        raise SystemExit("not DXBC")
    count = struct.unpack_from("<I", data, 28)[0]
    offsets = struct.unpack_from("<" + "I" * count, data, 32)
    out = {}
    for offset in offsets:
        tag = data[offset:offset + 4].decode("ascii")
        size = struct.unpack_from("<I", data, offset + 4)[0]
        out[tag] = data[offset + 8:offset + 8 + size]
    return out

def cstr(blob, offset):
    if offset >= len(blob):
        raise SystemExit("RDEF string offset out of range: %d" % offset)
    end = blob.find(b"\0", offset)
    if end < 0:
        raise SystemExit("unterminated RDEF string")
    return blob[offset:end].decode("ascii")

def rdef_semantics(blob):
    cb_count, cb_offset, res_count, res_offset = struct.unpack_from("<4I", blob, 0)
    resources = []
    for i in range(res_count):
        off = res_offset + i * 32
        values = struct.unpack_from("<8I", blob, off)
        resources.append({
            "name": cstr(blob, values[0]),
            "type": values[1],
            "bind_point": values[5],
            "bind_count": values[6],
        })
    constant_buffers = []
    for i in range(cb_count):
        off = cb_offset + i * 24
        name_offset, var_count, var_offset, size, _, _ = struct.unpack_from("<6I", blob, off)
        variables = []
        for j in range(var_count):
            voff = var_offset + j * 40
            values = struct.unpack_from("<10I", blob, voff)
            variables.append({
                "name": cstr(blob, values[0]),
                "start": values[1],
                "size": values[2],
            })
        constant_buffers.append({
            "name": cstr(blob, name_offset),
            "size": size,
            "variables": variables,
        })
    return resources, constant_buffers

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shader-dir", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()
    root = Path(args.shader_dir)
    rows = []
    for pair in PAIRS:
        source_path = find_one(root, pair["source_suffix"])
        target_path = find_one(root, pair["target_suffix"])
        source = source_path.read_bytes()
        target = target_path.read_bytes()
        if sha256(source) != pair["source_sha256"]:
            raise SystemExit("source SHA mismatch: " + source_path.name)
        if sha256(target) != pair["target_sha256"]:
            raise SystemExit("target SHA mismatch: " + target_path.name)
        sc = chunks(source)
        tc = chunks(target)
        sres, scb = rdef_semantics(sc["RDEF"])
        tres, tcb = rdef_semantics(tc["RDEF"])
        sset = {(x["name"], x["bind_point"], x["type"]) for x in sres}
        tset = {(x["name"], x["bind_point"], x["type"]) for x in tres}
        rows.append({
            "variant": pair["variant"],
            "source_subsurf_name": pair["source_suffix"],
            "source_subsurf_sha256": pair["source_sha256"],
            "target_plain_name": pair["target_suffix"],
            "target_plain_sha256": pair["target_sha256"],
            "target_plain_receiver_id": pair["target_receiver_id"],
            "isgn_identical": sc["ISGN"] == tc["ISGN"],
            "osgn_identical": sc["OSGN"] == tc["OSGN"],
            "constant_buffer_semantics_identical": scb == tcb,
            "resources_only_in_subsurf": [
                {"name": n, "bind_point": b, "type": t}
                for n, b, t in sorted(sset - tset)
            ],
            "resources_only_in_plain": [
                {"name": n, "bind_point": b, "type": t}
                for n, b, t in sorted(tset - sset)
            ],
            "shader_model_token_identical":
                struct.unpack_from("<I", sc["SHEX"], 0)[0]
                == struct.unpack_from("<I", tc["SHEX"], 0)[0],
        })
    expected_removed = {("gSMP_10", 10, 2), ("gSMP_10Sampler", 10, 3)}
    passed = all(
        r["isgn_identical"]
        and r["osgn_identical"]
        and r["constant_buffer_semantics_identical"]
        and r["shader_model_token_identical"]
        and not r["resources_only_in_plain"]
        and {(x["name"], x["bind_point"], x["type"]) for x in r["resources_only_in_subsurf"]}
            == expected_removed
        for r in rows
    )
    report = {
        "schema": 1,
        "audit": "DSR Ps_Body Subsurf -> ordinary DifSpcBmp create-time shader bypass ABI",
        "pass": passed,
        "pairs": rows,
        "conclusion": (
            "Exact create-time pixel-shader substitution/reuse is ABI-compatible for all three "
            "certified Subsurf->ordinary pairs. The ordinary target removes only t10/s10 from "
            "the stock declaration set. Ordinary PTDE surface-route readiness and pixel "
            "equivalence remain separate gates."
        ),
    }
    Path(args.output).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if passed else 1

if __name__ == "__main__":
    raise SystemExit(main())
