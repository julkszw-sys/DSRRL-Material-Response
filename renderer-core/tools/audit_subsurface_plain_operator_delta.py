#!/usr/bin/env python3
import argparse
import difflib
import hashlib
import json
import struct
from pathlib import Path

PAIRS = [
    {
        "variant": "Csd",
        "source_name": "FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo",
        "source_sha256": "0b8288d686c8f349ad87352946be51ffd007462f25357326bf47e736e690e511",
        "target_name": "FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo",
        "target_sha256": "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",
        "target_receiver_id": 33,
    },
    {
        "variant": "Sdw",
        "source_name": "FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf.fpo",
        "source_sha256": "885337e50f3d29f086fd18e1f7524f28712031d0264964aef5f37037df7d7bcb",
        "target_name": "FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo",
        "target_sha256": "d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",
        "target_receiver_id": 34,
    },
    {
        "variant": "Plain",
        "source_name": "FRPG_Phn_DifSpcBmp__________HemEnvSubsurf.fpo",
        "source_sha256": "3002cfb9aee6835412399c3be267ab94c5706d7d5030fc4cafc82bc54c55a860",
        "target_name": "FRPG_Phn_DifSpcBmp__________HemEnv.fpo",
        "target_sha256": "7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701",
        "target_receiver_id": 35,
    },
]

EXPECTED_DELETE_OPCODE_BLOCKS = [
    [0x5A, 0x58],
    [0x45, 0x38, 0x00, 0x4B, 0x38],
    [0x31, 0x31, 0x01, 0x1F, 0x36, 0x38, 0x48, 0x38, 0x38, 0x38,
     0x0E, 0x38, 0x38, 0x38, 0x38, 0x19, 0x38, 0x32, 0x32, 0x32,
     0x38, 0x19, 0x32, 0x32, 0x32, 0x12, 0x36, 0x15],
    [0x1C],
    [0x38],
]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def find_one(root, name):
    hits = list(root.rglob(name))
    if len(hits) != 1:
        raise SystemExit("expected exactly one %s, found %d" % (name, len(hits)))
    return hits[0]


def dxbc_chunks(data):
    if len(data) < 32 or data[:4] != b"DXBC":
        raise SystemExit("not DXBC")
    count = struct.unpack_from("<I", data, 28)[0]
    if count == 0 or count > 64 or 32 + 4 * count > len(data):
        raise SystemExit("invalid DXBC chunk table")
    out = {}
    for i in range(count):
        off = struct.unpack_from("<I", data, 32 + 4 * i)[0]
        if off + 8 > len(data):
            raise SystemExit("chunk offset out of range")
        tag = data[off:off + 4].decode("ascii")
        size = struct.unpack_from("<I", data, off + 4)[0]
        if off + 8 + size > len(data):
            raise SystemExit("chunk size out of range")
        out[tag] = data[off + 8:off + 8 + size]
    return out


