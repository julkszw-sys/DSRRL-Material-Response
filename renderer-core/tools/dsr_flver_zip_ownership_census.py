#!/usr/bin/env python3
"""Build a deterministic DSR FLVER material-slot ownership census from a ZIP.

This scanner is intentionally ownership/resource-only. It records what each
FLVER material slot names and which texture semantics/paths the slot binds.
It does NOT infer PTDE homology, material-response equations, or NO_USE from
missing resources.

Input:
  ZIP containing raw FLVER2 files and/or DFLT DCX-wrapped FLVER2 files.

Outputs:
  <prefix>_raw.jsonl
  <prefix>_materials.tsv
  <prefix>_mtd_projection.tsv
  <prefix>_errors.jsonl
  <prefix>_summary.json

The material TSV is the DSR source layer used before exact MTD-SHA resolution
and PTDE cross-version joins.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import zlib
import zipfile
from collections import Counter, defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path

DCX_MAGIC=b"DCX\x00"
FLVER_MAGIC=b"FLVER\x00"


def sha256_bytes(data:bytes)->str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path:Path,chunk:int=8<<20)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        while True:
            b=f.read(chunk)
            if not b:
                return h.hexdigest()
            h.update(b)


def norm_slashes(s:str)->str:
    return s.replace("\\","/")


def basename_any(s:str)->str:
    s=norm_slashes(s).rstrip("/")
    return s.rsplit("/",1)[-1]


def _i32(data:bytes,off:int,endian:str)->int:
    return struct.unpack_from(endian+"i",data,off)[0]


def read_cstr(data:bytes,off:int,encoding:str,limit:int=8192)->str:
    if off<=0 or off>=len(data):
        return ""
    if encoding.startswith("utf-16"):
        end=off
        cap=min(len(data),off+limit)
        while end+1<cap and data[end:end+2]!=b"\x00\x00":
            end+=2
        return data[off:end].decode(encoding,errors="replace")
    end=data.find(b"\x00",off,min(len(data),off+limit))
    if end<0:
        end=min(len(data),off+limit)
    raw=data[off:end]
    for enc in (encoding,"shift_jis","cp932","utf-8","latin1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return raw.decode("latin1",errors="replace")


def decompress_dflt_dcx(data:bytes)->bytes:
    """Narrow DS1-family DFLT DCX support; unknown compression fails closed."""
    if len(data)<0x50 or not data.startswith(DCX_MAGIC):
        raise ValueError("not DCX")
    # PTDE/DSR DFLT layouts seen in retained tooling place DCS/DCP/DFLT/DCA
    # in these fields. Never guess another codec.
    if data[0x18:0x1c]!=b"DCS\x00" or data[0x24:0x28]!=b"DCP\x00":
        raise ValueError("unsupported DCX header")
    if data[0x28:0x2c]!=b"DFLT" or data[0x44:0x48]!=b"DCA\x00":
        raise ValueError("unsupported non-DFLT DCX")
    uncomp=struct.unpack_from(">I",data,0x1c)[0]
    comp=struct.unpack_from(">I",data,0x20)[0]
    off=0x4c
    if comp<=0 or off+comp>len(data):
        raise ValueError("invalid DCX payload range")
    out=zlib.decompress(data[off:off+comp])
    if uncomp and len(out)!=uncomp:
        raise ValueError(f"DCX size mismatch {len(out)} != {uncomp}")
    return out


def unwrap_dcx(data:bytes,max_layers:int=4)->tuple[bytes,int]:
    layers=0
    while data.startswith(DCX_MAGIC):
        if layers>=max_layers:
            raise ValueError("too many DCX layers")
        data=decompress_dflt_dcx(data)
        layers+=1
    return data,layers


@dataclass(frozen=True)
class TextureBinding:
    semantic:str
    path:str


@dataclass(frozen=True)
class Material:
    slot:int
    material_name:str
    mtd_path:str
    textures:tuple[TextureBinding,...]


def parse_flver2_materials(data:bytes)->tuple[int,list[Material]]:
    if len(data)<0x80 or not data.startswith(FLVER_MAGIC):
        raise ValueError("not FLVER2")
    tag=data[6:8]
    if tag==b"L\x00":
        endian="<"; utf16="utf-16-le"
    elif tag==b"B\x00":
        endian=">"; utf16="utf-16-be"
    else:
        raise ValueError(f"invalid FLVER endian tag {tag!r}")

    version=_i32(data,8,endian)
    if version<0x20000:
        raise ValueError(f"FLVER0/unsupported version 0x{version:X}")

    dummy_count=_i32(data,0x14,endian)
    material_count=_i32(data,0x18,endian)
    bone_count=_i32(data,0x1c,endian)
    mesh_count=_i32(data,0x20,endian)
    vertex_buffer_count=_i32(data,0x24,endian)
    face_set_count=_i32(data,0x50,endian)
    buffer_layout_count=_i32(data,0x54,endian)
    texture_count=_i32(data,0x58,endian)
    counts=(dummy_count,material_count,bone_count,mesh_count,
            vertex_buffer_count,face_set_count,buffer_layout_count,texture_count)
    if any(x<0 or x>1_000_000 for x in counts):
        raise ValueError("implausible FLVER table count")

    unicode_strings=bool(data[0x49])
    enc=utf16 if unicode_strings else "shift_jis"

    materials_off=0x80+dummy_count*0x40
    bones_off=materials_off+material_count*0x20
    meshes_off=bones_off+bone_count*0x80
    facesets_off=meshes_off+mesh_count*0x30
    # DS1-family FLVER2 uses 0x20 face-set headers after 0x20005.
    face_stride=0x20 if version>0x20005 else 0x1c
    vertex_buffers_off=facesets_off+face_set_count*face_stride
    layouts_off=vertex_buffers_off+vertex_buffer_count*0x20
    textures_off=layouts_off+buffer_layout_count*0x10
    if textures_off<0x80 or textures_off+texture_count*0x20>len(data):
        raise ValueError("FLVER material/texture tables out of range")

    headers=[]
    for slot in range(material_count):
        off=materials_off+slot*0x20
        if off+0x20>len(data):
            raise ValueError(f"material {slot}: truncated header")
        name_off,mtd_off,tex_count,tex_index,flags,gx_off,unk18,zero=struct.unpack_from(
            endian+"8i",data,off)
        if tex_count<0 or tex_index<0 or tex_index+tex_count>texture_count:
            raise ValueError(f"material {slot}: texture range out of bounds")
        # Do not require the last field to be zero for DSR; preserve forward
        # compatibility while still validating every referenced range.
        headers.append((
            slot,
            read_cstr(data,name_off,enc),
            read_cstr(data,mtd_off,enc),
            tex_count,
            tex_index,
        ))

    textures=[]
    for ti in range(texture_count):
        off=textures_off+ti*0x20
        path_off=_i32(data,off,endian)
        type_off=_i32(data,off+4,endian)
        textures.append(TextureBinding(
            read_cstr(data,type_off,enc),
            read_cstr(data,path_off,enc),
        ))

    out=[]
    for slot,name,mtd_path,tex_count,tex_index in headers:
        out.append(Material(slot,name,mtd_path,tuple(textures[tex_index:tex_index+tex_count])))
    return version,out


def identity(member:str,sha:str)->str:
    return f"DSR|{norm_slashes(member)}#{sha}"


def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("zip",type=Path)
    ap.add_argument("--out-dir",type=Path,required=True)
    ap.add_argument("--prefix",default="dsr_flver_ownership_v1")
    a=ap.parse_args()

    a.out_dir.mkdir(parents=True,exist_ok=True)
    raw_path=a.out_dir/f"{a.prefix}_raw.jsonl"
    mat_path=a.out_dir/f"{a.prefix}_materials.tsv"
    proj_path=a.out_dir/f"{a.prefix}_mtd_projection.tsv"
    err_path=a.out_dir/f"{a.prefix}_errors.jsonl"
    sum_path=a.out_dir/f"{a.prefix}_summary.json"

    stats=Counter()
    errors=[]
    rows=[]
    seen_payloads=set()
    versions=Counter()
    mtd_projection=defaultdict(lambda:{
        "flver_identities":set(),
        "material_instances":0,
        "semantics":Counter(),
        "paths":set(),
    })

    with zipfile.ZipFile(a.zip) as z:
        members=[x for x in z.infolist() if not x.is_dir()]
        stats["zip_members"]=len(members)
        for zi in members:
            stats["bytes_uncompressed"]+=zi.file_size
            try:
                data=z.read(zi)
            except Exception as e:
                errors.append({"member":zi.filename,"stage":"ZIP_READ","error":str(e)})
                continue
            try:
                data,layers=unwrap_dcx(data)
                stats["dcx_layers"]+=layers
            except Exception as e:
                errors.append({"member":zi.filename,"stage":"DCX","error":str(e)})
                continue
            if not data.startswith(FLVER_MAGIC):
                stats["non_flver_members"]+=1
                continue
            stats["flver_instances"]+=1
            fsha=sha256_bytes(data)
            if fsha in seen_payloads:
                stats["duplicate_flver_payloads"]+=1
            else:
                seen_payloads.add(fsha)
                stats["unique_flver_payloads"]+=1
            try:
                version,mats=parse_flver2_materials(data)
            except Exception as e:
                errors.append({"member":zi.filename,"stage":"FLVER_PARSE","sha256":fsha,"error":str(e)})
                continue
            versions[f"0x{version:X}"]+=1
            fid=identity(zi.filename,fsha)
            stats["material_instances"]+=len(mats)
            for m in mats:
                mtd_name=basename_any(m.mtd_path)
                binds=m.textures or (TextureBinding("",""),)
                if m.textures:
                    stats["texture_bindings"]+=len(m.textures)
                else:
                    stats["materials_without_textures"]+=1
                proj=mtd_projection[mtd_name.casefold()]
                proj["flver_identities"].add(fid)
                proj["material_instances"]+=1
                proj["paths"].add(norm_slashes(m.mtd_path))
                for t in m.textures:
                    if t.semantic:
                        proj["semantics"][t.semantic]+=1
                for t in binds:
                    rows.append({
                        "game":"DSR",
                        "flver_identity":fid,
                        "flver_member":norm_slashes(zi.filename),
                        "flver_sha256":fsha,
                        "flver_version":f"0x{version:X}",
                        "material_slot":m.slot,
                        "material_name":m.material_name,
                        "mtd_path":norm_slashes(m.mtd_path),
                        "mtd_name":mtd_name,
                        "texture_semantic":t.semantic,
                        "texture_path":norm_slashes(t.path),
                    })

    rows.sort(key=lambda r:(
        r["flver_identity"],r["material_slot"],r["mtd_name"].casefold(),
        r["texture_semantic"],r["texture_path"].casefold()))
    with raw_path.open("w",encoding="utf-8",newline="\n") as f:
        for r in rows:
            f.write(json.dumps(r,sort_keys=True,separators=(",",":"))+"\n")

    cols=("game","flver_identity","flver_member","flver_sha256","flver_version",
          "material_slot","material_name","mtd_path","mtd_name",
          "texture_semantic","texture_path")
    with mat_path.open("w",encoding="utf-8",newline="") as f:
        w=csv.DictWriter(f,fieldnames=cols,delimiter="\t",lineterminator="\n")
        w.writeheader()
        w.writerows(rows)

    with proj_path.open("w",encoding="utf-8",newline="") as f:
        cols2=("mtd_name","flver_identity_count","material_instance_count",
               "mtd_path_count","texture_semantics_json")
        w=csv.DictWriter(f,fieldnames=cols2,delimiter="\t",lineterminator="\n")
        w.writeheader()
        for key,p in sorted(mtd_projection.items()):
            # Preserve one display basename from observed paths.
            names={basename_any(x) for x in p["paths"] if x}
            display=sorted(names,key=str.casefold)[0] if names else key
            w.writerow({
                "mtd_name":display,
                "flver_identity_count":len(p["flver_identities"]),
                "material_instance_count":p["material_instances"],
                "mtd_path_count":len(p["paths"]),
                "texture_semantics_json":json.dumps(dict(sorted(p["semantics"].items())),sort_keys=True,separators=(",",":")),
            })

    with err_path.open("w",encoding="utf-8",newline="\n") as f:
        for e in errors:
            f.write(json.dumps(e,sort_keys=True,separators=(",",":"))+"\n")

    summary={
        "schema":1,
        "game":"DSR",
        "claim_scope":"FLVER_MATERIAL_SLOT_TEXTURE_BINDING_CONSTRUCTION_EVIDENCE",
        "input":{
            "name":a.zip.name,
            "size_bytes":a.zip.stat().st_size,
            "sha256":sha256_file(a.zip),
        },
        "counts":{
            **{k:int(v) for k,v in sorted(stats.items())},
            "canonical_rows":len(rows),
            "parse_errors":len(errors),
            "unique_mtd_names":len(mtd_projection),
        },
        "flver_versions":dict(sorted(versions.items())),
        "outputs":{
            raw_path.name:sha256_file(raw_path),
            mat_path.name:sha256_file(mat_path),
            proj_path.name:sha256_file(proj_path),
            err_path.name:sha256_file(err_path),
        },
        "policy":{
            "missing_texture":"UNKNOWN_NOT_NO_USE",
            "mtd_sha256":"UNRESOLVED_UNTIL_EXACT_DSR_MTD_JOIN",
            "cross_version_homology":"OPEN",
            "runtime_activation":"OPEN",
            "pixel_equivalence":"OPEN",
        },
    }
    sum_path.write_text(json.dumps(summary,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(json.dumps(summary,indent=2,sort_keys=True))
    return 0 if stats["flver_instances"]>0 else 2


if __name__=="__main__":
    raise SystemExit(main())
