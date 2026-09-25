#!/usr/bin/env python3
"""PTDE full FLVER ownership exporter for DSRRL.

Read-only scanner for Dark Souls Prepare to Die Edition (PC) retail archives.
It walks every dvdbnd*.bhd5/.bdt pair, unwraps PTDE DCX containers, parses
BND3 binders and FLVER2 material records, indexes raw MTD payloads, and emits
the deterministic TSV consumed by renderer-core/tools/flver_material_ownership_census.py.

The scanner NEVER writes to the game directory and never treats missing data as
operator absence. Missing/ambiguous MTD resolution is emitted to unresolved TSV.

Typical use:
    py -3 ptde_full_flver_ownership_export.py "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dark Souls Prepare to Die Edition\\DATA"

Optional byte-exact FLVER extraction:
    py -3 ptde_full_flver_ownership_export.py "C:\\DarkSouls\\DATA" --dump-flver
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import struct
import sys
import zlib
from collections import Counter, defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Optional

DCX_MAGIC = b"DCX\x00"
BND3_MAGIC = b"BND3"
FLVER_MAGIC = b"FLVER\x00"

SCHEMA_COLUMNS = (
    "game",
    "flver_identity",
    "material_slot",
    "mtd_name",
    "mtd_sha256",
    "spx_sha256",
    "receiver_index",
    "consumer_family",
    "material_family",
    "texture_semantic",
    "resource_hash",
    "srv_slot",
    "sampler_slot",
)

# Verified DSRRL PTDE path-hash self-tests. The scanner does not require known
# logical archive paths for full coverage, but retaining these tests protects
# the path-hash implementation used by follow-up targeted joins.
PATH_HASH_SELFTESTS = {
    "parts/HD_A_3460.partsbnd.dcx": 0x2F5735DD,
    "parts/HD_A_3460_M.partsbnd.dcx": 0x77137485,
    "parts/BD_F_0000.partsbnd.dcx": 0xEA1A9EEF,
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path, chunk: int = 8 << 20) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            b = f.read(chunk)
            if not b:
                return h.hexdigest()
            h.update(b)


def norm_slashes(s: str) -> str:
    return s.replace("\\", "/")


def basename_any(s: str) -> str:
    s = norm_slashes(s).rstrip("/")
    return s.rsplit("/", 1)[-1]


def from_path_hash(path: str) -> int:
    """Dark Souls 1 BHD5 path hash used by DSRRL evidence."""
    p = norm_slashes(path).lower()
    if not p.startswith("/"):
        p = "/" + p
    h = 0
    for ch in p:
        h = (h * 37 + ord(ch)) & 0xFFFFFFFF
    return h


def check_path_hash_selftests() -> None:
    bad = []
    for p, expected in PATH_HASH_SELFTESTS.items():
        got = from_path_hash(p)
        if got != expected:
            bad.append((p, got, expected))
    if bad:
        msg = "; ".join(f"{p}: got 0x{g:08X}, expected 0x{e:08X}" for p, g, e in bad)
        raise RuntimeError("PTDE path-hash self-test failed: " + msg)


def read_cstr(data: bytes, off: int, *, utf16: bool = False, limit: int = 4096) -> str:
    if off < 0 or off >= len(data):
        return ""
    if utf16:
        end = off
        cap = min(len(data), off + limit)
        while end + 1 < cap and data[end:end + 2] != b"\x00\x00":
            end += 2
        raw = data[off:end]
        for enc in ("utf-16-le", "utf-16-be"):
            try:
                return raw.decode(enc)
            except UnicodeDecodeError:
                pass
        return raw.decode("utf-16-le", errors="replace")

    end = data.find(b"\x00", off, min(len(data), off + limit))
    if end < 0:
        end = min(len(data), off + limit)
    raw = data[off:end]
    for enc in ("shift_jis", "cp932", "ascii", "latin1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return raw.decode("latin1", errors="replace")


def _u32(data: bytes, off: int, endian: str = "<") -> int:
    return struct.unpack_from(endian + "I", data, off)[0]


def _i32(data: bytes, off: int, endian: str = "<") -> int:
    return struct.unpack_from(endian + "i", data, off)[0]


def _i64(data: bytes, off: int, endian: str = "<") -> int:
    return struct.unpack_from(endian + "q", data, off)[0]


def decompress_ptde_dcx(data: bytes) -> bytes:
    """Decompress the verified DS1 DFLT DCX layout used by retained PTDE tools."""
    if len(data) < 0x50 or not data.startswith(DCX_MAGIC):
        raise ValueError("not a PTDE DCX container")
    if data[0x18:0x1C] != b"DCS\x00" or data[0x24:0x28] != b"DCP\x00":
        raise ValueError("unsupported DCX header")
    if data[0x28:0x2C] != b"DFLT" or data[0x44:0x48] != b"DCA\x00":
        raise ValueError("unsupported non-DFLT DCX")
    uncomp_size = struct.unpack_from(">I", data, 0x1C)[0]
    comp_size = struct.unpack_from(">I", data, 0x20)[0]
    zoff = 0x4C
    if comp_size <= 0 or zoff + comp_size > len(data):
        raise ValueError("invalid DCX compressed size")
    comp = data[zoff:zoff + comp_size]
    try:
        dec = zlib.decompress(comp)
    except zlib.error:
        dobj = zlib.decompressobj()
        dec = dobj.decompress(comp) + dobj.flush()
    if uncomp_size and len(dec) != uncomp_size:
        raise ValueError(f"DCX size mismatch: {len(dec)} != {uncomp_size}")
    return dec


def unwrap_dcx(data: bytes, max_layers: int = 4) -> tuple[bytes, int]:
    layers = 0
    while data.startswith(DCX_MAGIC):
        if layers >= max_layers:
            raise ValueError("too many nested DCX layers")
        data = decompress_ptde_dcx(data)
        layers += 1
    return data, layers


@dataclass(frozen=True)
class BHDEntry:
    file_hash: int
    size: int
    offset: int


def parse_bhd5_ds1(path: Path) -> list[BHDEntry]:
    data = path.read_bytes()
    if len(data) < 0x20 or data[:4] != b"BHD5":
        raise ValueError("not BHD5")
    endian_flag = data[4]
    if endian_flag == 0:
        endian = ">"
    elif endian_flag == 0xFF:
        endian = "<"
    else:
        raise ValueError(f"unsupported BHD5 endian flag 0x{endian_flag:02X}")
    if _i32(data, 8, endian) != 1:
        raise ValueError("unexpected BHD5 version")
    bucket_count = _i32(data, 0x10, endian)
    buckets_off = _i32(data, 0x14, endian)
    if bucket_count < 0 or bucket_count > 1_000_000:
        raise ValueError("invalid BHD5 bucket count")
    if buckets_off < 0 or buckets_off + bucket_count * 8 > len(data):
        raise ValueError("invalid BHD5 bucket table")

    out: list[BHDEntry] = []
    for i in range(bucket_count):
        bo = buckets_off + i * 8
        count = _i32(data, bo, endian)
        headers_off = _i32(data, bo + 4, endian)
        if count < 0 or count > 1_000_000:
            raise ValueError(f"bucket {i}: invalid file count")
        if headers_off < 0 or headers_off + count * 16 > len(data):
            raise ValueError(f"bucket {i}: invalid file header table")
        for j in range(count):
            fo = headers_off + j * 16
            file_hash = _u32(data, fo, endian)
            size = _i32(data, fo + 4, endian)
            offset = _i64(data, fo + 8, endian)
            if size < 0 or offset < 0:
                raise ValueError(f"bucket {i} entry {j}: invalid size/offset")
            out.append(BHDEntry(file_hash, size, offset))
    return out


@dataclass
class BNDEntry:
    index: int
    file_id: int
    flags: int
    name: str
    payload: bytes


def parse_bnd3_ptde(data: bytes) -> list[BNDEntry]:
    """Parse the already-validated PTDE PC BND3 layout used by DSRRL tools."""
    if len(data) < 0x20 or not data.startswith(BND3_MAGIC):
        raise ValueError("not BND3")
    count = _u32(data, 0x10)
    table_off = 0x20
    entry_size = 24
    if count <= 0 or count > 100_000 or table_off + count * entry_size > len(data):
        raise ValueError("invalid BND3 file table")

    meta = []
    for i in range(count):
        off = table_off + i * entry_size
        flags, size_a, data_off, file_id, name_off, size_b = struct.unpack_from("<6I", data, off)
        if name_off >= len(data) or data_off >= len(data):
            raise ValueError(f"BND3 entry {i}: out-of-range name/data offset")
        name = read_cstr(data, name_off) or f"entry_{i:05d}_id_{file_id}"
        size = size_a
        if size <= 0 or data_off + size > len(data):
            size = size_b
        if size <= 0 or data_off + size > len(data):
            # Conservative fallback to the next higher data offset, not simply next table row.
            candidates = []
            for k in range(count):
                ko = table_off + k * entry_size
                doff = _u32(data, ko + 8)
                if data_off < doff <= len(data):
                    candidates.append(doff)
            if candidates:
                size = min(candidates) - data_off
        if size <= 0 or data_off + size > len(data):
            raise ValueError(f"BND3 entry {i}: invalid payload range")
        meta.append(BNDEntry(i, file_id, flags, name, data[data_off:data_off + size]))
    return meta


@dataclass
class TextureBinding:
    semantic: str
    path: str


@dataclass
class FLVERMaterial:
    slot: int
    material_name: str
    mtd_path: str
    textures: list[TextureBinding]


def parse_flver2_materials(data: bytes) -> tuple[int, list[FLVERMaterial]]:
    """Parse DS1 FLVER2 material and global texture records; geometry is ignored.

    DS1 uses FLVER2 versions 0x2000B/0x2000C/0x2000D. The table sizes below
    match the retail DS1 FLVER2 structs used by SoulsFormats: header 0x80,
    dummy 0x40, material 0x20, bone 0x80, mesh 0x30, face-set 0x20,
    vertex-buffer 0x20, buffer-layout header 0x10 and texture 0x20.
    """
    if len(data) < 0x80 or not data.startswith(FLVER_MAGIC):
        raise ValueError("not FLVER2")
    endian_tag = data[6:8]
    if endian_tag == b"L\x00":
        endian = "<"
        utf16_encoding = "utf-16-le"
    elif endian_tag == b"B\x00":
        endian = ">"
        utf16_encoding = "utf-16-be"
    else:
        raise ValueError("invalid FLVER endian marker")

    version = _i32(data, 8, endian)
    if version < 0x20000:
        raise ValueError(f"FLVER0/non-DS1 version 0x{version:X}")
    if version > 0x2000D:
        raise ValueError(f"post-DS1 FLVER2 version 0x{version:X}")

    dummy_count = _i32(data, 0x14, endian)
    material_count = _i32(data, 0x18, endian)
    bone_count = _i32(data, 0x1C, endian)
    mesh_count = _i32(data, 0x20, endian)
    vertex_buffer_count = _i32(data, 0x24, endian)
    face_set_count = _i32(data, 0x50, endian)
    buffer_layout_count = _i32(data, 0x54, endian)
    texture_count = _i32(data, 0x58, endian)
    counts = {
        "dummy": dummy_count, "material": material_count, "bone": bone_count,
        "mesh": mesh_count, "vertex_buffer": vertex_buffer_count,
        "face_set": face_set_count, "buffer_layout": buffer_layout_count,
        "texture": texture_count,
    }
    for label, count in counts.items():
        if count < 0 or count > 1_000_000:
            raise ValueError(f"invalid FLVER {label} count: {count}")

    unicode = bool(data[0x49])

    def read_flver_string(off: int) -> str:
        if off <= 0 or off >= len(data):
            return ""
        if not unicode:
            return read_cstr(data, off, utf16=False)
        end = off
        cap = min(len(data), off + 4096)
        while end + 1 < cap and data[end:end + 2] != b"\x00\x00":
            end += 2
        return data[off:end].decode(utf16_encoding, errors="replace")

    materials_off = 0x80 + dummy_count * 0x40
    bones_off = materials_off + material_count * 0x20
    meshes_off = bones_off + bone_count * 0x80
    facesets_off = meshes_off + mesh_count * 0x30
    # DS1 FLVER2 versions are > 0x20005, so face-set headers are 0x20 bytes.
    vertex_buffers_off = facesets_off + face_set_count * 0x20
    layouts_off = vertex_buffers_off + vertex_buffer_count * 0x20
    textures_off = layouts_off + buffer_layout_count * 0x10
    end_of_textures = textures_off + texture_count * 0x20
    if materials_off < 0x80 or end_of_textures > len(data):
        raise ValueError("FLVER2 material/texture tables out of range")

    material_headers = []
    for slot in range(material_count):
        off = materials_off + slot * 0x20
        name_off, mtd_off, tex_count, tex_index, flags, gx_off, unk18, zero = struct.unpack_from(endian + "8i", data, off)
        if zero != 0:
            raise ValueError(f"material {slot}: trailing field is not zero")
        if tex_count < 0 or tex_index < 0 or tex_index + tex_count > texture_count:
            raise ValueError(f"material {slot}: texture range out of bounds")
        material_headers.append((
            slot,
            read_flver_string(name_off),
            read_flver_string(mtd_off),
            tex_count,
            tex_index,
        ))

    global_textures: list[TextureBinding] = []
    for ti in range(texture_count):
        off = textures_off + ti * 0x20
        path_off = _i32(data, off, endian)
        type_off = _i32(data, off + 4, endian)
        global_textures.append(TextureBinding(
            read_flver_string(type_off),
            read_flver_string(path_off),
        ))

    materials: list[FLVERMaterial] = []
    for slot, material_name, mtd_path, tex_count, tex_index in material_headers:
        textures = list(global_textures[tex_index:tex_index + tex_count])
        materials.append(FLVERMaterial(slot, material_name, mtd_path, textures))
    return version, materials


def safe_component(s: str, max_len: int = 120) -> str:
    s = basename_any(s) or "unnamed"
    s = re.sub(r"[^A-Za-z0-9._\-\[\]()]+", "_", s)
    return s[:max_len]


@dataclass
class RawOwnershipRow:
    game: str
    flver_identity: str
    flver_sha256: str
    flver_version: str
    material_slot: int
    material_name: str
    mtd_path: str
    mtd_name: str
    texture_semantic: str
    texture_path: str
    source_bhd: str
    source_bdt: str
    source_file_hash: str
    source_chain: str
    mtd_resolution: str = "UNRESOLVED"
    mtd_sha256: str = ""


class Scanner:
    def __init__(self, root: Path, out: Path, dump_flver: bool, max_entry_size: int):
        self.root = root
        self.out = out
        self.dump_flver = dump_flver
        self.max_entry_size = max_entry_size
        self.stats = Counter()
        self.errors: list[dict] = []
        self.raw_rows: list[RawOwnershipRow] = []
        self.mtd_by_basename: dict[str, dict[str, set[str]]] = defaultdict(lambda: defaultdict(set))
        self.mtd_sources: dict[str, list[dict]] = defaultdict(list)
        self.seen_flver_sha: set[str] = set()
        self.seen_mtd_sha: set[str] = set()

    def error(self, stage: str, source: str, exc: Exception | str) -> None:
        self.stats["errors"] += 1
        self.errors.append({"stage": stage, "source": source, "error": str(exc)})

    def index_mtd(self, payload: bytes, entry_name: str, source: str) -> None:
        try:
            payload, layers = unwrap_dcx(payload)
        except Exception as e:
            self.error("MTD_DCX", source, e)
            return
        if not entry_name.lower().endswith(".mtd"):
            return
        sha = sha256_bytes(payload)
        key = basename_any(entry_name).casefold()
        preserved = basename_any(entry_name)
        self.mtd_by_basename[key][sha].add(preserved)
        self.mtd_sources[sha].append({"name": entry_name, "source": source, "dcx_layers": layers})
        if sha not in self.seen_mtd_sha:
            self.seen_mtd_sha.add(sha)
            self.stats["unique_mtd_payloads"] += 1
        self.stats["mtd_entries"] += 1

    def record_flver(self, payload: bytes, chain: list[str], bhd: Path, bdt: Path, file_hash: int) -> None:
        try:
            payload, layers = unwrap_dcx(payload)
            version, materials = parse_flver2_materials(payload)
        except Exception as e:
            self.error("FLVER_PARSE", "::".join(chain), e)
            return
        fsha = sha256_bytes(payload)
        flver_identity = f"PTDE|{bhd.name}|0x{file_hash:08X}|" + "::".join(chain)
        if fsha in self.seen_flver_sha:
            self.stats["duplicate_flver_payloads"] += 1
        else:
            self.seen_flver_sha.add(fsha)
            self.stats["unique_flver_payloads"] += 1
        self.stats["flver_instances"] += 1
        self.stats["material_instances"] += len(materials)

        if self.dump_flver:
            d = self.out / "flver"
            d.mkdir(parents=True, exist_ok=True)
            leaf = safe_component(chain[-1] if chain else f"hash_{file_hash:08X}.flver")
            dst = d / f"{file_hash:08X}_{fsha[:16]}_{leaf}"
            if not dst.suffix.lower().startswith(".flver"):
                dst = dst.with_suffix(".flver")
            if not dst.exists():
                dst.write_bytes(payload)

        for mat in materials:
            mtd_name = basename_any(mat.mtd_path)
            bindings = mat.textures or [TextureBinding("", "")]
            if mat.textures:
                self.stats["texture_bindings"] += len(mat.textures)
            else:
                self.stats["materials_without_texture_bindings"] += 1
            for tex in bindings:
                self.raw_rows.append(RawOwnershipRow(
                    game="PTDE",
                    flver_identity=flver_identity,
                    flver_sha256=fsha,
                    flver_version=f"0x{version:X}",
                    material_slot=mat.slot,
                    material_name=mat.material_name,
                    mtd_path=mat.mtd_path,
                    mtd_name=mtd_name,
                    texture_semantic=tex.semantic,
                    texture_path=tex.path,
                    source_bhd=bhd.name,
                    source_bdt=bdt.name,
                    source_file_hash=f"0x{file_hash:08X}",
                    source_chain="::".join(chain),
                ))

    def walk_payload(self, payload: bytes, chain: list[str], bhd: Path, bdt: Path, file_hash: int, depth: int = 0) -> None:
        if depth > 5:
            self.error("CONTAINER_DEPTH", "::".join(chain), "container nesting exceeds 5")
            return
        try:
            payload, layers = unwrap_dcx(payload)
            if layers:
                self.stats["dcx_layers"] += layers
        except Exception as e:
            self.error("DCX", "::".join(chain), e)
            return

        if payload.startswith(FLVER_MAGIC):
            self.record_flver(payload, chain, bhd, bdt, file_hash)
            return

        if payload.startswith(BND3_MAGIC):
            try:
                entries = parse_bnd3_ptde(payload)
            except Exception as e:
                self.error("BND3_PARSE", "::".join(chain), e)
                return
            self.stats["bnd3_binders"] += 1
            self.stats["bnd3_entries"] += len(entries)
            for e in entries:
                child_source = f"{bhd.name}:0x{file_hash:08X}:" + "::".join(chain + [e.name])
                if e.name.lower().endswith(".mtd"):
                    self.index_mtd(e.payload, e.name, child_source)
                # Recurse only into relevant known container/model shapes. This avoids
                # false positives inside textures/shaders while still covering nested binders.
                probe = e.payload
                try:
                    unwrapped, _ = unwrap_dcx(probe)
                except Exception:
                    unwrapped = probe
                if unwrapped.startswith((FLVER_MAGIC, BND3_MAGIC)):
                    self.walk_payload(e.payload, chain + [e.name], bhd, bdt, file_hash, depth + 1)
            return

        # A raw archive entry can itself be an MTD only when its logical name is known;
        # BHD5 stores only hashes, so no absence inference is made here.
        self.stats["non_target_archive_entries"] += 1

    def scan_pair(self, bhd: Path, bdt: Path) -> None:
        self.stats["archive_pairs"] += 1
        try:
            entries = parse_bhd5_ds1(bhd)
        except Exception as e:
            self.error("BHD5_PARSE", str(bhd), e)
            return
        self.stats["bhd_entries"] += len(entries)
        bdt_size = bdt.stat().st_size
        try:
            with bdt.open("rb") as f:
                for idx, entry in enumerate(entries):
                    self.stats["archive_entries_scanned"] += 1
                    if entry.size == 0:
                        self.stats["zero_size_entries"] += 1
                        continue
                    if entry.size > self.max_entry_size:
                        self.stats["oversize_entries_skipped"] += 1
                        self.error("ENTRY_SIZE", f"{bhd.name}:0x{entry.file_hash:08X}", f"{entry.size} > max {self.max_entry_size}")
                        continue
                    if entry.offset + entry.size > bdt_size:
                        self.error("BDT_RANGE", f"{bhd.name}:0x{entry.file_hash:08X}", "entry exceeds BDT size")
                        continue
                    f.seek(entry.offset)
                    payload = f.read(entry.size)
                    if len(payload) != entry.size:
                        self.error("BDT_READ", f"{bhd.name}:0x{entry.file_hash:08X}", "short read")
                        continue
                    self.walk_payload(payload, [f"archive_hash_0x{entry.file_hash:08X}"], bhd, bdt, entry.file_hash)
        except OSError as e:
            self.error("BDT_OPEN", str(bdt), e)

    def resolve_mtds(self) -> None:
        for row in self.raw_rows:
            key = row.mtd_name.casefold()
            if not key:
                row.mtd_resolution = "MISSING_MTD_NAME"
                continue
            candidates = self.mtd_by_basename.get(key)
            if not candidates:
                row.mtd_resolution = "MTD_NOT_FOUND"
                continue
            if len(candidates) != 1:
                row.mtd_resolution = "MTD_AMBIGUOUS_SHA"
                continue
            sha = next(iter(candidates))
            row.mtd_sha256 = sha
            row.mtd_resolution = "EXACT_BASENAME_SHA"

    def write_outputs(self) -> dict:
        self.out.mkdir(parents=True, exist_ok=True)
        self.resolve_mtds()

        raw_jsonl = self.out / "flver_material_ownership_raw_v1.jsonl"
        with raw_jsonl.open("w", encoding="utf-8", newline="\n") as f:
            for row in sorted(self.raw_rows, key=lambda r: (
                r.flver_identity, r.material_slot, r.mtd_name.casefold(),
                r.texture_semantic.casefold(), r.texture_path.casefold())):
                f.write(json.dumps(asdict(row), ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n")

        canonical_tsv = self.out / "flver_material_ownership_input_v1.tsv"
        unresolved_tsv = self.out / "flver_material_ownership_unresolved_v1.tsv"
        exact = [r for r in self.raw_rows if r.mtd_resolution == "EXACT_BASENAME_SHA"]
        unresolved = [r for r in self.raw_rows if r.mtd_resolution != "EXACT_BASENAME_SHA"]

        with canonical_tsv.open("w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=SCHEMA_COLUMNS, delimiter="\t", lineterminator="\n")
            w.writeheader()
            # The current canonical schema has no texture-path column. Collapse
            # duplicate same-semantic bindings per exact material ownership row; the
            # lossless path-level bindings remain in raw_jsonl. With resource_hash
            # intentionally blank these rows remain resource_state=UNKNOWN downstream.
            canonical_seen = set()
            for r in sorted(exact, key=lambda x: (
                x.flver_identity, x.material_slot, x.mtd_sha256,
                x.texture_semantic.casefold(), x.texture_path.casefold())):
                ck = (r.flver_identity, r.material_slot, r.mtd_sha256, r.texture_semantic.casefold())
                if ck in canonical_seen:
                    self.stats["canonical_same_semantic_rows_collapsed"] += 1
                    continue
                canonical_seen.add(ck)
                # resource_hash intentionally remains blank: FLVER proves logical binding,
                # not the byte identity of the texture resource itself.
                w.writerow({
                    "game": "PTDE",
                    "flver_identity": r.flver_identity,
                    "material_slot": r.material_slot,
                    "mtd_name": r.mtd_name,
                    "mtd_sha256": r.mtd_sha256,
                    "spx_sha256": "",
                    "receiver_index": "",
                    "consumer_family": "",
                    "material_family": "",
                    "texture_semantic": r.texture_semantic,
                    "resource_hash": "",
                    "srv_slot": "",
                    "sampler_slot": "",
                })

        unresolved_fields = [
            "mtd_resolution", "flver_identity", "flver_sha256", "material_slot",
            "material_name", "mtd_path", "mtd_name", "texture_semantic", "texture_path",
            "source_bhd", "source_bdt", "source_file_hash", "source_chain",
        ]
        with unresolved_tsv.open("w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=unresolved_fields, delimiter="\t", lineterminator="\n")
            w.writeheader()
            for r in unresolved:
                w.writerow({k: getattr(r, k) for k in unresolved_fields})

        mtd_index = self.out / "ptde_mtd_index_v1.jsonl"
        with mtd_index.open("w", encoding="utf-8", newline="\n") as f:
            for key in sorted(self.mtd_by_basename):
                for sha, names in sorted(self.mtd_by_basename[key].items()):
                    f.write(json.dumps({
                        "basename_key": key,
                        "names": sorted(names),
                        "sha256": sha,
                        "sources": self.mtd_sources.get(sha, []),
                    }, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n")

        error_path = self.out / "flver_scan_errors_v1.jsonl"
        with error_path.open("w", encoding="utf-8", newline="\n") as f:
            for e in self.errors:
                f.write(json.dumps(e, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n")

        resolution_counts = Counter(r.mtd_resolution for r in self.raw_rows)
        summary = {
            "schema": 1,
            "game": "PTDE",
            "root": str(self.root.resolve()),
            "read_only": True,
            "path_hash_selftests": "PASS",
            "counts": dict(sorted(self.stats.items())),
            "mtd_resolution": dict(sorted(resolution_counts.items())),
            "raw_rows": len(self.raw_rows),
            "canonical_rows": len({(r.flver_identity, r.material_slot, r.mtd_sha256, r.texture_semantic.casefold()) for r in exact}),
            "unresolved_rows": len(unresolved),
            "flver_identities": len({r.flver_identity for r in self.raw_rows}),
            "unique_flver_payloads": len(self.seen_flver_sha),
            "unique_mtd_payloads": len(self.seen_mtd_sha),
            "outputs": {
                "canonical_tsv": canonical_tsv.name,
                "raw_jsonl": raw_jsonl.name,
                "unresolved_tsv": unresolved_tsv.name,
                "mtd_index_jsonl": mtd_index.name,
                "errors_jsonl": error_path.name,
            },
            "policy": {
                "missing_or_ambiguous_mtd": "UNRESOLVED_NOT_NO_USE",
                "texture_resource_hash": "UNKNOWN_UNLESS_SEPARATELY_PROVEN",
                "runtime_activation": "OPEN",
                "pixel_equivalence": "OPEN",
            },
        }
        for p in (canonical_tsv, raw_jsonl, unresolved_tsv, mtd_index, error_path):
            summary.setdefault("sha256", {})[p.name] = sha256_file(p)
        summary_path = self.out / "flver_scan_summary_v1.json"
        summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False, sort_keys=True) + "\n", encoding="utf-8")
        return summary


def find_archive_pairs(root: Path) -> list[tuple[Path, Path]]:
    bhds = sorted(p for p in root.rglob("*.bhd5") if p.is_file())
    pairs: list[tuple[Path, Path]] = []
    for bhd in bhds:
        stem = bhd.name[:-5]  # strip .bhd5, preserve original basename
        candidates = [
            bhd.with_name(stem + ".bdt"),
            bhd.with_name(stem + ".BDT"),
        ]
        bdt = next((p for p in candidates if p.is_file()), None)
        if bdt is None:
            # Case-insensitive fallback for non-Windows execution/tests.
            target = (stem + ".bdt").casefold()
            bdt = next((p for p in bhd.parent.iterdir() if p.is_file() and p.name.casefold() == target), None)
        if bdt is not None:
            pairs.append((bhd, bdt))
    return pairs


def main(argv: Optional[list[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Export full PTDE FLVER -> material slot -> MTD ownership census.")
    ap.add_argument("ptde_root", type=Path, help="PTDE DATA/game directory containing dvdbnd*.bhd5/.bdt")
    ap.add_argument("-o", "--output", type=Path, default=Path("DSRRL_PTDE_FLVER_CENSUS"), help="output directory")
    ap.add_argument("--dump-flver", action="store_true", help="also write byte-exact decompressed FLVER payloads")
    ap.add_argument("--max-entry-mib", type=int, default=512, help="safety cap per BDT entry, default 512 MiB")
    args = ap.parse_args(argv)

    root = args.ptde_root.resolve()
    if not root.is_dir():
        print(f"ERROR: PTDE root is not a directory: {root}", file=sys.stderr)
        return 2
    check_path_hash_selftests()
    pairs = find_archive_pairs(root)
    if not pairs:
        print(f"ERROR: no .bhd5/.bdt archive pairs found under {root}", file=sys.stderr)
        return 3

    out = args.output.resolve()
    scanner = Scanner(root, out, args.dump_flver, max(1, args.max_entry_mib) << 20)
    print(f"[PTDE] root: {root}")
    print(f"[PTDE] archive pairs: {len(pairs)}")
    print(f"[OUT ] {out}")
    for i, (bhd, bdt) in enumerate(pairs, 1):
        print(f"[{i:02d}/{len(pairs):02d}] {bhd.name} + {bdt.name}")
        scanner.scan_pair(bhd, bdt)

    summary = scanner.write_outputs()
    print("\n=== DSRRL PTDE FLVER CENSUS ===")
    print(f"FLVER identities : {summary['flver_identities']}")
    print(f"Materials/rows   : {summary['raw_rows']}")
    print(f"Canonical rows   : {summary['canonical_rows']}")
    print(f"Unresolved rows  : {summary['unresolved_rows']}")
    print(f"Errors           : {summary['counts'].get('errors', 0)}")
    print(f"Canonical TSV    : {out / summary['outputs']['canonical_tsv']}")
    print(f"Summary          : {out / 'flver_scan_summary_v1.json'}")
    if summary["unresolved_rows"]:
        print("NOTE: unresolved rows were preserved as UNKNOWN; inspect flver_material_ownership_unresolved_v1.tsv")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