def shader_words(data):
    chunks = dxbc_chunks(data)
    code = chunks.get("SHEX") or chunks.get("SHDR")
    if code is None or len(code) % 4:
        raise SystemExit("missing/invalid SHEX/SHDR")
    return list(struct.unpack("<%dI" % (len(code) // 4), code))


def instructions(words):
    if len(words) < 2:
        raise SystemExit("short shader code")
    out = []
    i = 2
    while i < len(words):
        token = words[i]
        length = (token >> 24) & 0x7F
        if length == 0 or i + length > len(words):
            raise SystemExit("invalid instruction at word %d" % i)
        out.append({
            "word_offset": i,
            "opcode": token & 0x7FF,
            "words": words[i:i + length],
        })
        i += length
    if i != len(words):
        raise SystemExit("instruction parse did not end at stream boundary")
    return out


def compare_pair(root, pair):
    source_path = find_one(root, pair["source_name"])
    target_path = find_one(root, pair["target_name"])
    source = source_path.read_bytes()
    target = target_path.read_bytes()

    if sha256(source) != pair["source_sha256"]:
        raise SystemExit("source SHA mismatch: " + source_path.name)
    if sha256(target) != pair["target_sha256"]:
        raise SystemExit("target SHA mismatch: " + target_path.name)

    source_ins = instructions(shader_words(source))
    target_ins = instructions(shader_words(target))
    source_ops = [x["opcode"] for x in source_ins]
    target_ops = [x["opcode"] for x in target_ins]

    matcher = difflib.SequenceMatcher(
        a=source_ops,
        b=target_ops,
        autojunk=False,
    )

    deletes = []
    invalid = []
    matched = 0
    exact_words = 0
    operand_mismatch = 0

    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            matched += i2 - i1
            for source_i, target_i in zip(
                    source_ins[i1:i2],
                    target_ins[j1:j2]):
                if source_i["words"] == target_i["words"]:
                    exact_words += 1
                else:
                    operand_mismatch += 1
        elif tag == "delete":
            deletes.append(source_ops[i1:i2])
        else:
            invalid.append({
                "tag": tag,
                "source": [i1, i2],
                "target": [j1, j2],
            })

    return {
        "variant": pair["variant"],
        "source_subsurf_name": pair["source_name"],
        "source_subsurf_sha256": pair["source_sha256"],
        "source_byte_size": len(source),
        "target_plain_name": pair["target_name"],
        "target_plain_sha256": pair["target_sha256"],
        "target_byte_size": len(target),
        "target_plain_receiver_id": pair["target_receiver_id"],
        "source_instruction_count": len(source_ins),
        "target_instruction_count": len(target_ins),
        "instruction_delta": len(source_ins) - len(target_ins),
        "opcode_delete_blocks": [
            ["0x%02X" % opcode for opcode in block]
            for block in deletes
        ],
        "opcode_delete_block_lengths": [len(block) for block in deletes],
        "delete_only_opcode_transform": (
            not invalid and
            sum(map(len, deletes)) == len(source_ins) - len(target_ins) and
            matched == len(target_ins)
        ),
        "matched_opcode_instructions": matched,
        "byte_identical_matched_instructions": exact_words,
        "operand_or_token_word_mismatch_matched_instructions":
            operand_mismatch,
        "non_delete_opcode_edits": invalid,
    }, deletes


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--shader-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--source-artifact-id", type=int, default=99)
    parser.add_argument(
        "--source-artifact-sha256",
        default="57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076",
    )
    args = parser.parse_args()
    root = Path(args.shader_dir)

    rows = []
    delete_blocks = []
    for pair in PAIRS:
        row, deleted = compare_pair(root, pair)
        rows.append(row)
        delete_blocks.append(deleted)

    same_pattern = all(
        blocks == EXPECTED_DELETE_OPCODE_BLOCKS
        for blocks in delete_blocks
    )
    passed = (
        all(
            row["delete_only_opcode_transform"] and
            row["instruction_delta"] == 37
            for row in rows
        ) and
        same_pattern
    )

    report = {
        "schema": "dsrrl.subsurface_plain_operator_delta.v1",
        "status": "CONFIRMED" if passed else "FAILED",
        "source_artifact_id": args.source_artifact_id,
        "source_artifact_sha256": args.source_artifact_sha256,
        "source_member":
            "DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx",
        "pairs": rows,
        "cross_pair_invariants": {
            "pair_count": len(rows),
            "instruction_delta_each": 37,
            "delete_block_count_each": 5,
            "delete_block_lengths": [2, 5, 28, 1, 1],
            "same_opcode_delete_signature_3_of_3": same_pattern,
        },
        "operator_interpretation":
            "The exact three DSR Subsurf receivers contain the same isolated "
            "37-instruction opcode-level fork relative to their ordinary "
            "HemEnv targets. This strengthens the Subsurface/SSS "
            "operator-island boundary.",
        "guardrail":
            "Opcode-level delete-only equivalence does NOT authorize deleting "
            "those instructions from the source shader at runtime: surviving "
            "instructions are not byte-identical because compiler temp/operand "
            "allocation differs. A direct source-to-target materializer "
            "requires independently proven operand/register re-lowering or "
            "exact target bytes. Current draw-time material-aware replacement "
            "remains fail-open.",
        "runtime_activation": "OPEN",
        "pixel_behavior": "OPEN",
        "pass": passed,
    }

    Path(args.output).write_text(
        json.dumps(report, indent=2) + "\n",
        encoding="utf-8",
    )
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
