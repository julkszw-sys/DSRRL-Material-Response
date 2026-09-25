#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


def build_fixture() -> bytes:
    data=bytearray(0x200)
    data[0:6]=b"FLVER\x00"
    data[6:8]=b"L\x00"
    struct.pack_into("<i",data,8,0x2000D)
    struct.pack_into("<i",data,0x14,0)  # dummy
    struct.pack_into("<i",data,0x18,1)  # material
    struct.pack_into("<i",data,0x1c,0)  # bone
    struct.pack_into("<i",data,0x20,0)  # mesh
    struct.pack_into("<i",data,0x24,0)  # vertex buffers
    data[0x49]=0                       # non-Unicode strings
    struct.pack_into("<i",data,0x50,0) # face sets
    struct.pack_into("<i",data,0x54,0) # layouts
    struct.pack_into("<i",data,0x58,2) # textures

    material_off=0x80
    texture0_off=0xa0
    texture1_off=0xc0
    cursor=0xe0

    def put(s:str)->int:
        nonlocal cursor
        raw=s.encode("ascii")+b"\x00"
        off=cursor
        data[off:off+len(raw)]=raw
        cursor+=len(raw)
        return off

    mat_name=put("ArmorMetal")
    mtd_path=put(r"N:\FRPG\data\Material\mtd\P_Metal[DSB].mtd")
    d_path=put(r"N:\FRPG\data\INTERROOT_win32\parts\HD_A_0000_d.tpf")
    d_sem=put("g_Diffuse")
    s_path=put(r"N:\FRPG\data\INTERROOT_win32\parts\HD_A_0000_s.tpf")
    s_sem=put("g_Specular")

    struct.pack_into("<8i",data,material_off,
                     mat_name,mtd_path,2,0,0,0,0,0)
    struct.pack_into("<2i",data,texture0_off,d_path,d_sem)
    struct.pack_into("<2i",data,texture1_off,s_path,s_sem)
    return bytes(data[:cursor])


def main()->int:
    repo=Path(__file__).resolve().parents[2]
    tool=repo/"renderer-core/tools/dsr_flver_zip_ownership_census.py"
    with tempfile.TemporaryDirectory() as td:
        root=Path(td)
        zp=root/"fixture.zip"
        with zipfile.ZipFile(zp,"w",compression=zipfile.ZIP_DEFLATED) as z:
            z.writestr("parts/HD_A_0000.flver",build_fixture())
            z.writestr("notes/readme.txt",b"not a flver")
        out=root/"out"
        subprocess.run(
            [sys.executable,str(tool),str(zp),"--out-dir",str(out),"--prefix","fixture"],
            check=True,
        )
        summary=json.loads((out/"fixture_summary.json").read_text(encoding="utf-8"))
        assert summary["counts"]["flver_instances"]==1
        assert summary["counts"]["unique_flver_payloads"]==1
        assert summary["counts"]["material_instances"]==1
        assert summary["counts"]["texture_bindings"]==2
        assert summary["counts"]["canonical_rows"]==2
        assert summary["counts"]["non_flver_members"]==1
        assert summary["counts"]["parse_errors"]==0
        with (out/"fixture_materials.tsv").open(encoding="utf-8",newline="") as f:
            rows=list(csv.DictReader(f,delimiter="\t"))
        assert [r["texture_semantic"] for r in rows]==["g_Diffuse","g_Specular"]
        assert all(r["material_slot"]=="0" for r in rows)
        assert all(r["mtd_name"]=="P_Metal[DSB].mtd" for r in rows)
    print("dsr_flver_zip_ownership_census: PASS")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
