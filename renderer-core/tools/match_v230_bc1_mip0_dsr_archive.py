#!/usr/bin/env python3
"""Read-only DSR BC1 mip0 SHA match: DDS, TPF, DCX/DFLT BND3, ZIP evidence.
Unknown split TPF BHD/BDT index formats are NOT decoded. Run on extracted TPF.
No new resource authority, PTDE correspondence or addon patch is implied.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile
import zlib

OWNER_SHA = "68136bd6146339192c790f3b058b4fd621b20740b4f1f3a5227b7d0a35f1511d"

def u32(b, at):
    return struct.unpack_from("<I", b, at)[0]

def name_at(b, at):
    if at >= len(b): return None
    end = b.find(b"\0", at, min(len(b), at+256))
    return None if end < 0 else b[at:end].decode("utf-8", "replace")

def unpack_dcx(b):
    return zlib.decompress(b[0x4c:]) if b[:4] == b"DCX\0" and b[0x28:0x2c] == b"DFLT" else b

def bnd3_members(b):
    if b[:4] != b"BND3": return None
    n = u32(b, 0x10)
    if n > 10000: return []
    members = []
    for i in range(n):
        at = 0x20 + 24*i
        if at+24 > len(b): break
        _, length, ofs, _, name_ofs, _ = struct.unpack_from("<IIIIII", b, at)
        name = name_at(b, name_ofs)
        if name and ofs <= len(b) and length <= len(b)-ofs:
            members.append((name, b[ofs:ofs+length]))
    return members

def tpf_members(b):
    if b[:4] != b"TPF\0": return None
    n = u32(b, 8)
    if n > 2000: return []
    members = []
    for i in range(n):
        at = 0x10 + 20*i
        if at+20 > len(b): break
        ofs, length, _, name_ofs, _ = struct.unpack_from("<IIIII", b, at)
        name = name_at(b, name_ofs)
        if name and ofs <= len(b) and length <= len(b)-ofs:
            members.append((name, b[ofs:ofs+length]))
    return members

def inspect_dds(b, at, wanted):
    if b[at:at+4] != b"DDS " or at+148 > len(b): return None
    height, width, mips = u32(b, at+12), u32(b, at+16), u32(b, at+28)
    if (width, height, mips) != (256,256,9): return None
    fourcc = b[at+84:at+88]
    bc1 = fourcc == b"DXT1" or (fourcc == b"DX10" and u32(b,at+128) in (70,71,72))
    if not bc1: return None
    start = at + (148 if fourcc == b"DX10" else 128)
    top = b[start:start+32768]
    if len(top) != 32768: return None
    digest = hashlib.sha256(top).hexdigest()
    return {"sha256":digest,"match":digest==wanted,"width":width,
            "height":height,"mips":mips,"top_bytes":32768}

def scan_blob(b, source, wanted, report):
    try: b = unpack_dcx(b)
    except zlib.error:
        report["invalid_dcx"] += 1
        return
    report["containers"] += 1
    elements = bnd3_members(b)
    if elements is None: elements = [(source,b)]
    for member, raw in elements:
        if member.lower().endswith(".dcx"):
            try: raw = unpack_dcx(raw)
            except zlib.error: continue
        textures = tpf_members(raw)
        if textures is None: textures = [(member,raw)]
        for texture, data in textures:
            at = 0
            while True:
                at = data.find(b"DDS ",at)
                if at < 0: break
                report["dds_seen"] += 1
                item = inspect_dds(data,at,wanted)
                if item is not None:
                    report["bc1_candidates"] += 1
                    if item["match"]:
                        item.update(container=source,member=member,
                                    texture=texture,dds_offset=at)
                        report["matches"].append(item)
                at += 4

def scan_file(p, wanted, report):
    if p.suffix.lower() == ".zip":
        with zipfile.ZipFile(p) as z:
            for member in z.infolist():
                if member.is_dir() or member.file_size > 200_000_000: continue
                if member.filename.lower().endswith((".dcx",".dds",".tpf",".tpfbdt")):
                    scan_blob(z.read(member),p.name+":"+member.filename,wanted,report)
    else:
        scan_blob(p.read_bytes(),str(p),wanted,report)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths",nargs="+",type=Path)
    parser.add_argument("--sha",default=OWNER_SHA)
    parser.add_argument("--json",type=Path)
    opt = parser.parse_args()
    target = opt.sha.lower()
    if len(target)!=64 or any(x not in "0123456789abcdef" for x in target):
        parser.error("SHA must be 64 hexadecimal characters")
    report={"sha256":target,"containers":0,"dds_seen":0,
            "bc1_candidates":0,"invalid_dcx":0,"matches":[],
            "coverage_note":"Split TPF BHD/BDT indexes not decoded; extract first."}
    files=[]
    for path in opt.paths:
        if path.is_dir():
            files.extend(p for p in path.rglob("*") if p.is_file() and
                         p.suffix.lower() in (".zip",".dcx",".dds",".tpf",".tpfbdt"))
        else: files.append(path)
    for p in files:
        try: scan_file(p,target,report)
        except (OSError,ValueError,zipfile.BadZipFile,MemoryError) as exc:
            print("SKIP",p,exc,file=sys.stderr)
    output=json.dumps(report,indent=2,ensure_ascii=False)
    if opt.json: opt.json.write_text(output+"\n",encoding="utf-8")
    print(output)

if __name__=="__main__": main()
