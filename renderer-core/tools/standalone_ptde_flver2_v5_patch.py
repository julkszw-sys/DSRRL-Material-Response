#!/usr/bin/env python3
"""Replay V5 FLVER2 support on standalone V4 (offline source tool, NOT addon).

Input V4 sha256=5ff47497212d16831d595d622d047a28cc9bd4d8a1c99bc9093c5482f122bca8
V5 exact binary sha256=f5816b56b2d5a60a5ed076e449f942aab2296e6eaaab61a42476693b5c3a5d2a
No dependencies; no PTDE game file modification.
"""
from pathlib import Path
import hashlib,sys
if len(sys.argv)!=3:raise SystemExit("python standalone_ptde_flver2_v5_patch.py V4.py V5.py")
blob=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(blob).hexdigest()=="5ff47497212d16831d595d622d047a28cc9bd4d8a1c99bc9093c5482f122bca8"
s=blob.decode("utf-8")
def patch(old,new):
 global s
 assert s.count(old)==1,(old[:100],s.count(old))
 s=s.replace(old,new,1)
flver2='''
def flver2(data):
    """FLVER2 mesh-used material -> exact texture ParamName/path bindings.

    Based on FLVER2 structure layout: 0x80 header, 0x40 dummy,
    0x20 material, 0x80 node, 0x30 mesh, version-dependent face sets,
    0x20 vertex buffer, 0x10 layout, 0x20 texture.
    All offsets in material/texture entries are absolute to FLVER start.
    """
    if not data.startswith(b'FLVER\\x00') or data[6:8] not in (b'L\\x00',b'B\\x00'):
        raise ParseError('not a FLVER2 header')
    end = '<' if data[6:7] == b'L' else '>'
    version = u32(data, 8, end)
    if not (0x20000 <= version < 0x30000):
        raise ParseError(f'not FLVER2 version 0x{version:x}')
    dummy_count, mat_count, node_count, mesh_count, vb_count = (
        u32(data, o, end) for o in (0x14, 0x18, 0x1c, 0x20, 0x24))
    face_count, layout_count, texture_count = (u32(data, o, end)
                                                for o in (0x50, 0x54, 0x58))
    allcounts = (dummy_count, mat_count, node_count, mesh_count,
                 vb_count, face_count, layout_count, texture_count)
    if any(x > 50000 for x in allcounts):
        raise ParseError(f'FLVER2 count implausible {allcounts}')
    unicode = bool(u8(data, 0x49))
    if not mat_count or not mesh_count:
        # Valid empty/invisible FLVER2 placeholders occur in stock DSR.
        return []
    # These are serialized consecutively following the 0x80 header.
    mat_start = 0x80 + dummy_count * 0x40
    node_start = mat_start + mat_count * 0x20
    mesh_start = node_start + node_count * 0x80
    face_stride = 0x20 if version > 0x20007 else 0x10
    face_start = mesh_start + mesh_count * 0x30
    vb_start = face_start + face_count * face_stride
    layout_start = vb_start + vb_count * 0x20
    tex_start = layout_start + layout_count * 0x10
    check(data, mat_start, mat_count * 0x20, 'FLVER2 material list')
    check(data, mesh_start, mesh_count * 0x30, 'FLVER2 mesh list')
    check(data, tex_start, texture_count * 0x20, 'FLVER2 texture list')
    used = {i32(data, mesh_start + i * 0x30 + 4, end)
            for i in range(mesh_count)}
    if min(used) < 0 or max(used) >= mat_count:
        raise ParseError(f'FLVER2 mesh material indices {sorted(used)} / {mat_count}')
    mats = []
    for slot in sorted(used):
        off = mat_start + slot * 0x20
        nameoff, mtdoff = u32(data, off, end), u32(data, off+4, end)
        ntex = i32(data, off+8, end)
        first = i32(data, off+12, end)
        if ntex < 0 or first < 0 or first + ntex > texture_count:
            raise ParseError(f'FLVER2 material slot {slot}: invalid texture span {first}+{ntex}/{texture_count}')
        name = string_at(data, nameoff, unicode, end)
        mtd = string_at(data, mtdoff, unicode, end)
        refs = []
        for t in range(first, first + ntex):
            toff = tex_start + t * 0x20
            pathoff, paramoff = u32(data, toff, end), u32(data, toff+4, end)
            param = string_at(data, paramoff, unicode, end) if paramoff else ''
            path = string_at(data, pathoff, unicode, end) if pathoff else ''
            if path:
                refs.append((param, path))
        mats.append((slot, name, mtd, refs))
    return mats

def flver_any(data):
    version = u32(data, 8, '<' if data[6:7] == b'L' else '>')
    if version >= 0x20000:
        return flver2(data)
    return flver0(data)

'''
patch("\ndef tpf(data):\n","\n"+flver2+"\ndef tpf(data):\n")
patch("mats=flver0(body);models+=1","mats=flver_any(body);models+=1")
patch("'BHD5 PTDE, DCP/DCX DFLT, BND3, FLVER0, TPF PC DDS'","'BHD5 PTDE, DCP/DCX DFLT, BND3, FLVER0+FLVER2, TPF PC DDS'")
patch("for suffix in ('.dds','.tpf','.dcx'):","for suffix in ('.dds','.tpf','.dcx','.tga','.tif','.tiff','.png','.bmp','.psd'):")
out=s.encode("utf-8")
assert hashlib.sha256(out).hexdigest()=="f5816b56b2d5a60a5ed076e449f942aab2296e6eaaab61a42476693b5c3a5d2a","V5 output drift"
Path(sys.argv[2]).write_bytes(out)
print("STANDALONE_V5_REPLAY_PASS")
