#!/usr/bin/env python3
"""Extract exact compressed GPU upload-byte fingerprints from owner DSR TPF/DDS.
Source asset identity ONLY: does not infer physical PS-t1 or PTDE pixel match.
"""
import argparse
import csv
import hashlib
import json
import struct
import zipfile
import zlib
from collections import defaultdict
from pathlib import Path

EXPECTED_ZIP_SHA = "a76b13c909fb4a30a1a312cba7d594a851193bef55e578934829fd873f872c97"
# DDS FourCC / DX10 map to D3D11 DXGI format and compressed block size.
FORMAT = {b"DXT1": (71, 8, 128), b"DXT5": (77, 16, 128)}
DX10_FORMAT = {83: (83, 16, 148)}

def u32(data, at):
    return struct.unpack_from("<I", data, at)[0]

def dds_fingerprint(blob):
    if len(blob) < 128 or blob[:4] != b"DDS " or u32(blob, 4) != 124:
        raise ValueError("Unexpected DDS header")
    h, w = u32(blob, 12), u32(blob, 16)
    mips = u32(blob, 28) or 1
    if not (0 < w <= 16384 and 0 < h <= 16384 and 1 <= mips <= 16):
        raise ValueError("Invalid DDS dimensions or mips")
    fourcc = bytes(blob[84:88])
    if fourcc == b"DX10":
        if len(blob) < 148:
            raise ValueError("Truncated DX10 DDS")
        fmt = u32(blob, 128)
        if fmt not in DX10_FORMAT or u32(blob, 132) != 3 or u32(blob, 140) != 1:
            raise ValueError("Unsupported DX10 2D texture")
        dxgi, block, start = DX10_FORMAT[fmt]
    elif fourcc in FORMAT:
        dxgi, block, start = FORMAT[fourcc]
    else:
        raise ValueError("Unsupported FourCC " + repr(fourcc))
    expected = sum(
        max(1, (max(1, w >> k) + 3) // 4)
        * max(1, (max(1, h >> k) + 3) // 4)
        * block for k in range(mips)
    )
    if len(blob) != start + expected:
        raise ValueError("DDS compressed mip payload size does not match")
    body = blob[start:]
    return {
        "width": w, "height": h, "mips": mips, "dxgi_format": dxgi,
        "bytes": len(body),
        "dds_sha256": hashlib.sha256(blob).hexdigest(),
        "gpu_mip_bytes_sha256": hashlib.sha256(body).hexdigest(),
    }

def parse_tpf(data, archive_name):
    pos = data.find(b"TPF\0")
    if pos < 0:
        return "absent", []
    if data.find(b"TPF\0", pos + 4) >= 0:
        raise ValueError("More than one TPF in " + archive_name)
    if len(data) < pos + 16:
        raise ValueError("Truncated TPF")
    count, payload_size = u32(data, pos + 8), u32(data, pos + 4)
    if count == 0:
        if payload_size:
            raise ValueError("Empty TPF with non-zero payload")
        return "empty", []
    if count > 128 or pos + 16 + 20 * count > len(data):
        raise ValueError("Invalid TPF count")
    records = []
    ranges = []
    for i in range(count):
        off, size, flags, label_pos, reserved = struct.unpack_from(
            "<5I", data, pos + 16 + i * 20)
        if off < 16 + count * 20 or pos + off + size > len(data):
            raise ValueError("TPF DDS offset outside decompressed BND")
        if label_pos < 16 + count * 20 or pos + label_pos >= len(data):
            raise ValueError("TPF label pointer out of bounds")
        end = data.find(b"\0", pos + label_pos, min(len(data), pos + label_pos + 256))
        if end < 0:
            raise ValueError("Unterminated TPF basename")
        label = data[pos + label_pos:end].decode("ascii")
        if not label or any(not (c.isalnum() or c == "_") for c in label):
            raise ValueError("Noncanonical TPF basename")
        dds = data[pos + off:pos + off + size]
        record = dds_fingerprint(dds)
        record.update(tpf_name=label, archive=archive_name, tpf_flags=flags)
        records.append(record)
        ranges.append((off, off + size))
    if payload_size != sum(a[1] - a[0] for a in ranges):
        raise ValueError("TPF payload sum mismatch")
    ranges.sort()
    if any(a[1] > b[0] for a, b in zip(ranges, ranges[1:])):
        raise ValueError("Overlapping DDS payloads")
    return "nonempty", records

def main(source_zip, slots_csv, output):
    sha = hashlib.sha256(Path(source_zip).read_bytes()).hexdigest()
    if sha != EXPECTED_ZIP_SHA:
        raise ValueError("Unpinned source ZIP: " + sha)
    all_entries = []
    states = {"nonempty": 0, "empty": 0, "absent": 0}
    with zipfile.ZipFile(source_zip) as z:
        paths = sorted(
            p for p in z.namelist()
            if p.startswith("DSR/parts/") and p.endswith(".partsbnd.dcx"))
        for path in paths:
            compressed = z.read(path)
            if compressed[:4] != b"DCX\0" or compressed[0x24:0x2c] != b"DCP\0DFLT":
                raise ValueError("Not pinned DFLT DCX: " + path)
            raw = zlib.decompress(compressed[0x4c:])
            if len(raw) != struct.unpack_from(">I", compressed, 0x1c)[0]:
                raise ValueError("DCX decompressed length mismatch")
            state, found = parse_tpf(raw, path)
            states[state] += 1
            all_entries.extend(found)
    by_name = defaultdict(list)
    for r in all_entries:
        by_name[r["tpf_name"].casefold()].append(r)
    collision_names = {
        k: sorted({v["gpu_mip_bytes_sha256"] for v in rr})
        for k, rr in by_name.items()
        if len({v["gpu_mip_bytes_sha256"] for v in rr}) > 1
    }
    # Content equality is not a unique logical identity: two differently
    # named resources may have bit-identical original compressed mip bytes.
    digest_to_names = defaultdict(set)
    for r in all_entries:
        digest_to_names[r["gpu_mip_bytes_sha256"]].add(r["tpf_name"].casefold())
    ambiguous_digests = {
        digest: sorted(names) for digest, names in digest_to_names.items()
        if len(names) != 1
    }
    with open(slots_csv, encoding="utf-8-sig", newline="") as f:
        slot_rows = list(csv.DictReader(f))
    exact_matches = []
    import ast
    for slot in slot_rows:
        for name in ast.literal_eval(slot["g_specular"]):
            candidates = by_name.get(name.casefold(), [])
            hashes = {v["gpu_mip_bytes_sha256"] for v in candidates}
            if len(hashes) == 1 and next(iter(hashes)) not in ambiguous_digests:
                exact_matches.append({
                    "flver_sha256": slot["flver_sha256"],
                    "material_slot": int(slot["material_slot"]),
                    "mtd": slot["mtd"], "logical_specular": name,
                    "source_archive": slot["origin"],
                    "source_dds": candidates[0],
                    "gpu_identity": "NOT_PROVEN"
                })
    result = {
        "archive_sha256": sha, "source_status": "EXACT_TPF_DDS_BYTES_ONLY",
        "stock_gpu_ps_t1": "OPEN", "ptde_visible_pixels": "OPEN",
        "archives": len(paths), "tpf_states": states,
        "extracted_DDS_entries": len(all_entries), "unique_logical_names": len(by_name),
        "logical_name_content_conflicts": collision_names,
        "byte_identical_distinct_logical_names": ambiguous_digests,
        "formats": {
            str(dxgi): sum(v["dxgi_format"] == dxgi for v in all_entries)
            for dxgi in (71, 77, 83)
        },
        "matching_spc_slot_records": len(exact_matches),
        "matching_spc_profiles": sorted({m["mtd"] for m in exact_matches}),
        "slot_fingerprints": exact_matches,
        "all_tpf_fingerprints": all_entries
    }
    Path(output).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items()
                      if k not in ("slot_fingerprints","all_tpf_fingerprints")}, indent=2))
if __name__ == "__main__":
    p=argparse.ArgumentParser()
    p.add_argument("original_archive")
    p.add_argument("exact_slot_csv")
    p.add_argument("--out", required=True)
    a=p.parse_args()
    main(a.original_archive, a.exact_slot_csv, a.out)
